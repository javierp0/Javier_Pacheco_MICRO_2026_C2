#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#define PB1_GPIO GPIO_NUM_4
#define PB2_GPIO GPIO_NUM_5
#define LED_GPIO GPIO_NUM_2
#define BUZZER_GPIO GPIO_NUM_18

#define DEBOUNCE_US (25 * 1000)
#define RANDOM_WAIT_MIN_MS 2000
#define RANDOM_WAIT_MAX_MS 6000
#define RESULT_DISPLAY_MS 3000
#define ERROR_DISPLAY_MS 3000
#define RESULT_BEEP_MS 120

#define WIFI_CONNECTED_BIT BIT0
#define BUTTON_QUEUE_LEN 16

typedef enum {
    STATE_READY = 0,
    STATE_RANDOM_WAIT,
    STATE_SIGNAL_ON,
    STATE_WAIT_PB2,
    STATE_RESULT_SHOWN,
    STATE_ERROR_TRIAL
} reaction_state_t;

typedef struct {
    gpio_num_t gpio;
    int level;
    int64_t timestamp_us;
} button_event_t;

typedef struct {
    reaction_state_t state;
    uint32_t trial;
    uint32_t random_wait_ms;
    int64_t trial_start_us;
    int64_t random_deadline_us;
    int64_t count_start_us;
    int64_t state_until_us;
    int64_t buzzer_off_us;
} reaction_fsm_t;

static const char *TAG = "reaction";

static QueueHandle_t button_queue;
static EventGroupHandle_t wifi_event_group;
static esp_mqtt_client_handle_t mqtt_client;
static bool mqtt_connected;
static bool wifi_connected;
static char ip_text[16] = "0.0.0.0";

static char topic_estado[128];
static char topic_evento[128];
static char topic_resultado[128];
static char topic_error[128];
static char topic_ip[128];

static reaction_fsm_t fsm;
static int64_t last_irq_us[2];

static void mqtt_start(void);

static bool button_is_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == 0;
}

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static int64_t now_ms(void)
{
    return now_us() / 1000;
}

static void output_set(gpio_num_t gpio, bool on)
{
    gpio_set_level(gpio, on ? 1 : 0);
}

static void publish_text(const char *topic, const char *payload, int qos, int retain)
{
    if (!mqtt_connected || mqtt_client == NULL) {
        ESP_LOGI(TAG, "MQTT pendiente %s -> %s", topic, payload);
        return;
    }

    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, qos, retain);
}

static void publish_estado(const char *message)
{
    ESP_LOGI(TAG, "Estado: %s", message);
    publish_text(topic_estado, message, 1, 1);
}

static void publish_event(const char *event_name)
{
    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"ensayo\":%" PRIu32 ",\"evento\":\"%s\",\"tiempo_ms\":%" PRId64 "}",
             fsm.trial, event_name, now_ms());
    ESP_LOGI(TAG, "Evento: %s", payload);
    publish_text(topic_evento, payload, 1, 0);
}

static void publish_error(const char *error_text)
{
    char payload[220];
    snprintf(payload, sizeof(payload),
             "{\"ensayo\":%" PRIu32 ",\"error\":\"%s\",\"tiempo_ms\":%" PRId64 "}",
             fsm.trial, error_text, now_ms());
    ESP_LOGW(TAG, "Error: %s", payload);
    publish_text(topic_error, payload, 1, 0);
}

static void publish_result(uint32_t reaction_ms)
{
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"ensayo\":%" PRIu32
             ",\"tiempo_pb1_suelto_a_pb2_ms\":%" PRIu32
             ",\"inicio_conteo\":\"pb1_suelto\""
             ",\"fin_conteo\":\"pb2_presionado\""
             ",\"espera_aleatoria_ms\":%" PRIu32 "}",
             fsm.trial, reaction_ms, fsm.random_wait_ms);

    ESP_LOGI(TAG, "Resultado: %s", payload);
    publish_text(topic_resultado, payload, 1, 0);
}

static void publish_ip(void)
{
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"ip\":\"%s\",\"broker\":\"%s\"}", ip_text, CONFIG_REACTION_MQTT_URI);
    publish_text(topic_ip, payload, 1, 1);
}

static void enter_ready(void)
{
    fsm.state = STATE_READY;
    fsm.count_start_us = 0;
    fsm.random_deadline_us = 0;
    fsm.state_until_us = 0;
    output_set(LED_GPIO, false);
    output_set(BUZZER_GPIO, false);
    publish_estado("Listo: presiona y manten PB1");
}

static void enter_error(const char *message)
{
    fsm.state = STATE_ERROR_TRIAL;
    fsm.state_until_us = now_us() + (ERROR_DISPLAY_MS * 1000);
    output_set(LED_GPIO, false);
    output_set(BUZZER_GPIO, false);
    publish_error(message);
    publish_estado("Error en el ensayo; vuelve a intentarlo");
}

static void start_trial(int64_t timestamp_us)
{
    fsm.trial++;
    fsm.state = STATE_RANDOM_WAIT;
    fsm.trial_start_us = timestamp_us;
    fsm.random_wait_ms = RANDOM_WAIT_MIN_MS + (esp_random() % (RANDOM_WAIT_MAX_MS - RANDOM_WAIT_MIN_MS + 1));
    fsm.random_deadline_us = timestamp_us + ((int64_t)fsm.random_wait_ms * 1000);
    fsm.count_start_us = 0;

    output_set(LED_GPIO, false);
    output_set(BUZZER_GPIO, false);

    publish_event("pb1_presionado_inicio_ensayo");
    publish_estado("Ensayo iniciado: manten PB1 y espera la senal");
}

static void turn_signal_on(void)
{
    fsm.state = STATE_SIGNAL_ON;
    output_set(LED_GPIO, true);
    output_set(BUZZER_GPIO, true);
    publish_event("senal_led_buzzer");
    publish_estado("Senal activa: suelta PB1");
}

static void start_main_count(int64_t timestamp_us)
{
    fsm.state = STATE_WAIT_PB2;
    fsm.count_start_us = timestamp_us;
    output_set(BUZZER_GPIO, false);
    publish_event("pb1_suelto_inicio_conteo");
    publish_estado("Conteo iniciado: presiona PB2");
}

static void finish_trial(int64_t timestamp_us)
{
    uint32_t reaction_ms = (uint32_t)((timestamp_us - fsm.count_start_us) / 1000);
    fsm.state = STATE_RESULT_SHOWN;
    fsm.state_until_us = timestamp_us + (RESULT_DISPLAY_MS * 1000);
    fsm.buzzer_off_us = timestamp_us + (RESULT_BEEP_MS * 1000);

    output_set(LED_GPIO, false);
    output_set(BUZZER_GPIO, true);

    publish_event("pb2_presionado_fin_conteo");
    publish_result(reaction_ms);
    publish_estado("Resultado publicado; preparando siguiente ensayo");
}

static void handle_button_event(const button_event_t *event)
{
    bool pressed = event->level == 0;

    if (event->gpio == PB1_GPIO) {
        if (pressed && fsm.state == STATE_READY) {
            start_trial(event->timestamp_us);
        } else if (!pressed && fsm.state == STATE_RANDOM_WAIT) {
            enter_error("Error: soltaste PB1 antes de la senal");
        } else if (!pressed && fsm.state == STATE_SIGNAL_ON) {
            start_main_count(event->timestamp_us);
        }
        return;
    }

    if (event->gpio == PB2_GPIO && pressed) {
        if (fsm.state == STATE_RANDOM_WAIT) {
            enter_error("Error: presionaste PB2 antes de tiempo");
        } else if (fsm.state == STATE_SIGNAL_ON) {
            enter_error("Error: presionaste PB2 antes de soltar PB1");
        } else if (fsm.state == STATE_WAIT_PB2) {
            finish_trial(event->timestamp_us);
        }
    }
}

static void fsm_tick(void)
{
    int64_t t_us = now_us();

    if (fsm.buzzer_off_us > 0 && t_us >= fsm.buzzer_off_us) {
        output_set(BUZZER_GPIO, false);
        fsm.buzzer_off_us = 0;
    }

    switch (fsm.state) {
    case STATE_RANDOM_WAIT:
        if (!button_is_pressed(PB1_GPIO)) {
            enter_error("Error: soltaste PB1 antes de la senal");
        } else if (button_is_pressed(PB2_GPIO)) {
            enter_error("Error: presionaste PB2 antes de tiempo");
        } else if (t_us >= fsm.random_deadline_us) {
            turn_signal_on();
        }
        break;

    case STATE_SIGNAL_ON:
        if (button_is_pressed(PB2_GPIO)) {
            enter_error("Error: presionaste PB2 antes de soltar PB1");
        }
        break;

    case STATE_RESULT_SHOWN:
    case STATE_ERROR_TRIAL:
        if (t_us >= fsm.state_until_us) {
            enter_ready();
        }
        break;

    default:
        break;
    }
}

static void gpio_isr_handler(void *arg)
{
    gpio_num_t gpio = (gpio_num_t)(intptr_t)arg;
    int index = gpio == PB1_GPIO ? 0 : 1;
    int64_t t_us = esp_timer_get_time();

    if ((t_us - last_irq_us[index]) < DEBOUNCE_US) {
        return;
    }
    last_irq_us[index] = t_us;

    button_event_t event = {
        .gpio = gpio,
        .level = gpio_get_level(gpio),
        .timestamp_us = t_us,
    };

    BaseType_t high_task_woken = pdFALSE;
    xQueueSendFromISR(button_queue, &event, &high_task_woken);
    if (high_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void configure_gpio(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << LED_GPIO) | (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
    output_set(LED_GPIO, false);
    output_set(BUZZER_GPIO, false);

    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << PB1_GPIO) | (1ULL << PB2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_config));

    esp_err_t isr_result = gpio_install_isr_service(0);
    if (isr_result != ESP_OK && isr_result != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(isr_result);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(PB1_GPIO, gpio_isr_handler, (void *)PB1_GPIO));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PB2_GPIO, gpio_isr_handler, (void *)PB2_GPIO));
}

static void init_topics(void)
{
    snprintf(topic_estado, sizeof(topic_estado), "%s/estado", CONFIG_REACTION_TOPIC_BASE);
    snprintf(topic_evento, sizeof(topic_evento), "%s/evento", CONFIG_REACTION_TOPIC_BASE);
    snprintf(topic_resultado, sizeof(topic_resultado), "%s/resultado", CONFIG_REACTION_TOPIC_BASE);
    snprintf(topic_error, sizeof(topic_error), "%s/error", CONFIG_REACTION_TOPIC_BASE);
    snprintf(topic_ip, sizeof(topic_ip), "%s/ip", CONFIG_REACTION_TOPIC_BASE);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT conectado");
        publish_ip();
        publish_estado("Listo: presiona y manten PB1");
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error de transporte");
        break;

    default:
        ESP_LOGD(TAG, "MQTT event id=%" PRId32, event->event_id);
        break;
    }
}

static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = CONFIG_REACTION_MQTT_URI,
    };

    if (strlen(CONFIG_REACTION_MQTT_USERNAME) > 0) {
        mqtt_config.credentials.username = CONFIG_REACTION_MQTT_USERNAME;
        mqtt_config.credentials.authentication.password = CONFIG_REACTION_MQTT_PASSWORD;
    }

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi desconectado, reintentando");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP obtenida: %s", ip_text);
        if (mqtt_client == NULL) {
            mqtt_start();
        }
    }
}

static void wifi_start(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(wifi_event_group == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", CONFIG_REACTION_WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", CONFIG_REACTION_WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = strlen(CONFIG_REACTION_WIFI_PASSWORD) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a WiFi SSID: %s", CONFIG_REACTION_WIFI_SSID);
}

static void reaction_task(void *arg)
{
    (void)arg;

    enter_ready();

    while (true) {
        button_event_t event;
        while (xQueueReceive(button_queue, &event, 0) == pdTRUE) {
            handle_button_event(&event);
        }

        fsm_tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Tarea 7 - Medidor de reaccion humana con ESP32 clasico");
    ESP_LOGI(TAG, "PB1=GPIO4, PB2=GPIO5, LED=GPIO2, Buzzer=GPIO18");

    init_topics();

    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    configure_gpio();

    button_queue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(button_event_t));
    ESP_ERROR_CHECK(button_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    wifi_start();

    xTaskCreate(reaction_task, "reaction_task", 4096, NULL, 5, NULL);
}
