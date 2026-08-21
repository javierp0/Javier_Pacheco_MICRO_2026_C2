#include "wifi_manager.h"

#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi";
static EventGroupHandle_t wifi_event_group;
static bool wifi_connected;

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
        wifi_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_manager_start(void)
{
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = strlen(WIFI_PASSWORD) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a WiFi SSID: %s", WIFI_SSID);
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}
