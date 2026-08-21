$ErrorActionPreference = "Stop"

$project = Split-Path -Parent $PSScriptRoot

$env:IDF_PATH = "C:\esp\v6.0\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0\venv"
$env:ESP_IDF_VERSION = "6.0.0"
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;C:\Program Files\Git\cmd;$env:PATH"

Push-Location $project
try {
    & "C:\Espressif\tools\python\v6.0\venv\Scripts\python.exe" "C:\esp\v6.0\esp-idf\tools\idf.py" -p COM10 monitor
}
finally {
    Pop-Location
}
