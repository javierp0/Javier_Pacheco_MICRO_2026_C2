#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    GATE_STATE_STARTING = 0,
    GATE_STATE_STOPPED,
    GATE_STATE_OPENING,
    GATE_STATE_CLOSING,
    GATE_STATE_OPEN,
    GATE_STATE_CLOSED,
    GATE_STATE_WAITING_AUTO_CLOSE,
    GATE_STATE_PAUSED_BY_FTC,
    GATE_STATE_ERROR,
    GATE_STATE_CALIBRATING
} gate_state_t;

typedef enum {
    GATE_ERROR_NONE = 0,
    GATE_ERROR_BOTH_LIMITS,
    GATE_ERROR_TRAVEL_TIMEOUT,
    GATE_ERROR_SENSOR_FAULT,
    GATE_ERROR_MAINTENANCE_LOCK
} gate_error_t;

typedef enum {
    GATE_EVENT_NONE = 0,
    GATE_EVENT_OPEN_BUTTON,
    GATE_EVENT_CLOSE_BUTTON,
    GATE_EVENT_STOP_BUTTON,
    GATE_EVENT_MQTT_OPEN,
    GATE_EVENT_MQTT_CLOSE,
    GATE_EVENT_MQTT_STOP,
    GATE_EVENT_LIMIT_OPEN,
    GATE_EVENT_LIMIT_CLOSED,
    GATE_EVENT_FTC_BLOCKED,
    GATE_EVENT_TRAVEL_TIMEOUT,
    GATE_EVENT_RESET_ERROR,
    GATE_EVENT_START_CALIBRATION,
    GATE_EVENT_FINISH_CALIBRATION,
    GATE_EVENT_AUTO_CLOSE
} gate_event_t;

typedef struct {
    uint16_t auto_close_s;
    uint16_t max_travel_s;
    uint16_t ftc_wait_s;
    uint16_t reverse_pause_s;
    bool auto_close_sw;
    bool maintenance_sw;
} gate_config_t;

typedef struct {
    bool limit_open_active;
    bool limit_closed_active;
    bool ftc_blocked;
    bool button_open_pressed;
    bool button_close_pressed;
    bool button_stop_pressed;
    bool dip_encoder_enabled;
    bool dip_ftc_reverse;
    bool dip_auto_close;
    bool dip_maintenance;
} input_snapshot_t;

typedef struct {
    gate_state_t state;
    gate_error_t error;
    gate_config_t config;
    uint32_t state_enter_ms;
    uint32_t movement_start_ms;
    uint32_t last_position_ms;
    int position_pct;
    bool pending_ftc_reverse;
    bool last_event_accepted;
} gate_fsm_t;

void gate_config_defaults(gate_config_t *config);
void gate_fsm_init(gate_fsm_t *fsm, const gate_config_t *config, uint32_t now_ms);
bool gate_fsm_dispatch(gate_fsm_t *fsm, gate_event_t event, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len);
bool gate_fsm_tick(gate_fsm_t *fsm, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len);
void gate_fsm_update_config(gate_fsm_t *fsm, const gate_config_t *config);
const char *gate_state_name(gate_state_t state);
const char *gate_error_name(gate_error_t error);
const char *gate_event_name(gate_event_t event);
bool gate_state_is_moving(gate_state_t state);
bool gate_state_relays_off(gate_state_t state);
