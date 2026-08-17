#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MQTT_CMD_TOPIC CONFIG_TAREA3_TOPIC_BASE "/cmd"
#define MQTT_STATE_TOPIC CONFIG_TAREA3_TOPIC_BASE "/estado"
#define MQTT_AVAILABILITY_TOPIC CONFIG_TAREA3_TOPIC_BASE "/disponible"

#define BUTTON_DEBOUNCE_MS 250

static const char *TAG = "tarea3";

typedef enum {
    STATE_LED_OFF = 0,
    STATE_LED_ON
} led_state_t;

typedef enum {
    APP_EVENT_BUTTON_PRESSED = 0,
    APP_EVENT_MQTT_ON,
    APP_EVENT_MQTT_OFF,
    APP_EVENT_MQTT_TOGGLE
} app_event_t;

static QueueHandle_t app_queue;
static QueueHandle_t button_queue;
static esp_mqtt_client_handle_t mqtt_client;
static bool mqtt_connected;
static led_state_t current_state = STATE_LED_OFF;

static const char *state_to_text(led_state_t state)
{
    return state == STATE_LED_ON ? "ON" : "OFF";
}

static const char *event_to_text(app_event_t event)
{
    switch (event) {
    case APP_EVENT_BUTTON_PRESSED:
        return "BOTON";
    case APP_EVENT_MQTT_ON:
        return "MQTT_ON";
    case APP_EVENT_MQTT_OFF:
        return "MQTT_OFF";
    case APP_EVENT_MQTT_TOGGLE:
        return "MQTT_TOGGLE";
    default:
        return "DESCONOCIDO";
    }
}

static void mqtt_publish_state(void)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        return;
    }

    const char *payload = state_to_text(current_state);
    esp_mqtt_client_publish(mqtt_client, MQTT_STATE_TOPIC, payload, 0, 1, 1);
}

static void transition_to(led_state_t next_state)
{
    current_state = next_state;
    gpio_set_level((gpio_num_t)CONFIG_TAREA3_LED_GPIO, current_state == STATE_LED_ON);
    ESP_LOGI(TAG, "Estado actual: %s", state_to_text(current_state));
    mqtt_publish_state();
}

static led_state_t get_next_state(led_state_t state, app_event_t event)
{
    switch (state) {
    case STATE_LED_OFF:
        switch (event) {
        case APP_EVENT_BUTTON_PRESSED:
        case APP_EVENT_MQTT_TOGGLE:
        case APP_EVENT_MQTT_ON:
            return STATE_LED_ON;
        case APP_EVENT_MQTT_OFF:
        default:
            return STATE_LED_OFF;
        }

    case STATE_LED_ON:
        switch (event) {
        case APP_EVENT_BUTTON_PRESSED:
        case APP_EVENT_MQTT_TOGGLE:
        case APP_EVENT_MQTT_OFF:
            return STATE_LED_OFF;
        case APP_EVENT_MQTT_ON:
        default:
            return STATE_LED_ON;
        }

    default:
        return STATE_LED_OFF;
    }
}

static void state_machine_task(void *arg)
{
    (void)arg;

    app_event_t event;

    transition_to(STATE_LED_OFF);

    while (true) {
        if (xQueueReceive(app_queue, &event, portMAX_DELAY) == pdTRUE) {
            led_state_t next_state = get_next_state(current_state, event);
            ESP_LOGI(TAG, "Evento recibido: %s", event_to_text(event));
            transition_to(next_state);
        }
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t press = 1;
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(button_queue, &press, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg)
{
    (void)arg;

    uint32_t press;
    TickType_t last_press_tick = 0;

    while (true) {
        if (xQueueReceive(button_queue, &press, portMAX_DELAY) == pdTRUE) {
            (void)press;

            TickType_t now = xTaskGetTickCount();

            if ((now - last_press_tick) < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
                continue;
            }

            last_press_tick = now;

            app_event_t event = APP_EVENT_BUTTON_PRESSED;
            xQueueSend(app_queue, &event, portMAX_DELAY);
        }
    }
}

static void configure_led(void)
{
    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << CONFIG_TAREA3_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&led_config));
    gpio_set_level((gpio_num_t)CONFIG_TAREA3_LED_GPIO, 0);
}

static void configure_button(void)
{
    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << CONFIG_TAREA3_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    ESP_ERROR_CHECK(gpio_config(&button_config));

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(
        (gpio_num_t)CONFIG_TAREA3_BUTTON_GPIO,
        button_isr_handler,
        NULL
    ));
}

static void trim_command(char *text)
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
    if (event->topic == NULL) {
        return false;
    }

    size_t expected_len = strlen(topic);
    size_t received_len = event->topic_len > 0 ? (size_t)event->topic_len : 0;

    return received_len == expected_len &&
           strncmp(event->topic, topic, expected_len) == 0;
}

static void send_app_event(app_event_t event)
{
    if (xQueueSend(app_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "No se pudo enviar el evento %s", event_to_text(event));
    }
}

static void handle_mqtt_data(esp_mqtt_event_handle_t event)
{
    if (event->data == NULL) {
        return;
    }

    if (!topic_equals(event, MQTT_CMD_TOPIC)) {
        return;
    }

    char command[24];
    size_t data_len = event->data_len > 0 ? (size_t)event->data_len : 0;
    size_t copy_len = data_len < sizeof(command) - 1
                          ? data_len
                          : sizeof(command) - 1;

    memcpy(command, event->data, copy_len);
    command[copy_len] = '\0';
    trim_command(command);

    ESP_LOGI(TAG, "Comando MQTT recibido: %s", command);

    if (strcasecmp(command, "ON") == 0) {
        send_app_event(APP_EVENT_MQTT_ON);
    } else if (strcasecmp(command, "OFF") == 0) {
        send_app_event(APP_EVENT_MQTT_OFF);
    } else if (strcasecmp(command, "TOGGLE") == 0) {
        send_app_event(APP_EVENT_MQTT_TOGGLE);
    } else {
        ESP_LOGW(TAG, "Comando no reconocido. Usa ON, OFF o TOGGLE");
    }
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT conectado");
        esp_mqtt_client_subscribe(mqtt_client, MQTT_CMD_TOPIC, 1);
        esp_mqtt_client_publish(mqtt_client, MQTT_AVAILABILITY_TOPIC, "online", 0, 1, 1);
        mqtt_publish_state();
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado");
        break;

    case MQTT_EVENT_DATA:
        handle_mqtt_data(event);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error MQTT");
        break;

    default:
        break;
    }
}

static void mqtt_app_start(void)
{
    if (mqtt_client != NULL) {
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_TAREA3_MQTT_URI,
        .session.last_will.topic = MQTT_AVAILABILITY_TOPIC,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
#else
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_TAREA3_MQTT_URI,
        .lwt_topic = MQTT_AVAILABILITY_TOPIC,
        .lwt_msg = "offline",
        .lwt_qos = 1,
        .lwt_retain = 1,
    };
#endif

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "No se pudo inicializar el cliente MQTT");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    ));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        mqtt_connected = false;
        ESP_LOGW(TAG, "WiFi desconectado. Reintentando...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        mqtt_app_start();
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {0};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", CONFIG_TAREA3_WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", CONFIG_TAREA3_WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = strlen(CONFIG_TAREA3_WIFI_PASSWORD) == 0
                                             ? WIFI_AUTH_OPEN
                                             : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a WiFi SSID: %s", CONFIG_TAREA3_WIFI_SSID);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    app_queue = xQueueCreate(10, sizeof(app_event_t));
    button_queue = xQueueCreate(10, sizeof(uint32_t));
    if (app_queue == NULL || button_queue == NULL) {
        ESP_LOGE(TAG, "No se pudieron crear las colas de eventos");
        return;
    }

    configure_led();
    configure_button();

    ESP_LOGI(TAG, "ESP32 clasico target esp32");
    ESP_LOGI(TAG, "LED GPIO%d, boton GPIO%d", CONFIG_TAREA3_LED_GPIO, CONFIG_TAREA3_BUTTON_GPIO);
    ESP_LOGI(TAG, "MQTT broker: %s", CONFIG_TAREA3_MQTT_URI);
    ESP_LOGI(TAG, "MQTT comandos: %s", MQTT_CMD_TOPIC);
    ESP_LOGI(TAG, "MQTT estado: %s", MQTT_STATE_TOPIC);

    xTaskCreate(state_machine_task, "state_machine", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button", 2048, NULL, 6, NULL);

    wifi_init_sta();
}
