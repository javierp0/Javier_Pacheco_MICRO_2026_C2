#include "buzzer.h"

#include <stdbool.h>

#include "board_config.h"
#include "driver/gpio.h"

#define BUZZER_ERROR_MS 15000U

static bool buzzer_active;
static uint32_t buzzer_off_at_ms;

static void set_buzzer(bool on)
{
    gpio_set_level(PIN_BUZZER, on ? BUZZER_ACTIVE_LEVEL : !BUZZER_ACTIVE_LEVEL);
    buzzer_active = on;
}

esp_err_t buzzer_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    buzzer_off();
    return ESP_OK;
}

void buzzer_enter_error(uint32_t now_ms)
{
    buzzer_off_at_ms = now_ms + BUZZER_ERROR_MS;
    set_buzzer(true);
}

void buzzer_update(uint32_t now_ms)
{
    if (buzzer_active && (int32_t)(now_ms - buzzer_off_at_ms) >= 0) {
        buzzer_off();
    }
}

void buzzer_off(void)
{
    set_buzzer(false);
    buzzer_off_at_ms = 0;
}
