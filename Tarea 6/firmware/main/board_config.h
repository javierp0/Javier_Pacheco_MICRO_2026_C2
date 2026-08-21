#pragma once

#include "driver/gpio.h"

#define BOARD_NAME "Porton Seguro IoT"
#define BOARD_ID "device01"

#define PIN_LIMIT_OPEN GPIO_NUM_34
#define PIN_LIMIT_CLOSED GPIO_NUM_35
#define PIN_FTC GPIO_NUM_36

#define PIN_BUTTON_OPEN GPIO_NUM_18
#define PIN_BUTTON_CLOSE GPIO_NUM_19
#define PIN_BUTTON_STOP GPIO_NUM_23

#define PIN_DIP1 GPIO_NUM_2
#define PIN_DIP2 GPIO_NUM_4
#define PIN_DIP3 GPIO_NUM_16
#define PIN_DIP4 GPIO_NUM_17

#define PIN_RELAY_OPEN GPIO_NUM_25
#define PIN_RELAY_CLOSE GPIO_NUM_26
#define PIN_BUZZER GPIO_NUM_27
#define PIN_STATUS_LED GPIO_NUM_14

#define PIN_ENCODER_A GPIO_NUM_32
#define PIN_ENCODER_B GPIO_NUM_33
#define PIN_OLED_SDA GPIO_NUM_21
#define PIN_OLED_SCL GPIO_NUM_22

#define INPUT_ACTIVE_LOW 0
#define RELAY_ACTIVE_LEVEL 1
#define BUZZER_ACTIVE_LEVEL 1
#define LED_ACTIVE_LEVEL 1

#define MQTT_TOPIC_AVAILABILITY "porton/device01/availability"
#define MQTT_TOPIC_STATE "porton/device01/state"
#define MQTT_TOPIC_TELEMETRY "porton/device01/telemetry"
#define MQTT_TOPIC_EVENT "porton/device01/event"
#define MQTT_TOPIC_COMMAND "porton/device01/command"
#define MQTT_TOPIC_CONFIG_SET "porton/device01/config/set"
#define MQTT_TOPIC_CONFIG_STATE "porton/device01/config/state"
#define MQTT_TOPIC_ACK "porton/device01/ack"

#define DEFAULT_AUTO_CLOSE_S 20
#define DEFAULT_MAX_TRAVEL_S 25
#define DEFAULT_FTC_WAIT_S 5
#define DEFAULT_REVERSE_PAUSE_S 2
#define DEFAULT_AUTO_CLOSE_SW false
#define DEFAULT_MAINTENANCE_SW false
