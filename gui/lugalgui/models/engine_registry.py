"""Engine Registry and Auto-Discovery Manager for LugalChess GUI."""

import os
import shutil
from PySide6.QtCore import QObject, Signal


class EngineInfo:
    """Dataclass holding engine name, executable path, command line arguments, and hardware flags."""

    def __init__(
        self,
        name: str,
        path: str,
        args: list[str] | None = None,
        is_hardware: bool = False,
        is_xboard: bool = False,
        elo_handicap: int | None = None,
        depth_limit: int | None = None,
        time_multiplier: float = 1.0,
        assumed_elo: float | None = None,
    ) -> None:
        self.name: str = name
        self.path: str = path
        self.args: list[str] = args or []
        self.is_hardware: bool = is_hardware
        self.is_xboard: bool = is_xboard
        self.elo_handicap: int | None = elo_handicap
        self.depth_limit: int | None = depth_limit
        self.time_multiplier: float = time_multiplier
        self.assumed_elo: float = assumed_elo if assumed_elo is not None else (float(elo_handicap) if elo_handicap else 1500.0)

    def __repr__(self) -> str:
        return f"EngineInfo({self.name}, {self.path}, args={self.args}, is_xboard={self.is_xboard})"


class EngineRegistry(QObject):
    """Manages configured UCI engine executables and hardware targets."""

    registry_updated = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.engines: list[EngineInfo] = []
        self.auto_discover()

    def auto_discover(self) -> None:
        """Auto-detect installed system engines (Stockfish, Lc0, Gnuchess --uci, Crafty) and local build binaries."""
        self.engines.clear()

        # 1. Check local build directory for LugalChess
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
        local_lugal = os.path.join(project_root, "build", "engine", "lugalchess")
        if os.path.exists(local_lugal):
            self.engines.append(EngineInfo("LugalChess (Local Build)", local_lugal, assumed_elo=1500.0))

        # 2. Check system PATH for standard engines
        for eng_bin, extra_args, is_xb, elo_val in [("crafty", [], True, 2400.0), ("gnuchess", ["--uci"], False, 2700.0)]:
            found_path = shutil.which(eng_bin)
            if found_path:
                display_name = f"{eng_bin.capitalize()} (System PATH)"
                self.engines.append(EngineInfo(display_name, found_path, args=extra_args, is_xboard=is_xb, assumed_elo=elo_val))

        # Check for Stockfish and add benchmark handicap presets
        stockfish_path = shutil.which("stockfish")
        if stockfish_path:
            self.engines.append(EngineInfo("Stockfish (1350 ELO - Novice)", stockfish_path, elo_handicap=1350, assumed_elo=1350.0))
            self.engines.append(EngineInfo("Stockfish (1500 ELO - Intermediate)", stockfish_path, elo_handicap=1500, assumed_elo=1500.0))
            self.engines.append(EngineInfo("Stockfish (1800 ELO - Club)", stockfish_path, elo_handicap=1800, assumed_elo=1800.0))
            self.engines.append(EngineInfo("Stockfish (2200 ELO - Master)", stockfish_path, elo_handicap=2200, assumed_elo=2200.0))
            self.engines.append(EngineInfo("Stockfish (Full Strength)", stockfish_path, assumed_elo=3500.0))

        # Check for Lc0
        lc0_path = shutil.which("lc0")
        if lc0_path:
            self.engines.append(EngineInfo("Lc0 (System PATH)", lc0_path, assumed_elo=3400.0))

        # 3. Add RP2350 USB Hardware Engine option (with 15x time odds budget for MCU)
        self.engines.append(EngineInfo("RP2350 USB Hardware Engine (15x Time Odds)", "RP2350_USB_CDC", is_hardware=True, time_multiplier=15.0, assumed_elo=1300.0))

        self.registry_updated.emit()

    def add_custom_engine(self, name: str, path: str, args: list[str] | None = None) -> None:
        """Register a user-specified UCI engine executable with optional CLI arguments."""
        if os.path.exists(path) and not any(e.path == path for e in self.engines):
            self.engines.append(EngineInfo(name, path, args=args))
            self.registry_updated.emit()

    def get_engine_by_path(self, path: str) -> EngineInfo | None:
        """Find registered EngineInfo by path."""
        for e in self.engines:
            if e.path == path:
                return e
        return None
