# LugalChess GUI: Architecture & Implementation Blueprint

> **System Target**: Cross-platform Chess GUI for macOS, Linux, and Windows  
> **Core Stack**: Python 3.12+ with PySide6 (Qt 6) managed via `uv`, C11 Engine Core built with CMake & Ninja.

---

## 1. Executive Summary & Architecture Choices

The **LugalChess GUI** extends LugalChess into a full-featured, cross-platform desktop chess software suite. 

### Selected Technology Stack: PySide6 (Qt 6) + C11 Shared Engine Core
1. **GUI & Application Core**: **PySide6 (Qt 6)** with Python 3.12+, managed via **`uv`**. Full type hints using modern style (`pyrefly` type checker compliant).
   * High-DPI scalable SVG piece rendering.
   * Cross-platform multi-dock layout management (`QDockWidget`).
   * Seamless import of physical chessboard drivers (reusing [`python-mchess`](https://github.com/domschl/python-mchess)).
   * Direct asynchronous integration with local LLMs (`llama.cpp` `llama-server`) via `qasync` / `aiohttp`.
2. **C Engine Core (`liblugalengine`)**: Kept in C11 built via **CMake and Ninja**. Compiles natively on macOS (`AppleClang`), Linux (`gcc`/`clang`), and Windows (`msvc`). Exposed both as a standalone UCI CLI executable and as a shared library with CFFI bindings.

---

## 2. System Architecture Overview

```mermaid
graph TD
    subgraph GUI Layer (PySide6 / Qt 6 + Python 3.12)
        MB[Main Board Widget]
        AB[Auxiliary Analysis Boards]
        NT[Move Tree & PGN Notation]
        EG[Live Eval Bar & Graph Widget]
        TM[Tournament & ELO Dashboard]
        AI[AI Commentary Panel]
    end

    subgraph Controller & Hardware Layer
        EM[UCI Engine Controller]
        HW[Peripheral Manager (USB/BT)]
        DB[Database & Opening Manager]
        LLM[Local LLM Client (llama-server)]
    end

    subgraph Peripherals & Hardware
        RP[RP2350 Board over USB CDC Serial]
        MC[Millennium Chessboard over USB/BT]
    end

    subgraph Engine Core
        LE[LugalChess C Engine]
        SF[Stockfish / External UCI Engines]
    end

    subgraph Data Sources
        PGN[PGN Database (SQLite FTS5)]
        OB[Polyglot Opening Books]
        TB[Syzygy Tablebases]
    end

    MB <--> EM
    AB <--> EM
    NT <--> DB
    EG <--> EM
    TM <--> EM
    AI <--> LLM

    EM <--> LE
    EM <--> SF
    HW <--> RP
    HW <--> MC
    HW --> EM
    DB <--> PGN
    DB <--> OB
    DB <--> TB
    LLM <--> EM
```

---

## 3. Hardware USB CDC Protocol & Mode Switching

### RP2350 USB CDC Serial Output Format
* **Default Mode**: **UCI Protocol Mode** (`UCI`). Output is standard UCI text stream (`id name LugalChess`, `uciok`, `info depth ... score cp ... pv ...`, `bestmove ...`). This allows the RP2350 hardware device to be plugged directly into LugalChess GUI, CuteChess, or Stockfish-compatible GUIs over USB CDC serial as a native UCI engine!
* **Toggle Mode**: Human-readable terminal output (`TLE`).
* **Persistence & Keypad Toggle**: A `usb_uci_mode` setting is stored in RP2350 QSPI flash (`SaveData`). The TM1638 Options Menu provides an option (`USb ucI` / `USb TLE`) to toggle USB output format on the fly.

---

## 4. Reorganized Directory Structure

```
LugalChess/
├── CMakeLists.txt              # Root C/C++ CMake script (CMake + Ninja)
├── README.md
├── gui_architecture_and_implementation_plan.md
├── engine/                     # Core C Engine source files
│   ├── CMakeLists.txt
│   ├── include/                # Header files (.h)
│   └── src/                    # C implementation files (.c)
├── firmware/                   # RP2350 Microcontroller firmware
│   ├── CMakeLists.txt
│   └── ...
├── gui/                        # PySide6 GUI Application (uv workspace)
│   ├── pyproject.toml
│   ├── lugalgui/
│   │   ├── __init__.py
│   │   ├── main.py             # Entry point
│   │   ├── views/              # Qt Board & Dock Widgets
│   │   ├── controllers/        # UCI & Peripheral Controllers
│   │   ├── models/             # Game State & PGN Models
│   │   └── ai/                 # llama.cpp Commentary Client
│   └── tests/
└── resources/                  # Piece SVG graphic sets & sounds
    └── pieces/
        └── cburnett/
```

---

## 5. Detailed Implementation Roadmap

### Non-GUI Refactoring Phase
1. Reorganize project directories (`engine/`, `firmware/`, `gui/`).
2. Update root `CMakeLists.txt` and `engine/CMakeLists.txt` for macOS & Linux builds with CMake and Ninja.
3. Update RP2350 firmware with persistent `usb_uci_mode` (defaulting to UCI stream output).

---

### Level 1: Basic GUI Workspace
* High-DPI SVG graphical chessboard rendering.
* Move tree navigation with rich Unicode chess symbols (`♔`, `♕`, `♖`, `♗`, `♘`, `♙`).
* Asynchronous non-blocking UCI controller process runner.
* Auto-detect and stream moves/thinking from RP2350 USB CDC device.

---

### Level 2: Multi-Board Layout & Analysis
* `QDockWidget` dynamic workspace allowing multiple board views.
* Background analysis engines running Multi-PV search.
* Live evaluation bar and score chart timeline.

---

### Level 3: Engine Tournaments & ELO Ratings
* Automated round-robin and Swiss tournaments between multiple UCI engines.
* Opening book variation suite forcing.
* Integrated BayesElo / Ordo statistical rating estimation with confidence intervals ($\pm \sigma$).

---

### Level 4: Database Integration
* SQLite FTS5 PGN database indexer with sub-50ms position search.
* Polyglot (`.bin`) opening book explorer with win/draw/loss stats.
* Syzygy 3-4-5-6 piece endgame tablebase probe interface.

---

### Level 5: Physical Board Hardware Integration
* Direct integration of `python-mchess` Millennium chessboard driver (USB/Bluetooth LE).
* Bidirectional piece move sync and LED move highlighting on physical boards.

---

### Level 6: AI Grandmaster Commentary
* Integration with local `llama.cpp` `llama-server` exposing an OpenAI-compatible REST endpoint (`http://localhost:8080/v1/chat/completions`).
* Support for local open models: **Gemma 4**, **Qwen 2.5 / 3.6**, **Llama 3**.
* Live streaming commentary panel translating score deltas and engine PVs into natural language insights.
* Automatic PGN text annotation export (`{ White gains space on the kingside }`).
