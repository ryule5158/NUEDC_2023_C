# MSP-LITO-G3507 图形化配置说明

## 1. 工程结构

本工程已经由手写 `ti_msp_dl_config.c/.h` 迁移到 TI SysConfig：

- 图形配置源文件：`SysConfig/MSP_LITO_G3507_Board.syscfg`
- 自动生成源码：`SysConfig/Generated/ti_msp_dl_config.c`
- 自动生成头文件：`SysConfig/Generated/ti_msp_dl_config.h`
- 手动生成脚本：`generate_sysconfig.ps1`
- 打开图形界面：`open_sysconfig.ps1`
- Keil 工程：`Keil/TI_MSPM0G3507.uvprojx`

图形配置文件是引脚、时钟和外设参数的唯一配置源。不要手工修改
`SysConfig/Generated` 中的文件，也不要再在 `Src` 或 `Inc` 中建立另一套
`ti_msp_dl_config.c/.h`。

## 2. 本机官方工具

当前脚本默认使用：

- TI SysConfig：`D:\ti\sysconfig_1.26.2`
- MSPM0 SDK：`D:\ti\mspm0_sdk_2_10_00_04`
- Keil uVision：`C:\Keil_v5\UV4\UV4.exe`

这些路径只指向官方工具，不指向 STM32 工程、FPGA 工程或其他 TI 工程。
如以后更换工具位置，可设置下列环境变量，或者给脚本传同名参数：

```powershell
$env:NUEDC_TI_SYSCONFIG_ROOT = "D:\ti\sysconfig_1.26.2"
$env:NUEDC_TI_MSPM0_SDK_ROOT = "D:\ti\mspm0_sdk_2_10_00_04"
```

## 3. 打开图形配置

在 `TI_MSPM0G3507` 目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\open_sysconfig.ps1
```

打开后可在左侧模块中查看：

- `UART_0`
- `FPGA_SPI`
- `ADC12_0`
- `GPIO_FPGA_CONTROL`
- `GPIO_BOARD`
- `GPIO_DAC_SHARED`
- `GPIO_AD9910`
- `GPIO_ADS8363`

在右侧 PinMux/器件封装视图中可直接查看每个功能对应的物理引脚。

如果希望从 Keil 的 Tools 菜单打开，可在 uVision 的
`Tools -> Customize Tools Menu` 中导入
`Keil/SysConfig_DriveD_Menu.cfg`。推荐优先使用 `open_sysconfig.ps1`，
因为它会把图形界面的生成目录固定为工程内的 `SysConfig/Generated`。

## 4. 保存、生成和编译

图形界面保存 `.syscfg` 后，在工程根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\generate_sysconfig.ps1
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

也可以直接在 Keil 中 Build/Rebuild。Keil 的 Before Build 已接入
`generate_sysconfig.ps1`，会先严格校验并重新生成配置，再编译固件。
SysConfig 出现错误或警告时，构建会停止，避免使用过期生成文件。

## 5. 已保留的实测硬件配置

| 功能 | 配置 |
|---|---|
| 系统时钟 | 内部 SYSOSC 32 MHz，不启用 HFXT/PLL |
| UART0 | PA10 TX、PA11 RX，115200-8-N-1，FIFO 1/2 阈值 |
| FPGA SPI0 | PA12 SCLK、PA14 PICO/MOSI、PA13 POCI/MISO |
| FPGA SPI 参数 | Controller，Mode 0，8 位，MSB，1 MHz |
| FPGA 软件片选 | PA15，低有效，上电先置高再使能输出 |
| 片上 ADC0 | PB25/通道 4，12 位无符号，VDDA 3.3 V |
| ADC 触发 | 软件触发、单次转换、SYSOSC/1、采样时间 8 周期 |
| 调试保留 | PA19/PA20 保留 SWD，PA18 不占用 |

TI SysConfig 把“没有硬件 CS、由 PA15 软件控制片选”的 SPI 表示为
Motorola 3-wire，但 BX71 实板链路要求 SPI0 的 FRF 寄存器保持为
Motorola 4-wire。`Board/MSP_LITO_G3507_RuntimeConfig.c` 会在生成代码完成
初始化后，只把帧格式恢复为已实测通过的 `MOTO4_POL0_PHA0`，PA15 仍由
GPIO 软件控制。启动串口会检查并打印
`[SPI] FORMAT=MOTO4 SOFTWARE-CS OK`。其余时钟、引脚、位宽和位序继续由
SysConfig 管理。

其余图形 GPIO 已按真实器件功能命名，包括 AD9910、ADS8363、
AD5687、DAC8831 和板载状态脚。AD9910、DAC 和 ADS8363 的低有效片选
均配置为空闲高电平。

## 6. 生成后检查

每次修改引脚或外设后至少检查：

1. SysConfig 校验为 0 error、0 warning。
2. Keil 全量构建为 `0 Error(s), 0 Warning(s)`。
3. 串口启动信息仍显示：

```text
MSP-LITO-G3507 FPGA AD/DA TEST READY
SPI0 PA12/PA14/PA13, CS PA15 ACTIVE LOW
[SPI] CS POWER-UP=HIGH OK
[SPI] FORMAT=MOTO4 SOFTWARE-CS OK
```

4. 再执行一次 FPGA 设备标识读取和 ProMax 实时测试，确认通信与迁移前一致。
