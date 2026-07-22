"""Engine Registry and Auto-Discovery Manager for LugalChess GUI."""

import os
import shutil
from PySide6.QtCore import QObject, Signal


class EngineInfo:
    """Dataclass holding engine name, executable path, and hardware flags."""

    def __init__(self, name: str, path: str, is_hardware: bool = False) -> None:
        self.name: str = name
        self.path: str = path
        self.is_hardware: bool = is_hardware

    def __repr__(self) -> str:
        return f"EngineInfo({self.name}, {self.path})"


class EngineRegistry(QObject):
    """Manages configured UCI engine executables and hardware targets."""

    registry_updated = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.engines: list[EngineInfo] = []
        self.auto_discover()

    def auto_discover(self) -> None:
        """Auto-detect installed system engines (Stockfish, Lc0) and local build binaries."""
        self.engines.clear()

        # 1. Check local build directory for LugalChess
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
        local_lugal = os.path.join(project_root, "build", "engine", "lugalchess")
        if os.path.exists(local_lugal):
            self.engines.append(EngineInfo("LugalChess (Local Build)", local_lugal))

        # 2. Check system PATH for standard UCI engines
        known_engines = ["stockfish", "lc0", "komodo", "crafty", "gnuchess"]
        for eng_bin in known_engines:
            found_path = shutil.which(eng_bin)
            if found_path and not any(e.path == found_path for e in self.engines):
                display_name = f"{eng_bin.capitalize()} (System PATH)"
                self.engines.append(EngineInfo(display_name, found_path))

        # 3. Add RP2350 USB Hardware Engine option
        self.engines.append(EngineInfo("RP2350 USB Hardware Engine", "RP2350_USB_CDC", is_hardware=True))

        self.registry_updated.emit()

    def add_custom_engine(self, name: str, path: str) -> None:
        """Register a user-specified UCI engine executable."""
        if os.path.exists(path) and not any(e.path == path for e in self.engines):
            self.engines.append(EngineInfo(name, path))
            self.registry_updated.emit()

    def get_engine_by_path(self, path: str) -> EngineInfo | None:
        """Find registered EngineInfo by path."""
        for e in self.engines:
            if e.path == path:
                return e
        return None
