#include "display.h"

#include "esp_log.h"

static const char *TAG = "display";

esp_err_t display_init(bool enabled)
{
    if (enabled) {
        ESP_LOGI(TAG, "OLED reservado para expansion futura");
    }
    return ESP_OK;
}

void display_show_reserved_notice(void)
{
    ESP_LOGI(TAG, "Display deshabilitado inicialmente");
}
