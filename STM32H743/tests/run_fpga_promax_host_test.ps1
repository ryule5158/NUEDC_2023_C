$ErrorActionPreference = 'Stop'

# 使用Vivado 2018.3自带的完整MinGW工具链。
$gcc = 'D:\Tools\Xilinx\Vivado\2018.3\msys64\mingw64\bin\gcc.exe'
if (-not (Test-Path -LiteralPath $gcc)) {
    throw 'Vivado MinGW C compiler was not found.'
}

# 生成物仅放入系统临时目录，并在测试后删除。
$testRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$moduleRoot = Join-Path (Split-Path -Parent $testRoot) 'Modules'
$buildRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("nuedc_fpga_promax_test_{0}" -f $PID)
$resolvedTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$resolvedBuild = [System.IO.Path]::GetFullPath($buildRoot)
if (-not $resolvedBuild.StartsWith($resolvedTemp,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'The generated build path is outside the system TEMP directory.'
}

New-Item -ItemType Directory -Path $resolvedBuild -Force | Out-Null
$originalPath = $env:PATH
$env:PATH = (Split-Path -Parent $gcc) + ';' + $env:PATH
try {
    # 将生产源码复制到纯ASCII路径，兼容旧版MinGW运行库。
    Copy-Item -LiteralPath (Join-Path $testRoot 'fpga_promax_host_test.c') `
        -Destination $resolvedBuild
    Copy-Item -LiteralPath (Join-Path $moduleRoot 'fpga_promax.c') `
        -Destination $resolvedBuild
    Copy-Item -LiteralPath (Join-Path $moduleRoot 'fpga_promax.h') `
        -Destination $resolvedBuild
    Copy-Item -LiteralPath (Join-Path $moduleRoot 'fpga_promax_link_map.h') `
        -Destination $resolvedBuild

    $testExe = Join-Path $resolvedBuild 'fpga_promax_host_test.exe'
    Push-Location $resolvedBuild
    try {
        & $gcc -std=c11 -Wall -Wextra -Wpedantic -Werror `
            -finput-charset=UTF-8 `
            fpga_promax_host_test.c fpga_promax.c `
            -o $testExe 2>&1 | ForEach-Object { Write-Output $_ }
        if ($LASTEXITCODE -ne 0) {
            throw "Host test compilation failed with exit code $LASTEXITCODE."
        }

        & $testExe
        if ($LASTEXITCODE -ne 0) {
            throw "Host test failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    $env:PATH = $originalPath
    if (Test-Path -LiteralPath $resolvedBuild) {
        Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
    }
}
