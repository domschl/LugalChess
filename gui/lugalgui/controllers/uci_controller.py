"""Asynchronous UCI Engine Controller Manager."""

import os
import subprocess
import threading
from typing import Any
from PySide6.QtCore import QObject, Signal


class UCIController(QObject):
    """Manages an external or internal UCI engine subprocess and handles non-blocking communication."""

    # Qt Signals emitted to GUI widgets
    engine_started = Signal(str)
    engine_stopped = Signal()
    search_progress = Signal(dict)  # depth, score, nps, nodes, time, pv
    best_move_found = Signal(str, str)  # best_move_uci, ponder_move_uci
    log_received = Signal(str)

    def __init__(self, engine_path: str | None = None, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.engine_path: str | None = engine_path
        self.engine_args: list[str] = []
        self.process: subprocess.Popen[str] | None = None
        self.reader_thread: threading.Thread | None = None
        self._is_running: bool = False
        self.engine_name: str = "Unknown Engine"
        self.engine_author: str = "Unknown Author"
        self.options: dict[str, Any] = {}

    def start_engine(self, engine_executable_path: str | None = None) -> bool:
        """Launch the UCI engine binary process."""
        if engine_executable_path:
            self.engine_path = engine_executable_path
            
        if not self.engine_path or not os.path.exists(self.engine_path):
            self.log_received.emit(f"Error: Engine executable not found at '{self.engine_path}'")
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
            
            # Start background reader thread
            self.reader_thread = threading.Thread(target=self._read_stdout_loop, daemon=True)
            self.reader_thread.start()

            # Initialize UCI handshake
            self.send_command("uci")
            return True
        except Exception as e:
            self.log_received.emit(f"Failed to launch engine process: {e}")
            return False

    @property
    def is_running(self) -> bool:
        """Return True if engine process is active."""
        return self._is_running and self.process is not None

    def stop_engine(self) -> None:
        """Terminate the running UCI engine process."""
        self._is_running = False
        if self.process:
            try:
                self.send_command("quit")
                self.process.terminate()
                self.process.wait(timeout=1.0)
            except Exception:
                if self.process:
                    self.process.kill()
            self.process = None
        self.engine_stopped.emit()

    def send_command(self, command: str) -> None:
        """Write a string command line to the engine stdin."""
        if self.process and self.process.stdin and self._is_running:
            try:
                self.process.stdin.write(command.strip() + "\n")
                self.process.stdin.flush()
                self.log_received.emit(f"> {command}")
            except Exception as e:
                self.log_received.emit(f"Error writing to engine stdin: {e}")

    def set_position(self, fen: str | None = None, moves: list[str] | None = None) -> None:
        """Send position FEN and move list to engine."""
        cmd = "position "
        if fen:
            cmd += f"fen {fen}"
        else:
            cmd += "startpos"
            
        if moves:
            cmd += " moves " + " ".join(moves)
            
        self.send_command(cmd)

    def start_search_time(self, time_limit_ms: int) -> None:
        """Start engine search with a fixed time limit per move in ms."""
        if time_limit_ms <= 0:
            self.send_command("go infinite")
        else:
            self.send_command(f"go movetime {time_limit_ms}")

    def start_search_depth(self, depth: int) -> None:
        """Start search to fixed depth limit."""
        self.send_command(f"go depth {depth}")

    def set_multipv(self, count: int) -> None:
        """Set engine MultiPV search count."""
        self.send_command(f"setoption name MultiPV value {count}")

    def stop_search(self) -> None:
        """Send stop command to interrupt current search."""
        self.send_command("stop")

    def _read_stdout_loop(self) -> None:
        """Background thread loop reading lines from engine stdout."""
        if not self.process or not self.process.stdout:
            return

        while self._is_running:
            line = self.process.stdout.readline()
            if not line:
                break
                
            line_str = line.strip()
            if line_str:
                self.log_received.emit(line_str)
                self._parse_uci_line(line_str)

    def _parse_uci_line(self, line: str) -> None:
        """Parse standard UCI engine output line."""
        tokens = line.split()
        if not tokens:
            return

        if tokens[0] == "id":
            if len(tokens) >= 3 and tokens[1] == "name":
                self.engine_name = " ".join(tokens[2:])
            elif len(tokens) >= 3 and tokens[1] == "author":
                self.engine_author = " ".join(tokens[2:])
        elif tokens[0] == "uciok":
            self.engine_started.emit(self.engine_name)
            self.send_command("isready")
        elif tokens[0] == "info":
            info_data = self._parse_info_line(tokens[1:])
            if info_data:
                self.search_progress.emit(info_data)
        elif tokens[0] == "bestmove":
            best_move = tokens[1] if len(tokens) > 1 else ""
            ponder_move = tokens[3] if len(tokens) > 3 and tokens[2] == "ponder" else ""
            self.best_move_found.emit(best_move, ponder_move)

    def _parse_info_line(self, tokens: list[str]) -> dict[str, Any]:
        """Parse fields from a UCI 'info' line."""
        info: dict[str, Any] = {}
        i = 0
        while i < len(tokens):
            tok = tokens[i]
            if tok == "depth" and i + 1 < len(tokens):
                info["depth"] = int(tokens[i + 1])
                i += 2
            elif tok == "multipv" and i + 1 < len(tokens):
                info["multipv"] = int(tokens[i + 1])
                i += 2
            elif tok == "score" and i + 2 < len(tokens):
                score_type = tokens[i + 1]
                score_val = tokens[i + 2]
                if score_type == "cp":
                    info["score_cp"] = int(score_val)
                elif score_type == "mate":
                    info["score_mate"] = int(score_val)
                i += 3
            elif tok == "nodes" and i + 1 < len(tokens):
                info["nodes"] = int(tokens[i + 1])
                i += 2
            elif tok == "nps" and i + 1 < len(tokens):
                info["nps"] = int(tokens[i + 1])
                i += 2
            elif tok == "time" and i + 1 < len(tokens):
                info["time_ms"] = int(tokens[i + 1])
                i += 2
            elif tok == "pv":
                info["pv"] = tokens[i + 1:]
                break
            else:
                i += 1
        return info
