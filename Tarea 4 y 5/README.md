# Tarea 4 y 5 - KAKATA RC433

Firmware ESP-IDF para un control RC con ESP32 clasico ESP32-D0WD-V3, dos joysticks analogicos,
botones activos en bajo, MPU-6050 y pantalla OLED I2C 128x64.

## Hardware

- Target: `esp32`
- Flash: 4 MB
- Placa esperada: ESP32 DevKit / ESP32-WROOM-32 / ESP32-D0WD-V3 revision 3
- I2C compartido:
  - SDA: GPIO21
  - SCL: GPIO22
- OLED SSD1306 128x64:
  - Direccion: `0x3C`
- MPU-6050:
  - Direccion: `0x68`
- Botones:
  - Activos en bajo
  - Pull-up externo
  - Pull-up interno deshabilitado
- LEDs:
  - No se usan

Todos los pines estan en `main/board_config.h`.

## Pinout para ESP32 clasico

| Funcion | GPIO | Nota |
|---|---:|---|
| I2C SDA | 21 | OLED y MPU-6050 |
| I2C SCL | 22 | OLED y MPU-6050 |
| JOY0 MT | 36 | ADC1_CH0 |
| JOY0 MD | 39 | ADC1_CH3 |
| JOY1 MT | 32 | ADC1_CH4 |
| JOY1 MD | 33 | ADC1_CH5 |
| JOY0 BTN | 25 | Entrada digital |
| JOY1 BTN | 26 | Entrada digital |
| BTN0 | 13 | Entrada digital |
| BTN1 | 14 | Entrada digital |
| BTN2 | 16 | Entrada digital |
| BTN3 | 17 | Entrada digital |
| BTN4 | 18 | Entrada digital |
| BTNL1 | 19 | Entrada digital |
| BTNL2 | 23 | Entrada digital |
| BTNL3 | 27 | Entrada digital |
| BTNL4 | 35 | Entrada digital, requiere pull-up externo |

No se usan GPIO6-GPIO11 porque pertenecen a la memoria flash del ESP32 clasico. Tampoco se usan GPIO40-GPIO46 porque no existen en el ESP32-D0WD-V3.

## Botones de funcion

- `BTNL4`: cambia entre modo `IMU` y modo `JOY0`.
- `BTNL3`: calibra centro de MPU-6050 y JOY0.
- `BTNL2`: aplica neutro rapido usando la posicion actual.
- `BTNL1`: cambia entre pantalla principal y diagnostico.
- `BTN0`: activa/desactiva diagnostico por monitor serial.
- `BTN4`: boton de prueba/confirmacion.
- `BTN1`, `BTN2`, `BTN3`: se muestran en pantalla como `ON/OFF`.

## Control

Modo `IMU`:

- Direccion por inclinacion lateral.
- Acelerador/freno por inclinacion adelante/atras.
- Zona neutra direccion: `+/-10 grados`.
- Direccion al 100%: `90 grados`.
- Zona neutra pedal: `+/-15 grados`.
- Pedal al 100%: `45 grados`.

Modo `JOY0`:

- Direccion por `JOY0 MD`.
- Acelerador/freno por `JOY0 MT`.
- Valor positivo acelera.
- Valor negativo frena.
- Zona muerta de JOY0: `5%`, con reescalado hasta `100%`.

## Pantalla

La vista principal usa un diseno diferente al de cuatro triangulos:

- Barra horizontal central para direccion.
- Barra vertical izquierda para freno.
- Barra vertical derecha para acelerador.
- Indicador `CAL OK` / `CAL NO`.
- Estados `B1`, `B2`, `B3`.
- Marca `N` cuando todo esta en neutro.

La vista de diagnostico muestra modo, direccion, acelerador, freno, ADC de JOY0
y JOY1, angulos del MPU y calibracion.

## Estructura futura para Bluetooth

El firmware ya llena `rc433_control_packet_t` en `main/app_types.h`. Esa
estructura incluye modo actual, direccion, acelerador, freno, botones frontales,
botones laterales, botones de joysticks, valores de JOY0, valores de JOY1 y
estado de calibracion.

## Compilar

Desde esta carpeta:

```bash
idf.py set-target esp32
idf.py build
```

Para grabar:

```bash
idf.py -p COM3 flash monitor
```

Cambia `COM3` por el puerto real del ESP32 clasico.

En VS Code usa la tarea `ESP-IDF: Build`. No ejecutes `ninja.exe` directamente,
porque `build.ninja` solo existe despues de configurar el proyecto con ESP-IDF.
