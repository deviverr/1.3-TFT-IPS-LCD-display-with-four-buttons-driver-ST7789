#include "st7789.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "st7789";

extern const uint8_t font8x8_basic[96][8];

static esp_lcd_panel_handle_t s_panel = NULL;

/* ST7789 expects the high byte of each 16-bit pixel first on the wire,
 * but a uint16_t on the little-endian ESP32 has the low byte at the lower
 * address. Byteswap when filling pixel buffers. */
static inline uint16_t swap16(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

/* ── Primitives ───────────────────────────────────────────────────────── */

void st7789_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_panel) return;
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7789_WIDTH)  w = ST7789_WIDTH  - x;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    /* Stripe 8 rows at a time so the DMA buffer stays small (240*8*2 = 3840 B). */
    const int chunk = 8;
    size_t n = (size_t)w * chunk;
    uint16_t *buf = heap_caps_malloc(n * sizeof(uint16_t),
                                     MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "fill_rect: oom");
        return;
    }
    uint16_t c = swap16(color);
    for (size_t i = 0; i < n; i++) buf[i] = c;

    int rows_left = h, yy = y;
    while (rows_left > 0) {
        int rows = rows_left > chunk ? chunk : rows_left;
        esp_lcd_panel_draw_bitmap(s_panel, x, yy, x + w, yy + rows, buf);
        yy += rows;
        rows_left -= rows;
    }
    free(buf);
}

void st7789_fill(uint16_t color)
{
    st7789_fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void st7789_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    if (!s_panel) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (c < 0x20 || c > 0x7F) c = '?';
    const uint8_t *glyph = font8x8_basic[(int)(c - 0x20)];

    int w = 8 * scale, h = 8 * scale;
    uint16_t buf[32 * 32];  /* 4x scale max → 32×32×2 = 2048 B on stack */
    uint16_t fg_be = swap16(fg), bg_be = swap16(bg);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint16_t pix = (bits & (1 << col)) ? fg_be : bg_be;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    buf[(row * scale + sy) * w + col * scale + sx] = pix;
        }
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
}

void st7789_draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    while (*s) {
        if (x + 8 * scale > ST7789_WIDTH) break;
        st7789_draw_char(x, y, *s, fg, bg, scale);
        x += 8 * scale;
        s++;
    }
}

void st7789_draw_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    int total_w = (int)strlen(s) * 8 * scale;
    int x = (ST7789_WIDTH - total_w) / 2;
    if (x < 0) x = 0;
    st7789_draw_text(x, y, s, fg, bg, scale);
}

/* ── Init ─────────────────────────────────────────────────────────────── */

esp_err_t st7789_init(void)
{
    /* Backlight on as early as possible. */
    gpio_config_t bl_io = {
        .pin_bit_mask = 1ULL << ST7789_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_io);
    gpio_set_level(ST7789_PIN_BL, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num    = ST7789_PIN_SCLK,
        .mosi_io_num    = ST7789_PIN_MOSI,
        .miso_io_num    = -1,
        .quadwp_io_num  = -1,
        .quadhd_io_num  = -1,
        .max_transfer_sz = ST7789_WIDTH * 8 * sizeof(uint16_t) + 64,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num        = ST7789_PIN_DC,
        .cs_gpio_num        = ST7789_PIN_CS,
        .pclk_hz            = 10 * 1000 * 1000,
        .lcd_cmd_bits       = 8,
        .lcd_param_bits     = 8,
        .spi_mode           = 3,   /* CPOL=1, CPHA=1 */
        .trans_queue_depth  = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                   &io_config, &io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel_io_spi: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = ST7789_PIN_RST,
        .bits_per_pixel = 16,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
    };
    err = esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new_panel_st7789: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    st7789_fill(ST7789_BLACK);
    return ESP_OK;
}
