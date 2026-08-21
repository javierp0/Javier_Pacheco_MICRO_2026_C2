#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t buffer[OLED_BUFFER_SIZE];
    bool ready;
} oled_t;

esp_err_t oled_init(oled_t *oled, i2c_master_bus_handle_t bus);
esp_err_t oled_update(oled_t *oled);
void oled_clear(oled_t *oled);
void oled_draw_pixel(oled_t *oled, int x, int y, bool on);
void oled_draw_line(oled_t *oled, int x0, int y0, int x1, int y1, bool on);
void oled_draw_rect(oled_t *oled, int x, int y, int w, int h, bool on);
void oled_fill_rect(oled_t *oled, int x, int y, int w, int h, bool on);
void oled_draw_text(oled_t *oled, int x, int y, const char *text, bool on);
