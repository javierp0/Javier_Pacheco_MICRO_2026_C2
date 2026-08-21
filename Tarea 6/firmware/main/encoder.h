#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t encoder_init(bool enabled);
int encoder_get_position_ticks(void);
