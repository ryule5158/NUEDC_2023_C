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

$sysConfigGui = Join-Path $SysConfigToolRoot "sysconfig_gui.bat"
$productJson = Join-Path $Mspm0SdkRoot ".metadata\product.json"
$configuration = Join-Path $projectRoot "SysConfig\MSP_LITO_G3507_Board.syscfg"
$generatedDirectory = Join-Path $projectRoot "SysConfig\Generated"

if (-not (Test-Path -LiteralPath $sysConfigGui)) {
    throw "TI SysConfig GUI not found: $sysConfigGui"
}
if (-not (Test-Path -LiteralPath $productJson)) {
    throw "MSPM0 SDK product metadata not found: $productJson"
}
if (-not (Test-Path -LiteralPath $configuration)) {
    throw "SysConfig project not found: $configuration"
}

New-Item -ItemType Directory -Path $generatedDirectory -Force | Out-Null

& $sysConfigGui `
    --product $productJson `
    --compiler keil `
    --output $generatedDirectory `
    $configuration
