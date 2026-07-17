#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "defs.h"
#include "bitboard.h"
#include "zobrist.h"
#include "console.h"
#include "tm1638.h"

// Core 1 entry point: User Interface handler (keypad scanning and 7-segment display)
void core1_entry(void) {
    tm1638_init();

    printf("[TM1638 Debug] Driver initialized. Showing boot message.\n");
    fflush(stdout);

    // Show splash string "LUgAL CH" (LugalChess) on boot
    tm1638_display_string("LUgAL CH");

    char move_input[5] = ""; // stores coordinates (e.g. "e2e4\0")
    int input_len = 0;

    // Show initial LEDs status (all off)
    tm1638_set_leds(0x00);

    while (1) {
        // 1. Scan TM1638 Keypad (keys 0 to 15)
        int key = tm1638_get_key();
        if (key != -1) {
            printf("[TM1638 Debug] Key index pressed: %d\n", key);
            fflush(stdout);

            // Key press debouncing
            sleep_ms(250);


            // Determine character type
            if (key >= 0 && key <= 7) {
                // Keys 0..7 map to Files 'a' through 'h'
                char file_char = 'a' + key;
                if (input_len == 0 || input_len == 2) {
                    move_input[input_len++] = file_char;
                    move_input[input_len] = '\0';
                }
            } else if (key >= 8 && key <= 15) {
                // Keys 8..15 map to Ranks '1' through '8'
                char rank_char = '1' + (key - 8);
                if (input_len == 1 || input_len == 3) {
                    move_input[input_len++] = rank_char;
                    move_input[input_len] = '\0';
                }
            }

            // Render current input string on left 4 digits of display
            char disp_str[16];
            snprintf(disp_str, sizeof(disp_str), "%-4s    ", move_input);
            tm1638_display_string(disp_str);

            // Turn on LEDs progressively to show input progression
            uint8_t led_mask = 0;
            for (int i = 0; i < input_len; i++) {
                led_mask |= (1 << i);
            }
            tm1638_set_leds(led_mask);

            // Once 4 characters (e.g. "e2e4") are entered, submit the move
            if (input_len == 4) {
                // Package move as uint32_t: byte0=char0, byte1=char1, etc.
                uint32_t msg = ((uint32_t)move_input[0] << 24) |
                               ((uint32_t)move_input[1] << 16) |
                               ((uint32_t)move_input[2] << 8)  |
                               ((uint32_t)move_input[3]);
                
                // Push to Core 0 blocking FIFO queue
                multicore_fifo_push_blocking(msg);

                // Flash LEDs 1-4 to acknowledge submission
                for (int flash = 0; flash < 2; flash++) {
                    tm1638_set_leds(0x0F);
                    sleep_ms(80);
                    tm1638_set_leds(0x00);
                    sleep_ms(80);
                }

                // Reset input state
                input_len = 0;
                memset(move_input, 0, sizeof(move_input));
            }
        }

        // 2. Check if Core 0 has sent a move string (e.g. engine's reply or move mirror)
        if (multicore_fifo_rvalid()) {
            uint32_t msg = multicore_fifo_pop_blocking();
            char rcv_move[5];
            rcv_move[0] = (char)((msg >> 24) & 0xFF);
            rcv_move[1] = (char)((msg >> 16) & 0xFF);
            rcv_move[2] = (char)((msg >> 8) & 0xFF);
            rcv_move[3] = (char)(msg & 0xFF);
            rcv_move[4] = '\0';

            // Render it on the right 4 digits of display
            char disp_str[16];
            snprintf(disp_str, sizeof(disp_str), "    %-4s", rcv_move);
            tm1638_display_string(disp_str);

            // Blink all LEDs to signal that a move has occurred
            for (int i = 0; i < 3; i++) {
                tm1638_set_leds(0xFF);
                sleep_ms(120);
                tm1638_set_leds(0x00);
                sleep_ms(120);
            }
        }

        sleep_ms(15);
    }
}

int main() {
    // Initialize stdio
    stdio_init_all();

    // Start Core 1 UI handler thread immediately (will initialize TM1638 display)
    multicore_launch_core1(core1_entry);

    // Wait for USB CDC terminal connection
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
