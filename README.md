# LugalChess ♔

**LugalChess** is a lightweight, high-performance console and embedded chess engine written in pure C (C11). It is designed from the ground up to support both desktop computing and resource-constrained embedded microcontrollers, specifically the **Raspberry Pi Pico 2 (RP2350)** with sub-520KB SRAM limits.

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
* **Dual Level System (Time-Based & Ply-Based)**:
  * **Time-Based Mode (Default)**: 1s, 2s, 5s, 10s, 15s, 30s, 60s per move, or Infinite (`t-In`). Features a **dynamic time estimation algorithm** ($t_e = b \cdot T_{\text{last}}$) predicting branching factor $b \in [2.5, 5.0]$ to stop search cleanly before launching an overshooting depth iteration.
  * **Ply-Based Mode**: Fixed search depths (Depths 1, 2, 3, 5, 7, 9, 11, 13).
* **Rich Graphic & Hardware Interfaces**:
  * **1.8" ST7735 SPI TFT LCD**: Renders a $128 \times 128$ px color chessboard with piece bitmaps and a 2-line real-time status area (showing level mode, search depth, White-perspective evaluation score, live PV lines, and move comments).
  * **QYF-TM1638 8-Digit 7-Segment & 4x4 Keypad**: Dual 4-digit display for move inputs, real-time thinking feedback, and level/options menus.
  * **Single Half-Move Undo & Redo**: `Back` and `Fwrd` keys step backward and forward through history by single half-moves ($1$ ply).
  * **Rule Engine**: Full detection of Check, Checkmate, Stalemate, 50-move rule, and **Threefold Repetition Draw**.
* **Dual Interface Protocols**:
  * **UCI Protocol**: Standard Chess interface to connect to GUIs (Arena, Cutechess, Lichess).
  * **Interactive Console**: Human-friendly terminal CLI and embedded control interface.

---

## 📁 Repository Structure

* `src/`: Core engine code (board representations, move generation, search, evaluation, CLI console).
* `firmware/`: Embedded firmware code, boot routines, TM1638 keypad/7-segment driver, and ST7735 TFT LCD driver for RP2350.
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
* `level <n>` - Configure search level (e.g. `level 3s` for 5sec time level, `level 5d` for depth 7 ply level).
* `eval` - Prints static evaluation of current position (PeSTO centipawns).
* `moves` - Lists all legal moves available.
* `go` - Forces the computer to think and play.
* `undo` - Takes back 1 half-move (1 ply).
* `redo` - Re-applies 1 half-move (1 ply) from history.
* `<move>` (e.g., `e2e4`, `g1f3`, `g7g8q`) - Type a standard coordinate move to play it.

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
3. Connect a USB cable or terminal emulator program:
   ```bash
   picocom -b 115200 /dev/ttyACM0
   ```

---

## 📺 3. Hardware Pinout & Wiring

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

## ⌨️ 4. Keypad Layout & Sub-Modes

```
[ A/1 ]  [ B/2 ]  [ C/3 ]  [ D/4 ]   <-- Input Keys (0..3)
[ E/5 ]  [ F/6 ]  [ G/7 ]  [ H/8 ]   <-- Input Keys (4..7)
[Back ]  [Fwrd ]  [Board]  [Stop ]   <-- Function Keys (8..11)
[ Lvl ]  [Anal ]  [ Opt ]  [Enter]   <-- Function Keys (12..15)
```

### 🎮 Move Entry (`MODE_NORMAL`)
To play a move (e.g. `e2e4`), type it as a 4-key sequence using the 8 input keys:
1. **Key 1**: File (e.g. `E/5` $\to$ `'e'`).
2. **Key 2**: Rank (e.g. `B/2` $\to$ `'2'`).
3. **Key 3**: File (e.g. `E/5` $\to$ `'e'`).
4. **Key 4**: Rank (e.g. `D/4` $\to$ `'4'`).

*Pawn promotions display `1n2b3r4q` and wait for key selection `1`–`4` (defaulting to Queen).*

### ⚙️ Function Keys

* **Back (Key 8)**: Takes back **1 half-move** ($1$ ply). In Board View, scrolls rank UP. In Options Menu, scrolls to PREVIOUS option.
* **Fwrd (Key 9)**: Re-applies **1 half-move** ($1$ ply) from history. In Board View, scrolls rank DOWN. In Options Menu, scrolls to NEXT option.
* **Board (Key 10)**: Enters **Board View Mode**. During engine thinking, toggles real-time board evaluation observer.
* **Stop (Key 11)**: Cancels typing input, exits sub-menus, or interrupts an active engine search (playing the best move evaluated up to that exact millisecond).
* **Lvl (Key 12)**: Enters **Level Selection Mode** (Levels 1 to 8).
* **Anal (Key 13)**: Forces the engine to calculate and play a move immediately.
* **Opt (Key 14)**: Enters the **Options Menu**.
* **Enter (Key 15)**: Confirms the selected option or level.

---

## 🔎 5. Options Menu & Level Modes

### ⏱️ Level Modes
LugalChess supports two distinct level modes:

### ⏱️ Level System (Time-Based)
LugalChess uses a unified **Time-Based Level System** (Levels 1–8) featuring dynamic time estimation ($t_e = b \cdot T_{\text{last}}$) to predict branching factor $b \in [2.5, 5.0]$ and prevent overshooting:
* **Level 1**: 1s per move (`t-1s` on 7-segment / `Lvl:1s` on TFT)
* **Level 2**: 2s per move (`t-2s` on 7-segment / `Lvl:2s` on TFT)
* **Level 3**: 5s per move (`t-5s` on 7-segment / `Lvl:5s` on TFT)
* **Level 4**: 10s per move (`t10s` on 7-segment / `Lvl:10s` on TFT)
* **Level 5**: 15s per move (`t15s` on 7-segment / `Lvl:15s` on TFT)
* **Level 6**: 30s per move (`t30s` on 7-segment / `Lvl:30s` on TFT)
* **Level 7**: 60s per move (`t60s` on 7-segment / `Lvl:60s` on TFT)
* **Level 8**: Infinite / Manual Stop (`t-In` on 7-segment / `Lvl:Inf` on TFT)

### 🛠️ Options Menu List
Cycle through options using `Back`/`Fwrd` and confirm with `Enter`:
1. `nEU gAnE`: Resets board to standard starting position.
2. `PLAy bL`: Sets user to White (engine stays idle until you move).
3. `PLAy UH`: Sets user to Black (engine plays first move immediately).
4. `ScOrE`: Briefly displays current evaluation score (e.g. `ScO +1.50` or mate `ScO +M3`).
5. `LEuEL`: Enters Level Selection Mode (select Levels 1 to 8).
6. `SIdES`: Displays active turn (`SIdE WH` or `SIdE bL`).
7. `HAlF`: Displays halfmove clock for the 50-move rule (e.g. `H- 04`).
8. `MOuES`: Displays total plies played (e.g. `n- 12`).
9. `SAuE`: Saves current game and settings to QSPI flash / file.
10. `LOAd`: Loads saved game and settings.

---

## 📺 6. TFT Display Layout ($128 \times 160$ px)

* **Top $128 \times 128$ px**: Color graphic chessboard with piece bitmaps and square colors.
* **Bottom Status Area ($128 \times 32$ px)**:
  * **Line 1 (Yellow)**: `Lvl:time/mm Side Score` (e.g., `Lvl:2s/05 W +0.50`, `Lvl:10s/07 W +1.20`, `Lvl:Inf/05 W +M2`).
  * **Line 2 (Cyan/White/Red)**:
    * *During Calculation*: Live **PV line** sequence (e.g., `F3D4 E7E5 G1F3 B8C6`).
    * *After Move*: Last move played plus status comment (e.g., `f3d4 Check`, `e7e8q Mate`, `Draw`).
