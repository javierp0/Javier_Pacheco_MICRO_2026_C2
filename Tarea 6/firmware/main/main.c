#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "board_config.h"
#include "buzzer.h"
#include "display.h"
#include "encoder.h"
#include "gate_fsm.h"
#include "input_manager.h"
#include "motor_control.h"
#include "mqtt_manager.h"
#include "status_led.h"
#include "storage.h"
#include "wifi_manager.h"

#define LOOP_PERIOD_MS 20
#define MQTT_STATE_PERIOD_MS 500
#define MQTT_TELEMETRY_PERIOD_MS 2000
#define MQTT_CONFIG_PERIOD_MS 5000
#define MQTT_QUEUE_DEPTH 10

static const char *TAG = "porton";

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void publish_transition(const char *message)
{
    ESP_LOGI(TAG, "%s", message);
    mqtt_publish_event("fsm", message);
}

static void apply_outputs(const gate_fsm_t *fsm)
{
    motor_control_apply_state(fsm->state);
    status_led_apply_state(fsm->state);
}

static void handle_fsm_message(gate_fsm_t *fsm, const input_snapshot_t *input, bool changed, const char *message)
{
    static gate_state_t previous_state = GATE_STATE_STARTING;

    if (changed && message != NULL && message[0] != '\0') {
        publish_transition(message);
    }

    if (fsm->state == GATE_STATE_ERROR && previous_state != GATE_STATE_ERROR) {
        buzzer_enter_error(now_ms());
        mqtt_publish_event("error", gate_error_name(fsm->error));
    }

    previous_state = fsm->state;
    apply_outputs(fsm);
    mqtt_publish_state(fsm, input);
}

static bool dispatch_event(
    gate_fsm_t *fsm,
    gate_event_t event,
    const input_snapshot_t *input,
    const char *ack_name
)
{
    char message[96] = {0};
    bool accepted = gate_fsm_dispatch(fsm, event, input, now_ms(), message, sizeof(message));

    ESP_LOGI(TAG, "Evento %s -> %s", gate_event_name(event), message);

    if (ack_name != NULL) {
        mqtt_publish_ack(ack_name, accepted, message);
    }

    handle_fsm_message(fsm, input, true, message);
    return accepted;
}

static void handle_input_events(gate_fsm_t *fsm, const input_snapshot_t *snapshot, const input_events_t *events)
{
    if (events->open_pressed_edge) {
        dispatch_event(fsm, GATE_EVENT_OPEN_BUTTON, snapshot, NULL);
    }
    if (events->close_pressed_edge) {
        dispatch_event(fsm, GATE_EVENT_CLOSE_BUTTON, snapshot, NULL);
    }
    if (events->stop_pressed_edge) {
        dispatch_event(fsm, GATE_EVENT_STOP_BUTTON, snapshot, NULL);
    }
}

static void handle_mqtt_msg(
    gate_fsm_t *fsm,
    const input_snapshot_t *snapshot,
    const mqtt_rx_msg_t *msg
)
{
    if (msg->type == MQTT_RX_COMMAND) {
        dispatch_event(fsm, msg->event, snapshot, gate_event_name(msg->event));
    } else if (msg->type == MQTT_RX_CONFIG_SET) {
        gate_config_t config = msg->config;

        if (!snapshot->dip_maintenance && config.maintenance_sw) {
            config.maintenance_sw = false;
            mqtt_publish_ack("CONFIG_SET", false, "DIP4 no permite mantenimiento por app");
        } else {
            mqtt_publish_ack("CONFIG_SET", true, "Configuracion aplicada en RAM");
        }

        gate_fsm_update_config(fsm, &config);
        mqtt_publish_config_state(&fsm->config, snapshot);
    } else if (msg->type == MQTT_RX_SAVE_CONFIG) {
        esp_err_t err = storage_save_config(&fsm->config);
        mqtt_publish_ack("SAVE_CONFIG", err == ESP_OK, err == ESP_OK ? "Configuracion guardada" : "Error guardando NVS");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "%s iniciando", BOARD_NAME);

    ESP_ERROR_CHECK(storage_init());

    gate_config_t config;
    ESP_ERROR_CHECK(storage_load_config(&config));

    input_manager_t input_manager = {0};
    input_snapshot_t input_snapshot = {0};
    input_events_t input_events = {0};

    ESP_ERROR_CHECK(input_manager_init(&input_manager));
    input_manager_update(&input_manager, &input_snapshot, &input_events);

    ESP_ERROR_CHECK(motor_control_init());
    ESP_ERROR_CHECK(buzzer_init());
    ESP_ERROR_CHECK(status_led_init());
    ESP_ERROR_CHECK(encoder_init(input_snapshot.dip_encoder_enabled));
    ESP_ERROR_CHECK(display_init(false));

    gate_fsm_t fsm;
    gate_fsm_init(&fsm, &config, now_ms());

    QueueHandle_t mqtt_queue = xQueueCreate(MQTT_QUEUE_DEPTH, sizeof(mqtt_rx_msg_t));
    if (mqtt_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear cola MQTT");
        return;
    }

    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_ERROR_CHECK(mqtt_manager_start(mqtt_queue, &config));

    uint32_t last_state_pub = 0;
    uint32_t last_telemetry_pub = 0;
    uint32_t last_config_pub = 0;

    while (true) {
        uint32_t now = now_ms();

        input_manager_update(&input_manager, &input_snapshot, &input_events);
        handle_input_events(&fsm, &input_snapshot, &input_events);

        mqtt_rx_msg_t mqtt_msg;
        while (xQueueReceive(mqtt_queue, &mqtt_msg, 0) == pdTRUE) {
            handle_mqtt_msg(&fsm, &input_snapshot, &mqtt_msg);
        }

        char tick_msg[96] = {0};
        bool changed = gate_fsm_tick(&fsm, &input_snapshot, now, tick_msg, sizeof(tick_msg));
        handle_fsm_message(&fsm, &input_snapshot, changed, tick_msg);

        if ((now - last_state_pub) >= MQTT_STATE_PERIOD_MS) {
            mqtt_publish_state(&fsm, &input_snapshot);
            last_state_pub = now;
        }

        if ((now - last_telemetry_pub) >= MQTT_TELEMETRY_PERIOD_MS) {
            mqtt_publish_telemetry(&fsm, &input_snapshot);
            last_telemetry_pub = now;
        }

        if ((now - last_config_pub) >= MQTT_CONFIG_PERIOD_MS) {
            mqtt_publish_config_state(&fsm.config, &input_snapshot);
            last_config_pub = now;
        }

        buzzer_update(now);
        apply_outputs(&fsm);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}
