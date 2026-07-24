param(
    [string]$SysConfigToolRoot = $env:NUEDC_TI_SYSCONFIG_ROOT,
    [string]$Mspm0SdkRoot = $env:NUEDC_TI_MSPM0_SDK_ROOT
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if ([string]::IsNullOrWhiteSpace($SysConfigToolRoot)) {
    $SysConfigToolRoot = "D:\ti\sysconfig_1.26.2"
}
if ([string]::IsNullOrWhiteSpace($Mspm0SdkRoot)) {
    $Mspm0SdkRoot = "D:\ti\mspm0_sdk_2_10_00_04"
}

$sysConfigCli = Join-Path $SysConfigToolRoot "sysconfig_cli.bat"
$productJson = Join-Path $Mspm0SdkRoot ".metadata\product.json"
$configuration = Join-Path $projectRoot "SysConfig\MSP_LITO_G3507_Board.syscfg"
$generatedDirectory = Join-Path $projectRoot "SysConfig\Generated"
$generatedSource = Join-Path $generatedDirectory "ti_msp_dl_config.c"
$generatedHeader = Join-Path $generatedDirectory "ti_msp_dl_config.h"
$eventGraph = Join-Path $generatedDirectory "Event.dot"

if (-not (Test-Path -LiteralPath $sysConfigCli)) {
    throw "TI SysConfig CLI not found: $sysConfigCli"
}
if (-not (Test-Path -LiteralPath $productJson)) {
    throw "MSPM0 SDK product metadata not found: $productJson"
}
if (-not (Test-Path -LiteralPath $configuration)) {
    throw "SysConfig project not found: $configuration"
}

New-Item -ItemType Directory -Path $generatedDirectory -Force | Out-Null

& $sysConfigCli `
    --product $productJson `
    --compiler keil `
    --script $configuration `
    --output $generatedDirectory `
    --treatWarningsAsErrors

if ($LASTEXITCODE -ne 0) {
    throw "TI SysConfig generation failed with exit code $LASTEXITCODE"
}
if ((-not (Test-Path -LiteralPath $generatedSource)) -or
    (-not (Test-Path -LiteralPath $generatedHeader))) {
    throw "TI SysConfig did not create ti_msp_dl_config.c/.h"
}

# Event.dot is an auxiliary event-fabric graph; the firmware build does not use it.
if (Test-Path -LiteralPath $eventGraph) {
    Remove-Item -LiteralPath $eventGraph -Force
}

Write-Output "SysConfig generated:"
Write-Output "  $generatedSource"
Write-Output "  $generatedHeader"
