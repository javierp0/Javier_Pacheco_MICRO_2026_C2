#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "gate_fsm.h"

typedef enum {
    MQTT_RX_COMMAND = 0,
    MQTT_RX_CONFIG_SET,
    MQTT_RX_SAVE_CONFIG
} mqtt_rx_type_t;

typedef struct {
    mqtt_rx_type_t type;
    gate_event_t event;
    gate_config_t config;
} mqtt_rx_msg_t;

esp_err_t mqtt_manager_start(QueueHandle_t queue, const gate_config_t *config);
bool mqtt_manager_is_connected(void);
void mqtt_publish_state(const gate_fsm_t *fsm, const input_snapshot_t *input);
void mqtt_publish_telemetry(const gate_fsm_t *fsm, const input_snapshot_t *input);
void mqtt_publish_config_state(const gate_config_t *config, const input_snapshot_t *input);
void mqtt_publish_event(const char *type, const char *comment);
void mqtt_publish_ack(const char *command, bool accepted, const char *detail);
