#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "gate_fsm.h"

typedef struct {
    bool open_pressed_edge;
    bool close_pressed_edge;
    bool stop_pressed_edge;
} input_events_t;

typedef struct {
    uint8_t button_level[3];
    uint8_t candidate_level[3];
    uint8_t stable_count[3];
} input_manager_t;

esp_err_t input_manager_init(input_manager_t *manager);
void input_manager_update(input_manager_t *manager, input_snapshot_t *snapshot, input_events_t *events);
uint8_t input_dip_mask(const input_snapshot_t *snapshot);
uint8_t input_sensor_mask(const input_snapshot_t *snapshot);
