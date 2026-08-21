# Guia de pruebas sin encoder ni OLED

El encoder y el OLED estan reservados y no son necesarios para validar el sistema principal.

## Banco de prueba recomendado

- ESP32 DevKit con target `esp32`.
- Tres pulsadores para abrir, cerrar y stop.
- Tres interruptores para simular final abierto, final cerrado y FTC.
- Cuatro DIP switch.
- Dos LEDs o modulo de reles para abrir/cerrar.
- Un LED o buzzer en GPIO27.
- Broker Mosquitto local.

## Casos funcionales

| Caso | Pasos | Resultado esperado |
|---|---|---|
| Arranque sin limites activos | Encender ESP32 con finales en alto | Estado `DETENIDO` |
| Abrir | Enviar `OPEN`, activar final abierto | Rele abrir ON, luego `ABIERTO` y rele OFF |
| Cerrar | Enviar `CLOSE`, activar final cerrado | Rele cerrar ON, luego `CERRADO` y rele OFF |
| Stop | Enviar `STOP` durante movimiento | Estado `DETENIDO`, ambos reles OFF |
| FTC durante cierre | Bloquear FTC durante `CERRANDO` | Estado `PAUSADO_POR_FTC`, rele cerrar OFF |
| Timeout | No activar ningun final hasta pasar tiempo maximo | Estado `ERROR`, reles OFF |
| Reset error | Enviar `RESET_ERROR` | Vuelve a `DETENIDO` si sensores estan coherentes |
| Mantenimiento | Activar DIP4 e intentar auto-cierre | El movimiento automatico queda bloqueado |

## Pruebas automatizadas educativas

```powershell
python .\tests\test_gate_fsm.py
```

Si `python` no esta en el PATH:

```powershell
& "C:\Espressif\tools\python\v6.0\venv\Scripts\python.exe" .\tests\test_gate_fsm.py
```

Estas pruebas simulan la logica esperada de la maquina de estados y sirven para explicar los escenarios principales antes de probar con hardware.
