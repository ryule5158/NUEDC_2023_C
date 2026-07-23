$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$keilRoot = Join-Path $projectRoot "MDK-ARM"
$uv4 = "C:\Keil_v5\UV4\UV4.exe"
$project = Join-Path $keilRoot "STM32H743.uvprojx"
$log = Join-Path $keilRoot "build.log"
$output = Join-Path $projectRoot "output"

if (-not (Test-Path -LiteralPath $uv4)) {
    throw "Keil uVision not found: $uv4"
}

if (-not (Test-Path -LiteralPath $project)) {
    throw "Keil project not found: $project"
}

Push-Location $keilRoot
try {
    if (Test-Path -LiteralPath $log) {
        Remove-Item -LiteralPath $log
    }
    Start-Process -FilePath $uv4 `
        -ArgumentList '-r "STM32H743.uvprojx" -j0 -o "build.log"' `
        -WindowStyle Hidden -Wait | Out-Null
    $buildResult = Get-Content -Raw -LiteralPath $log
    Write-Output $buildResult
    if ($buildResult -notmatch '0 Error\(s\), 0 Warning\(s\)') {
        throw "Keil rebuild failed or produced warnings. See: $log"
    }
} finally {
    Pop-Location
}

# 保存可直接烧录和复核的最终产物。
New-Item -ItemType Directory -Path $output -Force | Out-Null
$artifacts = @(
    @{ Source = "MDK-ARM\STM32H743\STM32H743.axf"; Name = "STM32H743.axf" },
    @{ Source = "MDK-ARM\STM32H743\STM32H743.hex"; Name = "STM32H743.hex" },
    @{ Source = "MDK-ARM\STM32H743\STM32H743.map"; Name = "STM32H743.map" },
    @{ Source = "MDK-ARM\build.log"; Name = "build.log" }
)
$hashLines = foreach ($artifact in $artifacts) {
    $source = Join-Path $projectRoot $artifact.Source
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Expected build artifact not found: $source"
    }
    $target = Join-Path $output $artifact.Name
    Copy-Item -LiteralPath $source -Destination $target -Force
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    "$hash  $($artifact.Name)"
}
$hashLines | Set-Content -LiteralPath (Join-Path $output "SHA256SUMS.txt") -Encoding ASCII
