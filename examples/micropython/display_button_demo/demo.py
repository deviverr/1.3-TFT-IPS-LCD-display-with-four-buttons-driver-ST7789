"""
3-phase demo for the 1.3" ST7789 TFT + 4 buttons on ESP32-S3 Mini.

Phase 1 — Color test:  7 solid colors cycle on screen.
Phase 2 — Graphics:    Concentric rectangles + "Hello ST7789!" text.
Phase 3 — Button test: On-screen boxes light up as buttons are pressed.
                        Ends when all 4 buttons have been pressed at least once.

Wiring (confirmed hardware pinout):
  SCL → GPIO 3  |  SDA → GPIO 4  |  RES → GPIO 7
  DC  → GPIO 8  |  BLK → GPIO 9
  K1  → GPIO 13 (UP)    K2 → GPIO 12 (DOWN)
  K3  → GPIO 11 (SEL)   K4 → GPIO 10 (BACK)

Flash with:
  mpremote connect <PORT> cp st7789.py :st7789.py + cp ui.py :ui.py + cp demo.py :demo.py + run demo.py
"""

from machine import Pin, SPI
import time
import st7789
from ui import draw_text

# ── Pin config ────────────────────────────────────────────────────────────
SPI_SCK  = 3
SPI_MOSI = 4
LCD_RST  = 7
LCD_DC   = 8
LCD_BL   = 9

BTN_PINS   = [13, 12, 11, 10]          # K1 K2 K3 K4
BTN_NAMES  = ["K1", "K2", "K3", "K4"]
BTN_LABELS = ["UP", "DOWN", "SEL", "BACK"]

# ── Colors (RGB565) ───────────────────────────────────────────────────────
BLACK   = 0x0000
WHITE   = 0xFFFF
RED     = 0xF800
GREEN   = 0x07E0
BLUE    = 0x001F
YELLOW  = 0xFFE0
CYAN    = 0x07FF
MAGENTA = 0xF81F
GRAY    = 0x7BEF
DGRAY   = 0x2104
DGREEN  = 0x0300

# ── Hardware init ─────────────────────────────────────────────────────────

def init_display():
    spi = SPI(2, baudrate=10_000_000, polarity=1, phase=1,
              sck=Pin(SPI_SCK), mosi=Pin(SPI_MOSI))
    display = st7789.ST7789(
        spi, 240, 240,
        reset=Pin(LCD_RST, Pin.OUT),
        dc=Pin(LCD_DC, Pin.OUT),
        backlight=Pin(LCD_BL, Pin.OUT),
        rotation=0,
    )
    display.init()
    display.fill(BLACK)
    return display


def init_buttons():
    return [Pin(p, Pin.IN, Pin.PULL_UP) for p in BTN_PINS]

# ── Helper: centered text ─────────────────────────────────────────────────

def draw_centered(display, y, text, color, scale=1):
    x = (240 - len(text) * 6 * scale) // 2
    if x < 0:
        x = 0
    draw_text(display, x, y, text, color, scale)

# ── Phase 1: Color test ───────────────────────────────────────────────────

def phase_color_test(display):
    colors = [
        (RED,     "RED"),
        (GREEN,   "GREEN"),
        (BLUE,    "BLUE"),
        (YELLOW,  "YELLOW"),
        (CYAN,    "CYAN"),
        (MAGENTA, "MAGENTA"),
        (WHITE,   "WHITE"),
    ]
    for color, name in colors:
        display.fill(color)
        tc = BLACK if color == WHITE else WHITE
        draw_centered(display, 95,  name,         tc, scale=3)
        draw_centered(display, 134, "COLOR TEST", GRAY if color != WHITE else DGRAY, scale=1)
        time.sleep_ms(800)

# ── Phase 2: Graphics test ────────────────────────────────────────────────

def phase_graphics_test(display):
    display.fill(BLACK)
    rect_colors = [RED, GREEN, BLUE, YELLOW, CYAN]
    for i, c in enumerate(rect_colors):
        m = i * 22
        display.fill_rect(m, m, 240 - 2 * m, 240 - 2 * m, c)
    display.fill_rect(70, 70, 100, 100, BLACK)
    draw_centered(display, 92,  "HELLO",   CYAN,  scale=3)
    draw_centered(display, 122, "ST7789!", WHITE, scale=2)
    time.sleep_ms(3000)

# ── Phase 3: Button test ──────────────────────────────────────────────────

BTN_BOX_W, BTN_BOX_H = 100, 75
BTN_X = [10, 130, 10, 130]
BTN_Y = [55, 55, 145, 145]


def draw_button_box(display, idx, pressed, visited):
    bg = CYAN   if pressed else \
         DGREEN if visited else \
         DGRAY
    fg = BLACK  if (pressed or visited) else GRAY

    x, y = BTN_X[idx], BTN_Y[idx]
    display.fill_rect(x, y, BTN_BOX_W, BTN_BOX_H, bg)

    name  = BTN_NAMES[idx]
    label = BTN_LABELS[idx]

    name_x  = x + (BTN_BOX_W - len(name)  * 12) // 2
    label_x = x + (BTN_BOX_W - len(label) *  6) // 2

    draw_text(display, name_x,  y + 10, name,  fg, scale=2)
    draw_text(display, label_x, y + 36, label, fg, scale=1)

    if visited and not pressed:
        ok_x = x + (BTN_BOX_W - 12) // 2
        draw_text(display, ok_x, y + 54, "OK", GREEN, scale=1)


def phase_button_test(display, buttons):
    visited       = [False] * 4
    prev_pressed  = [False] * 4
    visited_count = 0

    display.fill(BLACK)
    draw_centered(display, 8,  "BUTTON TEST",      CYAN, scale=2)
    display.hline(0, 32, 240, GRAY)
    draw_centered(display, 36, "PRESS ALL 4 BUTTONS", GRAY, scale=1)

    for i in range(4):
        draw_button_box(display, i, False, False)

    draw_centered(display, 224, "TESTED: 0 / 4", GRAY, scale=1)

    while visited_count < 4:
        for i, btn in enumerate(buttons):
            pressed = btn.value() == 0  # active-low

            if pressed == prev_pressed[i]:
                continue

            if pressed and not visited[i]:
                visited[i] = True
                visited_count += 1

            draw_button_box(display, i, pressed, visited[i])
            prev_pressed[i] = pressed

            display.fill_rect(0, 224, 240, 16, BLACK)
            status = "TESTED: {} / 4".format(visited_count)
            sc = GREEN if visited_count == 4 else GRAY
            draw_centered(display, 224, status, sc, scale=1)

        time.sleep_ms(20)

    time.sleep_ms(500)
    display.fill(BLACK)
    draw_centered(display, 88,  "ALL PASS",        GREEN, scale=3)
    draw_centered(display, 130, "ALL 4 BUTTONS OK", GRAY, scale=1)
    time.sleep_ms(2000)

# ── Main loop ─────────────────────────────────────────────────────────────

display = init_display()
buttons = init_buttons()

while True:
    phase_color_test(display)
    phase_graphics_test(display)
    phase_button_test(display, buttons)
