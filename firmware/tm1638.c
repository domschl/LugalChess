#include "tm1638.h"
#include <string.h>

// 7-segment font mapping for printable ASCII characters
static const uint8_t font7seg[128] = {
    [' '] = 0x00, ['-'] = 0x40, ['_'] = 0x08, ['='] = 0x48,
    ['0'] = 0x3F, ['1'] = 0x06, ['2'] = 0x5B, ['3'] = 0x4F,
    ['4'] = 0x66, ['5'] = 0x6D, ['6'] = 0x7D, ['7'] = 0x07,
    ['8'] = 0x7F, ['9'] = 0x6F,
    ['A'] = 0x77, ['B'] = 0x7C, ['C'] = 0x39, ['D'] = 0x5E,
    ['E'] = 0x79, ['F'] = 0x71, ['G'] = 0x3D, ['H'] = 0x76,
    ['I'] = 0x06, ['J'] = 0x1E, ['L'] = 0x38, ['O'] = 0x3F,
    ['P'] = 0x73, ['S'] = 0x6D, ['U'] = 0x3E, ['Y'] = 0x6E,
    ['a'] = 0x5F, ['b'] = 0x7C, ['c'] = 0x58, ['d'] = 0x5E,
    ['e'] = 0x7B, ['f'] = 0x71, ['g'] = 0x6F, ['h'] = 0x74,
    ['i'] = 0x04, ['j'] = 0x0E, ['l'] = 0x06, ['n'] = 0x54,
    ['o'] = 0x5C, ['p'] = 0x73, ['r'] = 0x50, ['u'] = 0x1C,
    ['t'] = 0x78, ['y'] = 0x6E
};

// Internal 16-byte RAM cache corresponding to display & LED addresses
static uint8_t tm1638_ram[16];

// Send command to the display module
static void tm1638_send_command(uint8_t cmd) {
    gpio_put(TM_STB_PIN, 0);
    tm1638_write_byte(cmd);
    gpio_put(TM_STB_PIN, 1);
}

// Flush local RAM cache to TM1638 display registers using Auto-Increment address mode (0x40)
static void tm1638_flush(void) {
    tm1638_send_command(0x40); // Set write mode to auto increment address

    gpio_put(TM_STB_PIN, 0);
    tm1638_write_byte(0xC0); // Start address
    for (int i = 0; i < 16; i++) {
        tm1638_write_byte(tm1638_ram[i]);
    }
    gpio_put(TM_STB_PIN, 1);
}

// Write a byte to DIO with stable timing delays for RP2350 high clock rate
void tm1638_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        gpio_put(TM_CLK_PIN, 0);
        gpio_put(TM_DIO_PIN, (byte >> i) & 1);
        sleep_us(3); // Clock low pulse width
        gpio_put(TM_CLK_PIN, 1);
        sleep_us(3); // Clock high pulse width
    }
}

// Read a byte from DIO (during key scanning)
uint8_t tm1638_read_byte(void) {
    uint8_t byte = 0;
    gpio_set_dir(TM_DIO_PIN, GPIO_IN);
    for (int i = 0; i < 8; i++) {
        gpio_put(TM_CLK_PIN, 0);
        sleep_us(3);
        gpio_put(TM_CLK_PIN, 1);
        sleep_us(3);
        if (gpio_get(TM_DIO_PIN)) {
            byte |= (1 << i);
        }
    }
    gpio_set_dir(TM_DIO_PIN, GPIO_OUT);
    return byte;
}

// Initialize GPIO pins and TM1638 chip
void tm1638_init(void) {
    gpio_init(TM_STB_PIN);
    gpio_init(TM_CLK_PIN);
    gpio_init(TM_DIO_PIN);

    gpio_set_dir(TM_STB_PIN, GPIO_OUT);
    gpio_set_dir(TM_CLK_PIN, GPIO_OUT);
    gpio_set_dir(TM_DIO_PIN, GPIO_OUT);

    // CRITICAL: Enable internal pull-up resistor on the bidirectional DIO pin
    // This prevents the pin from floating when the TM1638 drives it during scanning.
    gpio_pull_up(TM_DIO_PIN);

    gpio_put(TM_STB_PIN, 1);
    gpio_put(TM_CLK_PIN, 1);
    gpio_put(TM_DIO_PIN, 0);

    sleep_ms(10);

    // Command 1: Active display with max brightness (0x8F)
    tm1638_send_command(0x8F);

    // Clear RAM cache and flush it to clean the screen and LEDs
    memset(tm1638_ram, 0, 16);
    tm1638_flush();
}

// Display an 8-character string on the 7-segment displays
void tm1638_display_string(const char *str) {
    uint8_t buffer[8];
    memset(buffer, 0, 8);

    int len = strlen(str);
    int buf_idx = 0;

    for (int i = 0; i < len && buf_idx < 8; i++) {
        char c = str[i];
        
        // Handle decimal point combined with previous character
        if (c == '.' && buf_idx > 0) {
            buffer[buf_idx - 1] |= 0x80;
        } else {
            uint8_t pattern = 0x00;
            if ((uint8_t)c < 128) {
                pattern = font7seg[(uint8_t)c];
            }
            buffer[buf_idx++] = pattern;
        }
    }

    // Clear digit values in RAM cache (even indices)
    for (int seg = 0; seg < 8; seg++) {
        tm1638_ram[seg * 2] = 0x00;
    }

    // CRITICAL: Transpose the 8x8 matrix (since QYF-TM1638 is Common Anode)
    // Digit i segment s is bit s of buffer[i].
    // It maps to bit i of tm1638_ram[s * 2] (address 0xC0 + s*2).
    for (int seg = 0; seg < 8; seg++) {
        uint8_t val = 0;
        for (int digit = 0; digit < 8; digit++) {
            if ((buffer[digit] >> seg) & 1) {
                val |= (1 << digit);
            }
        }
        tm1638_ram[seg * 2] = val;
    }

    // Flush cache to screen
    tm1638_flush();
}

// Set states of the 8 individual LEDs (bitmask)
void tm1638_set_leds(uint8_t mask) {
    // Note: QYF-TM1638 modules typically do not have the 8 individual LEDs,
    // but we support it in cache anyway for completeness.
    for (int i = 0; i < 8; i++) {
        tm1638_ram[(i * 2) + 1] = (mask >> i) & 1;
    }

    // Flush cache to screen
    tm1638_flush();
}

// Scan the keyboard and return pressed key index (0 to 15), or -1 if none
int tm1638_get_key(void) {
    uint8_t keys[4];
    
    gpio_put(TM_STB_PIN, 0);
    tm1638_write_byte(0x42); // Read keys command
    
    // CRITICAL: Wait at least 5 microseconds for DIO pin state turnaround
    sleep_us(5);
    
    // Read 4 bytes of scan data
    for (int i = 0; i < 4; i++) {
        keys[i] = tm1638_read_byte();
    }
    gpio_put(TM_STB_PIN, 1);
    
    // Matrix key decoding (matched to the QYF-TM1638 matrix scan pattern on K3 and K2):
    for (int i = 0; i < 4; i++) {
        uint8_t i_keys = keys[i];
        for (int k = 0; k < 2; k++) {
            for (int j = 0; j < 2; j++) {
                uint8_t x = (0x04 >> k) << (j * 4);
                if ((i_keys & x) == x) {
                    // This button is pressed!
                    return j + k * 2 + i * 4;
                }
            }
        }
    }
    
    return -1; // No key pressed
}
