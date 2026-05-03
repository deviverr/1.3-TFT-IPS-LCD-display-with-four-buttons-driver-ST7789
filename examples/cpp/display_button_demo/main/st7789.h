#pragma once

#include <stdint.h>
#include "esp_err.h"

/*
 * ST7789 driver for 1.3" IPS TFT LCD (240x240) on ESP32-S3 Mini.
 *
 * Wiring (confirmed hardware pinout):
 *   SCL (SPI clock) ── GPIO 3
 *   SDA (SPI MOSI)  ── GPIO 4
 *   RES (reset)     ── GPIO 7
 *   DC              ── GPIO 8
 *   BLK (backlight) ── GPIO 9
 *   CS              ── GND (tied low on module, CS disabled in software)
 */

#define ST7789_PIN_SCLK    3
#define ST7789_PIN_MOSI    4
#define ST7789_PIN_CS      (-1)
#define ST7789_PIN_DC      8
#define ST7789_PIN_RST     7
#define ST7789_PIN_BL      9
#define ST7789_WIDTH       240
#define ST7789_HEIGHT      240

/* RGB565 color constants */
#define ST7789_BLACK    0x0000
#define ST7789_WHITE    0xFFFF
#define ST7789_RED      0xF800
#define ST7789_GREEN    0x07E0
#define ST7789_BLUE     0x001F
#define ST7789_YELLOW   0xFFE0
#define ST7789_CYAN     0x07FF
#define ST7789_MAGENTA  0xF81F
#define ST7789_ORANGE   0xFD20
#define ST7789_GRAY     0x7BEF
#define ST7789_DGRAY    0x2104
#define ST7789_DGREEN   0x0300

/*
 * Initialize SPI bus, panel IO, and ST7789 panel.
 * Clears screen to black on success.
 */
esp_err_t st7789_init(void);

/* Fill entire screen with color. */
void st7789_fill(uint16_t color);

/* Fill axis-aligned rectangle. Clips to screen bounds. */
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);

/*
 * Draw a single character from the built-in 8x8 bitmap font.
 * scale 1–4: pixel size multiplier. Unsupported chars render as '?'.
 */
void st7789_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale);

/* Draw a string left-aligned at (x, y). Clips at screen right edge. */
void st7789_draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);

/* Draw a string horizontally centered at row y. */
void st7789_draw_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale);
