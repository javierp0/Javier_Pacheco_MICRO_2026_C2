#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CONTROL_MODE_IMU = 0,
    CONTROL_MODE_JOY0
} control_mode_t;

typedef enum {
    SCREEN_VIEW_MAIN = 0,
    SCREEN_VIEW_DIAGNOSTIC
} screen_view_t;

typedef struct {
    control_mode_t mode;
    int8_t direction_pct;
    uint8_t throttle_pct;
    uint8_t brake_pct;
    uint16_t front_buttons_mask;
    uint16_t lateral_buttons_mask;
    uint8_t joystick_buttons_mask;
    int16_t joy0_mt_pct;
    int16_t joy0_md_pct;
    int16_t joy1_mt_pct;
    int16_t joy1_md_pct;
    uint16_t joy0_mt_raw;
    uint16_t joy0_md_raw;
    uint16_t joy1_mt_raw;
    uint16_t joy1_md_raw;
    bool calibrated;
} rc433_control_packet_t;
