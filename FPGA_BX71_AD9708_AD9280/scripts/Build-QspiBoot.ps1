# 构建开关及Xilinx工具、FSBL的可选显式路径。
[CmdletBinding()]
param(
    [switch]$SkipVivado,
    [string]$Fsbl,
    [string]$Vivado,
    [string]$Bootgen
)

$ErrorActionPreference = 'Stop' # 任一命令失败即停止打包。

# 按“用户指定优先、候选路径其次”的顺序查找Xilinx工具。
function Find-XilinxTool {
    # 指定路径、默认候选路径和报错用工具名。
    param(
        [string]$SpecifiedPath,
        [string[]]$CandidatePaths,
        [string]$ToolName
    )

    if (-not [string]::IsNullOrWhiteSpace($SpecifiedPath)) {
        return $SpecifiedPath
    }

    foreach ($candidate in $CandidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "$ToolName was not found. Pass its full path explicitly."
}

# 自动定位Vivado 2018.3和配套bootgen。
$Vivado = Find-XilinxTool $Vivado @(
    'D:\Tools\Xilinx\Vivado\2018.3\bin\vivado.bat',
    'C:\Xilinx\Vivado\2018.3\bin\vivado.bat',
    'E:\Vivado\2018.3\bin\vivado.bat'
) 'Vivado 2018.3'
$Bootgen = Find-XilinxTool $Bootgen @(
    'D:\Tools\Xilinx\SDK\2018.3\bin\bootgen.bat',
    'C:\Xilinx\SDK\2018.3\bin\bootgen.bat',
    'E:\SDK\2018.3\bin\bootgen.bat'
) 'bootgen 2018.3'

# FPGA工程根目录、构建入口和输出文件路径。
$fpgaRoot = Split-Path -Parent $PSScriptRoot
$buildTcl = Join-Path $PSScriptRoot 'build_ps7_project.tcl'
$bitFile = Join-Path $fpgaRoot 'output\BX71_V4_U3_tail_AD9708_PS7.bit'
$outputDir = Join-Path $fpgaRoot 'output'
$bootFile = Join-Path $outputDir 'BOOT_PS7.bin'
$vivadoProjectDir = Join-Path $fpgaRoot 'Vivado\BX71_AD9708_AD9280_PS7'

# 未显式指定时使用FPGA工程自己的启动支持文件。
if ([string]::IsNullOrWhiteSpace($Fsbl)) {
    $Fsbl = Join-Path $fpgaRoot '启动支持\boot\FSBL.elf'
}

# 构建开始前一次性检查所有必需文件。
foreach ($required in @($Vivado, $Bootgen, $Fsbl, $buildTcl)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required file not found: $required"
    }
}

# Vivado 2018.3不能通过目录联接自动创建上级目录，先在真实工程中建好固定目录。
if (-not $SkipVivado) {
    New-Item -ItemType Directory -Path $vivadoProjectDir -Force | Out-Null
}

# 工具使用的ASCII工程入口，规避Vivado 2018.3的中文路径子进程故障。
$toolFpgaRoot = $fpgaRoot
$asciiLink = $null
if ($fpgaRoot -match '[^\x00-\x7F]') {
    # 名称刻意保持很短，避免Vivado 2018.3运行目录触发Windows MAX_PATH限制。
    $asciiLink = Join-Path $env:TEMP 'NF'
    if (Test-Path -LiteralPath $asciiLink) {
        $linkItem = Get-Item -LiteralPath $asciiLink -Force
        if (($linkItem.LinkType -ne 'Junction') -or
            ($linkItem.Target -notcontains $fpgaRoot)) {
            throw "ASCII build path is occupied: $asciiLink"
        }
        [System.IO.Directory]::Delete($asciiLink)
    }
    New-Item -ItemType Junction -Path $asciiLink -Target $fpgaRoot | Out-Null
    $toolFpgaRoot = $asciiLink
}

try {
    # 将工程内路径映射到工具可访问的ASCII入口。
    $toolBuildTcl = Join-Path $toolFpgaRoot 'scripts\build_ps7_project.tcl'
    $toolBitFile = Join-Path $toolFpgaRoot 'output\BX71_V4_U3_tail_AD9708_PS7.bit'
    $toolBifFile = Join-Path $toolFpgaRoot 'output\ad9708_ps7_qspi.bif'
    $toolBootFile = Join-Path $toolFpgaRoot 'output\BOOT_PS7.bin'
    $toolFsbl = if ($Fsbl.StartsWith($fpgaRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        $toolFpgaRoot + $Fsbl.Substring($fpgaRoot.Length)
    } else {
        $Fsbl
    }

    # 默认先生成含PS7的位流，调试时可选择复用已有位流。
    if (-not $SkipVivado) {
        Write-Host 'Building the PS7-integrated bitstream...'
        & $Vivado -mode batch -source $toolBuildTcl
        if ($LASTEXITCODE -ne 0) {
            throw "Vivado failed with exit code $LASTEXITCODE"
        }
    }

    if (-not (Test-Path -LiteralPath $bitFile)) {
        throw "PS7-integrated bitstream not found: $bitFile"
    }

    # 生成bootgen所需BIF，依次装入FSBL和PL位流。
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $fsblBif = (Resolve-Path -LiteralPath $toolFsbl).Path.Replace('\', '/')
    $bitBif = (Resolve-Path -LiteralPath $toolBitFile).Path.Replace('\', '/')
    @"
the_ROM_image:
{
  [bootloader] $fsblBif
  $bitBif
}
"@ | Set-Content -LiteralPath $toolBifFile -Encoding ASCII

    Write-Host 'Packaging BOOT_PS7.bin...'
    & $Bootgen -image $toolBifFile -arch zynq -o $toolBootFile -w on
    if ($LASTEXITCODE -ne 0) {
        throw "bootgen failed with exit code $LASTEXITCODE"
    }
    # BIF只用于本次打包，成功后删除，避免输出目录残留临时路径。
    [System.IO.File]::Delete($toolBifFile)
} finally {
    # 无论成功或失败都清理临时目录联接。
    if (($null -ne $asciiLink) -and (Test-Path -LiteralPath $asciiLink)) {
        [System.IO.Directory]::Delete($asciiLink)
    }
}

# 输出镜像长度和SHA-256，便于烧录后完整读回校验。
$bootItem = Get-Item -LiteralPath $bootFile
$bitHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $bitFile).Hash
$bootHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $bootFile).Hash
$hashFile = Join-Path $outputDir 'SHA256SUMS.txt'
@(
    "$bitHash  BX71_V4_U3_tail_AD9708_PS7.bit"
    "$bootHash  BOOT_PS7.bin"
) | Set-Content -LiteralPath $hashFile -Encoding ASCII
Write-Host ''
Write-Host 'QSPI boot image is ready:'
Write-Host "  $($bootItem.FullName)"
Write-Host "  Length: $($bootItem.Length) bytes"
Write-Host "  SHA256: $bootHash"
Write-Host ''
Write-Host 'Program it in SDK with qspi-x1-single and Verify after flash enabled.'
