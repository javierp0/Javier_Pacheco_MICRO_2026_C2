#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float temperature_c;
    float roll_deg;
    float pitch_deg;
} mpu6050_sample_t;

typedef struct {
    float roll_center_deg;
    float pitch_center_deg;
    bool valid;
} mpu6050_calibration_t;

typedef struct {
    i2c_master_dev_handle_t dev;
    bool ready;
} mpu6050_t;

esp_err_t mpu6050_init(mpu6050_t *mpu, i2c_master_bus_handle_t bus);
esp_err_t mpu6050_read(mpu6050_t *mpu, mpu6050_sample_t *sample);
void mpu6050_calibration_defaults(mpu6050_calibration_t *cal);
