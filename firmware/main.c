#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "defs.h"
#include "bitboard.h"
#include "zobrist.h"
#include "console.h"


int main() {
    // Initialize standard I/O (redirects standard input/output to USB Serial and UART)
    stdio_init_all();

    // Wait for USB CDC connection to be opened by the host terminal
    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }


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
