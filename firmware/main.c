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
#include "st7735.h"

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

    // Initialize TM1638 7-segment display module directly on Core 0
    tm1638_init();

    // Initialize ST7735 TFT display module
    st7735_init();

    // Render TFT splash screen with chess motif, name, version, and platform info
    st7735_draw_splash(LUGALCHESS_VERSION, LUGALCHESS_PLATFORM);

    // Show alternating 7-segment display splash for 2 seconds: "LUgAL CH" -> "ARM|RISC 0.1.0"
    tm1638_display_string("LUgAL CH");
    sleep_ms(1000);
    tm1638_display_string(LUGALCHESS_7SEG_PLATFORM);
    sleep_ms(1000);

    printf("\n=========================================\n");
    printf("   %s v%s (%s)\n", LUGALCHESS_NAME, LUGALCHESS_VERSION, LUGALCHESS_PLATFORM);
    printf("=========================================\n");
    printf("Engine Name:       %s\n", LUGALCHESS_NAME);
    printf("Engine Version:    %s\n", LUGALCHESS_VERSION);
    printf("Hardware Platform: %s\n", LUGALCHESS_PLATFORM);
    printf("System Clock:      %lu Hz\n\n", (unsigned long)clock_get_hz(clk_sys));

    // Initialize chess engine sub-systems
    init_bitboards();
    init_zobrist();

    // Run interactive console interface
    console_loop();

    return 0;
}
