#include "mqtt_manager.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app_config.h"
#include "board_config.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "input_manager.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t mqtt_client;
static QueueHandle_t mqtt_queue;
static bool mqtt_connected;
static gate_config_t active_config;

static void copy_payload(char *dest, size_t dest_len, const char *data, int data_len)
{
    if (dest_len == 0) {
        return;
    }

    size_t copy_len = data_len > 0 ? (size_t)data_len : 0;
    if (copy_len >= dest_len) {
        copy_len = dest_len - 1;
    }

    memcpy(dest, data, copy_len);
    dest[copy_len] = '\0';
}

static void trim(char *text)
{
    char *start = text;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
}

static bool topic_equals(esp_mqtt_event_handle_t event, const char *topic)
{
    size_t expected_len = strlen(topic);
    size_t received_len = event->topic_len > 0 ? (size_t)event->topic_len : 0;
    return event->topic != NULL &&
           received_len == expected_len &&
           strncmp(event->topic, topic, expected_len) == 0;
}

static bool json_get_uint16(const char *json, const char *key, uint16_t *out)
{
    char pattern[40];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return false;
    }

    pos = strchr(pos + strlen(pattern), ':');
    if (pos == NULL) {
        return false;
    }

    pos++;
    while (isspace((unsigned char)*pos)) {
        pos++;
    }

    long value = strtol(pos, NULL, 10);
    if (value < 0) {
        value = 0;
    }
    if (value > 65535) {
        value = 65535;
    }

    *out = (uint16_t)value;
    return true;
}

static bool json_get_bool(const char *json, const char *key, bool *out)
{
    char pattern[40];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return false;
    }

    pos = strchr(pos + strlen(pattern), ':');
    if (pos == NULL) {
        return false;
    }

    pos++;
    while (isspace((unsigned char)*pos)) {
        pos++;
    }

    if (strncasecmp(pos, "true", 4) == 0 || strncmp(pos, "1", 1) == 0) {
        *out = true;
        return true;
    }
    if (strncasecmp(pos, "false", 5) == 0 || strncmp(pos, "0", 1) == 0) {
        *out = false;
        return true;
    }

    return false;
}

static void parse_config_json(const char *payload, gate_config_t *config)
{
    uint16_t seconds = 0;
    bool flag = false;

    if (json_get_uint16(payload, "auto_close_s", &seconds)) {
        config->auto_close_s = seconds;
    }
    if (json_get_uint16(payload, "max_travel_s", &seconds)) {
        config->max_travel_s = seconds;
    }
    if (json_get_uint16(payload, "ftc_wait_s", &seconds)) {
        config->ftc_wait_s = seconds;
    }
    if (json_get_uint16(payload, "reverse_pause_s", &seconds)) {
        config->reverse_pause_s = seconds;
    }
    if (json_get_bool(payload, "auto_close_sw", &flag)) {
        config->auto_close_sw = flag;
    }
    if (json_get_bool(payload, "maintenance_sw", &flag)) {
        config->maintenance_sw = flag;
    }
}

static void send_queue_msg(const mqtt_rx_msg_t *msg)
{
    if (mqtt_queue == NULL || msg == NULL) {
        return;
    }

    if (xQueueSend(mqtt_queue, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Cola MQTT llena");
    }
}

static void handle_command(const char *payload)
{
    mqtt_rx_msg_t msg = {
        .type = MQTT_RX_COMMAND,
        .event = GATE_EVENT_NONE,
        .config = active_config,
    };

    if (strcasecmp(payload, "OPEN") == 0) {
        msg.event = GATE_EVENT_MQTT_OPEN;
    } else if (strcasecmp(payload, "CLOSE") == 0) {
        msg.event = GATE_EVENT_MQTT_CLOSE;
    } else if (strcasecmp(payload, "STOP") == 0) {
        msg.event = GATE_EVENT_MQTT_STOP;
    } else if (strcasecmp(payload, "RESET_ERROR") == 0) {
        msg.event = GATE_EVENT_RESET_ERROR;
    } else if (strcasecmp(payload, "CALIBRATE") == 0) {
        msg.event = GATE_EVENT_START_CALIBRATION;
    } else if (strcasecmp(payload, "SAVE_CONFIG") == 0) {
        msg.type = MQTT_RX_SAVE_CONFIG;
    } else {
        mqtt_publish_ack(payload, false, "Comando desconocido");
        return;
    }

    send_queue_msg(&msg);
}

static void handle_config_set(const char *payload)
{
    mqtt_rx_msg_t msg = {
        .type = MQTT_RX_CONFIG_SET,
        .event = GATE_EVENT_NONE,
        .config = active_config,
    };

    parse_config_json(payload, &msg.config);
    active_config = msg.config;
    send_queue_msg(&msg);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT conectado");
        esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_COMMAND, 1);
        esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_CONFIG_SET, 1);
        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_AVAILABILITY, "online", 0, 1, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado");
        break;

    case MQTT_EVENT_DATA: {
        char payload[256];
        copy_payload(payload, sizeof(payload), event->data, event->data_len);
        trim(payload);

        if (topic_equals(event, MQTT_TOPIC_COMMAND)) {
            handle_command(payload);
        } else if (topic_equals(event, MQTT_TOPIC_CONFIG_SET)) {
            handle_config_set(payload);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error MQTT");
        break;

    default:
        break;
    }
}

esp_err_t mqtt_manager_start(QueueHandle_t queue, const gate_config_t *config)
{
    mqtt_queue = queue;
    if (config != NULL) {
        active_config = *config;
    } else {
        gate_config_defaults(&active_config);
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.last_will.topic = MQTT_TOPIC_AVAILABILITY,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
        .credentials.client_id = MQTT_CLIENT_ID,
    };
#else
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER_URI,
        .lwt_topic = MQTT_TOPIC_AVAILABILITY,
        .lwt_msg = "offline",
        .lwt_qos = 1,
        .lwt_retain = 1,
        .client_id = MQTT_CLIENT_ID,
    };
#endif

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    ESP_LOGI(TAG, "Broker MQTT: %s", MQTT_BROKER_URI);
    return ESP_OK;
}

bool mqtt_manager_is_connected(void)
{
    return mqtt_connected;
}

void mqtt_publish_state(const gate_fsm_t *fsm, const input_snapshot_t *input)
{
    if (!mqtt_connected || fsm == NULL || input == NULL) {
        return;
    }

    char payload[256];
    snprintf(
        payload,
        sizeof(payload),
        "{\"state\":\"%s\",\"position\":%d,\"error\":\"%s\",\"dip\":%u,\"sensors\":%u}",
        gate_state_name(fsm->state),
        fsm->position_pct,
        gate_error_name(fsm->error),
        input_dip_mask(input),
        input_sensor_mask(input)
    );
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATE, payload, 0, 1, 1);
}

void mqtt_publish_telemetry(const gate_fsm_t *fsm, const input_snapshot_t *input)
{
    if (!mqtt_connected || fsm == NULL || input == NULL) {
        return;
    }

    char payload[320];
    snprintf(
        payload,
        sizeof(payload),
        "{\"position\":%d,\"moving\":%s,\"limit_open\":%s,\"limit_closed\":%s,\"ftc\":%s,"
        "\"dip_encoder\":%s,\"dip_ftc_reverse\":%s,\"dip_auto_close\":%s,\"dip_maintenance\":%s}",
        fsm->position_pct,
        gate_state_is_moving(fsm->state) ? "true" : "false",
        input->limit_open_active ? "true" : "false",
        input->limit_closed_active ? "true" : "false",
        input->ftc_blocked ? "true" : "false",
        input->dip_encoder_enabled ? "true" : "false",
        input->dip_ftc_reverse ? "true" : "false",
        input->dip_auto_close ? "true" : "false",
        input->dip_maintenance ? "true" : "false"
    );
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_TELEMETRY, payload, 0, 0, 0);
}

void mqtt_publish_config_state(const gate_config_t *config, const input_snapshot_t *input)
{
    if (!mqtt_connected || config == NULL || input == NULL) {
        return;
    }

    char payload[320];
    snprintf(
        payload,
        sizeof(payload),
        "{\"auto_close_s\":%u,\"max_travel_s\":%u,\"ftc_wait_s\":%u,\"reverse_pause_s\":%u,"
        "\"auto_close_sw\":%s,\"maintenance_sw\":%s,\"dip_auto_close\":%s,\"dip_maintenance\":%s}",
        config->auto_close_s,
        config->max_travel_s,
        config->ftc_wait_s,
        config->reverse_pause_s,
        config->auto_close_sw ? "true" : "false",
        config->maintenance_sw ? "true" : "false",
        input->dip_auto_close ? "true" : "false",
        input->dip_maintenance ? "true" : "false"
    );
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_CONFIG_STATE, payload, 0, 1, 1);
}

void mqtt_publish_event(const char *type, const char *comment)
{
    if (!mqtt_connected) {
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"type\":\"%s\",\"comment\":\"%s\"}", type, comment);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_EVENT, payload, 0, 1, 0);
}

void mqtt_publish_ack(const char *command, bool accepted, const char *detail)
{
    if (!mqtt_connected) {
        return;
    }

    char payload[256];
    snprintf(
        payload,
        sizeof(payload),
        "{\"command\":\"%s\",\"accepted\":%s,\"detail\":\"%s\"}",
        command,
        accepted ? "true" : "false",
        detail
    );
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_ACK, payload, 0, 1, 0);
}
