#include "joystick.h"

#include <stdlib.h>

#include "board_config.h"

#define JOYSTICK_DEADZONE_PCT 5

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static esp_err_t configure_axis(joystick_t *joystick, adc_channel_t channel)
{
    adc_oneshot_chan_cfg_t cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    return adc_oneshot_config_channel(joystick->adc1, channel, &cfg);
}

void joystick_calibration_defaults(joystick_calibration_t *cal)
{
    if (cal == NULL) {
        return;
    }

    cal->joy0_mt_center = BOARD_ADC_DEFAULT_CENTER;
    cal->joy0_md_center = BOARD_ADC_DEFAULT_CENTER;
    cal->joy1_mt_center = BOARD_ADC_DEFAULT_CENTER;
    cal->joy1_md_center = BOARD_ADC_DEFAULT_CENTER;
}

esp_err_t joystick_init(joystick_t *joystick)
{
    if (joystick == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &joystick->adc1);
    if (err != ESP_OK) {
        return err;
    }

    err = configure_axis(joystick, BOARD_JOY0_MT_ADC_CHANNEL);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_axis(joystick, BOARD_JOY0_MD_ADC_CHANNEL);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_axis(joystick, BOARD_JOY1_MT_ADC_CHANNEL);
    if (err != ESP_OK) {
        return err;
    }
    return configure_axis(joystick, BOARD_JOY1_MD_ADC_CHANNEL);
}

int joystick_axis_to_percent(int raw, int center, bool apply_deadzone)
{
    raw = clamp_int(raw, BOARD_ADC_RAW_MIN, BOARD_ADC_RAW_MAX);
    center = clamp_int(center, BOARD_ADC_RAW_MIN + 1, BOARD_ADC_RAW_MAX - 1);

    int pct = 0;
    if (raw >= center) {
        pct = ((raw - center) * 100) / (BOARD_ADC_RAW_MAX - center);
    } else {
        pct = -((center - raw) * 100) / (center - BOARD_ADC_RAW_MIN);
    }

    pct = clamp_int(pct, -100, 100);

    if (apply_deadzone) {
        int abs_pct = abs(pct);
        if (abs_pct <= JOYSTICK_DEADZONE_PCT) {
            return 0;
        }

        int rescaled = ((abs_pct - JOYSTICK_DEADZONE_PCT) * 100) / (100 - JOYSTICK_DEADZONE_PCT);
        pct = pct < 0 ? -rescaled : rescaled;
    }

    return clamp_int(pct, -100, 100);
}

esp_err_t joystick_read(joystick_t *joystick, const joystick_calibration_t *cal, joystick_reading_t *reading)
{
    if (joystick == NULL || cal == NULL || reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = adc_oneshot_read(joystick->adc1, BOARD_JOY0_MT_ADC_CHANNEL, &reading->joy0.mt_raw);
    if (err != ESP_OK) {
        return err;
    }
    err = adc_oneshot_read(joystick->adc1, BOARD_JOY0_MD_ADC_CHANNEL, &reading->joy0.md_raw);
    if (err != ESP_OK) {
        return err;
    }
    err = adc_oneshot_read(joystick->adc1, BOARD_JOY1_MT_ADC_CHANNEL, &reading->joy1.mt_raw);
    if (err != ESP_OK) {
        return err;
    }
    err = adc_oneshot_read(joystick->adc1, BOARD_JOY1_MD_ADC_CHANNEL, &reading->joy1.md_raw);
    if (err != ESP_OK) {
        return err;
    }

    reading->joy0.mt_pct = joystick_axis_to_percent(reading->joy0.mt_raw, cal->joy0_mt_center, true);
    reading->joy0.md_pct = joystick_axis_to_percent(reading->joy0.md_raw, cal->joy0_md_center, true);
    reading->joy1.mt_pct = joystick_axis_to_percent(reading->joy1.mt_raw, cal->joy1_mt_center, false);
    reading->joy1.md_pct = joystick_axis_to_percent(reading->joy1.md_raw, cal->joy1_md_center, false);

    return ESP_OK;
}
