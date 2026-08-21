# Pinout completo

## Entradas

| Senal | GPIO | Activo | Pull interno | Recomendacion |
|---|---:|---|---|---|
| Final abierto NC | 34 | Bajo | No disponible | Resistencia externa obligatoria |
| Final cerrado NC | 35 | Bajo | No disponible | Resistencia externa obligatoria |
| Fotocelda FTC | 36 | Bajo | No disponible | Resistencia externa obligatoria |
| Boton abrir | 18 | Bajo | Pull-up | Pulsador a GND |
| Boton cerrar | 19 | Bajo | Pull-up | Pulsador a GND |
| Boton stop | 23 | Bajo | Pull-up | Pulsador a GND |
| DIP1 encoder | 2 | Bajo | Pull-up | Configuracion fisica |
| DIP2 FTC invertir | 4 | Bajo | Pull-up | Configuracion fisica |
| DIP3 auto-cierre | 16 | Bajo | Pull-up | Configuracion fisica |
| DIP4 mantenimiento | 17 | Bajo | Pull-up | Bloquea automatismos |

## Salidas

| Senal | GPIO | Activo | Nota |
|---|---:|---|---|
| Rele abrir | 25 | Alto | Usar modulo de rele o driver optoaislado |
| Rele cerrar | 26 | Alto | Nunca debe activarse junto con rele abrir |
| Buzzer | 27 | Alto | Se apaga automaticamente despues de 15 s de error |
| LED estado | 14 | Alto | Encendido solo en detenido, abriendo o cerrando |

## Reservado

| Funcion | GPIO | Estado actual |
|---|---:|---|
| Encoder A | 32 | Configurado como entrada, no usado |
| Encoder B | 33 | Configurado como entrada, no usado |
| OLED SDA | 21 | Reservado |
| OLED SCL | 22 | Reservado |

## Nota sobre finales NC

Los finales de carrera normalmente cerrados entregan alto en reposo. Cuando se activan, o si el cable se rompe, el ESP32 recibe bajo. El firmware toma ese bajo como limite activo y detiene el movimiento para fallar de forma segura.
