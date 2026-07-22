"""XBoard-to-UCI Protocol Adapter for LugalChess GUI.

Enables seamless execution of legacy XBoard/WinBoard chess engines (e.g. Crafty)
within standard UCI GUIs and analysis panels.
"""

import os
import re
import subprocess
import threading
import time
from typing import Any
import chess
from PySide6.QtCore import QObject, Signal


class XBoardAdapter(QObject):
    """Translates UCI protocol commands to XBoard protocol and vice-versa."""

    engine_started = Signal(str)
    engine_stopped = Signal()
    search_progress = Signal(dict)
    best_move_found = Signal(str, str)
    log_received = Signal(str)

    def __init__(self, engine_path: str, args: list[str] | None = None, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.engine_path: str = engine_path
        self.engine_args: list[str] = args or []
        self.process: subprocess.Popen[str] | None = None
        self.reader_thread: threading.Thread | None = None
        self._is_running: bool = False
        self.engine_name: str = "XBoard Engine"
        self.current_board: chess.Board = chess.Board()

        # Features supported by engine
        self.supports_setboard: bool = False
        self.supports_ping: bool = False
        self.myname: str = ""

        # Pending UCI state
        self._pending_fen: str | None = None
        self._pending_moves: list[str] = []

    @property
    def is_running(self) -> bool:
        return self._is_running and self.process is not None

    def start_engine(self) -> bool:
        """Launch the XBoard engine binary process and initiate handshake."""
        if not self.engine_path or not os.path.exists(self.engine_path):
            self.log_received.emit(f"Engine executable not found: {self.engine_path}")
            return False

        self.stop_engine()

        cmd = [self.engine_path] + self.engine_args
        try:
            self.process = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )
            self._is_running = True

            # Start stdout reader thread
            self.reader_thread = threading.Thread(target=self._read_stdout_loop, daemon=True)
            self.reader_thread.start()

            # Initialize XBoard handshake
            self._send_xboard_cmd("xboard")
            self._send_xboard_cmd("protover 2")

            self.engine_name = os.path.basename(self.engine_path).capitalize() + " (XBoard Adapter)"
            self.engine_started.emit(self.engine_name)
            return True
        except Exception as e:
            self.log_received.emit(f"Failed to launch XBoard engine: {e}")
            return False

    def stop_engine(self) -> None:
        """Stop XBoard engine process."""
        self._is_running = False
        if self.process:
            try:
                self._send_xboard_cmd("quit")
                self.process.terminate()
                self.process.wait(timeout=1.0)
            except Exception:
                if self.process:
                    self.process.kill()
            self.process = None
        self.engine_stopped.emit()

    def _send_xboard_cmd(self, cmd: str) -> None:
        """Send raw line command to XBoard engine stdin."""
        if self.process and self.process.stdin and self._is_running:
            try:
                self.process.stdin.write(cmd.strip() + "\n")
                self.process.stdin.flush()
                self.log_received.emit(f"XBoard > {cmd}")
            except Exception as e:
                self.log_received.emit(f"Error writing to XBoard stdin: {e}")

    def set_position(self, fen: str | None = None, moves: list[str] | None = None) -> None:
        """Translate UCI position command to XBoard setboard or move sequence."""
        self._pending_fen = fen
        self._pending_moves = moves or []
        self.current_board = chess.Board(fen or chess.STARTING_FEN)
        if moves:
            for m in moves:
                try:
                    mv = chess.Move.from_uci(m)
                    if mv in self.current_board.legal_moves:
                        self.current_board.push(mv)
                except ValueError:
                    try:
                        mv = self.current_board.parse_san(m)
                        if mv in self.current_board.legal_moves:
                            self.current_board.push(mv)
                    except ValueError:
                        pass

    def start_search_time(self, time_limit_ms: int) -> None:
        """Start XBoard engine search with time limit."""
        if not self.is_running:
            return

        sec = max(1, time_limit_ms // 1000)
        self._send_xboard_cmd("force")

        if self._pending_fen and self.supports_setboard:
            self._send_xboard_cmd(f"setboard {self._pending_fen}")
        else:
            for m in self._pending_moves:
                self._send_xboard_cmd(m)

        # Set time per move (st command in XBoard)
        self._send_xboard_cmd(f"st {sec}")
        self._send_xboard_cmd("go")

    def start_search_depth(self, depth: int) -> None:
        """Start XBoard engine search with depth limit."""
        if not self.is_running:
            return

        self._send_xboard_cmd("force")
        if self._pending_fen and self.supports_setboard:
            self._send_xboard_cmd(f"setboard {self._pending_fen}")
        else:
            for m in self._pending_moves:
                self._send_xboard_cmd(m)

        self._send_xboard_cmd(f"sd {depth}")
        self._send_xboard_cmd("go")

    def stop_search(self) -> None:
        """Interrupt active search."""
        if self.is_running:
            self._send_xboard_cmd("?")
            self._send_xboard_cmd("force")

    def set_multipv(self, count: int) -> None:
        """MultiPV placeholder (not natively supported in standard XBoard v2)."""
        pass

    def _read_stdout_loop(self) -> None:
        """Read and translate stdout stream from XBoard engine."""
        while self._is_running and self.process and self.process.stdout:
            line = self.process.stdout.readline()
            if not line:
                break

            line_str = line.strip()
            if line_str:
                self.log_received.emit(f"XBoard < {line_str}")
                self._parse_xboard_line(line_str)

    def _parse_xboard_line(self, line: str) -> None:
        """Parse XBoard line and translate to UCI progress or bestmove events."""
        # 1. Feature detection
        if line.startswith("feature "):
            if "setboard=1" in line:
                self.supports_setboard = True
            if "ping=1" in line:
                self.supports_ping = True
            m = re.search(r'myname="([^"]+)"', line)
            if m:
                self.engine_name = f"{m.group(1)} (XBoard Adapter)"

        # 2. Engine move output: "move c5" or "move e2e4" or "My move is: c5"
        elif line.startswith("move ") or "move is:" in line.lower():
            parts = line.split()
            raw_move = parts[1] if line.startswith("move ") else parts[-1]
            
            uci_move = ""
            try:
                m_obj = chess.Move.from_uci(raw_move)
                if m_obj in self.current_board.legal_moves:
                    uci_move = m_obj.uci()
            except ValueError:
                pass

            if not uci_move:
                try:
                    m_obj = self.current_board.parse_san(raw_move)
                    if m_obj in self.current_board.legal_moves:
                        uci_move = m_obj.uci()
                except ValueError:
                    uci_move = raw_move

            if uci_move:
                self.best_move_found.emit(uci_move, "")

        # 3. Thinking output line parsing (e.g. Crafty: " 12   +45   300   14500   e2e4 c7c5 g1f3")
        # Format: ply score_cp time_cs nodes pv...
        tokens = line.split()
        if len(tokens) >= 5 and tokens[0].isdigit():
            try:
                depth = int(tokens[0])
                score_str = tokens[1]
                
                score_cp = 0
                if score_str.startswith("+") or score_str.startswith("-") or score_str.lstrip("-").isdigit():
                    if "." in score_str:
                        score_cp = int(float(score_str) * 100)
                    else:
                        score_cp = int(score_str)

                pv_moves = tokens[4:]
                valid_pv = [m for m in pv_moves if len(m) >= 2]

                info_dict = {
                    "depth": depth,
                    "score_cp": score_cp,
                    "pv": valid_pv
                }
                self.search_progress.emit(info_dict)
            except ValueError:
                pass
