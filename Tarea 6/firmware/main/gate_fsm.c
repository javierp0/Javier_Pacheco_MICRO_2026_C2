#include "gate_fsm.h"

#include <stdio.h>
#include <string.h>

#include "board_config.h"

static uint32_t seconds_to_ms(uint16_t seconds)
{
    return (uint32_t)seconds * 1000U;
}

static void set_msg(char *msg, size_t msg_len, const char *text)
{
    if (msg != NULL && msg_len > 0) {
        snprintf(msg, msg_len, "%s", text);
    }
}

static bool effective_auto_close(const gate_fsm_t *fsm, const input_snapshot_t *input)
{
    return fsm->config.auto_close_sw &&
           input->dip_auto_close &&
           !input->dip_maintenance &&
           !fsm->config.maintenance_sw;
}

static bool movement_locked(const gate_fsm_t *fsm, const input_snapshot_t *input)
{
    return input->dip_maintenance || fsm->config.maintenance_sw;
}

static void enter_state(gate_fsm_t *fsm, gate_state_t state, uint32_t now_ms)
{
    fsm->state = state;
    fsm->state_enter_ms = now_ms;

    if (state == GATE_STATE_OPENING || state == GATE_STATE_CLOSING) {
        fsm->movement_start_ms = now_ms;
        fsm->last_position_ms = now_ms;
    }
}

static void enter_error(gate_fsm_t *fsm, gate_error_t error, uint32_t now_ms)
{
    fsm->error = error;
    enter_state(fsm, GATE_STATE_ERROR, now_ms);
}

static void update_position(gate_fsm_t *fsm, uint32_t now_ms)
{
    if (!gate_state_is_moving(fsm->state)) {
        return;
    }

    uint32_t max_ms = seconds_to_ms(fsm->config.max_travel_s);
    if (max_ms == 0) {
        max_ms = 1;
    }

    uint32_t elapsed = now_ms - fsm->movement_start_ms;
    int delta = (int)((elapsed * 100U) / max_ms);

    if (fsm->state == GATE_STATE_OPENING) {
        fsm->position_pct += delta;
    } else if (fsm->state == GATE_STATE_CLOSING) {
        fsm->position_pct -= delta;
    }

    if (fsm->position_pct < 0) {
        fsm->position_pct = 0;
    } else if (fsm->position_pct > 100) {
        fsm->position_pct = 100;
    }

    fsm->movement_start_ms = now_ms;
}

void gate_config_defaults(gate_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->auto_close_s = DEFAULT_AUTO_CLOSE_S;
    config->max_travel_s = DEFAULT_MAX_TRAVEL_S;
    config->ftc_wait_s = DEFAULT_FTC_WAIT_S;
    config->reverse_pause_s = DEFAULT_REVERSE_PAUSE_S;
    config->auto_close_sw = DEFAULT_AUTO_CLOSE_SW;
    config->maintenance_sw = DEFAULT_MAINTENANCE_SW;
}

void gate_fsm_init(gate_fsm_t *fsm, const gate_config_t *config, uint32_t now_ms)
{
    memset(fsm, 0, sizeof(*fsm));
    if (config != NULL) {
        fsm->config = *config;
    } else {
        gate_config_defaults(&fsm->config);
    }
    fsm->state = GATE_STATE_STARTING;
    fsm->error = GATE_ERROR_NONE;
    fsm->position_pct = 0;
    fsm->state_enter_ms = now_ms;
}

static bool start_opening(gate_fsm_t *fsm, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len)
{
    if (movement_locked(fsm, input)) {
        enter_error(fsm, GATE_ERROR_MAINTENANCE_LOCK, now_ms);
        set_msg(msg, msg_len, "Movimiento bloqueado por mantenimiento");
        return false;
    }

    if (input->limit_open_active) {
        fsm->position_pct = 100;
        enter_state(fsm, GATE_STATE_OPEN, now_ms);
        set_msg(msg, msg_len, "Porton ya abierto");
        return true;
    }

    fsm->error = GATE_ERROR_NONE;
    enter_state(fsm, GATE_STATE_OPENING, now_ms);
    set_msg(msg, msg_len, "Abriendo");
    return true;
}

static bool start_closing(gate_fsm_t *fsm, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len)
{
    if (movement_locked(fsm, input)) {
        enter_error(fsm, GATE_ERROR_MAINTENANCE_LOCK, now_ms);
        set_msg(msg, msg_len, "Movimiento bloqueado por mantenimiento");
        return false;
    }

    if (input->ftc_blocked) {
        enter_state(fsm, GATE_STATE_PAUSED_BY_FTC, now_ms);
        fsm->pending_ftc_reverse = false;
        set_msg(msg, msg_len, "Cierre bloqueado por FTC");
        return false;
    }

    if (input->limit_closed_active) {
        fsm->position_pct = 0;
        enter_state(fsm, GATE_STATE_CLOSED, now_ms);
        set_msg(msg, msg_len, "Porton ya cerrado");
        return true;
    }

    fsm->error = GATE_ERROR_NONE;
    enter_state(fsm, GATE_STATE_CLOSING, now_ms);
    set_msg(msg, msg_len, "Cerrando");
    return true;
}

bool gate_fsm_dispatch(gate_fsm_t *fsm, gate_event_t event, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len)
{
    if (fsm == NULL || input == NULL) {
        return false;
    }

    update_position(fsm, now_ms);
    fsm->last_event_accepted = true;

    if (event == GATE_EVENT_STOP_BUTTON || event == GATE_EVENT_MQTT_STOP) {
        enter_state(fsm, GATE_STATE_STOPPED, now_ms);
        set_msg(msg, msg_len, "Stop: reles apagados");
        return true;
    }

    if (event == GATE_EVENT_RESET_ERROR) {
        fsm->error = GATE_ERROR_NONE;
        enter_state(fsm, GATE_STATE_STOPPED, now_ms);
        set_msg(msg, msg_len, "Error reseteado");
        return true;
    }

    if (event == GATE_EVENT_START_CALIBRATION) {
        enter_state(fsm, GATE_STATE_CALIBRATING, now_ms);
        set_msg(msg, msg_len, "Calibrando");
        return true;
    }

    if (event == GATE_EVENT_FINISH_CALIBRATION) {
        enter_state(fsm, GATE_STATE_STOPPED, now_ms);
        set_msg(msg, msg_len, "Calibracion terminada");
        return true;
    }

    if (fsm->state == GATE_STATE_ERROR && event != GATE_EVENT_RESET_ERROR) {
        fsm->last_event_accepted = false;
        set_msg(msg, msg_len, "Rechazado: sistema en ERROR");
        return false;
    }

    switch (event) {
    case GATE_EVENT_OPEN_BUTTON:
    case GATE_EVENT_MQTT_OPEN:
        return start_opening(fsm, input, now_ms, msg, msg_len);

    case GATE_EVENT_CLOSE_BUTTON:
    case GATE_EVENT_MQTT_CLOSE:
    case GATE_EVENT_AUTO_CLOSE:
        return start_closing(fsm, input, now_ms, msg, msg_len);

    default:
        fsm->last_event_accepted = false;
        set_msg(msg, msg_len, "Evento sin accion directa");
        return false;
    }
}

bool gate_fsm_tick(gate_fsm_t *fsm, const input_snapshot_t *input, uint32_t now_ms, char *msg, size_t msg_len)
{
    if (fsm == NULL || input == NULL) {
        return false;
    }

    update_position(fsm, now_ms);

    if (input->limit_open_active && input->limit_closed_active) {
        enter_error(fsm, GATE_ERROR_BOTH_LIMITS, now_ms);
        set_msg(msg, msg_len, "ERROR: ambos finales activos");
        return true;
    }

    if (fsm->state == GATE_STATE_STARTING) {
        if (input->limit_open_active) {
            fsm->position_pct = 100;
            enter_state(fsm, GATE_STATE_OPEN, now_ms);
            set_msg(msg, msg_len, "Inicio: porton abierto");
        } else if (input->limit_closed_active) {
            fsm->position_pct = 0;
            enter_state(fsm, GATE_STATE_CLOSED, now_ms);
            set_msg(msg, msg_len, "Inicio: porton cerrado");
        } else {
            enter_state(fsm, GATE_STATE_STOPPED, now_ms);
            set_msg(msg, msg_len, "Inicio: posicion intermedia");
        }
        return true;
    }

    if (fsm->state == GATE_STATE_OPENING) {
        if (input->limit_open_active) {
            fsm->position_pct = 100;
            enter_state(fsm, GATE_STATE_OPEN, now_ms);
            set_msg(msg, msg_len, "Final abierto activo");
            return true;
        }
        if ((now_ms - fsm->state_enter_ms) >= seconds_to_ms(fsm->config.max_travel_s)) {
            enter_error(fsm, GATE_ERROR_TRAVEL_TIMEOUT, now_ms);
            set_msg(msg, msg_len, "ERROR: timeout abriendo");
            return true;
        }
    }

    if (fsm->state == GATE_STATE_CLOSING) {
        if (input->limit_closed_active) {
            fsm->position_pct = 0;
            enter_state(fsm, GATE_STATE_CLOSED, now_ms);
            set_msg(msg, msg_len, "Final cerrado activo");
            return true;
        }
        if (input->ftc_blocked) {
            fsm->pending_ftc_reverse = input->dip_ftc_reverse;
            enter_state(fsm, GATE_STATE_PAUSED_BY_FTC, now_ms);
            set_msg(msg, msg_len, input->dip_ftc_reverse ? "FTC: pausa antes de invertir" : "FTC: cierre pausado");
            return true;
        }
        if ((now_ms - fsm->state_enter_ms) >= seconds_to_ms(fsm->config.max_travel_s)) {
            enter_error(fsm, GATE_ERROR_TRAVEL_TIMEOUT, now_ms);
            set_msg(msg, msg_len, "ERROR: timeout cerrando");
            return true;
        }
    }

    if (fsm->state == GATE_STATE_OPEN && effective_auto_close(fsm, input)) {
        enter_state(fsm, GATE_STATE_WAITING_AUTO_CLOSE, now_ms);
        set_msg(msg, msg_len, "Esperando auto-cierre");
        return true;
    }

    if (fsm->state == GATE_STATE_WAITING_AUTO_CLOSE) {
        if (!effective_auto_close(fsm, input)) {
            enter_state(fsm, GATE_STATE_OPEN, now_ms);
            set_msg(msg, msg_len, "Auto-cierre cancelado por configuracion");
            return true;
        }
        if ((now_ms - fsm->state_enter_ms) >= seconds_to_ms(fsm->config.auto_close_s)) {
            return gate_fsm_dispatch(fsm, GATE_EVENT_AUTO_CLOSE, input, now_ms, msg, msg_len);
        }
    }

    if (fsm->state == GATE_STATE_PAUSED_BY_FTC) {
        if (fsm->pending_ftc_reverse &&
            (now_ms - fsm->state_enter_ms) >= seconds_to_ms(fsm->config.reverse_pause_s)) {
            return start_opening(fsm, input, now_ms, msg, msg_len);
        }

        if (!fsm->pending_ftc_reverse &&
            !input->ftc_blocked &&
            (now_ms - fsm->state_enter_ms) >= seconds_to_ms(fsm->config.ftc_wait_s)) {
            enter_state(fsm, GATE_STATE_STOPPED, now_ms);
            set_msg(msg, msg_len, "FTC liberada: detenido seguro");
            return true;
        }
    }

    return false;
}

void gate_fsm_update_config(gate_fsm_t *fsm, const gate_config_t *config)
{
    if (fsm == NULL || config == NULL) {
        return;
    }

    fsm->config = *config;
}

const char *gate_state_name(gate_state_t state)
{
    switch (state) {
    case GATE_STATE_STARTING:
        return "INICIANDO";
    case GATE_STATE_STOPPED:
        return "DETENIDO";
    case GATE_STATE_OPENING:
        return "ABRIENDO";
    case GATE_STATE_CLOSING:
        return "CERRANDO";
    case GATE_STATE_OPEN:
        return "ABIERTO";
    case GATE_STATE_CLOSED:
        return "CERRADO";
    case GATE_STATE_WAITING_AUTO_CLOSE:
        return "ESPERANDO_AUTO_CIERRE";
    case GATE_STATE_PAUSED_BY_FTC:
        return "PAUSADO_POR_FTC";
    case GATE_STATE_ERROR:
        return "ERROR";
    case GATE_STATE_CALIBRATING:
        return "CALIBRANDO";
    default:
        return "DESCONOCIDO";
    }
}

const char *gate_error_name(gate_error_t error)
{
    switch (error) {
    case GATE_ERROR_NONE:
        return "NONE";
    case GATE_ERROR_BOTH_LIMITS:
        return "BOTH_LIMITS";
    case GATE_ERROR_TRAVEL_TIMEOUT:
        return "TRAVEL_TIMEOUT";
    case GATE_ERROR_SENSOR_FAULT:
        return "SENSOR_FAULT";
    case GATE_ERROR_MAINTENANCE_LOCK:
        return "MAINTENANCE_LOCK";
    default:
        return "UNKNOWN";
    }
}

const char *gate_event_name(gate_event_t event)
{
    switch (event) {
    case GATE_EVENT_OPEN_BUTTON:
        return "BOTON_ABRIR";
    case GATE_EVENT_CLOSE_BUTTON:
        return "BOTON_CERRAR";
    case GATE_EVENT_STOP_BUTTON:
        return "BOTON_STOP";
    case GATE_EVENT_MQTT_OPEN:
        return "MQTT_OPEN";
    case GATE_EVENT_MQTT_CLOSE:
        return "MQTT_CLOSE";
    case GATE_EVENT_MQTT_STOP:
        return "MQTT_STOP";
    case GATE_EVENT_LIMIT_OPEN:
        return "FINAL_ABIERTO";
    case GATE_EVENT_LIMIT_CLOSED:
        return "FINAL_CERRADO";
    case GATE_EVENT_FTC_BLOCKED:
        return "FTC_BLOQUEADA";
    case GATE_EVENT_TRAVEL_TIMEOUT:
        return "TIMEOUT";
    case GATE_EVENT_RESET_ERROR:
        return "RESET_ERROR";
    case GATE_EVENT_START_CALIBRATION:
        return "CALIBRATE";
    case GATE_EVENT_FINISH_CALIBRATION:
        return "CALIBRATION_DONE";
    case GATE_EVENT_AUTO_CLOSE:
        return "AUTO_CLOSE";
    default:
        return "NONE";
    }
}

bool gate_state_is_moving(gate_state_t state)
{
    return state == GATE_STATE_OPENING || state == GATE_STATE_CLOSING;
}

bool gate_state_relays_off(gate_state_t state)
{
    return !gate_state_is_moving(state);
}
