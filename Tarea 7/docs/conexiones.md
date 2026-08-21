# Conexiones

| Elemento | GPIO | Conexion |
|---|---:|---|
| PB1 | GPIO4 | Pulsador entre GPIO4 y GND |
| PB2 | GPIO5 | Pulsador entre GPIO5 y GND |
| LED | GPIO2 | GPIO2 a resistencia y LED hacia GND |
| Buzzer activo | GPIO18 | GPIO18 al positivo del buzzer, negativo a GND |

## Botones

Los botones usan pull-up interno:

- Sin presionar: lectura alta (`1`).
- Presionado: lectura baja (`0`).

Esto se logra conectando cada boton entre el GPIO y GND.

## Pines y notas para ESP32 clasico

- GPIO6 a GPIO11 no deben usarse porque pertenecen a la memoria flash de la placa.
- GPIO22, GPIO23 y GPIO25 si son validos en ESP32 clasico.
- GPIO0, GPIO2, GPIO4, GPIO5, GPIO12 y GPIO15 son pines de arranque. En este proyecto se usan GPIO2 para LED y GPIO4 para PB1, por eso no deben quedar forzados a un nivel incorrecto durante el reset.
- La placa detectada es ESP32-D0WD-V3 revision 3 con 4 MB de flash.
