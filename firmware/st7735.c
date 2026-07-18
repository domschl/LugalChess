#include "st7735.h"
#include "defs.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <string.h>
#include <ctype.h>

// Standard 5x7 Pixel ASCII Font Table (covers ASCII 32 to 127)
static const uint8_t font5x7[96][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x08, 0x2a, 0x1c, 0x2a, 0x08}, // *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // z
    {0x08, 0x36, 0x41, 0x41, 0x00}, // {
    {0x00, 0x00, 0x77, 0x00, 0x00}, // |
    {0x00, 0x41, 0x41, 0x36, 0x08}, // }
    {0x08, 0x08, 0x2a, 0x1c, 0x08}, // ~
    {0x00, 0x00, 0x00, 0x00, 0x00}  // 127
};

// 16x16 Chess Piece Monochrome Bitmaps (represented as rows of 16-bits)
static const uint16_t piece_bitmaps[6][16] = {
    // Pawn (P)
    {
        0x0000, 0x0000, 0x03c0, 0x07e0,
        0x07e0, 0x03c0, 0x0180, 0x03c0,
        0x0ff0, 0x1ff8, 0x1ff8, 0x0ff0,
        0x0ff0, 0x1ff8, 0x3ffc, 0x0000
    },
    // Knight (N)
    {
        0x0000, 0x03c0, 0x0fe0, 0x1fe0,
        0x3de0, 0x3be0, 0x3c00, 0x3f00,
        0x3fc0, 0x1ff0, 0x0ff8, 0x0ff8,
        0x1ffc, 0x3ffe, 0x3ffe, 0x0000
    },
    // Bishop (B)
    {
        0x0000, 0x0180, 0x0180, 0x07e0,
        0x0ff0, 0x1e78, 0x3c3c, 0x381c,
        0x3c3c, 0x1e78, 0x0ff0, 0x07e0,
        0x03c0, 0x1ff8, 0x3ffc, 0x0000
    },
    // Rook (R)
    {
        0x0000, 0x0000, 0x3cc3, 0x3ffc,
        0x3ffc, 0x1ff8, 0x1ff8, 0x1ff8,
        0x1ff8, 0x1ff8, 0x1ff8, 0x1ff8,
        0x3ffc, 0x3ffc, 0x3ffc, 0x0000
    },
    // Queen (Q)
    {
        0x0000, 0x2492, 0x2cda, 0x3ffe,
        0x1ff8, 0x1ff8, 0x1e78, 0x1c38,
        0x1ff8, 0x1ff8, 0x0ff0, 0x0ff0,
        0x1ff8, 0x3ffc, 0x3ffc, 0x0000
    },
    // King (K)
    {
        0x0180, 0x07e0, 0x0180, 0x0ff0,
        0x1ff8, 0x3ffe, 0x3c3c, 0x3c3c,
        0x3ffe, 0x1ff8, 0x1e78, 0x0ff0,
        0x0ff0, 0x1ff8, 0x3ffc, 0x0000
    }
};

// Map chess engine piece ID (PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6) to bitmap index
static inline int get_piece_bitmap_idx(int piece) {
    if (piece >= PAWN && piece <= KING) return piece;
    return 0;
}

// Low-level command and data writers
static inline void st7735_write_cmd(uint8_t cmd) {
    gpio_put(ST7735_PIN_DC, 0); // DC low for command
    gpio_put(ST7735_PIN_CS, 0);
    spi_write_blocking(ST7735_SPI_PORT, &cmd, 1);
    gpio_put(ST7735_PIN_CS, 1);
}

static inline void st7735_write_data(uint8_t data) {
    gpio_put(ST7735_PIN_DC, 1); // DC high for data
    gpio_put(ST7735_PIN_CS, 0);
    spi_write_blocking(ST7735_SPI_PORT, &data, 1);
    gpio_put(ST7735_PIN_CS, 1);
}

static void st7735_write_data_buf(const uint8_t *buf, size_t len) {
    gpio_put(ST7735_PIN_DC, 1);
    gpio_put(ST7735_PIN_CS, 0);
    spi_write_blocking(ST7735_SPI_PORT, buf, len);
    gpio_put(ST7735_PIN_CS, 1);
}

// Set column and row writing windows
void st7735_set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    st7735_write_cmd(0x2A); // Column Address Set
    st7735_write_data(0x00);
    st7735_write_data(x0);
    st7735_write_data(0x00);
    st7735_write_data(x1);

    st7735_write_cmd(0x2B); // Row Address Set
    st7735_write_data(0x00);
    st7735_write_data(y0);
    st7735_write_data(0x00);
    st7735_write_data(y1);

    st7735_write_cmd(0x2C); // Memory Write
}

// Initialize ST7735 TFT SPI Controller
void st7735_init(void) {
    // 1. Initialize SPI0 at 24 MHz
    spi_init(ST7735_SPI_PORT, ST7735_BAUDRATE);
    gpio_set_function(ST7735_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(ST7735_PIN_MOSI, GPIO_FUNC_SPI);

    // 2. Initialize control pins (CS, DC, RST)
    gpio_init(ST7735_PIN_CS);
    gpio_set_dir(ST7735_PIN_CS, GPIO_OUT);
    gpio_put(ST7735_PIN_CS, 1);

    gpio_init(ST7735_PIN_DC);
    gpio_set_dir(ST7735_PIN_DC, GPIO_OUT);
    gpio_put(ST7735_PIN_DC, 0);

    gpio_init(ST7735_PIN_RST);
    gpio_set_dir(ST7735_PIN_RST, GPIO_OUT);
    gpio_put(ST7735_PIN_RST, 1);

    // 3. Hardware Reset
    gpio_put(ST7735_PIN_RST, 1);
    sleep_ms(5);
    gpio_put(ST7735_PIN_RST, 0);
    sleep_ms(15);
    gpio_put(ST7735_PIN_RST, 1);
    sleep_ms(15);

    // 4. Initialization Sequence
    st7735_write_cmd(0x01); // Software Reset
    sleep_ms(120);

    st7735_write_cmd(0x11); // Sleep Out
    sleep_ms(120);

    st7735_write_cmd(0xB1); // Frame Rate Control (normal mode)
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);

    st7735_write_cmd(0xB2); // Frame Rate Control (idle mode)
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);

    st7735_write_cmd(0xB3); // Frame Rate Control (partial mode)
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);

    st7735_write_cmd(0xB4); // Display Inversion Control
    st7735_write_data(0x07); // No inversion

    st7735_write_cmd(0xC0); // Power Control 1
    st7735_write_data(0xA2);
    st7735_write_data(0x02);
    st7735_write_data(0x84);

    st7735_write_cmd(0xC1); // Power Control 2
    st7735_write_data(0xC5);

    st7735_write_cmd(0xC2); // Power Control 3
    st7735_write_data(0x0A);
    st7735_write_data(0x00);

    st7735_write_cmd(0xC3); // Power Control 4
    st7735_write_data(0x8A);
    st7735_write_data(0x2A);

    st7735_write_cmd(0xC4); // Power Control 5
    st7735_write_data(0x8A);
    st7735_write_data(0xEE);

    st7735_write_cmd(0xC5); // VCOM Control 1
    st7735_write_data(0x0E);

    st7735_write_cmd(0x36); // MADCTL (Orientation / Scan Direction)
    st7735_write_data(0xC8); // Portrait, RGB color order, normal scan

    st7735_write_cmd(0x3A); // Interface Pixel Format
    st7735_write_data(0x05); // 16-bit color (RGB565)

    st7735_write_cmd(0xE0); // Gamma Adjust (+)
    uint8_t gamma_p[] = {
        0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22,
        0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10
    };
    for(int i=0; i<16; i++) st7735_write_data(gamma_p[i]);

    st7735_write_cmd(0xE1); // Gamma Adjust (-)
    uint8_t gamma_n[] = {
        0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e,
        0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10
    };
    for(int i=0; i<16; i++) st7735_write_data(gamma_n[i]);

    st7735_write_cmd(0x29); // Display On
    sleep_ms(100);

    // Clear display to solid black on start
    st7735_fill_screen(ST7735_BLACK);
}

// Fill screen with a single solid RGB565 color
void st7735_fill_screen(uint16_t color) {
    st7735_set_window(0, 0, 127, 159);
    uint8_t color_bytes[2] = {color >> 8, color & 0xFF};
    
    // Write 128 * 160 pixels (20480 pixels, 40960 bytes)
    // Send in chunks of 512 bytes to keep memory footprint minimal and fast
    uint8_t chunk[512];
    for (int i = 0; i < 512; i += 2) {
        chunk[i] = color_bytes[0];
        chunk[i + 1] = color_bytes[1];
    }
    for (int i = 0; i < 80; i++) {
        st7735_write_data_buf(chunk, 512);
    }
}

// Draw a single pixel at (x, y)
void st7735_draw_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 160) return;
    st7735_set_window(x, y, x, y);
    uint8_t data[2] = {color >> 8, color & 0xFF};
    st7735_write_data_buf(data, 2);
}

// Draw a filled rectangle of size (w, h) at (x, y)
void st7735_draw_rect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0 || x + w > 128 || y < 0 || y + h > 160) return;
    st7735_set_window(x, y, x + w - 1, y + h - 1);
    uint8_t color_bytes[2] = {color >> 8, color & 0xFF};
    
    // Allocate a small transmission line buffer to write efficiently
    // Draw 16 pixels per block (32 bytes) at a time
    uint8_t block[512];
    size_t block_pixels = 256;
    if (block_pixels > (size_t)(w * h)) block_pixels = w * h;
    for (size_t i = 0; i < block_pixels * 2; i += 2) {
        block[i] = block[i] = color_bytes[0];
        block[i + 1] = color_bytes[1];
    }
    
    int total_pixels = w * h;
    int written = 0;
    while (written < total_pixels) {
        int to_write = total_pixels - written;
        if (to_write > (int)block_pixels) to_write = block_pixels;
        st7735_write_data_buf(block, to_write * 2);
        written += to_write;
    }
}

// Draw a 1-bit monochrome bitmap of size (w, h) at (x, y)
void st7735_draw_bitmap_mono(int x, int y, int w, int h, const uint16_t *bitmap, uint16_t fg_color, uint16_t bg_color) {
    if (x < 0 || x + w > 128 || y < 0 || y + h > 160) return;
    st7735_set_window(x, y, x + w - 1, y + h - 1);
    
    // Formulate RGB565 stream
    uint8_t buffer[512]; // 16x16 pixels is 256 pixels, which is exactly 512 bytes!
    int idx = 0;
    
    uint8_t fg_h = fg_color >> 8;
    uint8_t fg_l = fg_color & 0xFF;
    uint8_t bg_h = bg_color >> 8;
    uint8_t bg_l = bg_color & 0xFF;

    for (int r = 0; r < h; r++) {
        uint16_t row_bits = bitmap[r];
        for (int c = 0; c < w; c++) {
            if ((row_bits >> (15 - c)) & 1) {
                buffer[idx++] = fg_h;
                buffer[idx++] = fg_l;
            } else {
                buffer[idx++] = bg_h;
                buffer[idx++] = bg_l;
            }
        }
    }
    st7735_write_data_buf(buffer, w * h * 2);
}

// Draw standard text character using 5x7 Font
void st7735_draw_char(int x, int y, char c, uint16_t color, int size) {
    if (c < 32 || c > 127) return;
    int font_idx = c - 32;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[font_idx][col];
        for (int row = 0; row < 8; row++) {
            if (line & 1) {
                if (size == 1) {
                    st7735_draw_pixel(x + col, y + row, color);
                } else {
                    st7735_draw_rect(x + col * size, y + row * size, size, size, color);
                }
            }
            line >>= 1;
        }
    }
}

// Draw text string
void st7735_draw_string(int x, int y, const char *str, uint16_t color, int size) {
    while (*str) {
        st7735_draw_char(x, y, *str, color, size);
        x += 6 * size; // 5 pixels width + 1 pixel padding
        if (x > 120) break; // wrap limit
        str++;
    }
}

// Render Chess Board graphically on the 128x128 pixel block (Y: 0..127)
void st7735_draw_board(const Position *pos) {
    for (int r = 7; r >= 0; r--) {
        for (int f = 0; f < 8; f++) {
            int sq = f + r * 8;
            int x = f * 16;
            int y = (7 - r) * 16; // rank 8 at top, rank 1 at bottom

            // Determine square color (alternating light/dark)
            uint16_t sq_color = ((r + f) % 2 == 1) ? ST7735_LIGHT_SQ : ST7735_DARK_SQ;
            
            // Check if square is occupied by a piece
            int piece = pos->board[sq];
            if (piece != NO_PIECE) {
                int color_side = (pos->color_bbs[WHITE] & (1ULL << sq)) ? WHITE : BLACK;
                uint16_t piece_fg = (color_side == WHITE) ? ST7735_WHITE : ST7735_BLACK;
                
                // Draw piece centered inside the square
                int bitmap_idx = get_piece_bitmap_idx(piece);
                st7735_draw_bitmap_mono(x, y, 16, 16, piece_bitmaps[bitmap_idx], piece_fg, sq_color);
            } else {
                // Square is empty, just fill it
                st7735_draw_rect(x, y, 16, 16, sq_color);
            }
        }
    }
}

// Draw console/game status metadata on the remaining 128x32 pixel block (Y: 128..159)
void st7735_draw_status(const Position *pos, int level, int side, const char *last_player, const char *last_engine, const char *status_msg) {
    // 1. Clear status area to solid black
    st7735_draw_rect(0, 128, 128, 32, ST7735_BLACK);
    
    // 2. Draw border line separating board and status
    st7735_draw_rect(0, 127, 128, 1, ST7735_GRAY);

    // 3. Draw Level & Side info
    char level_buf[16];
    snprintf(level_buf, sizeof(level_buf), "LVL:%d  %s", level, (side == WHITE) ? "WHITE" : "BLACK");
    st7735_draw_string(2, 131, level_buf, ST7735_YELLOW, 1);

    // 4. Draw moves or status message
    if (status_msg != NULL && strlen(status_msg) > 0 && strcmp(status_msg, "CHk     ") != 0) {
        // Draw primary game-end notifications (e.g. checkmate/draw) in highlighted colors
        uint16_t msg_color = ST7735_RED;
        if (strncmp(status_msg, "nAtE", 4) == 0) msg_color = ST7735_RED;
        else if (strncmp(status_msg, "drAU", 4) == 0) msg_color = ST7735_GRAY;
        
        st7735_draw_string(2, 145, status_msg, msg_color, 1);
    } else {
        // Render current moves
        char moves_buf[24];
        char player_formatted[8];
        char engine_formatted[8];
        
        // Copy player move (strip blanks and uppercase)
        int p_idx = 0;
        for (int i = 0; i < 5; i++) {
            if (last_player[i] != ' ' && last_player[i] != '\0') {
                player_formatted[p_idx++] = toupper((unsigned char)last_player[i]);
            }
        }
        player_formatted[p_idx] = '\0';

        // Copy engine move (strip blanks and uppercase)
        int e_idx = 0;
        for (int i = 0; i < 5; i++) {
            if (last_engine[i] != ' ' && last_engine[i] != '\0') {
                engine_formatted[e_idx++] = toupper((unsigned char)last_engine[i]);
            }
        }
        engine_formatted[e_idx] = '\0';

        snprintf(moves_buf, sizeof(moves_buf), "%s -> %s", 
                 (strlen(player_formatted) > 0) ? player_formatted : "----",
                 (strlen(engine_formatted) > 0) ? engine_formatted : "----");
        st7735_draw_string(2, 145, moves_buf, ST7735_WHITE, 1);
        
        // If check is active, overlay a "CHECK" flag at the end
        if (status_msg != NULL && strcmp(status_msg, "CHk     ") == 0) {
            st7735_draw_string(92, 131, "CHECK", ST7735_RED, 1);
        }
    }
}
