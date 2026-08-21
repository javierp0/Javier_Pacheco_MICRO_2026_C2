#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    BUTTON_JOY0 = 0,
    BUTTON_JOY1,
    BUTTON_BTN0,
    BUTTON_BTN1,
    BUTTON_BTN2,
    BUTTON_BTN3,
    BUTTON_BTN4,
    BUTTON_BTNL1,
    BUTTON_BTNL2,
    BUTTON_BTNL3,
    BUTTON_BTNL4,
    BUTTON_COUNT
} button_id_t;

typedef struct {
    button_id_t id;
    bool pressed;
} button_event_t;

typedef struct {
    bool pressed[BUTTON_COUNT];
} button_snapshot_t;

typedef struct {
    uint8_t debounced_level[BUTTON_COUNT];
    uint8_t candidate_level[BUTTON_COUNT];
    uint8_t stable_count[BUTTON_COUNT];
} buttons_t;

esp_err_t buttons_init(buttons_t *buttons);
int buttons_update(buttons_t *buttons, button_snapshot_t *snapshot, button_event_t *events, int max_events);
const char *button_name(button_id_t id);
uint16_t buttons_front_mask(const button_snapshot_t *snapshot);
uint16_t buttons_lateral_mask(const button_snapshot_t *snapshot);
uint8_t buttons_joystick_mask(const button_snapshot_t *snapshot);
