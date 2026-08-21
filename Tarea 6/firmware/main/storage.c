#include "storage.h"

#include "nvs.h"
#include "nvs_flash.h"

#define STORAGE_NAMESPACE "gate_cfg"

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t storage_load_config(gate_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gate_config_defaults(config);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint16_t value = 0;
    uint8_t flag = 0;

    if (nvs_get_u16(nvs, "auto_s", &value) == ESP_OK) {
        config->auto_close_s = value;
    }
    if (nvs_get_u16(nvs, "travel_s", &value) == ESP_OK) {
        config->max_travel_s = value;
    }
    if (nvs_get_u16(nvs, "ftc_s", &value) == ESP_OK) {
        config->ftc_wait_s = value;
    }
    if (nvs_get_u16(nvs, "rev_s", &value) == ESP_OK) {
        config->reverse_pause_s = value;
    }
    if (nvs_get_u8(nvs, "auto_sw", &flag) == ESP_OK) {
        config->auto_close_sw = flag != 0;
    }
    if (nvs_get_u8(nvs, "maint_sw", &flag) == ESP_OK) {
        config->maintenance_sw = flag != 0;
    }

    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t storage_save_config(const gate_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u16(nvs, "auto_s", config->auto_close_s);
    if (err == ESP_OK) {
        err = nvs_set_u16(nvs, "travel_s", config->max_travel_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(nvs, "ftc_s", config->ftc_wait_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(nvs, "rev_s", config->reverse_pause_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, "auto_sw", config->auto_close_sw ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, "maint_sw", config->maintenance_sw ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return err;
}
