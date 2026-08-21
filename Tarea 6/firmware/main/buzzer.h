#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t buzzer_init(void);
void buzzer_enter_error(uint32_t now_ms);
void buzzer_update(uint32_t now_ms);
void buzzer_off(void);
