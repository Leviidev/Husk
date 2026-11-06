# SPDX-License-Identifier: GPL-2.0-or-later
param([switch]$Setup, [switch]$Once, [switch]$Background, [switch]$Stop)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$python = Join-Path $projectRoot 'build\ios-log-tools\Scripts\python.exe'
$outputRoot = Join-Path $projectRoot 'device-logs'
$latest = Join-Path $outputRoot 'latest'
New-Item -ItemType Directory -Path $latest -Force | Out-Null
if ($Stop) {
    [IO.File]::WriteAllText((Join-Path $latest 'STOP'), 'stop')
    Write-Output 'Stop requested; the collector will exit after the current USB read.'
    exit 0
}
if ($Setup) {
    if (-not (Test-Path -LiteralPath $python)) {
        & py -3.12 -m venv (Join-Path $projectRoot 'build\ios-log-tools')
        if ($LASTEXITCODE -ne 0) { throw 'Python 3.12 is required' }
    }
    & $python -m pip install --no-cache-dir 'pymobiledevice3==11.50.0'
    if ($LASTEXITCODE -ne 0) { throw 'USB dependency installation failed' }
    Write-Output 'USB log tools ready. Run scripts\ios_logs.ps1 -Background to start.'
    exit 0
}
if (-not (Test-Path -LiteralPath $python)) { throw 'Run scripts\ios_logs.ps1 -Setup first' }
$scriptPath = Join-Path $PSScriptRoot 'capture_ios_logs.py'
$statusPath = Join-Path $latest 'status.json'
if (Test-Path -LiteralPath $statusPath) {
    $previous = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
    if ($previous.collectorPID) {
        $running = Get-CimInstance Win32_Process -Filter "ProcessId=$($previous.collectorPID)" -ErrorAction SilentlyContinue
        if ($running -and $running.CommandLine -like '*capture_ios_logs.py*') { throw 'A USB collector is already running' }
    }
}
$stopFile = Join-Path $latest 'STOP'
if (Test-Path -LiteralPath $stopFile) { Remove-Item -LiteralPath $stopFile }
if ($Background) {
    if ($Once) { throw 'Use -Once or -Background separately' }
    $arguments = @('-u', ('"'+$scriptPath+'"'), '--output', ('"'+$latest+'"'))
    $process = Start-Process -FilePath $python -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $outputRoot 'collector.out.log') -RedirectStandardError (Join-Path $outputRoot 'collector.err.log')
    Write-Output "USB collector started (PID $($process.Id)). Logs: $latest"
} elseif ($Once) { & $python $scriptPath --once --output $latest }
else { & $python -u $scriptPath --output $latest }
