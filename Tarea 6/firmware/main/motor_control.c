#include "motor_control.h"

#include "board_config.h"
#include "driver/gpio.h"

static motor_cmd_t current_command = MOTOR_CMD_STOP;

static void set_relay(gpio_num_t pin, bool on)
{
    gpio_set_level(pin, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
}

esp_err_t motor_control_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_RELAY_OPEN) | (1ULL << PIN_RELAY_CLOSE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    motor_control_apply(MOTOR_CMD_STOP);
    return ESP_OK;
}

void motor_control_apply(motor_cmd_t command)
{
    if (command == MOTOR_CMD_OPEN) {
        set_relay(PIN_RELAY_CLOSE, false);
        set_relay(PIN_RELAY_OPEN, true);
    } else if (command == MOTOR_CMD_CLOSE) {
        set_relay(PIN_RELAY_OPEN, false);
        set_relay(PIN_RELAY_CLOSE, true);
    } else {
        set_relay(PIN_RELAY_OPEN, false);
        set_relay(PIN_RELAY_CLOSE, false);
        command = MOTOR_CMD_STOP;
    }

    current_command = command;
}

void motor_control_apply_state(gate_state_t state)
{
    if (state == GATE_STATE_OPENING) {
        motor_control_apply(MOTOR_CMD_OPEN);
    } else if (state == GATE_STATE_CLOSING) {
        motor_control_apply(MOTOR_CMD_CLOSE);
    } else {
        motor_control_apply(MOTOR_CMD_STOP);
    }
}

motor_cmd_t motor_control_current(void)
{
    return current_command;
}
