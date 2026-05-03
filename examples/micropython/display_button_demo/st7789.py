from micropython import const
import time

ST7789_SWRESET = const(0x01)
ST7789_SLPOUT  = const(0x11)
ST7789_COLMOD  = const(0x3A)
ST7789_MADCTL  = const(0x36)
ST7789_CASET   = const(0x2A)
ST7789_RASET   = const(0x2B)
ST7789_RAMWR   = const(0x2C)
ST7789_DISPON  = const(0x29)

BLACK = 0x0000
RED   = 0xF800


class ST7789:
    def __init__(self, spi, width, height, reset, dc, backlight=None, rotation=0):
        self.spi = spi
        self.width = width
        self.height = height
        self.reset = reset
        self.dc = dc
        self.backlight = backlight
        self.rotation = rotation

    def write_cmd(self, cmd):
        self.dc(0)
        self.spi.write(bytearray([cmd]))

    def write_data(self, data):
        self.dc(1)
        self.spi.write(data)

    def init(self):
        self.reset(1)
        time.sleep_ms(50)
        self.reset(0)
        time.sleep_ms(50)
        self.reset(1)
        time.sleep_ms(150)

        self.write_cmd(ST7789_SWRESET)
        time.sleep_ms(150)

        self.write_cmd(ST7789_SLPOUT)
        time.sleep_ms(150)

        self.write_cmd(ST7789_COLMOD)
        self.write_data(bytearray([0x55]))

        self.set_rotation(self.rotation)

        self.write_cmd(ST7789_DISPON)
        time.sleep_ms(100)

        if self.backlight:
            self.backlight(1)

    def set_rotation(self, rotation):
        rotation = rotation % 360
        self.rotation = rotation

        if rotation == 0:
            madctl = 0x00
        elif rotation == 90:
            madctl = 0x60
        elif rotation == 180:
            madctl = 0xC0
        elif rotation == 270:
            madctl = 0xA0
        else:
            madctl = 0x00

        self.write_cmd(ST7789_MADCTL)
        self.write_data(bytearray([madctl]))

    def fill(self, color):
        self.set_window(0, 0, self.width, self.height)

        hi = color >> 8
        lo = color & 0xFF

        chunk = bytearray([hi, lo] * 128)

        pixels = self.width * self.height

        while pixels > 0:
            n = min(128, pixels)
            self.write_data(chunk[:n * 2])
            pixels -= n

    def pixel(self, x, y, color):
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        self.set_window(x, y, 1, 1)
        self.write_data(bytearray([color >> 8, color & 0xFF]))

    def fill_rect(self, x, y, w, h, color):
        if w <= 0 or h <= 0:
            return

        if x < 0:
            w += x
            x = 0
        if y < 0:
            h += y
            y = 0
        if x + w > self.width:
            w = self.width - x
        if y + h > self.height:
            h = self.height - y

        if w <= 0 or h <= 0:
            return

        self.set_window(x, y, w, h)

        hi = color >> 8
        lo = color & 0xFF
        chunk_pixels = 128
        chunk = bytearray([hi, lo] * chunk_pixels)
        pixels = w * h

        while pixels > 0:
            n = chunk_pixels if pixels >= chunk_pixels else pixels
            self.write_data(chunk[:n * 2])
            pixels -= n

    def hline(self, x, y, w, color):
        self.fill_rect(x, y, w, 1, color)

    def vline(self, x, y, h, color):
        self.fill_rect(x, y, 1, h, color)

    def rect(self, x, y, w, h, color):
        if w <= 0 or h <= 0:
            return
        self.hline(x, y, w, color)
        self.hline(x, y + h - 1, w, color)
        self.vline(x, y, h, color)
        self.vline(x + w - 1, y, h, color)

    def set_window(self, x, y, w, h):
        self.write_cmd(ST7789_CASET)
        self.write_data(bytearray([0, x, 0, x + w - 1]))

        self.write_cmd(ST7789_RASET)
        self.write_data(bytearray([0, y, 0, y + h - 1]))

        self.write_cmd(ST7789_RAMWR)

    def blit_buffer(self, buffer, x, y, w, h):
        self.set_window(x, y, w, h)
        self.write_data(buffer)
