#pragma once

#include "esp_err.h"
#include "gate_fsm.h"

esp_err_t storage_init(void);
esp_err_t storage_load_config(gate_config_t *config);
esp_err_t storage_save_config(const gate_config_t *config);
