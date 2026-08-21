#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

typedef struct {
    int mt_raw;
    int md_raw;
    int mt_pct;
    int md_pct;
} joystick_pair_t;

typedef struct {
    joystick_pair_t joy0;
    joystick_pair_t joy1;
} joystick_reading_t;

typedef struct {
    int joy0_mt_center;
    int joy0_md_center;
    int joy1_mt_center;
    int joy1_md_center;
} joystick_calibration_t;

typedef struct {
    adc_oneshot_unit_handle_t adc1;
} joystick_t;

void joystick_calibration_defaults(joystick_calibration_t *cal);
esp_err_t joystick_init(joystick_t *joystick);
esp_err_t joystick_read(joystick_t *joystick, const joystick_calibration_t *cal, joystick_reading_t *reading);
int joystick_axis_to_percent(int raw, int center, bool apply_deadzone);
