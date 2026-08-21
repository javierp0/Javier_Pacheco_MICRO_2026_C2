idf_component_register(
    SRCS
        "main.c"
        "buttons.c"
        "joystick.c"
        "mpu6050.c"
        "oled.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_adc
        esp_driver_gpio
        esp_driver_i2c
        esp_timer
        freertos
        log
)
