#include "oled.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "board_config.h"

#define OLED_CONTROL_CMD 0x00
#define OLED_CONTROL_DATA 0x40
#define OLED_TIMEOUT_MS 100

typedef struct {
    char ch;
    uint8_t rows[7];
} glyph_t;

static const glyph_t glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'/', {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}},
    {':', {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
};

static esp_err_t oled_write(oled_t *oled, const uint8_t *data, size_t len)
{
    if (oled == NULL || oled->dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_transmit(oled->dev, data, len, OLED_TIMEOUT_MS);
}

static esp_err_t oled_cmd(oled_t *oled, uint8_t cmd)
{
    uint8_t data[2] = {OLED_CONTROL_CMD, cmd};
    return oled_write(oled, data, sizeof(data));
}

static esp_err_t oled_cmd_list(oled_t *oled, const uint8_t *cmds, size_t len)
{
    uint8_t data[32];

    if (len + 1 > sizeof(data)) {
        return ESP_ERR_INVALID_SIZE;
    }

    data[0] = OLED_CONTROL_CMD;
    memcpy(&data[1], cmds, len);
    return oled_write(oled, data, len + 1);
}

static const glyph_t *find_glyph(char ch)
{
    ch = (char)toupper((unsigned char)ch);

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].ch == ch) {
            return &glyphs[i];
        }
    }

    return &glyphs[0];
}

esp_err_t oled_init(oled_t *oled, i2c_master_bus_handle_t bus)
{
    if (oled == NULL || bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(oled, 0, sizeof(*oled));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_OLED_ADDR,
        .scl_speed_hz = BOARD_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &oled->dev);
    if (err != ESP_OK) {
        return err;
    }

    static const uint8_t init_cmds[] = {
        0xAE,
        0x20, 0x00,
        0xB0,
        0xC8,
        0x00,
        0x10,
        0x40,
        0x81, 0x7F,
        0xA1,
        0xA6,
        0xA8, 0x3F,
        0xA4,
        0xD3, 0x00,
        0xD5, 0x80,
        0xD9, 0xF1,
        0xDA, 0x12,
        0xDB, 0x40,
        0x8D, 0x14,
        0xAF,
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        err = oled_cmd(oled, init_cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }

    oled->ready = true;
    oled_clear(oled);
    return oled_update(oled);
}

esp_err_t oled_update(oled_t *oled)
{
    if (oled == NULL || !oled->ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t page_data[OLED_WIDTH + 1];
    page_data[0] = OLED_CONTROL_DATA;

    for (uint8_t page = 0; page < 8; page++) {
        uint8_t cmds[] = {
            (uint8_t)(0xB0 + page),
            0x00,
            0x10,
        };

        esp_err_t err = oled_cmd_list(oled, cmds, sizeof(cmds));
        if (err != ESP_OK) {
            return err;
        }

        memcpy(&page_data[1], &oled->buffer[page * OLED_WIDTH], OLED_WIDTH);
        err = oled_write(oled, page_data, sizeof(page_data));
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

void oled_clear(oled_t *oled)
{
    if (oled == NULL) {
        return;
    }

    memset(oled->buffer, 0, sizeof(oled->buffer));
}

void oled_draw_pixel(oled_t *oled, int x, int y, bool on)
{
    if (oled == NULL || x < 0 || y < 0 || x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    uint16_t index = (uint16_t)x + (uint16_t)(y / 8) * OLED_WIDTH;
    uint8_t bit = (uint8_t)(1U << (y & 7));

    if (on) {
        oled->buffer[index] |= bit;
    } else {
        oled->buffer[index] &= (uint8_t)~bit;
    }
}

void oled_draw_line(oled_t *oled, int x0, int y0, int x1, int y1, bool on)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        oled_draw_pixel(oled, x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void oled_draw_rect(oled_t *oled, int x, int y, int w, int h, bool on)
{
    if (w <= 0 || h <= 0) {
        return;
    }

    oled_draw_line(oled, x, y, x + w - 1, y, on);
    oled_draw_line(oled, x, y + h - 1, x + w - 1, y + h - 1, on);
    oled_draw_line(oled, x, y, x, y + h - 1, on);
    oled_draw_line(oled, x + w - 1, y, x + w - 1, y + h - 1, on);
}

void oled_fill_rect(oled_t *oled, int x, int y, int w, int h, bool on)
{
    if (w <= 0 || h <= 0) {
        return;
    }

    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            oled_draw_pixel(oled, xx, yy, on);
        }
    }
}

void oled_draw_text(oled_t *oled, int x, int y, const char *text, bool on)
{
    if (oled == NULL || text == NULL) {
        return;
    }

    int cursor_x = x;

    while (*text != '\0') {
        const glyph_t *glyph = find_glyph(*text);

        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                bool pixel_on = (glyph->rows[row] & (1U << (4 - col))) != 0;
                if (pixel_on) {
                    oled_draw_pixel(oled, cursor_x + col, y + row, on);
                }
            }
        }

        cursor_x += 6;
        text++;
    }
}
