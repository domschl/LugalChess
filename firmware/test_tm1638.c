// test_tm1638.c - Isolated single-core TM1638 hardware test
// No chess engine, no multicore, no complex USB interaction.
// Just TM1638 display + keyboard scanning with USB serial debug output.
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "tm1638.h"

int main() {
    stdio_init_all();

    // Wait for USB terminal (required to see debug output)
    for (int i = 0; i < 500; i++) {
        if (stdio_usb_connected()) break;
        sleep_ms(10);
    }
    sleep_ms(200);

    printf("\n=============================\n");
    printf("  TM1638 Hardware Test v1.0\n");
    printf("=============================\n\n");

    tm1638_init();
    printf("[OK] TM1638 initialized\n");

    tm1638_display_string("tESt 1.0");
    printf("[OK] Display: 'tESt 1.0'\n");
    printf("[INFO] Press keys on the TM1638 board.\n");
    printf("[INFO] Keys 0-7 = Files A-H, Keys 8-15 = Ranks 1-8\n\n");

    int last_key = -1;
    int scan_count = 0;
    uint32_t heartbeat = 0;

    while (1) {
        int key = tm1638_get_key();

        if (key != last_key) {
            if (key != -1) {
                printf("[KEY] Pressed: index=%d", key);
                if (key >= 0 && key <= 7) {
                    printf(" (File '%c')\n", 'A' + key);
                } else if (key >= 8 && key <= 15) {
                    printf(" (Rank '%c')\n", '1' + (key - 8));
                } else {
                    printf(" (Unknown)\n");
                }

                // Show key info on display: "Knn  X  "
                // Left side: key index, Right side: character
                char buf[9] = "        ";
                buf[0] = 'K';
                buf[1] = '0' + (key / 10);
                buf[2] = '0' + (key % 10);
                // Show mapped character on digit 6
                if (key >= 0 && key <= 7) {
                    buf[5] = 'A' + key;
                } else {
                    buf[5] = '1' + (key - 8);
                }
                buf[8] = '\0';
                tm1638_display_string(buf);
                printf("[DISP] Updated display to: '%s'\n", buf);
            } else {
                printf("[KEY] Released\n");
            }
            last_key = key;
        }

        scan_count++;
        heartbeat++;

        // Print heartbeat every ~5 seconds (500 iterations * 10ms)
        if (heartbeat >= 500) {
            printf("[HEARTBEAT] scan_count=%d, last_key=%d\n", scan_count, last_key);
            heartbeat = 0;
        }

        sleep_ms(10);
    }

    return 0;
}
