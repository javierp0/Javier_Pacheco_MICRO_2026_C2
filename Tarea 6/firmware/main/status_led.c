#include "status_led.h"

#include <stdbool.h>

#include "board_config.h"
#include "driver/gpio.h"

static void set_led(bool on)
{
    gpio_set_level(PIN_STATUS_LED, on ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL);
}

esp_err_t status_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_STATUS_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    set_led(false);
    return ESP_OK;
}

void status_led_apply_state(gate_state_t state)
{
    bool on = state == GATE_STATE_OPENING ||
              state == GATE_STATE_CLOSING ||
              state == GATE_STATE_STOPPED;
    set_led(on);
}
