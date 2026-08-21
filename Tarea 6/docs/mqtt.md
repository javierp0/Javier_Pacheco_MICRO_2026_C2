# MQTT

## Broker

Se usa Mosquitto local. El ESP32, las herramientas de consola y la app .NET MAUI usan MQTT TCP en `1883`. El puerto `9001` queda abierto para clientes WebSocket, como la alternativa Expo.

Arranque recomendado:

```powershell
mosquitto -c .\scripts\mosquitto.conf -v
```

## Topicos

| Topico | Direccion | Contenido |
|---|---|---|
| `porton/device01/availability` | ESP32 -> app | `online` u `offline` |
| `porton/device01/state` | ESP32 -> app | Estado, error, posicion, flags |
| `porton/device01/telemetry` | ESP32 -> app | Sensores y DIP switch |
| `porton/device01/event` | ESP32 -> app | Eventos importantes |
| `porton/device01/command` | app -> ESP32 | Comandos simples |
| `porton/device01/config/set` | app -> ESP32 | JSON de configuracion |
| `porton/device01/config/state` | ESP32 -> app | Configuracion activa |
| `porton/device01/ack` | ESP32 -> app | Aceptado/rechazado |

## Comandos

```text
OPEN
CLOSE
STOP
RESET_ERROR
CALIBRATE
SAVE_CONFIG
```

## Configuracion

La app envia tiempos en segundos:

```json
{
  "auto_close_s": 30,
  "max_travel_s": 25,
  "ftc_wait_s": 4,
  "reverse_pause_s": 2,
  "auto_close_sw": true,
  "maintenance_sw": false
}
```

El firmware convierte internamente a milisegundos cuando necesita comparar tiempos.

## Pruebas manuales rapidas

Suscribirse a todo:

```powershell
.\scripts\mqtt_watch.ps1 -HostName 127.0.0.1
```

Enviar abrir:

```powershell
.\scripts\mqtt_command.ps1 -Command OPEN
```

Enviar configuracion:

```powershell
.\scripts\mqtt_config_set.ps1 -AutoCloseS 20 -MaxTravelS 25 -FtcWaitS 5 -ReversePauseS 2
```
