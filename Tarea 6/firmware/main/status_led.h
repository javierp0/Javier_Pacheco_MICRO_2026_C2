#pragma once

#include "esp_err.h"
#include "gate_fsm.h"

esp_err_t status_led_init(void);
void status_led_apply_state(gate_state_t state);
