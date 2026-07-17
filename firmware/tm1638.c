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

    // Update digit values in local RAM cache (even indices: 0, 2, 4, ... 14)
    for (int i = 0; i < 8; i++) {
        tm1638_ram[i * 2] = buffer[i];
    }

    // Flush cache to screen
    tm1638_flush();
}

// Set states of the 8 individual LEDs (bitmask)
void tm1638_set_leds(uint8_t mask) {
    // Update LED values in local RAM cache (odd indices: 1, 3, 5, ... 15)
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
    
    // Matrix key decoding:
    // Row 0 (K1, KS1..KS4): keys 0, 1, 2, 3
    // Row 1 (K1, KS5..KS8): keys 4, 5, 6, 7
    // Row 2 (K2, KS1..KS4): keys 8, 9, 10, 11
    // Row 3 (K2, KS5..KS8): keys 12, 13, 14, 15
    for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
        uint8_t b = keys[byte_idx];
        
        // KS(2*byte_idx+1) -> Column odd (KS1, KS3, KS5, KS7)
        if (b & 0x01) return byte_idx * 2;       // Row 0/1 (K1)
        if (b & 0x02) return byte_idx * 2 + 8;   // Row 2/3 (K2)
        
        // KS(2*byte_idx+2) -> Column even (KS2, KS4, KS6, KS8)
        if (b & 0x10) return byte_idx * 2 + 1;   // Row 0/1 (K1)
        if (b & 0x20) return byte_idx * 2 + 9;   // Row 2/3 (K2)
    }
    
    return -1; // No key pressed
}
