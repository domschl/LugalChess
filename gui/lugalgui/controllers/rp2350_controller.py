"""RP2350 Hardware USB CDC Serial Interface Controller."""

import threading
import time
from typing import Any
import serial
import serial.tools.list_ports
from PySide6.QtCore import QObject, Signal


class RP2350Controller(QObject):
    """Auto-detects and listens to connected RP2350 board over USB CDC Serial."""

    device_connected = Signal(str)      # port_name
    device_disconnected = Signal()
    line_received = Signal(str)         # raw output line
    move_received = Signal(str)         # uci move string (e.g. 'e2e4')
    search_progress = Signal(dict)      # depth, score, nps, nodes, time, pv
    new_game_received = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.port_name: str | None = None
        self.serial_inst: serial.Serial | None = None
        self.listen_thread: threading.Thread | None = None
        self._is_running: bool = False

    def start_autodetect(self) -> None:
        """Start background polling thread to detect RP2350 USB serial port."""
        if not self._is_running:
            self._is_running = True
            self.listen_thread = threading.Thread(target=self._connection_loop, daemon=True)
            self.listen_thread.start()

    def stop(self) -> None:
        """Stop background thread and close serial connection."""
        self._is_running = False
        if self.serial_inst and self.serial_inst.is_open:
            try:
                self.serial_inst.close()
            except Exception:
                pass
        self.serial_inst = None
        self.device_disconnected.emit()

    def send_command(self, cmd: str) -> None:
        """Send command string to RP2350 device over serial CDC stream."""
        if self.serial_inst and self.serial_inst.is_open:
            try:
                self.serial_inst.write((cmd.strip() + "\n").encode("utf-8"))
            except Exception:
                pass

    def set_position(self, fen: str | None = None, moves: list[str] | None = None) -> None:
        """Send position FEN and move list to RP2350 hardware over serial."""
        cmd = "position "
        if fen:
            cmd += f"fen {fen}"
        else:
            cmd += "startpos"
            
        if moves:
            cmd += " moves " + " ".join(moves)
            
        self.send_command(cmd)

    def start_search_time(self, time_limit_ms: int) -> None:
        """Tell RP2350 to calculate and play a move."""
        if time_limit_ms <= 0:
            self.send_command("go infinite")
        else:
            self.send_command(f"go movetime {time_limit_ms}")

    def stop_search(self) -> None:
        """Stop RP2350 calculation."""
        self.send_command("stop")

    def _find_rp2350_port(self) -> str | None:
        """Search connected USB COM/TTY ports for RP2350 or Raspberry Pi Pico 2 VID/PID."""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            # Check Vendor ID 0x2e8a (Raspberry Pi Trading Ltd)
            if p.vid == 0x2e8a:
                return p.device
            if "Pico" in p.description or "RP2350" in p.description or "LugalChess" in p.description:
                return p.device
        return None

    @property
    def is_connected(self) -> bool:
        """Return True if RP2350 serial connection is active."""
        if self.serial_inst and self.serial_inst.is_open:
            return True
        return self.connect_serial()

    def connect_serial(self, port: str | None = None) -> bool:
        """Attempt to open RP2350 USB CDC serial port."""
        if self.serial_inst and self.serial_inst.is_open:
            return True

        if not port:
            port = self._find_rp2350_port()

        if port:
            try:
                self.serial_inst = serial.Serial(port, 115200, timeout=1.0)
                self.port_name = port
                self.device_connected.emit(port)
                self.send_command("uci")
                return True
            except Exception:
                self.serial_inst = None
                return False
        return False

    def _connection_loop(self) -> None:
        """Background thread loop managing connection and serial line reading."""
        while self._is_running:
            if not self.serial_inst or not self.serial_inst.is_open:
                if not self.connect_serial():
                    time.sleep(2.0)
                    continue

            # Read lines while connected
            try:
                if self.serial_inst and self.serial_inst.in_waiting:
                    line_bytes = self.serial_inst.readline()
                    if line_bytes:
                        line_str = line_bytes.decode("utf-8", errors="replace").strip()
                        if line_str:
                            self.line_received.emit(line_str)
                            self._parse_line(line_str)
                else:
                    time.sleep(0.05)
            except (serial.SerialException, OSError):
                self.serial_inst = None
                self.device_disconnected.emit()
                time.sleep(2.0)

    def _parse_line(self, line: str) -> None:
        """Parse incoming line from RP2350 CDC serial stream."""
        tokens = line.split()
        if not tokens:
            return

        if tokens[0] == "bestmove" and len(tokens) >= 2:
            self.move_received.emit(tokens[1])
        elif len(tokens) >= 3 and tokens[0] == "Engine" and tokens[1] == "plays:":
            # Handle human-readable stream mode fallback
            self.move_received.emit(tokens[2])
        elif tokens[0] == "ucinewgame" or line == "New game started.":
            self.new_game_received.emit()
        elif tokens[0] == "info":
            info_data = self._parse_info_line(tokens[1:])
            if info_data:
                self.search_progress.emit(info_data)

    def _parse_info_line(self, tokens: list[str]) -> dict[str, Any]:
        """Parse fields from a UCI 'info' line from RP2350."""
        info: dict[str, Any] = {}
        i = 0
        while i < len(tokens):
            tok = tokens[i]
            if tok == "depth" and i + 1 < len(tokens):
                info["depth"] = int(tokens[i + 1])
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
