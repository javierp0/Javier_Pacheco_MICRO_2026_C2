param(
    [int]$AutoCloseS = 30,
    [int]$MaxTravelS = 25,
    [int]$FtcWaitS = 4,
    [int]$ReversePauseS = 2,
    [bool]$AutoCloseSw = $true,
    [bool]$MaintenanceSw = $false,
    [string]$HostName = "127.0.0.1",
    [int]$Port = 1883,
    [string]$BaseTopic = "porton/device01"
)

$ErrorActionPreference = "Stop"

$payload = @{
    auto_close_s = $AutoCloseS
    max_travel_s = $MaxTravelS
    ftc_wait_s = $FtcWaitS
    reverse_pause_s = $ReversePauseS
    auto_close_sw = $AutoCloseSw
    maintenance_sw = $MaintenanceSw
} | ConvertTo-Json -Compress

$topic = "$BaseTopic/config/set"
Write-Host "Publicando configuracion en $topic"
Write-Host $payload
mosquitto_pub -h $HostName -p $Port -t $topic -m $payload
