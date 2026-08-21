#include "encoder.h"

#include "board_config.h"
#include "driver/gpio.h"

static bool encoder_enabled;

esp_err_t encoder_init(bool enabled)
{
    encoder_enabled = enabled;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_ENCODER_A) | (1ULL << PIN_ENCODER_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&cfg);
}

int encoder_get_position_ticks(void)
{
    if (!encoder_enabled) {
        return 0;
    }

    return 0;
}
