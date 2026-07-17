#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "defs.h"
#include "bitboard.h"
#include "zobrist.h"
#include "console.h"
#include "tm1638.h"

int main() {
    // Initialize stdio
    stdio_init_all();

    // Seed the random number generator using the Pico hardware microsecond timer
    srand(time_us_32());

    // Wait up to 3 seconds for USB CDC terminal to connect.
    // If no terminal connects, proceed anyway (standalone TM1638 play).
    for (int i = 0; i < 300; i++) {
        if (stdio_usb_connected()) break;
        sleep_ms(10);
    }

    // Initialize TM1638 module directly on Core 0
    tm1638_init();

    // Show splash string "LUgAL CH" (LugalChess) on boot
    tm1638_display_string("LUgAL CH");

    printf("\n=========================================\n");
    printf("     LugalChess RP2350 Firmware Boot      \n");
    printf("=========================================\n");
    printf("System Clock: %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));

    // Initialize chess engine sub-systems
    init_bitboards();
    init_zobrist();

    // Run interactive console interface
    console_loop();

    return 0;
}
