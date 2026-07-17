# LugalChess ♔

**LugalChess** is a lightweight, high-performance console chess engine written in pure C (C11). It is designed from the ground up to support both desktop computing and extremely resource-constrained embedded systems, specifically the **Raspberry Pi Pico 2 (RP2350)** with sub-520KB SRAM limits.

---

## 🚀 Key Features

* **Bitboard Architecture**: High-speed bitboard operations with compiler-intrinsic bit scans.
* **Dual Attack Modes**:
  * **Magic Bitboards**: Highly optimized, dynamic collision-free magic search at startup (~840KB RAM, desktop default).
  * **On-The-Fly Ray Casting**: Memory-saving sliding attack generation (0KB RAM, embedded default).
* **Principal Variation Search (PVS)**: Advanced Alpha-Beta tree search with:
  * **Quiescence Search**: Prevents the horizon effect on tactical captures.
  * **Null Move Pruning (NMP)** & **Late Move Reduction (LMR)**: Skips non-critical branches.
  * **Transposition Table (TT)**: Memory-mapped transposition caching with aging. Automatically scales down to **32KB** on microcontrollers to fit in RAM.
  * **Move Ordering**: Hash move first, captures sorted by MVV-LVA (Most Valuable Victim - Least Valuable Aggressor), Quiet promotions, Killer Moves, and History Heuristics.
* **PeSTO Tapered Evaluation**: High-quality positional evaluation interpolating Middlegame and Endgame values based on game phase, using fast, integer-only mathematics.
* **Dual Interface Support**:
  * **UCI Protocol**: Standard Chess interface to connect to GUIs (like Arena, Cutechess, Lichess).
  * **Interactive Console**: Human-friendly terminal CLI to play, undo, evaluate, or list moves directly.

---

## 📁 Repository Structure

* `src/`: Core engine code (board representations, move generation, search, evaluation).
* `firmware/`: Embedded firmware code, boot routines, and configuration files for RP2350.
* `CMakeLists.txt`: Build configuration for host desktop execution.

---

## 💻 1. Desktop Build & Usage (Linux/macOS/Windows)

### Build Instructions

LugalChess compiles warning-free with CMake and Ninja:

```bash
# Configure and build
cmake -G Ninja -B build
cmake --build build
```

### Usage Modes

#### A. Interactive Console Mode (Play against LugalChess)
Run with the `-c` or `--console` flag:
```bash
./build/lugalchess -c
```
**Interactive Commands:**
* `help` - Show instructions.
* `new` - Reset board to standard start position.
* `board` (or `d`) - Print current board and status info.
* `level <depth>` - Configure engine search depth (e.g., `level 6`).
* `eval` - Prints static evaluation of current position (PeSTO centipawns).
* `moves` - Lists all legal moves available.
* `go` - Forces the computer to play.
* `undo` - Takes back the last turn (both your move and the engine's response).
* `<move>` (e.g., `e2e4`, `g1f3`) - Type a standard coordinate move to play it.

#### B. UCI Mode (Play via Chess GUIs)
Run without any flags to enter UCI standard mode:
```bash
./build/lugalchess
```
*You can load the resulting binary directly into Arena, Cutechess, or other chess GUI programs.*

#### C. PERFT Verification Suite
Run the built-in PERFT tests to verify move generator legality:
```bash
./build/lugalchess -p
```

---

## 🔌 2. RP2350 Microcontroller Build (Pico SDK)

The firmware is configured to run on the **Raspberry Pi Pico 2 (RP2350)** using the official **Pico SDK**. Stdio is redirected over both USB CDC Serial and physical UART0 pins.

### Prerequisites

Install the ARM GCC cross-compiler toolchain:
* On Debian/Ubuntu: `sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib`
* On Arch Linux: `sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib`

### Build Instructions

The CMake configuration automatically downloads the correct Pico SDK version if `PICO_SDK_PATH` is not defined:

```bash
# Create build directory for the firmware
mkdir build_firmware && cd build_firmware

# Configure for RP2350 (Pico 2)
cmake -DPICO_PLATFORM=rp2350 ../firmware

# Compile to UF2
make -j$(nproc)
```

This compiles and links `lugalchess_firmware.uf2`.

### Flashing & Playing

1. Plug in your RP2350 board while holding the **BOOTSEL** button.
2. Drag and drop `lugalchess_firmware.uf2` onto the mounted volume.
3. Open a terminal emulator program to connect to the USB serial device:
   ```bash
   picocom -b 115200 /dev/ttyACM0
   ```
   *(The boot screen and chessboard will display immediately upon connection, and you can play directly by typing moves!)*
