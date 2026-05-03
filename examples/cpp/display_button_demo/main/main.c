#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "st7789.h"

/*
 * 3-phase demo for the 1.3" ST7789 TFT + 4 buttons on ESP32-S3 Mini.
 *
 * Phase 1 — Color test:  7 solid colors cycle on screen, name displayed.
 * Phase 2 — Graphics:    Concentric rectangles, "Hello ST7789!" text.
 * Phase 3 — Button test: 4 on-screen boxes light up as buttons are pressed.
 *                         Ends when all 4 buttons have been pressed at least once.
 *
 * Buttons are active-low (GPIO reads 0 when pressed) via internal pull-up.
 * Wiring:  K1 → GPIO13 (UP)   K2 → GPIO12 (DOWN)
 *          K3 → GPIO11 (SEL)  K4 → GPIO10 (BACK)
 */

#define BTN_K1  13
#define BTN_K2  12
#define BTN_K3  11
#define BTN_K4  10
#define NUM_BTNS 4

static const int BTN_PINS[NUM_BTNS] = {BTN_K1, BTN_K2, BTN_K3, BTN_K4};
static const char * const BTN_NAMES[NUM_BTNS]  = {"K1", "K2", "K3", "K4"};
static const char * const BTN_LABELS[NUM_BTNS] = {"UP", "DOWN", "SEL", "BACK"};

/* Button box layout: 2×2 grid, 100×75 each */
#define BTN_BOX_W  100
#define BTN_BOX_H   75
static const int BTN_X[NUM_BTNS] = {10, 130, 10, 130};
static const int BTN_Y[NUM_BTNS] = {55, 55, 145, 145};

/* ── Hardware init ───────────────────────────────────────────────────── */

static void buttons_init(void)
{
    for (int i = 0; i < NUM_BTNS; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask   = 1ULL << BTN_PINS[i],
            .mode           = GPIO_MODE_INPUT,
            .pull_up_en     = GPIO_PULLUP_ENABLE,
            .pull_down_en   = GPIO_PULLDOWN_DISABLE,
            .intr_type      = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
    }
}

static inline bool btn_pressed(int idx)
{
    return gpio_get_level(BTN_PINS[idx]) == 0;  /* active-low */
}

/* ── Phase 1: Color test ─────────────────────────────────────────────── */

static void phase_color_test(void)
{
    typedef struct { uint16_t color; const char *name; } ColorEntry;
    static const ColorEntry colors[] = {
        { ST7789_RED,     "RED"     },
        { ST7789_GREEN,   "GREEN"   },
        { ST7789_BLUE,    "BLUE"    },
        { ST7789_YELLOW,  "YELLOW"  },
        { ST7789_CYAN,    "CYAN"    },
        { ST7789_MAGENTA, "MAGENTA" },
        { ST7789_WHITE,   "WHITE"   },
    };

    for (int i = 0; i < (int)(sizeof(colors) / sizeof(colors[0])); i++) {
        st7789_fill(colors[i].color);
        uint16_t tc = (colors[i].color == ST7789_WHITE) ? ST7789_BLACK : ST7789_WHITE;
        st7789_draw_text_centered(95,  colors[i].name, tc, colors[i].color, 3);
        st7789_draw_text_centered(134, "COLOR TEST",   tc, colors[i].color, 1);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

/* ── Phase 2: Graphics test ──────────────────────────────────────────── */

static void phase_graphics_test(void)
{
    st7789_fill(ST7789_BLACK);

    static const uint16_t rc[] = {
        ST7789_RED, ST7789_GREEN, ST7789_BLUE, ST7789_YELLOW, ST7789_CYAN
    };
    for (int i = 0; i < 5; i++) {
        int m = i * 22;
        st7789_fill_rect(m, m, 240 - 2 * m, 240 - 2 * m, rc[i]);
    }
    st7789_fill_rect(70, 70, 100, 100, ST7789_BLACK);
    st7789_draw_text_centered(95,  "HELLO",   ST7789_CYAN,  ST7789_BLACK, 2);
    st7789_draw_text_centered(113, "ST7789!", ST7789_WHITE, ST7789_BLACK, 2);

    vTaskDelay(pdMS_TO_TICKS(3000));
}

/* ── Phase 3: Button test ────────────────────────────────────────────── */

static void draw_button_box(int idx, bool pressed, bool visited)
{
    uint16_t bg = pressed  ? ST7789_CYAN  :
                  visited  ? ST7789_DGREEN :
                             ST7789_DGRAY;
    uint16_t fg = (pressed || visited) ? ST7789_BLACK : ST7789_GRAY;

    int x = BTN_X[idx], y = BTN_Y[idx];
    st7789_fill_rect(x, y, BTN_BOX_W, BTN_BOX_H, bg);

    /* Name label (scale 2 = 16 px/char) — centered horizontally in box */
    int name_len = (int)strlen(BTN_NAMES[idx]);
    int name_x   = x + (BTN_BOX_W - name_len * 16) / 2;
    st7789_draw_text(name_x, y + 10, BTN_NAMES[idx], fg, bg, 2);

    /* Sub-label (scale 1 = 8 px/char) */
    int lbl_len = (int)strlen(BTN_LABELS[idx]);
    int lbl_x   = x + (BTN_BOX_W - lbl_len * 8) / 2;
    st7789_draw_text(lbl_x, y + 36, BTN_LABELS[idx], fg, bg, 1);

    /* "OK" tick when visited but not currently held */
    if (visited && !pressed) {
        int ok_x = x + (BTN_BOX_W - 16) / 2;
        st7789_draw_text(ok_x, y + 52, "OK", ST7789_GREEN, bg, 1);
    }
}

static void phase_button_test(void)
{
    bool visited[NUM_BTNS] = {false};
    bool prev[NUM_BTNS]    = {false};
    int visited_count = 0;
    char status_buf[24];

    st7789_fill(ST7789_BLACK);
    st7789_draw_text_centered(8,  "BUTTON TEST",      ST7789_CYAN, ST7789_BLACK, 2);
    st7789_fill_rect(0, 32, 240, 1, ST7789_GRAY);
    st7789_draw_text_centered(36, "PRESS ALL 4 BUTTONS", ST7789_GRAY, ST7789_BLACK, 1);

    for (int i = 0; i < NUM_BTNS; i++) draw_button_box(i, false, false);

    snprintf(status_buf, sizeof(status_buf), "TESTED: 0 / 4");
    st7789_draw_text_centered(224, status_buf, ST7789_GRAY, ST7789_BLACK, 1);

    while (visited_count < NUM_BTNS) {
        for (int i = 0; i < NUM_BTNS; i++) {
            bool cur = btn_pressed(i);
            if (cur == prev[i]) continue;

            if (cur && !visited[i]) {
                visited[i] = true;
                visited_count++;
            }
            draw_button_box(i, cur, visited[i]);
            prev[i] = cur;

            snprintf(status_buf, sizeof(status_buf), "TESTED: %d / 4", visited_count);
            st7789_fill_rect(0, 224, 240, 16, ST7789_BLACK);
            uint16_t sc = (visited_count == NUM_BTNS) ? ST7789_GREEN : ST7789_GRAY;
            st7789_draw_text_centered(224, status_buf, sc, ST7789_BLACK, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    st7789_fill(ST7789_BLACK);
    st7789_draw_text_centered(88,  "ALL PASS",       ST7789_GREEN, ST7789_BLACK, 3);
    st7789_draw_text_centered(130, "ALL 4 BUTTONS OK", ST7789_GRAY, ST7789_BLACK, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

/* ── Entry point ─────────────────────────────────────────────────────── */

void app_main(void)
{
    buttons_init();
    st7789_init();

    while (1) {
        phase_color_test();
        phase_graphics_test();
        phase_button_test();
    }
}
