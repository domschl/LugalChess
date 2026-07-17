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

---

## 📺 3. QYF-TM1638 Display & Keypad Interface

LugalChess includes a native driver for the **QYF-TM1638** module, which combines 8 seven-segment displays and a 4x4 matrix keyboard (16 keys total). This allows for a completely standalone chess computer experience.

### 🔌 Hardware Pinout (RP2350 -> TM1638)

Connect the TM1638 module to your Pico 2 using the following GPIO mapping (defined in `firmware/tm1638.h`):

* **VCC** ➡️ `5V` (or `3.3V` VBUS pin)
* **GND** ➡️ `GND`
* **STB** ➡️ `GP6` (Pin 9)
* **CLK** ➡️ `GP7` (Pin 10)
* **DIO** ➡️ `GP8` (Pin 11)

### ⌨️ Keypad Layout

The 16 keys on the module are divided into two halves: input keys (first 2 rows) and function keys (last 2 rows).

```
[ A/1 ]  [ B/2 ]  [ C/3 ]  [ D/4 ]   <-- Input Keys (0..3)
[ E/5 ]  [ F/6 ]  [ G/7 ]  [ H/8 ]   <-- Input Keys (4..7)
[Back ]  [Fwrd ]  [Board]  [Stop ]   <-- Function Keys (8..11)
[ Lvl ]  [Anal ]  [ Opt ]  [Enter]   <-- Function Keys (12..15)
```

### 🎮 Move Entry (`MODE_NORMAL`)
To play a move (e.g. `e2e4`), type it as a 4-key sequence using the 8 input keys. The system dynamically expects files or ranks based on the sequence step:
1. **First Key**: Expects File. Pressing `E/5` (key 4) inputs `'e'`.
2. **Second Key**: Expects Rank. Pressing `B/2` (key 1) inputs `'2'`.
3. **Third Key**: Expects File. Pressing `E/5` (key 4) inputs `'e'`.
4. **Fourth Key**: Expects Rank. Pressing `D/4` (key 3) inputs `'4'`.

The display shows your entry progress on the left 4 digits (e.g. `E2__` -> `E2E4`). The right 4 digits show the engine's last move (e.g. `E2E4B8C6`).

### ⚙️ Function Keys

* **Back (Key 8)**: Takes back the last turn (pops both your move and the engine's reply from history). In Board View, scrolls rank index UP. In Options Menu, scrolls to PREVIOUS option.
* **Fwrd (Key 9)**: In Board View, scrolls rank index DOWN. In Options Menu, scrolls to NEXT option.
* **Board (Key 10)**: Enters **Board View Mode**.
* **Stop (Key 11)**: Cancels current typing progress, exits sub-menus (Board View, Level Select, Options), or aborts an active engine search (playing the best move found so far immediately).
* **Lvl (Key 12)**: Enters **Level Selection Mode**. Choose levels 1 to 8 by pressing input keys `A/1` through `H/8`.
* **Anal (Key 13)**: Forces the engine to calculate and play a move immediately (UCI `go` command).
* **Opt (Key 14)**: Enters the **Options Menu**.
* **Enter (Key 15)**: Confirms the selected option or level.

---

## 🔎 4. Interface Sub-Modes

### ♚ Board View Mode
Displays the pieces on the active rank. 
- When scrolled (using `Back` and `Fwrd`), the display briefly flashes the rank (e.g., `rAnK  1`) for 350ms, then shows all 8 squares of that rank.
- **Piece Representation**:
  - Empty squares are displayed as underscores (`_`).
  - Pieces are displayed as single letters: Pawn=`P`, Knight=`n`, Bishop=`b`, Rook=`r`, Queen=`q`, King=`H` (which represents K).
  - **White pieces are indicated with a trailing decimal point (`.`)**, whereas **Black pieces are shown without a decimal point**.

### 📶 Level Selection Mode
Select levels 1-8. Each level maps to a search depth configured for optimal timing:
- **Level 1** ➡️ Depth 1
- **Level 2** ➡️ Depth 2
- **Level 3** ➡️ Depth 3
- **Level 4** ➡️ Depth 5
- **Level 5** ➡️ Depth 7
- **Level 6** ➡️ Depth 9
- **Level 7** ➡️ Depth 11
- **Level 8** ➡️ Depth 13

### 🛠️ Options Menu Mode
Cycle through options using `Back`/`Fwrd` and confirm with `Enter`:
1. `nEU gAnE` (New Game): Resets board to startup position.
2. `PLAy bL` (Play Black): Sets your side to White (engine stays idle until you move).
3. `PLAy UH` (Play White): Sets your side to Black (engine plays first move immediately).
4. `ScOrE` (Score): Briefly displays current board evaluation in centipawns (e.g. `ScO +120` or mate `ScO t 03`).
5. `LEuEL` (Level): Enters Level Selection Mode.
6. `SIdES` (Sides): Briefly displays the current side to move (`SIdE WH` or `SIdE bL`).
7. `HAlF` (Halfmove): Displays the halfmove clock for the 50-move rule (e.g., `H- 04`).
8. `MOuES` (Moves): Displays the total number of plies played in the game (e.g., `n- 12`).

### 🧠 Live Thinking Feedback
During calculation, the display updates live with the engine's search progress:
- **Left 4 Digits**: The current best move found so far (e.g., `E2E4`).
- **Right 4 Digits**: The evaluation score in centipawns (e.g. `+125`, `-080`) or mate score (e.g., `t 02` for mate in 2 plies).
- Pressing `Stop` during calculation immediately aborts the search and forces the engine to execute the current best move.

