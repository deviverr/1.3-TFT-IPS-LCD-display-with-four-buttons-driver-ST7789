# 1.3" TFT IPS LCD Display with 4 Buttons — ST7789 Driver

Driver and examples for the **1.3-inch IPS TFT LCD module** (240×240 px, ST7789 controller, 4 integrated buttons) on **ESP32-S3 Mini** (Waveshare).

Created because getting this display running on ESP32 can be tricky — SPI mode, pin wiring, and color ordering all need to be correct. This repo has verified working code and the exact wiring.

---

## Hardware

| Component | Details |
|-----------|---------|
| Display | 1.3" IPS TFT, 240×240 pixels, RGB565 |
| Driver IC | ST7789 |
| Interface | SPI (mode 3: CPOL=1, CPHA=1) |
| MCU | ESP32-S3 Mini (Waveshare) |
| Buttons | 4× tactile (K1–K4, active-low via internal pull-up) |
| NFC (optional) | PN532 via I2C |

---

## Pinout

### ST7789 Display → ESP32-S3 Mini

| Display Pin | ESP32-S3 GPIO | Notes |
|-------------|---------------|-------|
| GND | GND | Ground |
| VCC | 3.3V | Power |
| SCL | GPIO 3 | SPI clock |
| SDA | GPIO 4 | SPI MOSI |
| RES | GPIO 7 | Reset (active-low) |
| DC | GPIO 8 | Data / Command select |
| BLK | GPIO 9 | Backlight (HIGH = on) |

### Buttons → ESP32-S3 Mini

| Board Label | ESP32-S3 GPIO | Function | Active |
|-------------|---------------|----------|--------|
| K1 | GPIO 13 | UP | LOW (internal pull-up) |
| K2 | GPIO 12 | DOWN | LOW (internal pull-up) |
| K3 | GPIO 11 | SELECT | LOW (internal pull-up) |
| K4 | GPIO 10 | BACK | LOW (internal pull-up) |

### PN532 NFC (optional) → ESP32-S3 Mini

| PN532 Pin | ESP32-S3 GPIO |
|-----------|---------------|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO 6 |
| SDA | GPIO 5 |

---

## Wiring Diagram

```
ESP32-S3 Mini               1.3" TFT ST7789
─────────────               ───────────────
3.3V    ─────────────────── VCC
GND     ─────────────────── GND
GPIO 3  ─────────────────── SCL  (SPI clock)
GPIO 4  ─────────────────── SDA  (SPI MOSI)
GPIO 7  ─────────────────── RES  (reset)
GPIO 8  ─────────────────── DC   (data/command)
GPIO 9  ─────────────────── BLK  (backlight)

Buttons — internal pull-up, read LOW when pressed:
GPIO 13 ─── K1 (UP)
GPIO 12 ─── K2 (DOWN)
GPIO 11 ─── K3 (SELECT)
GPIO 10 ─── K4 (BACK)
```

> **CS note:** The CS pin on this module is tied to GND on the PCB.
> Set `cs_gpio_num = -1` in C++ or omit `cs` in MicroPython SPI setup.

---

## Examples

Two standalone examples — same 3-phase demo, one in each language:

| Example | Language | Framework |
|---------|----------|-----------|
| [`examples/cpp/display_button_demo`](examples/cpp/display_button_demo/) | C++ | ESP-IDF 5.x |
| [`examples/micropython/display_button_demo`](examples/micropython/display_button_demo/) | MicroPython | v1.28+ |

### What the demo does

**Phase 1 — Color test**

Cycles through 7 solid colors (red → green → blue → yellow → cyan → magenta → white), each with the color name printed in the center. Runs for ~6 seconds total.

```
┌──────────────────────────┐
│                          │
│                          │
│                          │
│           CYAN           │
│         COLOR TEST       │
│                          │
│                          │
└──────────────────────────┘
    Screen filled solid cyan
```

**Phase 2 — Graphics test**

Draws 5 concentric filled rectangles (rainbow colors), then overlays "HELLO / ST7789!" text in the black center. Holds for 3 seconds.

```
┌──────────────────────────┐
│RRRRRRRRRRRRRRRRRRRRRRRRRR│
│R GGGGGGGGGGGGGGGGGGGGGG R│
│R G BBBBBBBBBBBBBBBBBB G R│
│R G B ┌────────────┐ B G R│
│R G B │   HELLO    │ B G R│
│R G B │  ST7789!   │ B G R│
│R G B └────────────┘ B G R│
│R G YYYYYYYYYYYYYYYYYYYY G│
│R CCCCCCCCCCCCCCCCCCCCCC R│
│RRRRRRRRRRRRRRRRRRRRRRRRRR│
└──────────────────────────┘
```

**Phase 3 — Button test**

Shows 4 labeled boxes for K1–K4. Each box lights up **cyan** when the button is held, and turns **dark green** (with "OK") after being released. Ends when all 4 have been pressed at least once.

```
┌──────────────────────────┐
│       BUTTON TEST        │
│ ─────────────────────── │
│   PRESS ALL 4 BUTTONS   │
│  ┌──────────┐ ┌────────┐ │
│  │    K1    │ │   K2   │ │
│  │    UP    │ │  DOWN  │ │
│  │          │ │        │ │
│  └──────────┘ └────────┘ │
│  ┌──────────┐ ┌────────┐ │
│  │    K3    │ │   K4   │ │
│  │   SEL    │ │  BACK  │ │
│  │          │ │        │ │
│  └──────────┘ └────────┘ │
│      TESTED: 0 / 4       │
└──────────────────────────┘
   Cyan on press → dark green + OK after release
```

---

## C++ / ESP-IDF Quick Start

Requires **ESP-IDF 5.x**. [Install guide →](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)

```bash
cd examples/cpp/display_button_demo

# Set target and build
idf.py set-target esp32s3
idf.py build

# Flash and open serial monitor (replace PORT with e.g. COM3 or /dev/ttyUSB0)
idf.py -p PORT flash monitor
```

### Key files

| File | Purpose |
|------|---------|
| `main/st7789.h` | Pin defines, color constants, public API |
| `main/st7789.c` | SPI init, fill, rect, and text primitives |
| `main/font8x8.c` | Public-domain 8×8 bitmap font |
| `main/main.c` | 3-phase demo using the driver |
| `sdkconfig.defaults` | ESP32-S3 target, SPI ISR in IRAM |

### Driver API (C++)

```c
esp_err_t st7789_init(void);

void st7789_fill(uint16_t color);
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);

void st7789_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale);
void st7789_draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
void st7789_draw_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale);
```

Colors are RGB565 `uint16_t`. Predefined constants in `st7789.h`:
`ST7789_BLACK`, `ST7789_WHITE`, `ST7789_RED`, `ST7789_GREEN`, `ST7789_BLUE`,
`ST7789_YELLOW`, `ST7789_CYAN`, `ST7789_MAGENTA`, `ST7789_ORANGE`, `ST7789_GRAY`.

Text `scale` controls pixel size: `1` = 8×8 px per char, `2` = 16×16, `3` = 24×24.

---

## MicroPython Quick Start

### 1. Flash MicroPython firmware

Download the ESP32-S3 build from [micropython.org/download/ESP32_GENERIC_S3](https://micropython.org/download/ESP32_GENERIC_S3/) (v1.28 or newer).

```bash
# Erase flash and write firmware (replace PORT)
esptool.py --chip esp32s3 --port PORT erase_flash
esptool.py --chip esp32s3 --port PORT write_flash -z 0 ESP32_GENERIC_S3-*.bin
```

### 2. Copy files and run

```bash
cd examples/micropython/display_button_demo

mpremote connect PORT \
  cp st7789.py :st7789.py + \
  cp ui.py :ui.py + \
  cp demo.py :demo.py + \
  run demo.py
```

### Key files

| File | Purpose |
|------|---------|
| `st7789.py` | `ST7789` class — SPI init, `fill`, `fill_rect`, `pixel`, `hline`, `vline`, `blit_buffer` |
| `ui.py` | 5×7 bitmap font, `draw_text()`, `draw_char()` |
| `demo.py` | 3-phase demo — entry point |

### Display setup (MicroPython)

```python
from machine import Pin, SPI
import st7789

spi = SPI(2, baudrate=10_000_000, polarity=1, phase=1,
          sck=Pin(3), mosi=Pin(4))

display = st7789.ST7789(
    spi, 240, 240,
    reset=Pin(7, Pin.OUT),
    dc=Pin(8, Pin.OUT),
    backlight=Pin(9, Pin.OUT),
    rotation=0,
)
display.init()
display.fill(0x0000)  # clear to black
```

### Button reading (MicroPython)

```python
from machine import Pin

buttons = [Pin(p, Pin.IN, Pin.PULL_UP) for p in [13, 12, 11, 10]]
# buttons[0] = K1 (UP), buttons[1] = K2 (DOWN),
# buttons[2] = K3 (SEL), buttons[3] = K4 (BACK)

if buttons[0].value() == 0:   # K1 pressed (active-low)
    ...
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Screen stays black | BLK not driven HIGH | Check GPIO 9 → BLK wire; verify backlight code runs |
| Inverted colors | Color inversion off | Try `esp_lcd_panel_invert_color(panel, true)` in C++ |
| Garbled / shifted pixels | Wrong SPI mode | Must use **mode 3** — `polarity=1, phase=1` |
| Display ignores writes | CS not low | Module has CS grounded on PCB; set `cs_gpio_num = -1` in C++ |
| Buttons always pressed | Pull-up missing | Enable `GPIO_PULLUP_ENABLE` (C++) or `Pin.PULL_UP` (MicroPython) |
| White noise on screen | Incomplete init | Ensure hardware RST pulse + SW reset + SLPOUT in full sequence |
