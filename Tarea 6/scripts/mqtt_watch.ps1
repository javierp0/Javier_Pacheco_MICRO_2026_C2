param(
    [string]$HostName = "127.0.0.1",
    [int]$Port = 1883,
    [string]$BaseTopic = "porton/device01"
)

$ErrorActionPreference = "Stop"

Write-Host "Escuchando $BaseTopic/# en ${HostName}:$Port"
mosquitto_sub -h $HostName -p $Port -t "$BaseTopic/#" -v
