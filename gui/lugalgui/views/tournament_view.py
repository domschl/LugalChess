"""Tournament Management and Live ELO Leaderboard View Widget for LugalChess GUI."""

import os
from typing import Any
import chess
from PySide6.QtCore import Qt, Slot
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFileDialog,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSplitter,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from lugalgui.models.elo_rating import EloRatingCalculator, EngineEloStats
from lugalgui.models.engine_registry import EngineInfo, EngineRegistry
from lugalgui.models.tournament_manager import TournamentManager
from lugalgui.views.board_widget import ChessBoardWidget


class TournamentView(QWidget):
    """Interactive Engine Tournament & ELO Rating Workspace Window."""

    def __init__(self, engine_registry: EngineRegistry, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Engine Tournaments & ELO Ratings Workspace")
        self.resize(950, 680)

        self.engine_registry: EngineRegistry = engine_registry
        self.tournament_manager: TournamentManager = TournamentManager(self.engine_registry, self)
        self.current_board: chess.Board = chess.Board()
        self.saved_pgn_text: str = ""

        self._init_ui()
        self._connect_signals()
        self.refresh_engine_list()

    def _init_ui(self) -> None:
        """Construct tabbed layout: Setup, Live Matches, and Standings."""
        self.tab_widget = QTabWidget(self)

        # TAB 1: Tournament Setup
        setup_tab = QWidget()
        setup_layout = QVBoxLayout(setup_tab)

        engine_group = QGroupBox("Select Participating Engines & Preset Rating Tiers")
        eng_group_layout = QVBoxLayout(engine_group)
        self.engine_list_widget = QListWidget(self)
        eng_group_layout.addWidget(self.engine_list_widget)

        config_group = QGroupBox("Tournament Settings")
        config_layout = QHBoxLayout(config_group)

        config_layout.addWidget(QLabel("Format:"))
        self.format_combo = QComboBox(self)
        self.format_combo.addItems(["Round-Robin", "Double Round-Robin", "Gauntlet"])
        config_layout.addWidget(self.format_combo)

        config_layout.addWidget(QLabel("Time per Move:"))
        self.time_combo = QComboBox(self)
        self.time_combo.addItem("1.0s / move", 1000)
        self.time_combo.addItem("2.0s / move", 2000)
        self.time_combo.addItem("3.0s / move", 3000)
        self.time_combo.addItem("5.0s / move", 5000)
        self.time_combo.setCurrentIndex(1)
        config_layout.addWidget(self.time_combo)

        btn_layout = QHBoxLayout()
        self.btn_start = QPushButton("▶ Start Tournament", self)
        self.btn_stop = QPushButton("⏹ Stop", self)
        self.btn_stop.setEnabled(False)

        self.btn_start.setStyleSheet("background-color: #008800; color: white; font-weight: bold; padding: 6px;")
        self.btn_stop.setStyleSheet("background-color: #CC0000; color: white; font-weight: bold; padding: 6px;")

        btn_layout.addWidget(self.btn_start)
        btn_layout.addWidget(self.btn_stop)

        setup_layout.addWidget(engine_group, stretch=2)
        setup_layout.addWidget(config_group)
        setup_layout.addLayout(btn_layout)

        # TAB 2: Live Matches View
        live_tab = QWidget()
        live_layout = QVBoxLayout(live_tab)

        self.progress_bar = QProgressBar(self)
        self.progress_bar.setValue(0)
        self.status_game_label = QLabel("No active tournament running.", self)
        self.status_game_label.setStyleSheet("font-weight: bold; color: #0055AA;")

        splitter = QSplitter(Qt.Orientation.Horizontal, self)
        self.mini_board_widget = ChessBoardWidget(self)
        self.mini_board_widget.set_board(self.current_board)

        self.live_pgn_edit = QTextEdit(self)
        self.live_pgn_edit.setReadOnly(True)

        splitter.addWidget(self.mini_board_widget)
        splitter.addWidget(self.live_pgn_edit)
        splitter.setSizes([450, 450])

        live_layout.addWidget(self.status_game_label)
        live_layout.addWidget(self.progress_bar)
        live_layout.addWidget(splitter, stretch=1)

        # TAB 3: ELO Standings & Cross-Table
        standings_tab = QWidget()
        standings_layout = QVBoxLayout(standings_tab)

        self.table_standings = QTableWidget(self)
        self.table_standings.setColumnCount(10)
        self.table_standings.setHorizontalHeaderLabels([
            "Rank", "Engine", "Points", "Played", "Wins", "Draws", "Losses", "Score %", "Elo Rating", "± Error"
        ])
        self.table_standings.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)

        self.btn_export_pgn = QPushButton("💾 Export Tournament PGN", self)
        self.btn_export_pgn.setEnabled(False)
        self.btn_export_pgn.clicked.connect(self.on_export_pgn)

        standings_layout.addWidget(self.table_standings, stretch=1)
        standings_layout.addWidget(self.btn_export_pgn)

        self.tab_widget.addTab(setup_tab, "Tournament Setup")
        self.tab_widget.addTab(live_tab, "Live Match View")
        self.tab_widget.addTab(standings_tab, "ELO Standings & Cross-Table")

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(6, 6, 6, 6)
        main_layout.addWidget(self.tab_widget)

    def _connect_signals(self) -> None:
        """Connect buttons and tournament manager signals."""
        self.btn_start.clicked.connect(self.on_start_clicked)
        self.btn_stop.clicked.connect(self.on_stop_clicked)
        self.engine_registry.registry_updated.connect(self.refresh_engine_list)

        self.tournament_manager.match_started.connect(self.on_match_started)
        self.tournament_manager.move_played.connect(self.on_move_played)
        self.tournament_manager.match_finished.connect(self.on_match_finished)
        self.tournament_manager.tournament_finished.connect(self.on_tournament_finished)

    def refresh_engine_list(self) -> None:
        """Refresh list of available engine targets and rating presets."""
        self.engine_list_widget.clear()
        for eng in self.engine_registry.engines:
            display_str = f"{eng.name} ({eng.path})"
            is_checked = True

            if eng.is_hardware:
                from lugalgui.controllers.rp2350_controller import RP2350Controller
                test_ctrl = RP2350Controller()
                if test_ctrl.is_connected:
                    display_str = f"{eng.name} ({test_ctrl.port_name})"
                    is_checked = True
                else:
                    display_str = f"{eng.name} (Disconnected USB Serial)"
                    is_checked = False

            item = QListWidgetItem(display_str)
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked if is_checked else Qt.CheckState.Unchecked)
            item.setData(Qt.ItemDataRole.UserRole, eng)
            self.engine_list_widget.addItem(item)

    def get_selected_engines(self) -> list[EngineInfo]:
        """Return list of EngineInfo presets checked by user."""
        selected: list[EngineInfo] = []
        for i in range(self.engine_list_widget.count()):
            item = self.engine_list_widget.item(i)
            if item and item.checkState() == Qt.CheckState.Checked:
                eng: EngineInfo = item.data(Qt.ItemDataRole.UserRole)
                selected.append(eng)
        return selected

    @Slot()
    def on_start_clicked(self) -> None:
        """Start scheduled tournament."""
        selected_engines = self.get_selected_engines()
        if len(selected_engines) < 2:
            QMessageBox.warning(self, "Insufficient Engines", "Please check at least 2 engines to start a tournament!")
            return

        fmt = self.format_combo.currentText()
        time_limit_ms = self.time_combo.currentData()

        pairings = self.tournament_manager.generate_pairings(selected_engines, fmt)
        if not pairings:
            QMessageBox.warning(self, "No Pairings", "Failed to generate match pairings!")
            return

        if self.tournament_manager.start_tournament(pairings, time_limit_ms=time_limit_ms):
            self.btn_start.setEnabled(False)
            self.btn_stop.setEnabled(True)
            self.tab_widget.setCurrentIndex(1)  # Switch to Live Match tab
            self.progress_bar.setRange(0, len(pairings))
            self.progress_bar.setValue(0)
            self._render_cross_table_matrix()

    def closeEvent(self, event: Any) -> None:
        """Ensure running tournament worker thread stops on window close."""
        self.tournament_manager.stop_tournament()
        super().closeEvent(event)

    @Slot()
    def on_stop_clicked(self) -> None:
        """Stop running tournament."""
        self.tournament_manager.stop_tournament()
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self.status_game_label.setText("Tournament stopped by user.")

    @Slot(int, int, str, str)
    def on_match_started(self, game_idx: int, total_games: int, white_name: str, black_name: str) -> None:
        """Update UI when new match begins."""
        self.progress_bar.setValue(game_idx - 1)
        self.status_game_label.setText(f"Game {game_idx} of {total_games}: {white_name} (White) vs {black_name} (Black)")
        self.current_board = chess.Board()
        self.mini_board_widget.set_board(self.current_board)
        self.live_pgn_edit.setPlainText(f"[Event \"LugalChess Tournament\"]\n[White \"{white_name}\"]\n[Black \"{black_name}\"]\n\n")

    @Slot(str, str, str)
    def on_move_played(self, white_name: str, black_name: str, uci_move: str) -> None:
        """Apply move to live mini board and update live PGN text."""
        try:
            m = chess.Move.from_uci(uci_move)
            if m in self.current_board.legal_moves:
                san_str = self.current_board.san(m)
                self.current_board.push(m)
                self.mini_board_widget.set_board(self.current_board, last_move=m)
                self.mini_board_widget.update()

                txt = self.live_pgn_edit.toPlainText()
                ply = len(self.current_board.move_stack)
                move_num = (ply + 1) // 2
                if ply % 2 == 1:
                    txt += f"{move_num}. {san_str} "
                else:
                    txt += f"{san_str} "
                self.live_pgn_edit.setPlainText(txt)
                scrollbar = self.live_pgn_edit.verticalScrollBar()
                if scrollbar:
                    scrollbar.setValue(scrollbar.maximum())
        except ValueError:
            pass

    @Slot(int, str, str, float, str)
    def on_match_finished(self, game_idx: int, white_name: str, black_name: str, score: float, pgn_str: str) -> None:
        """Called when a single match finishes."""
        self.progress_bar.setValue(game_idx)
        res_label = "1-0" if score == 1.0 else ("0-1" if score == 0.0 else "1/2-1/2")
        txt = self.live_pgn_edit.toPlainText() + f" {res_label}\n"
        self.live_pgn_edit.setPlainText(txt)

        # Update cross-table matrix in real-time during ongoing tournament
        self._render_cross_table_matrix()

    @Slot(list, dict)
    def on_tournament_finished(self, pgn_list: list, elo_stats: dict[str, EngineEloStats]) -> None:
        """Called when all tournament games complete."""
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self.btn_export_pgn.setEnabled(True)
        self.saved_pgn_text = "\n\n".join(pgn_list)
        self.status_game_label.setText("Tournament Complete! Standings updated.")

        # Update final Cross-Table Matrix Standings
        self._render_cross_table_matrix()
        self.tab_widget.setCurrentIndex(2)  # Switch to ELO Standings tab

    def _render_cross_table_matrix(self) -> None:
        """Render standard tournament N x N cross-table matrix with cumulative stats & ELO ratings."""
        worker = self.tournament_manager.worker
        if not worker:
            return

        participant_names = list({e.name for pair in worker.pairing_schedule for e in pair})
        match_results = list(worker.results)  # list of (white_name, black_name, score)

        if not participant_names:
            return

        # Build assumed benchmark ELO map for rating scale anchoring
        initial_elos = {eng.name: getattr(eng, "assumed_elo", 1500.0) for eng in self.engine_registry.engines}

        # Compute live ratings & W-D-L stats anchored to official rating scale
        elo_stats = EloRatingCalculator.calculate_ratings(participant_names, match_results, initial_elos=initial_elos)
        sorted_stats = sorted(elo_stats.values(), key=lambda s: (s.points, s.elo), reverse=True)
        sorted_names = [s.name for s in sorted_stats]

        # Build head-to-head match matrix: h2h[white_name][black_name] -> list of score strings
        h2h: dict[str, dict[str, list[str]]] = {n1: {n2: [] for n2 in participant_names} for n1 in participant_names}
        for w_name, b_name, score in match_results:
            w_score_str = "1" if score == 1.0 else ("0" if score == 0.0 else "½")
            b_score_str = "0" if score == 1.0 else ("1" if score == 0.0 else "½")
            h2h[w_name][b_name].append(w_score_str)
            h2h[b_name][w_name].append(b_score_str)

        num_participants = len(sorted_names)

        # Columns: Rank, Engine, 1, 2, ..., N, Pts, W-D-L, Score %, ELO, ± Error
        headers = ["Rank", "Engine"] + [str(i + 1) for i in range(num_participants)] + ["Pts", "W-D-L", "Score %", "ELO", "± Error"]
        self.table_standings.setColumnCount(len(headers))
        self.table_standings.setHorizontalHeaderLabels(headers)
        self.table_standings.setRowCount(num_participants)

        # Style headers
        header_view = self.table_standings.horizontalHeader()
        header_view.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header_view.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        for c in range(2, len(headers)):
            header_view.setSectionResizeMode(c, QHeaderView.ResizeMode.ResizeToContents)

        for row_idx, s in enumerate(sorted_stats):
            engine_name = s.name
            rank_num = row_idx + 1

            # 0: Rank
            item_rank = QTableWidgetItem(str(rank_num))
            item_rank.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, 0, item_rank)

            # 1: Engine Name
            item_name = QTableWidgetItem(engine_name)
            self.table_standings.setItem(row_idx, 1, item_name)

            # 2 .. 2 + N - 1: N x N Matrix Cells
            for col_idx, opp_name in enumerate(sorted_names):
                cell_col = 2 + col_idx
                if engine_name == opp_name:
                    # Diagonal cell (X)
                    item_diag = QTableWidgetItem("✕")
                    item_diag.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                    item_diag.setBackground(Qt.GlobalColor.darkGray)
                    item_diag.setForeground(Qt.GlobalColor.white)
                    self.table_standings.setItem(row_idx, cell_col, item_diag)
                else:
                    scores = h2h[engine_name][opp_name]
                    cell_text = ", ".join(scores) if scores else "-"
                    item_cell = QTableWidgetItem(cell_text)
                    item_cell.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                    self.table_standings.setItem(row_idx, cell_col, item_cell)

            # Cumulative Columns
            base_col = 2 + num_participants

            # Pts
            item_pts = QTableWidgetItem(f"{s.points:.1f}")
            item_pts.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, base_col, item_pts)

            # W-D-L
            item_wdl = QTableWidgetItem(f"{s.wins}-{s.draws}-{s.losses}")
            item_wdl.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, base_col + 1, item_wdl)

            # Score %
            item_pct = QTableWidgetItem(f"{s.score_percentage:.1f}%")
            item_pct.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, base_col + 2, item_pct)

            # ELO
            item_elo = QTableWidgetItem(f"{s.elo:.1f}")
            item_elo.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, base_col + 3, item_elo)

            # ± Error
            item_err = QTableWidgetItem(f"±{s.error:.1f}")
            item_err.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table_standings.setItem(row_idx, base_col + 4, item_err)

    @Slot()
    def on_export_pgn(self) -> None:
        """Export tournament PGN text to a file."""
        if not self.saved_pgn_text:
            return
        filename, _ = QFileDialog.getSaveFileName(self, "Export Tournament PGN", "tournament.pgn", "PGN Files (*.pgn)")
        if filename:
            with open(filename, "w", encoding="utf-8") as f:
                f.write(self.saved_pgn_text)
            QMessageBox.information(self, "Export Successful", f"Saved PGN to {filename}")
