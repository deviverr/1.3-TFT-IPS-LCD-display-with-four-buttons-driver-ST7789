idf_component_register(
    SRCS "main.c" "st7789.c" "font8x8.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd freertos esp_timer
)
