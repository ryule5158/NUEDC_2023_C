param(
    [switch]$InstallDevicePack,
    [switch]$DownloadSysConfig,
    [switch]$InstallSdkFromGit,
    [string]$ToolsRoot = "D:\ti",
    [string]$KeilRoot = "C:\Keil_v5"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$keilRoot = $KeilRoot
$uv4 = Join-Path $keilRoot "UV4\UV4.exe"
$armclang = Join-Path $keilRoot "ARM\ARMCLANG\bin\armclang.exe"
$packVersion = "1.3.1"
$packName = "TexasInstruments.MSPM0G1X0X_G3X0X_DFP"
$packFolder = "MSPM0G1X0X_G3X0X_DFP"
$packDir = Join-Path $keilRoot "ARM\PACK\TexasInstruments\$packFolder\$packVersion"
$packDPath = Join-Path $ToolsRoot "keil_packs\TexasInstruments\$packFolder\$packVersion"
$packPdsc = Join-Path $packDir "$packName.pdsc"
$armUserPackVendor = Join-Path $env:LOCALAPPDATA "Arm\Packs\TexasInstruments"
$sysconfigCli = Join-Path $ToolsRoot "sysconfig_1.26.2\sysconfig_cli.bat"
$sdkRoot = Join-Path $ToolsRoot "mspm0_sdk_2_10_00_04"
$downloads = Join-Path $ToolsRoot "downloads"

function Show-Check($name, $ok, $detail) {
    $mark = if ($ok) { "OK " } else { "MISS" }
    Write-Host ("[{0}] {1} - {2}" -f $mark, $name, $detail)
}

$toolsDriveName = ([System.IO.Path]::GetPathRoot($ToolsRoot)).Substring(0, 1)
$drive = Get-PSDrive -Name $toolsDriveName
Show-Check "Keil uVision" (Test-Path $uv4) $uv4
if (Test-Path $uv4) {
    $uv = Get-Item $uv4
    Write-Host ("      Version: {0}" -f $uv.VersionInfo.FileVersion)
}

Show-Check "Arm Compiler 6" (Test-Path $armclang) $armclang
Show-Check "TI MSPM0G1X0X/G3X0X DFP $packVersion" (Test-Path $packPdsc) $packPdsc
if (Test-Path $packDir) {
    $packItem = Get-Item -LiteralPath $packDir -Force
    if ($packItem.LinkType -eq "Junction") {
        Write-Host ("      Junction target: {0}" -f (($packItem.Target) -join ";"))
    }
}
Show-Check "Arm user Pack vendor junction" (Test-Path $armUserPackVendor) $armUserPackVendor
if (Test-Path $armUserPackVendor) {
    $armUserPackItem = Get-Item -LiteralPath $armUserPackVendor -Force
    if ($armUserPackItem.LinkType -eq "Junction") {
        Write-Host ("      Junction target: {0}" -f (($armUserPackItem.Target) -join ";"))
    }
}
Show-Check "TI SysConfig 1.26.2" (Test-Path $sysconfigCli) $sysconfigCli
Show-Check "MSPM0 SDK 2.10.00.04 root" (Test-Path (Join-Path $sdkRoot ".metadata\product.json")) $sdkRoot
Write-Host ("[INFO] {0}: free space: {1:N1} MB" -f $toolsDriveName, ($drive.Free / 1MB))

if ($InstallDevicePack -and -not (Test-Path $packPdsc)) {
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    $packFile = Join-Path $downloads "$packName.$packVersion.pack"
    $zipFile = Join-Path $downloads "$packName.$packVersion.zip"
    $packUrl = "https://software-dl.ti.com/msp430/esd/MSPM0-CMSIS/MSPM0G1X0X_G3X0X/latest/exports/$packName.$packVersion.pack"

    Write-Host "[INFO] Downloading $packUrl"
    curl.exe -L --retry 5 --retry-delay 5 -o $packFile $packUrl
    Copy-Item -LiteralPath $packFile -Destination $zipFile -Force
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $packDPath) | Out-Null
    Expand-Archive -LiteralPath $zipFile -DestinationPath $packDPath -Force
    if (-not (Test-Path $packDir)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $packDir) | Out-Null
        New-Item -ItemType Junction -Path $packDir -Target $packDPath | Out-Null
    }
    if (-not (Test-Path $armUserPackVendor)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $armUserPackVendor) | Out-Null
        New-Item -ItemType Junction -Path $armUserPackVendor -Target (Split-Path -Parent $packDPath) | Out-Null
    }
    Show-Check "TI MSPM0G1X0X/G3X0X DFP $packVersion" (Test-Path $packPdsc) $packPdsc
}

if ($DownloadSysConfig -and -not (Test-Path $sysconfigCli)) {
    if ($drive.Free -lt 300MB) {
        Write-Warning "Not enough free space to download/install SysConfig. Free at least 300 MB on $toolsDriveName`: first."
    } else {
        New-Item -ItemType Directory -Force -Path $downloads | Out-Null
        $installer = Join-Path $downloads "sysconfig-1.26.2_4477-setup.exe"
        $url = "https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.26.2.4477/sysconfig-1.26.2_4477-setup.exe"
        Write-Host "[INFO] Downloading $url"
        curl.exe -L --retry 5 --retry-delay 5 -o $installer $url
        Write-Host "[INFO] Installing SysConfig to $ToolsRoot\sysconfig_1.26.2"
        & $installer --mode unattended --prefix (Join-Path $ToolsRoot "sysconfig_1.26.2")
    }
}

if ($InstallSdkFromGit -and -not (Test-Path (Join-Path $sdkRoot ".metadata\product.json"))) {
    New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null
    Write-Host "[INFO] Cloning MSPM0 SDK to $sdkRoot"
    git clone --depth 1 --filter=blob:none --sparse https://github.com/TexasInstruments/mspm0-sdk.git $sdkRoot
    Push-Location $sdkRoot
    try {
        git sparse-checkout set .metadata source/ti/devices/msp/m0p source/ti/devices/msp/peripherals source/ti/driverlib source/ti/project_config source/ti/clockTree source/ti/tinyusb_meta source/third_party/CMSIS/Core/Include tools/keil examples/nortos/LP_MSPM0G3507/driverlib/empty examples/nortos/LP_MSPM0G3507/driverlib/gpio_toggle_output examples/nortos/LP_MSPM0G3507/driverlib/adc12_max_freq_dma examples/nortos/LP_MSPM0G3507/driverlib/spi_controller_multibyte_fifo_poll examples/nortos/LP_MSPM0G3507/driverlib/uart_rw_multibyte_fifo_poll
    } finally {
        Pop-Location
    }
}

$sdkSyscfgBat = Join-Path $sdkRoot "tools\keil\syscfg.bat"
if ((Test-Path $sdkSyscfgBat) -and (Test-Path $sysconfigCli)) {
    $content = Get-Content -LiteralPath $sdkSyscfgBat -Raw
    $escaped = $sysconfigCli.Replace("\", "\\")
    $content = $content -replace 'set SYSCFG_PATH="[^"]+"', ('set SYSCFG_PATH="' + $sysconfigCli + '"')
    Set-Content -LiteralPath $sdkSyscfgBat -Value $content -Encoding ASCII
}

Write-Host ""
Write-Host "Build command:"
Write-Host "  powershell -ExecutionPolicy Bypass -File `"$projectRoot\build_keil.ps1`""
