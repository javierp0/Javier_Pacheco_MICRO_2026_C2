param(
    [ValidateSet("OPEN", "CLOSE", "STOP", "RESET_ERROR", "CALIBRATE", "SAVE_CONFIG")]
    [string]$Command = "OPEN",
    [string]$HostName = "127.0.0.1",
    [int]$Port = 1883,
    [string]$BaseTopic = "porton/device01"
)

$ErrorActionPreference = "Stop"

$topic = "$BaseTopic/command"
Write-Host "Publicando $Command en $topic"
mosquitto_pub -h $HostName -p $Port -t $topic -m $Command
