# Guia MQTTX

## Conexion

Crear una conexion en MQTTX con estos datos:

| Campo | Valor |
|---|---|
| Protocol | `mqtt://` |
| Host | `broker.hivemq.com` |
| Port | `1883` |
| Username | vacio |
| Password | vacio |
| SSL/TLS | desactivado |

## Suscripcion

Para ver todos los mensajes del ESP32:

```text
Javier Pacheco/tarea8/reaccion/#
```

## Topicos

| Topico | Tipo |
|---|---|
| `Javier Pacheco/tarea8/reaccion/estado` | Texto |
| `Javier Pacheco/tarea8/reaccion/evento` | JSON |
| `Javier Pacheco/tarea8/reaccion/resultado` | JSON |
| `Javier Pacheco/tarea8/reaccion/error` | JSON |
| `Javier Pacheco/tarea8/reaccion/ip` | JSON |

## Flujo esperado en MQTTX

1. En `estado` aparece `Listo: presiona y manten PB1`.
2. Al presionar PB1 aparece un evento `pb1_presionado_inicio_ensayo`.
3. Cuando el LED y el buzzer se activan aparece `senal_led_buzzer`.
4. Al soltar PB1 aparece `pb1_suelto_inicio_conteo`.
5. Al presionar PB2 aparece el resultado final.
6. Si el usuario se adelanta aparece un mensaje en `error`.
