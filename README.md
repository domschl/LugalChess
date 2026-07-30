# LugalChess ♔

**LugalChess** is a lightweight, high-performance C11 chess engine and modern PySide6 desktop GUI application. It is designed from the ground up for both desktop computing (Linux, macOS, Windows) and resource-constrained embedded microcontrollers, specifically the **Raspberry Pi Pico 2 (RP2350)** with sub-520KB SRAM limits.

---

## 🚀 Key Features

* **Bitboard Architecture**: High-speed 64-bit bitboard operations with compiler-intrinsic bit scans.
* **Dual Attack Generation Modes**:
  * **Magic Bitboards**: Highly optimized, dynamic collision-free magic search at startup (~840KB RAM, desktop default).
  * **On-The-Fly Ray Casting**: Memory-saving sliding attack generation (0KB RAM, embedded default).
* **Principal Variation Search (PVS)**: Advanced Alpha-Beta tree search with:
  * **Quiescence Search**: Prevents the horizon effect on tactical captures.
  * **Null Move Pruning (NMP)** & **Late Move Reduction (LMR)**: Skips non-critical branches.
  * **Transposition Table (TT)**: Memory-mapped transposition caching with aging. Automatically scales down to **32KB** on microcontrollers to fit in SRAM.
  * **Move Ordering**: Hash move first, captures sorted by MVV-LVA (Most Valuable Victim - Least Valuable Aggressor), Quiet promotions, Killer Moves, and History Heuristics.
* **PeSTO Tapered Evaluation**: High-quality positional evaluation interpolating Middlegame and Endgame values based on game phase, using fast integer-only arithmetic.
* **Universal Level & Time Control System**:
  * **Preset Levels (1–8)**: 1s, 2s, 5s, 10s, 15s, 30s, 60s per move, or Infinite (`t-In`).
  * **Dynamic Time Estimation Algorithm** ($t_e = b \cdot T_{\text{last}}$): Predicts branching factor $b \in [2.5, 5.0]$ to stop search cleanly before launching an overshooting depth iteration.
  * **Flexible Custom Levels**: Support for custom time-per-move seconds (e.g., `3.5s`, `45s`) and fixed ply depth targets (e.g., `8d`, `12d`, `16d`).
* **PySide6 Modern Desktop GUI (`gui/lugalgui`)**:
  * High-DPI interactive 2D graphical chessboard with drag-and-drop piece moves, move highlighting, and check indicators.
  * Rich Unicode SAN move notation tree (`♔`, `♕`, `♖`, `♗`, `♘`, `♙`) with click-to-navigate move history.
  * Real-time evaluation bar, PV display, and console log output.
  * **Engine Target Selector**: Seamless toggle between local engine processes (LugalChess, Stockfish, Lc0) and physical RP2350 USB hardware engines.
* **Embedded RP2350 Hardware Integration**:
  * **1.8" ST7735 SPI TFT LCD**: Renders a $128 \times 128$ px color chessboard with piece bitmaps and a real-time status area.
  * **QYF-TM1638 8-Digit 7-Segment & 4x4 Keypad**: Move entry, thinking feedback, and level/options menus.
  * **Automatic Protocol Detection**: USB CDC serial automatically detects UCI protocol commands (`uci`, `isready`, `position`, `go`) vs. human interactive terminal commands (`help`, `board`, `d`).
  * **Full Bidirectional Sync**: Moves and position updates played in the GUI instantly update the RP2350's internal state and TFT display, while moves played on the RP2350 keypad automatically sync back to the GUI desktop board.
* **Dual Interface Protocols**:
  * **UCI Protocol**: Standard Chess interface to connect to GUIs (LugalGUI, CuteChess, Arena, Lichess).
  * **Interactive Console**: Human-friendly CLI and embedded control interface.

---

## 📁 Repository Structure

```
LugalChess/
├── engine/                       # Core C11 engine source code
│   ├── include/                  # Engine headers (bitboard, position, movegen, search, PVS, TT, UCI)
│   └── src/                      # C source files and CLI entry points
├── firmware/                     # RP2350 microcontroller embedded firmware (Pico 2)
│   ├── st7735.c / st7735.h      # 1.8" Color TFT LCD driver & board rendering
│   └── tm1638.c / tm1638.h      # 7-Segment display & 4x4 matrix keypad driver
├── gui/                          # Modern PySide6 (Qt 6) Desktop Application
│   ├── lugalgui/
│   │   ├── controllers/          # UCI process controller & RP2350 CDC serial controller
│   │   ├── models/               # Game tree navigation & Unicode notation engine
│   │   └── views/                # Interactive high-DPI board widget & notation widget
│   └── pyproject.toml            # Python packaging & uv workspace config
├── resources/                    # Test positions & mate solver test suites
└── CMakeLists.txt                # Root CMake project routing to engine/ and firmware/
```

---

## 💻 1. PySide6 Desktop GUI (`gui/lugalgui`)

The desktop GUI is built with Python 3.12+ and **PySide6 (Qt 6)**, managed via **`uv`**.

### Launching the GUI

```bash
cd gui
uv run lugalgui
```

### Features

* **Play vs Local UCI Engines**: Select `LugalChess`, `Stockfish`, `Lc0`, or any UCI-compliant executable.
* **Play vs RP2350 Hardware Engine**: Plug in your RP2350 over USB, choose `Engine Target: RP2350 USB Hardware Engine` in the dropdown, and play interactively against the physical hardware board!
* **Flexible Level Selection**: Choose preset Levels 1–8 or set custom time/depth limits via `Level -> Custom Level / Time Control...`.

---

## 💻 2. C Engine Build (Linux/macOS/Windows)

Compiles warning-free with CMake and Ninja:

```bash
# Configure and build host binary and shared library
cmake -G Ninja -B build
cmake --build build
```

### Usage Modes

#### A. Interactive Console Mode
```bash
./build/engine/lugalchess -c
```

#### B. UCI Engine Mode
```bash
./build/engine/lugalchess
```

#### C. PERFT Verification Suite
```bash
./build/engine/lugalchess -p
```

---

## 🔌 3. RP2350 Microcontroller Build (Pico SDK)

### Prerequisites (ARM Toolchain)

#### Debian / Ubuntu / Raspberry Pi OS
```bash
sudo apt update
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential cmake ninja-build
```

#### Arch Linux
```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib cmake ninja make
ls ```

#### macOS (Homebrew)
> [!NOTE]
> Install the official Arm GNU Toolchain cask (`gcc-arm-embedded`) rather than the bare `arm-none-eabi-gcc` formula, as the formula lacks Newlib (`libc` / `libg`) and will cause linker errors (`cannot find -lc`).

```bash
# Remove bare Homebrew formulas if previously installed
brew uninstall arm-none-eabi-gcc arm-none-eabi-binutils

# Install official Arm toolchain cask & build tools
brew install --cask gcc-arm-embedded
brew install cmake ninja
```

### Build Instructions

#### ARM Cortex-M33 Build (Default)
```bash
mkdir build_firmware && cd build_firmware
cmake -DPICO_PLATFORM=rp2350 ../firmware
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

#### RISC-V (Hazard3 Cores) Build
The RP2350 microcontroller features dual Hazard3 RISC-V cores. To build specifically for the RISC-V target architecture, pass `-DUSE_RISCV=ON` or `-DPICO_PLATFORM=rp2350-riscv` (requires a RISC-V cross-compiler toolchain such as `gcc-riscv64-unknown-elf` or `gcc-riscv-none-embed`):

For Arch Linux, install the following packages from AUR in the following order:

1. [riscv-none-elf-binutils](https://aur.archlinux.org/packages/riscv-none-elf-binutils)
2. [riscv-none-elf-gcc-stage1](https://aur.archlinux.org/packages/riscv-none-elf-gcc-stage1) needed in order to build `newlib`
3. [riscv-none-elf-newlib](https://aur.archlinux.org/packages/riscv-none-elf-newlib)
4. [riscv-none-elf-gcc](https://aur.archlinux.org/packages/riscv-none-elf-gcc) will uninstall the stage1 gcc that was only needed for newlib

Notes:
- the riscv32 bin package with precompiled toolchain and gcc does not contain the required newlib
- the packages compile from source, which can take several hours

```bash
mkdir build_firmware_riscv && cd build_firmware_riscv
cmake -DUSE_RISCV=ON ../firmware
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

> [!TIP]
> To clean and re-configure `build_firmware` without re-downloading the Pico SDK (`_deps` folder), preserve `_deps`:
> ```bash
> find build_firmware -mindepth 1 ! -path 'build_firmware/_deps*' -delete
> ```

Flash `lugalchess_firmware.uf2` by dropping it onto the RP2350 BOOTSEL volume. The RP2350 bootloader automatically detects the RISC-V binary metadata header in the UF2 file and boots the chip directly using its Hazard3 RISC-V cores.

---

## 📺 4. Hardware Pinout & Wiring

### 🔌 QYF-TM1638 (7-Segment Display & 4x4 Keypad)
* **VCC** ➡️ `5V` (or `3.3V` VBUS pin)
* **GND** ➡️ `GND`
* **STB** ➡️ `GP6` (Pin 9)
* **CLK** ➡️ `GP7` (Pin 10)
* **DIO** ➡️ `GP8` (Pin 11)

### 📺 1.8" ST7735 SPI TFT LCD Screen
* **VCC & LED** ➡️ `3.3V` (logic power & backlight)
* **GND** ➡️ `GND`
* **SCL/SCK** ➡️ `GP18` (Pin 24, SPI0 SCK)
* **SDA/MOSI** ➡️ `GP19` (Pin 25, SPI0 TX)
* **CS** ➡️ `GP17` (Pin 22, SPI0 CS)
* **D/C (A0)** ➡️ `GP20` (Pin 26, Data/Command select)
* **RESET** ➡️ `GP21` (Pin 27, Reset)

---

## 👥 5. Authors & References

### 👥 Authors
* **domschl** — Project Creator, Engine Architect & Hardware Lead.
* **Antigravity AI (Google DeepMind)** — AI Pair Programmer, GUI Architect & System Integration Lead.

### 📚 Third-Party Resources & Acknowledgments
* **PySide6 / Qt 6**: Official Python bindings for the Qt cross-platform application framework (https://www.qt.io/).
* **python-chess**: Pure Python chess library for move generation and game validation (https://github.com/niklasf/python-chess).
* **pyserial**: Python serial port communication library for USB CDC serial interfaces (https://github.com/pyserial/pyserial).
* **qasync**: Asyncio event loop integration for PySide6 (https://github.com/CabbageSound/qasync).
* **PeSTO Tapered Evaluation**: Piece-Square Table evaluation function by Ronald de Man (https://www.chessprogramming.org/PeSTO's_Evaluation_Function).
* **Raspberry Pi Pico SDK & TinyUSB**: Hardware abstraction layer and USB CDC stack for RP2350 microcontrollers (https://github.com/raspberrypi/pico-sdk).
* **cburnett Vector Chess Pieces**: SVG chess piece set designed by Colin M.L. Burnett (Wikimedia Commons, CC BY-SA 3.0).

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for details.
