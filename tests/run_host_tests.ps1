$ErrorActionPreference = 'Stop'

$project = Split-Path -Parent $PSScriptRoot
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$executable = Join-Path $env:TEMP 'master_comms_application_registers_test.exe'

& $gcc -std=c11 -Wall -Wextra -Werror "-I$project" `
    (Join-Path $PSScriptRoot 'test_application_registers.c') `
    (Join-Path $project 'application_registers.c') `
    -o $executable
if ($LASTEXITCODE -ne 0) {
    throw 'Application-register test compilation failed'
}

& $executable
if ($LASTEXITCODE -ne 0) {
    throw 'Application-register tests failed'
}

$sensorExecutable = Join-Path $env:TEMP 'master_comms_sensor_measurements_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror "-I$project" `
    (Join-Path $PSScriptRoot 'test_sensor_measurements.c') `
    (Join-Path $project 'sensor_measurements.c') `
    (Join-Path $project 'application_registers.c') `
    -o $sensorExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Sensor-measurement test compilation failed'
}

& $sensorExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Sensor-measurement tests failed'
}

$controlExecutable = Join-Path $env:TEMP 'master_comms_control_application_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror "-I$project" `
    (Join-Path $PSScriptRoot 'test_control_application.c') `
    (Join-Path $project 'control_application.c') `
    (Join-Path $project 'application_registers.c') `
    -o $controlExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Control-application test compilation failed'
}

& $controlExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Control-application tests failed'
}
