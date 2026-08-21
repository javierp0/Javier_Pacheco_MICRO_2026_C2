#include "mpu6050.h"

#include <math.h>
#include <string.h>

#include "board_config.h"

#define MPU6050_REG_SMPLRT_DIV 0x19
#define MPU6050_REG_CONFIG 0x1A
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_WHO_AM_I 0x75

#define MPU6050_WHO_AM_I_VALUE 0x68
#define MPU6050_TIMEOUT_MS 100
#define RAD_TO_DEG 57.2957795f

static int16_t be16_to_i16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static esp_err_t write_reg(mpu6050_t *mpu, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(mpu->dev, data, sizeof(data), MPU6050_TIMEOUT_MS);
}

static esp_err_t read_reg(mpu6050_t *mpu, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(mpu->dev, &reg, 1, value, 1, MPU6050_TIMEOUT_MS);
}

esp_err_t mpu6050_init(mpu6050_t *mpu, i2c_master_bus_handle_t bus)
{
    if (mpu == NULL || bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(mpu, 0, sizeof(*mpu));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_MPU6050_ADDR,
        .scl_speed_hz = BOARD_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &mpu->dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t who = 0;
    err = read_reg(mpu, MPU6050_REG_WHO_AM_I, &who);
    if (err != ESP_OK) {
        return err;
    }
    if (who != MPU6050_WHO_AM_I_VALUE) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = write_reg(mpu, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(mpu, MPU6050_REG_SMPLRT_DIV, 0x07);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(mpu, MPU6050_REG_CONFIG, 0x03);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(mpu, MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(mpu, MPU6050_REG_GYRO_CONFIG, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    mpu->ready = true;
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_t *mpu, mpu6050_sample_t *sample)
{
    if (mpu == NULL || sample == NULL || !mpu->ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t data[14] = {0};

    esp_err_t err = i2c_master_transmit_receive(mpu->dev, &reg, 1, data, sizeof(data), MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    int16_t ax = be16_to_i16(&data[0]);
    int16_t ay = be16_to_i16(&data[2]);
    int16_t az = be16_to_i16(&data[4]);
    int16_t temp = be16_to_i16(&data[6]);
    int16_t gx = be16_to_i16(&data[8]);
    int16_t gy = be16_to_i16(&data[10]);
    int16_t gz = be16_to_i16(&data[12]);

    sample->ax_g = (float)ax / 16384.0f;
    sample->ay_g = (float)ay / 16384.0f;
    sample->az_g = (float)az / 16384.0f;
    sample->gx_dps = (float)gx / 131.0f;
    sample->gy_dps = (float)gy / 131.0f;
    sample->gz_dps = (float)gz / 131.0f;
    sample->temperature_c = ((float)temp / 340.0f) + 36.53f;

    sample->roll_deg = atan2f(sample->ay_g, sample->az_g) * RAD_TO_DEG;
    sample->pitch_deg = atan2f(-sample->ax_g, sqrtf((sample->ay_g * sample->ay_g) + (sample->az_g * sample->az_g))) * RAD_TO_DEG;

    return ESP_OK;
}

void mpu6050_calibration_defaults(mpu6050_calibration_t *cal)
{
    if (cal == NULL) {
        return;
    }

    cal->roll_center_deg = 0.0f;
    cal->pitch_center_deg = 0.0f;
    cal->valid = false;
}
