#pragma once

#include "esp_err.h"
#include "gate_fsm.h"

typedef enum {
    MOTOR_CMD_STOP = 0,
    MOTOR_CMD_OPEN,
    MOTOR_CMD_CLOSE
} motor_cmd_t;

esp_err_t motor_control_init(void);
void motor_control_apply(motor_cmd_t command);
void motor_control_apply_state(gate_state_t state);
motor_cmd_t motor_control_current(void);
