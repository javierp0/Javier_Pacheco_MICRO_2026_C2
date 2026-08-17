# Tarea 3 - ESP32 clasico, maquina de estados, WiFi y MQTT

Proyecto ESP-IDF para un ESP32 clasico con target `esp32`.

Nota para ESP-IDF 6: MQTT ya no viene como componente interno completo de
ESP-IDF. Por eso este proyecto incluye `main/idf_component.yml` con la
dependencia administrada `espressif/mqtt`.

Segun la placa indicada:

- Target: `esp32`
- Flash esperada: 4 MB
- LED por defecto: `GPIO2`
- Boton por defecto: `GPIO0` (`BOOT`, activo en bajo)

Si tu placa no usa esos pines, cambialos en `sdkconfig.defaults` o con `idf.py menuconfig`.

## Funcionamiento

El programa tiene una maquina de estado de dos estados:

- `STATE_LED_OFF`: LED apagado.
- `STATE_LED_ON`: LED encendido.

Eventos que cambian el estado:

- Pulsar el boton cambia entre `ON` y `OFF`.
- MQTT con payload `ON` enciende el LED.
- MQTT con payload `OFF` apaga el LED.
- MQTT con payload `TOGGLE` cambia al estado contrario.

## Topics MQTT

Con el topic base por defecto `javier_pacheco_micro_2026_c2/tarea3`:

- Comandos: `javier_pacheco_micro_2026_c2/tarea3/cmd`
- Estado: `javier_pacheco_micro_2026_c2/tarea3/estado`
- Disponibilidad: `javier_pacheco_micro_2026_c2/tarea3/disponible`

El ESP32 publica `ON` u `OFF` en el topic de estado.

Si usas un broker publico y ves mensajes que no son tuyos, cambia
`CONFIG_TAREA3_TOPIC_BASE` por un nombre mas unico.

## Configuracion WiFi y MQTT

Edita `sdkconfig.defaults`:

```ini
CONFIG_TAREA3_WIFI_SSID="NOMBRE_DE_TU_WIFI"
CONFIG_TAREA3_WIFI_PASSWORD="CLAVE_DE_TU_WIFI"
CONFIG_TAREA3_MQTT_URI="mqtt://broker.hivemq.com:1883"
CONFIG_TAREA3_TOPIC_BASE="javier_pacheco_micro_2026_c2/tarea3"
CONFIG_TAREA3_LED_GPIO=2
CONFIG_TAREA3_BUTTON_GPIO=0
```

Tambien puedes usar:

```bash
idf.py menuconfig
```

Ruta del menu:

```text
Tarea 3 - ESP32 WiFi MQTT
```

## Compilar y grabar

Desde esta carpeta:

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Cambia `COM3` por el puerto de tu ESP32.

Si usas VS Code, no ejecutes `ninja.exe` directamente. Usa la tarea
`ESP-IDF: Build` o el boton de build de la extension ESP-IDF. Si aparece
`loading 'build.ninja': The system cannot find the file specified`, primero
ejecuta la tarea `ESP-IDF: Full Clean` y luego `ESP-IDF: Build`.

## App MQTT en el celular

Usa cualquier app de cliente MQTT en el celular. La configuracion debe tener estos valores:

```text
Broker/Host: broker.hivemq.com
Puerto: 1883
Usuario: vacio
Clave: vacio
Client ID: celular_tarea3_javier
```

Crea una suscripcion:

```text
Topic: javier_pacheco_micro_2026_c2/tarea3/estado
QoS: 1
```

Crea tres botones de publicacion:

```text
Boton ON
Topic: javier_pacheco_micro_2026_c2/tarea3/cmd
Payload: ON

Boton OFF
Topic: javier_pacheco_micro_2026_c2/tarea3/cmd
Payload: OFF

Boton TOGGLE
Topic: javier_pacheco_micro_2026_c2/tarea3/cmd
Payload: TOGGLE
```

Cuando pulses los botones en el celular, el ESP32 debe cambiar el LED y publicar el estado nuevo.

## Cableado sugerido

Si usas el LED integrado y el boton `BOOT`, no necesitas cableado extra.

Si usas un boton externo:

- Un lado del boton a `GPIO0` o al GPIO configurado.
- El otro lado del boton a `GND`.
- El programa usa resistencia pull-up interna.

Nota: no mantengas presionado `BOOT/GPIO0` al reiniciar la placa, porque el ESP32 puede entrar en modo de descarga.
