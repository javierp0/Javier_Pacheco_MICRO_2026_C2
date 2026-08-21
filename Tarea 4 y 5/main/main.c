#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_types.h"
#include "board_config.h"
#include "buttons.h"
#include "joystick.h"
#include "mpu6050.h"
#include "oled.h"

#define LOOP_PERIOD_MS 10
#define DISPLAY_PERIOD_MS 100
#define SERIAL_PERIOD_MS 500

#define CAL_MPU_SAMPLES 200
#define CAL_JOY0_SAMPLES 100
#define CAL_SAMPLE_DELAY_MS 5

#define IMU_DIRECTION_DEADZONE_DEG 10.0f
#define IMU_DIRECTION_FULL_SCALE_DEG 90.0f
#define IMU_PEDAL_DEADZONE_DEG 15.0f
#define IMU_PEDAL_FULL_SCALE_DEG 45.0f

#define DEG_TO_RAD 0.0174532925f
#define RAD_TO_DEG 57.2957795f

static const char *TAG = "kakata_rc433";

typedef struct {
    int direction_pct;
    int throttle_pct;
    int brake_pct;
} control_output_t;

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static const char *mode_text(control_mode_t mode)
{
    return mode == CONTROL_MODE_IMU ? "IMU" : "JOY0";
}

static const char *on_off_text(bool value)
{
    return value ? "ON" : "OFF";
}

static float normalize_angle_delta(float angle_deg)
{
    while (angle_deg > 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static int angle_to_percent(float delta_deg, float deadzone_deg, float full_scale_deg)
{
    float abs_delta = fabsf(delta_deg);
    if (abs_delta <= deadzone_deg) {
        return 0;
    }

    if (abs_delta > full_scale_deg) {
        abs_delta = full_scale_deg;
    }

    float pct = ((abs_delta - deadzone_deg) * 100.0f) / (full_scale_deg - deadzone_deg);
    int signed_pct = (int)(pct + 0.5f);
    return delta_deg < 0.0f ? -signed_pct : signed_pct;
}

static esp_err_t i2c_bus_init(i2c_master_bus_handle_t *bus)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    return i2c_new_master_bus(&bus_cfg, bus);
}

static void draw_signed_bar(oled_t *oled, int x, int y, int w, int h, int value)
{
    value = clamp_int(value, -100, 100);
    int center_x = x + (w / 2);
    int inner_w = (w - 2) / 2;
    int fill = (abs(value) * inner_w) / 100;

    oled_draw_rect(oled, x, y, w, h, true);
    oled_draw_line(oled, center_x, y - 2, center_x, y + h + 1, true);

    if (value > 0) {
        oled_fill_rect(oled, center_x + 1, y + 2, fill, h - 4, true);
    } else if (value < 0) {
        oled_fill_rect(oled, center_x - fill, y + 2, fill, h - 4, true);
    }
}

static void draw_vertical_percent_bar(oled_t *oled, int x, int y, int w, int h, int value)
{
    value = clamp_int(value, 0, 100);
    int fill = ((h - 2) * value) / 100;

    oled_draw_rect(oled, x, y, w, h, true);
    if (fill > 0) {
        oled_fill_rect(oled, x + 2, y + h - 1 - fill, w - 4, fill, true);
    }
}

static void draw_main_screen(
    oled_t *oled,
    control_mode_t mode,
    const control_output_t *control,
    const button_snapshot_t *buttons,
    bool calibrated
)
{
    char text[24];

    oled_clear(oled);
    oled_draw_text(oled, 0, 0, mode_text(mode), true);
    oled_draw_text(oled, 48, 0, calibrated ? "CAL OK" : "CAL NO", true);

    draw_vertical_percent_bar(oled, 3, 15, 13, 37, control->brake_pct);
    draw_vertical_percent_bar(oled, 112, 15, 13, 37, control->throttle_pct);

    oled_draw_text(oled, 1, 54, "BRK", true);
    oled_draw_text(oled, 109, 54, "ACC", true);

    draw_signed_bar(oled, 26, 29, 76, 11, control->direction_pct);
    snprintf(text, sizeof(text), "D%+d%%", control->direction_pct);
    oled_draw_text(oled, 45, 18, text, true);

    snprintf(text, sizeof(text), "%d", control->brake_pct);
    oled_draw_text(oled, 2, 6, text, true);
    snprintf(text, sizeof(text), "%d", control->throttle_pct);
    oled_draw_text(oled, 109, 6, text, true);

    if (control->direction_pct == 0 && control->throttle_pct == 0 && control->brake_pct == 0) {
        oled_draw_rect(oled, 58, 42, 12, 10, true);
        oled_draw_text(oled, 61, 44, "N", true);
    }

    snprintf(
        text,
        sizeof(text),
        "B1%s B2%s B3%s",
        on_off_text(buttons->pressed[BUTTON_BTN1]),
        on_off_text(buttons->pressed[BUTTON_BTN2]),
        on_off_text(buttons->pressed[BUTTON_BTN3])
    );
    oled_draw_text(oled, 18, 56, text, true);
}

static void draw_diagnostic_screen(
    oled_t *oled,
    control_mode_t mode,
    const control_output_t *control,
    const joystick_reading_t *joy,
    const mpu6050_sample_t *imu,
    bool calibrated
)
{
    char text[24];

    oled_clear(oled);
    snprintf(text, sizeof(text), "DIAG %s", mode_text(mode));
    oled_draw_text(oled, 0, 0, text, true);

    snprintf(text, sizeof(text), "DIR %+d", control->direction_pct);
    oled_draw_text(oled, 0, 9, text, true);

    snprintf(text, sizeof(text), "ACC %d BRK %d", control->throttle_pct, control->brake_pct);
    oled_draw_text(oled, 0, 18, text, true);

    snprintf(text, sizeof(text), "J0 %d/%d", joy->joy0.mt_raw, joy->joy0.md_raw);
    oled_draw_text(oled, 0, 27, text, true);

    snprintf(text, sizeof(text), "J1 %d/%d", joy->joy1.mt_raw, joy->joy1.md_raw);
    oled_draw_text(oled, 0, 36, text, true);

    snprintf(text, sizeof(text), "R %.0f P %.0f", imu->roll_deg, imu->pitch_deg);
    oled_draw_text(oled, 0, 45, text, true);

    oled_draw_text(oled, 0, 56, calibrated ? "CAL OK" : "CAL NO", true);
}

static void fill_packet(
    rc433_control_packet_t *packet,
    control_mode_t mode,
    const control_output_t *control,
    const button_snapshot_t *buttons,
    const joystick_reading_t *joy,
    bool calibrated
)
{
    memset(packet, 0, sizeof(*packet));

    packet->mode = mode;
    packet->direction_pct = (int8_t)clamp_int(control->direction_pct, -100, 100);
    packet->throttle_pct = (uint8_t)clamp_int(control->throttle_pct, 0, 100);
    packet->brake_pct = (uint8_t)clamp_int(control->brake_pct, 0, 100);
    packet->front_buttons_mask = buttons_front_mask(buttons);
    packet->lateral_buttons_mask = buttons_lateral_mask(buttons);
    packet->joystick_buttons_mask = buttons_joystick_mask(buttons);
    packet->joy0_mt_pct = (int16_t)joy->joy0.mt_pct;
    packet->joy0_md_pct = (int16_t)joy->joy0.md_pct;
    packet->joy1_mt_pct = (int16_t)joy->joy1.mt_pct;
    packet->joy1_md_pct = (int16_t)joy->joy1.md_pct;
    packet->joy0_mt_raw = (uint16_t)clamp_int(joy->joy0.mt_raw, 0, 4095);
    packet->joy0_md_raw = (uint16_t)clamp_int(joy->joy0.md_raw, 0, 4095);
    packet->joy1_mt_raw = (uint16_t)clamp_int(joy->joy1.mt_raw, 0, 4095);
    packet->joy1_md_raw = (uint16_t)clamp_int(joy->joy1.md_raw, 0, 4095);
    packet->calibrated = calibrated;
}

static control_output_t calculate_control(
    control_mode_t mode,
    const joystick_reading_t *joy,
    const mpu6050_sample_t *imu,
    const mpu6050_calibration_t *imu_cal
)
{
    control_output_t control = {0};
    int pedal_pct = 0;

    if (mode == CONTROL_MODE_JOY0) {
        control.direction_pct = joy->joy0.md_pct;
        pedal_pct = joy->joy0.mt_pct;
    } else if (imu_cal->valid) {
        float roll_delta = normalize_angle_delta(imu->roll_deg - imu_cal->roll_center_deg);
        float pitch_delta = normalize_angle_delta(imu->pitch_deg - imu_cal->pitch_center_deg);

        control.direction_pct = angle_to_percent(
            roll_delta,
            IMU_DIRECTION_DEADZONE_DEG,
            IMU_DIRECTION_FULL_SCALE_DEG
        );
        pedal_pct = angle_to_percent(
            pitch_delta,
            IMU_PEDAL_DEADZONE_DEG,
            IMU_PEDAL_FULL_SCALE_DEG
        );
    }

    control.direction_pct = clamp_int(control.direction_pct, -100, 100);
    pedal_pct = clamp_int(pedal_pct, -100, 100);
    control.throttle_pct = pedal_pct > 0 ? pedal_pct : 0;
    control.brake_pct = pedal_pct < 0 ? -pedal_pct : 0;

    return control;
}

static esp_err_t perform_calibration(
    mpu6050_t *mpu,
    joystick_t *joystick,
    mpu6050_calibration_t *imu_cal,
    joystick_calibration_t *joy_cal,
    oled_t *oled
)
{
    ESP_LOGW(TAG, "Calibracion: mantenga quieta la placa y suelte JOY0");

    if (oled->ready) {
        oled_clear(oled);
        oled_draw_text(oled, 0, 0, "CALIBRANDO", true);
        oled_draw_text(oled, 0, 16, "NO MOVER", true);
        oled_draw_text(oled, 0, 32, "SUELTE JOY0", true);
        oled_update(oled);
    }

    float roll_sin = 0.0f;
    float roll_cos = 0.0f;
    float pitch_sin = 0.0f;
    float pitch_cos = 0.0f;
    int mpu_count = 0;

    int joy_mt_sum = 0;
    int joy_md_sum = 0;
    int joy_count = 0;

    joystick_reading_t joy = {0};

    for (int i = 0; i < CAL_MPU_SAMPLES; i++) {
        mpu6050_sample_t sample = {0};
        if (mpu6050_read(mpu, &sample) == ESP_OK) {
            float roll_rad = sample.roll_deg * DEG_TO_RAD;
            float pitch_rad = sample.pitch_deg * DEG_TO_RAD;
            roll_sin += sinf(roll_rad);
            roll_cos += cosf(roll_rad);
            pitch_sin += sinf(pitch_rad);
            pitch_cos += cosf(pitch_rad);
            mpu_count++;
        }

        if (i < CAL_JOY0_SAMPLES && joystick_read(joystick, joy_cal, &joy) == ESP_OK) {
            joy_mt_sum += joy.joy0.mt_raw;
            joy_md_sum += joy.joy0.md_raw;
            joy_count++;
        }

        vTaskDelay(pdMS_TO_TICKS(CAL_SAMPLE_DELAY_MS));
    }

    if (mpu_count > 0) {
        imu_cal->roll_center_deg = atan2f(roll_sin, roll_cos) * RAD_TO_DEG;
        imu_cal->pitch_center_deg = atan2f(pitch_sin, pitch_cos) * RAD_TO_DEG;
        imu_cal->valid = true;
    }

    if (joy_count > 0) {
        joy_cal->joy0_mt_center = joy_mt_sum / joy_count;
        joy_cal->joy0_md_center = joy_md_sum / joy_count;
    }

    ESP_LOGI(
        TAG,
        "Calibracion lista: roll=%.2f pitch=%.2f joy0_mt=%d joy0_md=%d",
        imu_cal->roll_center_deg,
        imu_cal->pitch_center_deg,
        joy_cal->joy0_mt_center,
        joy_cal->joy0_md_center
    );

    return (mpu_count > 0 && joy_count > 0) ? ESP_OK : ESP_FAIL;
}

static void quick_neutral(
    mpu6050_t *mpu,
    joystick_t *joystick,
    mpu6050_calibration_t *imu_cal,
    joystick_calibration_t *joy_cal
)
{
    mpu6050_sample_t imu = {0};
    joystick_reading_t joy = {0};

    if (mpu6050_read(mpu, &imu) == ESP_OK) {
        imu_cal->roll_center_deg = imu.roll_deg;
        imu_cal->pitch_center_deg = imu.pitch_deg;
        imu_cal->valid = true;
    }

    if (joystick_read(joystick, joy_cal, &joy) == ESP_OK) {
        joy_cal->joy0_mt_center = joy.joy0.mt_raw;
        joy_cal->joy0_md_center = joy.joy0.md_raw;
    }

    ESP_LOGI(TAG, "Neutro rapido aplicado");
}

static void log_serial_diagnostics(
    control_mode_t mode,
    bool calibrated,
    const joystick_reading_t *joy,
    const mpu6050_sample_t *imu,
    const control_output_t *control
)
{
    ESP_LOGI(
        TAG,
        "modo=%s cal=%s adc_j0=%d/%d adc_j1=%d/%d pct_j0=%d/%d pct_j1=%d/%d",
        mode_text(mode),
        calibrated ? "OK" : "NO",
        joy->joy0.mt_raw,
        joy->joy0.md_raw,
        joy->joy1.mt_raw,
        joy->joy1.md_raw,
        joy->joy0.mt_pct,
        joy->joy0.md_pct,
        joy->joy1.mt_pct,
        joy->joy1.md_pct
    );
    ESP_LOGI(
        TAG,
        "imu acc=%.2f/%.2f/%.2f gyro=%.2f/%.2f/%.2f temp=%.1f roll=%.1f pitch=%.1f",
        imu->ax_g,
        imu->ay_g,
        imu->az_g,
        imu->gx_dps,
        imu->gy_dps,
        imu->gz_dps,
        imu->temperature_c,
        imu->roll_deg,
        imu->pitch_deg
    );
    ESP_LOGI(
        TAG,
        "final dir=%d throttle=%d brake=%d",
        control->direction_pct,
        control->throttle_pct,
        control->brake_pct
    );
}

void app_main(void)
{
    ESP_LOGI(TAG, "%s iniciando en ESP32 clasico", BOARD_NAME);

    buttons_t buttons = {0};
    button_snapshot_t button_snapshot = {0};
    joystick_t joystick = {0};
    joystick_calibration_t joy_cal = {0};
    mpu6050_t mpu = {0};
    mpu6050_calibration_t imu_cal = {0};
    oled_t oled = {0};
    i2c_master_bus_handle_t i2c_bus = NULL;

    joystick_calibration_defaults(&joy_cal);
    mpu6050_calibration_defaults(&imu_cal);

    ESP_ERROR_CHECK(buttons_init(&buttons));
    ESP_ERROR_CHECK(joystick_init(&joystick));
    ESP_ERROR_CHECK(i2c_bus_init(&i2c_bus));

    esp_err_t err = oled_init(&oled, i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OLED no disponible: %s", esp_err_to_name(err));
    }

    err = mpu6050_init(&mpu, i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MPU-6050 no disponible: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "MPU-6050 detectado");
    }

    control_mode_t mode = CONTROL_MODE_IMU;
    screen_view_t screen_view = SCREEN_VIEW_MAIN;
    bool serial_diagnostics = false;

    joystick_reading_t joy = {0};
    mpu6050_sample_t imu = {0};
    control_output_t control = {0};
    rc433_control_packet_t packet = {0};
    button_event_t events[BUTTON_COUNT];

    TickType_t last_display_tick = 0;
    TickType_t last_serial_tick = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();

        int event_count = buttons_update(&buttons, &button_snapshot, events, BUTTON_COUNT);
        for (int i = 0; i < event_count; i++) {
            ESP_LOGI(TAG, "%s %s", button_name(events[i].id), events[i].pressed ? "presionado" : "liberado");

            if (!events[i].pressed) {
                continue;
            }

            switch (events[i].id) {
            case BUTTON_BTNL4:
                mode = mode == CONTROL_MODE_IMU ? CONTROL_MODE_JOY0 : CONTROL_MODE_IMU;
                ESP_LOGI(TAG, "Modo actual: %s", mode_text(mode));
                break;

            case BUTTON_BTNL3:
                perform_calibration(&mpu, &joystick, &imu_cal, &joy_cal, &oled);
                break;

            case BUTTON_BTNL2:
                quick_neutral(&mpu, &joystick, &imu_cal, &joy_cal);
                break;

            case BUTTON_BTNL1:
                screen_view = screen_view == SCREEN_VIEW_MAIN ? SCREEN_VIEW_DIAGNOSTIC : SCREEN_VIEW_MAIN;
                ESP_LOGI(TAG, "Vista: %s", screen_view == SCREEN_VIEW_MAIN ? "principal" : "diagnostico");
                break;

            case BUTTON_BTN0:
                serial_diagnostics = !serial_diagnostics;
                ESP_LOGI(TAG, "Diagnostico serial: %s", serial_diagnostics ? "ON" : "OFF");
                break;

            case BUTTON_BTN4:
                ESP_LOGI(TAG, "BTN4 confirmacion/prueba");
                break;

            default:
                break;
            }
        }

        if (joystick_read(&joystick, &joy_cal, &joy) != ESP_OK) {
            ESP_LOGW(TAG, "No se pudo leer joystick");
        }

        if (mpu.ready && mpu6050_read(&mpu, &imu) != ESP_OK) {
            ESP_LOGW(TAG, "No se pudo leer MPU-6050");
        }

        control = calculate_control(mode, &joy, &imu, &imu_cal);
        fill_packet(&packet, mode, &control, &button_snapshot, &joy, imu_cal.valid);

        if (oled.ready && (now - last_display_tick) >= pdMS_TO_TICKS(DISPLAY_PERIOD_MS)) {
            if (screen_view == SCREEN_VIEW_MAIN) {
                draw_main_screen(&oled, mode, &control, &button_snapshot, imu_cal.valid);
            } else {
                draw_diagnostic_screen(&oled, mode, &control, &joy, &imu, imu_cal.valid);
            }
            oled_update(&oled);
            last_display_tick = now;
        }

        if (serial_diagnostics && (now - last_serial_tick) >= pdMS_TO_TICKS(SERIAL_PERIOD_MS)) {
            log_serial_diagnostics(mode, imu_cal.valid, &joy, &imu, &control);
            last_serial_tick = now;
        }

        (void)packet;
        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}
