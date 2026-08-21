#include "input_manager.h"

#include <string.h>

#include "board_config.h"
#include "driver/gpio.h"

#define BUTTON_COUNT 3
#define BUTTON_STABLE_SAMPLES 3

typedef enum {
    BUTTON_OPEN_IDX = 0,
    BUTTON_CLOSE_IDX,
    BUTTON_STOP_IDX
} button_idx_t;

static const gpio_num_t button_pins[BUTTON_COUNT] = {
    PIN_BUTTON_OPEN,
    PIN_BUTTON_CLOSE,
    PIN_BUTTON_STOP,
};

static bool active_low_pressed(gpio_num_t pin)
{
    return gpio_get_level(pin) == INPUT_ACTIVE_LOW;
}

static esp_err_t configure_input(gpio_num_t pin, bool enable_pullup)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = enable_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&cfg);
}

esp_err_t input_manager_init(input_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(manager, 0, sizeof(*manager));

    esp_err_t err = configure_input(PIN_LIMIT_OPEN, false);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_LIMIT_CLOSED, false);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_FTC, false);
    if (err != ESP_OK) {
        return err;
    }

    err = configure_input(PIN_BUTTON_OPEN, true);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_BUTTON_CLOSE, true);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_BUTTON_STOP, true);
    if (err != ESP_OK) {
        return err;
    }

    err = configure_input(PIN_DIP1, true);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_DIP2, true);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_DIP3, true);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_input(PIN_DIP4, true);
    if (err != ESP_OK) {
        return err;
    }

    for (int i = 0; i < BUTTON_COUNT; i++) {
        manager->button_level[i] = (uint8_t)gpio_get_level(button_pins[i]);
        manager->candidate_level[i] = manager->button_level[i];
    }

    return ESP_OK;
}

void input_manager_update(input_manager_t *manager, input_snapshot_t *snapshot, input_events_t *events)
{
    if (manager == NULL || snapshot == NULL || events == NULL) {
        return;
    }

    memset(events, 0, sizeof(*events));

    snapshot->limit_open_active = active_low_pressed(PIN_LIMIT_OPEN);
    snapshot->limit_closed_active = active_low_pressed(PIN_LIMIT_CLOSED);
    snapshot->ftc_blocked = active_low_pressed(PIN_FTC);
    snapshot->dip_encoder_enabled = active_low_pressed(PIN_DIP1);
    snapshot->dip_ftc_reverse = active_low_pressed(PIN_DIP2);
    snapshot->dip_auto_close = active_low_pressed(PIN_DIP3);
    snapshot->dip_maintenance = active_low_pressed(PIN_DIP4);

    bool pressed_now[BUTTON_COUNT] = {0};

    for (int i = 0; i < BUTTON_COUNT; i++) {
        uint8_t raw_level = (uint8_t)gpio_get_level(button_pins[i]);

        if (raw_level == manager->button_level[i]) {
            manager->candidate_level[i] = raw_level;
            manager->stable_count[i] = 0;
        } else {
            if (raw_level == manager->candidate_level[i]) {
                if (manager->stable_count[i] < BUTTON_STABLE_SAMPLES) {
                    manager->stable_count[i]++;
                }
            } else {
                manager->candidate_level[i] = raw_level;
                manager->stable_count[i] = 1;
            }

            if (manager->stable_count[i] >= BUTTON_STABLE_SAMPLES) {
                uint8_t previous_level = manager->button_level[i];
                manager->button_level[i] = raw_level;
                manager->stable_count[i] = 0;

                bool was_pressed = previous_level == INPUT_ACTIVE_LOW;
                bool is_pressed = raw_level == INPUT_ACTIVE_LOW;
                if (!was_pressed && is_pressed) {
                    if (i == BUTTON_OPEN_IDX) {
                        events->open_pressed_edge = true;
                    } else if (i == BUTTON_CLOSE_IDX) {
                        events->close_pressed_edge = true;
                    } else if (i == BUTTON_STOP_IDX) {
                        events->stop_pressed_edge = true;
                    }
                }
            }
        }

        pressed_now[i] = manager->button_level[i] == INPUT_ACTIVE_LOW;
    }

    snapshot->button_open_pressed = pressed_now[BUTTON_OPEN_IDX];
    snapshot->button_close_pressed = pressed_now[BUTTON_CLOSE_IDX];
    snapshot->button_stop_pressed = pressed_now[BUTTON_STOP_IDX];
}

uint8_t input_dip_mask(const input_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    uint8_t mask = 0;
    mask |= snapshot->dip_encoder_enabled ? (1U << 0) : 0;
    mask |= snapshot->dip_ftc_reverse ? (1U << 1) : 0;
    mask |= snapshot->dip_auto_close ? (1U << 2) : 0;
    mask |= snapshot->dip_maintenance ? (1U << 3) : 0;
    return mask;
}

uint8_t input_sensor_mask(const input_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    uint8_t mask = 0;
    mask |= snapshot->limit_open_active ? (1U << 0) : 0;
    mask |= snapshot->limit_closed_active ? (1U << 1) : 0;
    mask |= snapshot->ftc_blocked ? (1U << 2) : 0;
    return mask;
}
