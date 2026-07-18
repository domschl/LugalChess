#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stddef.h>
#include "position.h"

// ST7735 SPI0 Pin Configuration (Approved standard GP16-GP21 layout)
#define ST7735_SPI_PORT   spi0
#define ST7735_BAUDRATE   (24 * 1000 * 1000) // 24 MHz
#define ST7735_PIN_SCK    18
#define ST7735_PIN_MOSI   19
#define ST7735_PIN_CS     17
#define ST7735_PIN_DC     20
#define ST7735_PIN_RST    21

// RGB565 Colors
#define ST7735_BLACK      0x0000
#define ST7735_WHITE      0xFFFF
#define ST7735_RED        0xF800
#define ST7735_BLUE       0x001F
#define ST7735_GREEN      0x07E0
#define ST7735_YELLOW     0xFFE0
#define ST7735_GRAY       0x8410
#define ST7735_LIGHT_SQ   0xEF7B // Soft Cream
#define ST7735_DARK_SQ    0x2444 // Dark Greenish-Blue
#define ST7735_TEXT_COLOR 0xFFFF

// Driver Functions
void st7735_init(void);
void st7735_fill_screen(uint16_t color);
void st7735_draw_pixel(int x, int y, uint16_t color);
void st7735_draw_rect(int x, int y, int w, int h, uint16_t color);
void st7735_draw_bitmap_mono(int x, int y, int w, int h, const uint16_t *bitmap, uint16_t fg_color, uint16_t bg_color);
void st7735_draw_char(int x, int y, char c, uint16_t color, int size);
void st7735_draw_string(int x, int y, const char *str, uint16_t color, int size);

// Chessboard Graphic Rendering
void st7735_draw_board(const Position *pos);
void st7735_draw_status(const Position *pos, int level, int side, const char *last_player, const char *last_engine, const char *status_msg);

#endif // ST7735_H
