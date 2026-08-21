#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID "CAMBIA_TU_WIFI"
#define WIFI_PASSWORD "CAMBIA_TU_CLAVE"
#define MQTT_BROKER_URI "mqtt://192.168.1.50:1883"
#define MQTT_CLIENT_ID "porton-seguro-device01"
#endif
