$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$uv4 = "C:\Keil_v5\UV4\UV4.exe"
$project = Join-Path $projectRoot "Keil\TI_MSPM0G3507.uvprojx"
$log = Join-Path $projectRoot "Keil\build.log"
$output = Join-Path $projectRoot "output"

if (-not (Test-Path $uv4)) {
    throw "Keil uVision not found: $uv4"
}

if (-not (Test-Path $project)) {
    throw "Keil project not found: $project"
}

Push-Location (Join-Path $projectRoot "Keil")
try {
    if (Test-Path $log) {
        Remove-Item -LiteralPath $log
    }
    Start-Process -FilePath $uv4 `
        -ArgumentList '-r "TI_MSPM0G3507.uvprojx" -j0 -o "build.log"' `
        -WindowStyle Hidden -Wait | Out-Null
    $buildResult = Get-Content -Raw $log
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
    @{ Source = "Keil\Objects\TI_MSPM0G3507.axf"; Name = "TI_MSPM0G3507.axf" },
    @{ Source = "Keil\Objects\TI_MSPM0G3507.hex"; Name = "TI_MSPM0G3507.hex" },
    @{ Source = "Keil\TI_MSPM0G3507.map"; Name = "TI_MSPM0G3507.map" },
    @{ Source = "Keil\build.log"; Name = "build.log" }
)
$hashLines = foreach ($artifact in $artifacts) {
    $source = Join-Path $projectRoot $artifact.Source
    $target = Join-Path $output $artifact.Name
    Copy-Item -LiteralPath $source -Destination $target -Force
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    "$hash  $($artifact.Name)"
}
$hashLines | Set-Content -LiteralPath (Join-Path $output "SHA256SUMS.txt") -Encoding ASCII
