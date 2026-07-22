"""Tournament Manager and Automated Match Execution Engine for LugalChess GUI."""

import time
from typing import Any
import chess
import chess.pgn
from PySide6.QtCore import QObject, QThread, Signal, Slot

from lugalgui.controllers.uci_controller import UCIController
from lugalgui.controllers.xboard_adapter import XBoardAdapter
from lugalgui.models.elo_rating import EloRatingCalculator, EngineEloStats
from lugalgui.models.engine_registry import EngineInfo, EngineRegistry


class TournamentMatchWorker(QThread):
    """Worker thread running a series of automated engine vs engine chess games."""

    match_started = Signal(int, int, str, str)  # (game_idx, total_games, white_name, black_name)
    move_played = Signal(str, str, str)          # (white_name, black_name, uci_move)
    match_finished = Signal(int, str, str, float, str) # (game_idx, white_name, black_name, result, pgn_str)
    tournament_finished = Signal(list, dict)    # (pgn_list, elo_stats)

    def __init__(
        self,
        pairing_schedule: list[tuple[EngineInfo, EngineInfo]],
        time_limit_ms: int = 2000,
        parent: QObject | None = None,
    ) -> None:
        super().__init__(parent)
        self.pairing_schedule: list[tuple[EngineInfo, EngineInfo]] = pairing_schedule
        self.time_limit_ms: int = time_limit_ms
        self._is_cancelled: bool = False
        self.results: list[tuple[str, str, float]] = []
        self.pgn_games: list[str] = []

    def cancel(self) -> None:
        self._is_cancelled = True

    def run(self) -> None:
        """Execute all scheduled matches sequentially."""
        total_games = len(self.pairing_schedule)
        participant_names = list({e.name for pair in self.pairing_schedule for e in pair})

        for game_idx, (white_info, black_info) in enumerate(self.pairing_schedule, start=1):
            if self._is_cancelled:
                break

            self.match_started.emit(game_idx, total_games, white_info.name, black_info.name)

            # Instantiate engine controllers
            white_ctrl = self._create_controller(white_info)
            black_ctrl = self._create_controller(black_info)

            if not white_ctrl.start_engine() or not black_ctrl.start_engine():
                self._stop_controllers(white_ctrl, black_ctrl)
                continue

            board = chess.Board()
            game_pgn = chess.pgn.Game()
            game_pgn.headers["Event"] = "LugalChess Engine Tournament"
            game_pgn.headers["Site"] = "LugalChess GUI"
            game_pgn.headers["White"] = white_info.name
            game_pgn.headers["Black"] = black_info.name
            pgn_node = game_pgn

            half_move_count = 0
            max_moves = 200
            result_score = 0.5

            while not board.is_game_over() and half_move_count < max_moves and not self._is_cancelled:
                active_ctrl = white_ctrl if board.turn == chess.WHITE else black_ctrl
                active_name = white_info.name if board.turn == chess.WHITE else black_info.name

                moves_uci = [m.uci() for m in board.move_stack]
                active_ctrl.set_position(moves=moves_uci)

                # Wait for move from active engine
                chosen_move_uci = self._get_engine_move(active_ctrl, self.time_limit_ms)

                if not chosen_move_uci:
                    # Engine failed / timed out -> forfeit
                    result_score = 0.0 if board.turn == chess.WHITE else 1.0
                    break

                try:
                    move = chess.Move.from_uci(chosen_move_uci)
                    if move in board.legal_moves:
                        board.push(move)
                        pgn_node = pgn_node.add_main_variation(move)
                        self.move_played.emit(white_info.name, black_info.name, chosen_move_uci)
                    else:
                        # Illegal move -> forfeit
                        result_score = 0.0 if board.turn == chess.WHITE else 1.0
                        break
                except ValueError:
                    result_score = 0.0 if board.turn == chess.WHITE else 1.0
                    break

                half_move_count += 1
                time.sleep(0.05)

            # Determine final game result
            if board.is_checkmate():
                result_score = 1.0 if board.turn == chess.BLACK else 0.0
                res_str = "1-0" if result_score == 1.0 else "0-1"
            elif board.is_game_over() or half_move_count >= max_moves:
                result_score = 0.5
                res_str = "1/2-1/2"
            else:
                res_str = "1-0" if result_score == 1.0 else "0-1"

            game_pgn.headers["Result"] = res_str
            pgn_text = str(game_pgn)
            self.pgn_games.append(pgn_text)
            self.results.append((white_info.name, black_info.name, result_score))

            self._stop_controllers(white_ctrl, black_ctrl)
            self.match_finished.emit(game_idx, white_info.name, black_info.name, result_score, pgn_text)
            time.sleep(0.2)

        # Calculate final ELO ratings
        elo_stats = EloRatingCalculator.calculate_ratings(participant_names, self.results)
        self.tournament_finished.emit(self.pgn_games, elo_stats)

    def _create_controller(self, info: EngineInfo) -> Any:
        """Instantiate UCIController or XBoardAdapter based on engine protocol."""
        if info.is_xboard:
            return XBoardAdapter(info.path, args=info.args)
        else:
            ctrl = UCIController()
            ctrl.engine_path = info.path
            ctrl.engine_args = info.args
            return ctrl

    def _stop_controllers(self, c1: Any, c2: Any) -> None:
        """Cleanly stop both engine processes."""
        try:
            c1.stop_engine()
        except Exception:
            pass
        try:
            c2.stop_engine()
        except Exception:
            pass

    def _get_engine_move(self, controller: Any, timeout_ms: int) -> str | None:
        """Request move from engine and wait synchronously on worker thread."""
        chosen_move: list[str | None] = [None]

        def on_best_move(move: str, ponder: str) -> None:
            chosen_move[0] = move

        controller.best_move_found.connect(on_best_move)
        controller.start_search_time(timeout_ms)

        start_t = time.time()
        timeout_sec = (timeout_ms / 1000.0) + 2.0  # 2s safety buffer
        while chosen_move[0] is None and (time.time() - start_t) < timeout_sec:
            time.sleep(0.02)

        try:
            controller.best_move_found.disconnect(on_best_move)
        except Exception:
            pass

        return chosen_move[0]


class TournamentManager(QObject):
    """Manages tournament configuration, pairings, and worker execution."""

    match_started = Signal(int, int, str, str)
    move_played = Signal(str, str, str)
    match_finished = Signal(int, str, str, float, str)
    tournament_finished = Signal(list, dict)

    def __init__(self, engine_registry: EngineRegistry, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.engine_registry: EngineRegistry = engine_registry
        self.worker: TournamentMatchWorker | None = None

    def generate_pairings(self, selected_engines: list[EngineInfo], format_type: str, rounds: int = 1) -> list[tuple[EngineInfo, EngineInfo]]:
        """Generate scheduled match pairings for Round-Robin, Double Round-Robin, or Gauntlet."""
        pairings: list[tuple[EngineInfo, EngineInfo]] = []
        n = len(selected_engines)
        if n < 2:
            return pairings

        if format_type in ("Round-Robin", "Double Round-Robin"):
            repeat = 2 if format_type == "Double Round-Robin" else rounds
            for _ in range(repeat):
                for i in range(n):
                    for j in range(i + 1, n):
                        # Play game 1: Engine i (White) vs Engine j (Black)
                        pairings.append((selected_engines[i], selected_engines[j]))
                        # Play game 2: Engine j (White) vs Engine i (Black)
                        pairings.append((selected_engines[j], selected_engines[i]))

        elif format_type == "Gauntlet":
            # First engine is the gauntlet hero, playing against all other engines
            hero = selected_engines[0]
            challengers = selected_engines[1:]
            for _ in range(rounds):
                for ch in challengers:
                    pairings.append((hero, ch))
                    pairings.append((ch, hero))

        return pairings

    def start_tournament(self, pairings: list[tuple[EngineInfo, EngineInfo]], time_limit_ms: int = 2000) -> bool:
        """Launch background tournament worker thread."""
        if self.worker and self.worker.isRunning():
            return False

        self.worker = TournamentMatchWorker(pairings, time_limit_ms=time_limit_ms, parent=self)
        self.worker.match_started.connect(self.match_started.emit)
        self.worker.move_played.connect(self.move_played.emit)
        self.worker.match_finished.connect(self.match_finished.emit)
        self.worker.tournament_finished.connect(self.tournament_finished.emit)
        self.worker.start()
        return True

    def stop_tournament(self) -> None:
        """Cancel ongoing tournament."""
        if self.worker and self.worker.isRunning():
            self.worker.cancel()
            self.worker.wait(2000)
            self.worker = None
