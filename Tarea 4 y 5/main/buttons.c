#include "buttons.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"
#include "driver/gpio.h"

#define BUTTON_STABLE_SAMPLES 3

typedef struct {
    button_id_t id;
    gpio_num_t gpio;
    const char *name;
} button_pin_t;

static const button_pin_t button_pins[BUTTON_COUNT] = {
    {BUTTON_JOY0, BOARD_JOY0_BTN_GPIO, "JOY0_BTN"},
    {BUTTON_JOY1, BOARD_JOY1_BTN_GPIO, "JOY1_BTN"},
    {BUTTON_BTN0, BOARD_BTN0_GPIO, "BTN0"},
    {BUTTON_BTN1, BOARD_BTN1_GPIO, "BTN1"},
    {BUTTON_BTN2, BOARD_BTN2_GPIO, "BTN2"},
    {BUTTON_BTN3, BOARD_BTN3_GPIO, "BTN3"},
    {BUTTON_BTN4, BOARD_BTN4_GPIO, "BTN4"},
    {BUTTON_BTNL1, BOARD_BTNL1_GPIO, "BTNL1"},
    {BUTTON_BTNL2, BOARD_BTNL2_GPIO, "BTNL2"},
    {BUTTON_BTNL3, BOARD_BTNL3_GPIO, "BTNL3"},
    {BUTTON_BTNL4, BOARD_BTNL4_GPIO, "BTNL4"},
};

static uint8_t read_button_level(button_id_t id)
{
    return (uint8_t)gpio_get_level(button_pins[id].gpio);
}

static bool level_is_pressed(uint8_t level)
{
    return level == BOARD_BUTTON_ACTIVE_LEVEL;
}

esp_err_t buttons_init(buttons_t *buttons)
{
    if (buttons == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(buttons, 0, sizeof(*buttons));

    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << button_pins[i].gpio,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t level = read_button_level((button_id_t)i);
        buttons->debounced_level[i] = level;
        buttons->candidate_level[i] = level;
        buttons->stable_count[i] = 0;
    }

    return ESP_OK;
}

int buttons_update(buttons_t *buttons, button_snapshot_t *snapshot, button_event_t *events, int max_events)
{
    if (buttons == NULL || snapshot == NULL || events == NULL || max_events <= 0) {
        return 0;
    }

    int event_count = 0;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        uint8_t raw_level = read_button_level((button_id_t)i);

        if (raw_level == buttons->debounced_level[i]) {
            buttons->candidate_level[i] = raw_level;
            buttons->stable_count[i] = 0;
        } else {
            if (raw_level == buttons->candidate_level[i]) {
                if (buttons->stable_count[i] < BUTTON_STABLE_SAMPLES) {
                    buttons->stable_count[i]++;
                }
            } else {
                buttons->candidate_level[i] = raw_level;
                buttons->stable_count[i] = 1;
            }

            if (buttons->stable_count[i] >= BUTTON_STABLE_SAMPLES) {
                buttons->debounced_level[i] = raw_level;
                buttons->stable_count[i] = 0;

                if (event_count < max_events) {
                    events[event_count].id = (button_id_t)i;
                    events[event_count].pressed = level_is_pressed(raw_level);
                    event_count++;
                }
            }
        }

        snapshot->pressed[i] = level_is_pressed(buttons->debounced_level[i]);
    }

    return event_count;
}

const char *button_name(button_id_t id)
{
    if (id < 0 || id >= BUTTON_COUNT) {
        return "UNKNOWN";
    }

    return button_pins[id].name;
}

uint16_t buttons_front_mask(const button_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    uint16_t mask = 0;
    mask |= snapshot->pressed[BUTTON_BTN0] ? (1U << 0) : 0;
    mask |= snapshot->pressed[BUTTON_BTN1] ? (1U << 1) : 0;
    mask |= snapshot->pressed[BUTTON_BTN2] ? (1U << 2) : 0;
    mask |= snapshot->pressed[BUTTON_BTN3] ? (1U << 3) : 0;
    mask |= snapshot->pressed[BUTTON_BTN4] ? (1U << 4) : 0;
    return mask;
}

uint16_t buttons_lateral_mask(const button_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    uint16_t mask = 0;
    mask |= snapshot->pressed[BUTTON_BTNL1] ? (1U << 0) : 0;
    mask |= snapshot->pressed[BUTTON_BTNL2] ? (1U << 1) : 0;
    mask |= snapshot->pressed[BUTTON_BTNL3] ? (1U << 2) : 0;
    mask |= snapshot->pressed[BUTTON_BTNL4] ? (1U << 3) : 0;
    return mask;
}

uint8_t buttons_joystick_mask(const button_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    uint8_t mask = 0;
    mask |= snapshot->pressed[BUTTON_JOY0] ? (1U << 0) : 0;
    mask |= snapshot->pressed[BUTTON_JOY1] ? (1U << 1) : 0;
    return mask;
}
