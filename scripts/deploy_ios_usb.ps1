# SPDX-License-Identifier: GPL-2.0-or-later
param(
    [Parameter(Mandatory = $true)][string]$AppPath,
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$toolPython = Join-Path $projectRoot 'build\ios-log-tools\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $toolPython)) { $toolPython = (Get-Command python -ErrorAction Stop).Source }
$appDirectory = (Resolve-Path -LiteralPath $AppPath).Path
$deploymentScript = Join-Path $PSScriptRoot 'deploy_ios_usb.py'
if ($Install) { & $toolPython $deploymentScript --app $appDirectory --install }
else { & $toolPython $deploymentScript --app $appDirectory --check }
exit $LASTEXITCODE
