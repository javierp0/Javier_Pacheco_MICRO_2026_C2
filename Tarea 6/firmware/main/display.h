#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t display_init(bool enabled);
void display_show_reserved_notice(void);
