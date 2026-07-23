$ErrorActionPreference = 'Stop'

# 定位Vivado随附的MinGW C编译器。
$compiler = 'D:\Tools\Xilinx\Vivado\2018.3\msys64\mingw64\bin\gcc.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "未找到MinGW C编译器：$compiler"
}

$root = Split-Path -Parent $PSScriptRoot
$sharedModules = Join-Path $root 'Modules'
$buildRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ("nuedc_fpga_promax_ti_test_{0}" -f $PID)
$resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$resolvedBuild = [IO.Path]::GetFullPath($buildRoot)
if (-not $resolvedBuild.StartsWith(
        $resolvedTemp, [StringComparison]::OrdinalIgnoreCase)) {
    throw '生成目录不在系统TEMP目录内。'
}

New-Item -ItemType Directory -Path (Join-Path $resolvedBuild 'tests') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $resolvedBuild 'Port') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $resolvedBuild 'Modules') -Force | Out-Null
$originalPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
try {
    # 旧版MinGW运行时不能可靠访问中文路径，因此复制生产源码到ASCII临时目录。
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fpga_promax_ti_host_test.c') `
        -Destination (Join-Path $resolvedBuild 'tests')
    Copy-Item -LiteralPath (Join-Path $root 'Port\fpga_promax_link_ti.c') `
        -Destination (Join-Path $resolvedBuild 'Port')
    Copy-Item -LiteralPath (Join-Path $root 'Port\stm32h7xx_hal.h') `
        -Destination (Join-Path $resolvedBuild 'Port')
    Copy-Item -LiteralPath (Join-Path $root 'Port\main.h') `
        -Destination (Join-Path $resolvedBuild 'Port')
    foreach ($name in @('fpga_promax.c', 'fpga_promax.h',
                         'fpga_promax_link.h', 'fpga_promax_link_map.h',
                         'fpga_link.h')) {
        Copy-Item -LiteralPath (Join-Path $sharedModules $name) `
            -Destination (Join-Path $resolvedBuild 'Modules')
    }

    Push-Location (Join-Path $resolvedBuild 'tests')
    try {
        $executable = Join-Path $resolvedBuild 'fpga_promax_ti_host_test.exe'
        & $compiler -std=c11 -Wall -Wextra -Wpedantic -Werror `
            -finput-charset=UTF-8 -I..\Port -I..\Modules `
            fpga_promax_ti_host_test.c ..\Modules\fpga_promax.c `
            -o $executable 2>&1 | ForEach-Object { Write-Output $_ }
        if ($LASTEXITCODE -ne 0) {
            throw "ProMax TI主机测试编译失败，退出码：$LASTEXITCODE"
        }

        & $executable
        if ($LASTEXITCODE -ne 0) {
            throw "ProMax TI主机测试失败，退出码：$LASTEXITCODE"
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
