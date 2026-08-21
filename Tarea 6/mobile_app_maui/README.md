# App MAUI - Porton Seguro IoT

App movil en .NET MAUI para controlar el ESP32 por MQTT.

## Funciones incluidas

- Conexion al broker Mosquitto local por MQTT TCP.
- Pantalla Inicio con estado, error, progreso, comandos abrir/cerrar/stop y sensores.
- Pantalla Configuracion con IP, puerto, usuario opcional, tiempos en segundos y switches.
- Pantalla Eventos con historial de eventos, ACK y mensajes del broker.
- Persistencia local usando `Preferences`.
- Sin credenciales reales dentro del repositorio.

## Requisitos

- Visual Studio 2022 con carga de trabajo `.NET Multi-platform App UI development`.
- .NET 8 SDK.
- Telefono Android en la misma red que el broker.
- Mosquitto escuchando en puerto `1883`.

## Ejecutar

Abre `PortonSeguroIoT.Maui.csproj` en Visual Studio, selecciona Android y ejecuta.

En la pantalla Configuracion coloca:

- IP del broker: IP de la PC donde corre Mosquitto.
- Puerto MQTT: `1883`.
- Usuario/contrasena: dejar vacio si Mosquitto esta en modo anonimo.

Luego toca `Conectar`.
