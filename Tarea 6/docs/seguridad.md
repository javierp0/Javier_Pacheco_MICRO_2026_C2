# Notas de seguridad electrica

- Este proyecto es educativo y no reemplaza un controlador industrial certificado.
- No conectes el ESP32 directamente a un motor, contactor o tension de red.
- Usa modulos de rele optoaislados o drivers adecuados para la etapa de potencia.
- Coloca fusibles, proteccion contra sobrecorriente y supresores para bobinas.
- Mantiene separadas las tierras y rutas de baja tension y alta tension.
- Usa finales de carrera NC para que una rotura de cable produzca parada segura.
- Agrega parada de emergencia fisica independiente del firmware.
- Prueba primero con LEDs antes de conectar reles reales.
- Para un porton real, solicita revision de un tecnico calificado.
