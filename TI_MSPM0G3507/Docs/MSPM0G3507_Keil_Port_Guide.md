# MSPM0G3507 Keil 移植说明

## 1. 工程与环境

工程根目录：

`D:\NUEDC\fpga\集成工程\TI_MSPM0G3507`

使用 Keil uVision 5.38、ARMClang 6.23、TI MSPM0G1X0X/G3X0X DFP 1.3.1。环境检查脚本是 `setup_keil_mspm0_env.ps1`，全量构建脚本是 `build_keil.ps1`。

当前 `ti_msp_dl_config.h/.c` 是工程唯一有效的硬件配置源。旧 `.syscfg` 与手写端口不一致且不参与构建，已移除，避免误操作重新生成并覆盖真实映射。

## 2. 移植结构

| 层 | 关键文件 | 作用 |
|---|---|---|
| TI配置 | `Inc/ti_msp_dl_config.h`、`Src/ti_msp_dl_config.c` | 时钟、GPIO、UART0、ADC0、SPI0 |
| HAL兼容 | `Port/stm32h7xx_hal.h`、`Port/nuedc_hal_compat.c` | GPIO映射、SysTick、UART、ADS8363软件SPI |
| FPGA链路 | `Port/fpga_link_ti.c` | TI DriverLib SPI0、6字节帧、ACK与超时 |
| FPGA ProMax | `../Modules/fpga_promax*`、`Port/fpga_promax_link_ti.c` | 8路DDC与4×32匹配滤波控制、压缩结果读取 |
| 高速AD/DA | `../Modules/ad9708*`、`ad9280*` | TI工程内独立保存的协议和APP接口 |
| TI入口 | `Src/nuedc_port.c` | 初始化开关和非阻塞主循环调度 |
| 片上ADC | `BSP/adc_app.c` | PB25单次定时采样 |

TI工程编译共享的 AD9708/AD9280 源码，但不会编译 STM32 专用 `fpga_link.c`。APP层在 TI 平台选择 `hspi1`，它只作为独占 SPI0 的平台句柄；ADS8363 继续使用 `hspi2` 的20位GPIO软件SPI，两条链路互不切换。

ProMax 通用核心不依赖 STM32 HAL、FPU、DMA 或动态内存。TI 适配层把本地9字节逻辑事务映射到现有6字节SPI寄存器，并对稳定配置做写后读回校验。当前FPGA能力字为 `0x09200408`，只开放SPI实时DDC/匹配滤波；UART、线级CRC和FFT能力位保持清零。工程默认不调用 `FPGA_ProMax_Init(&句柄)`，使用者应在实际业务明确需要时初始化，并在主循环串行访问共享SPI0。

## 3. 系统引脚

| 功能 | MSPM0G3507 | MSP-LITO位置 |
|---|---|---|
| UART0 TX/RX | PA10 / PA11 | J1-17 / J1-18，同时接到J3调试口 |
| ADC0_CH4 | PB25 | J2-15 |
| 状态灯 | PB14 | 板载D1 |
| FPGA SCLK/MOSI/MISO/CS | PA12 / PA14 / PA13 / PA15 | J1-30 / J1-32 / J1-31 / J2-32 |

FPGA SPI0：Controller、Motorola Mode 0、8 bit、MSB first、1 MHz、硬件CS关闭。PA15由软件在整帧前后拉低/拉高。以调试接口在上、按键在下的元件面观察：J1最下排左/右分别为PA13/PA14，J1倒数第二排右侧为PA12，J2最下排右侧为PA15。

## 4. 外接模块映射

### AD9910

| 信号 | MSPM0G3507 | MSP-LITO位置 |
|---|---|---|
| SCK / SDIO | PB20 / PB2 | J2-23 / J1-11 |
| PWR | PB13 | J1-26 |
| DROVER / DRCTL / DRHOLD | PB1 / PA8 / PA16 | J1-9 / J1-15 / J2-31 |
| RESET | PB4 | J1-13 |
| PF0 / PF1 / PF2 | PB5 / PA17 / PA9 | J1-14 / J2-30 / J1-16 |
| OSK / IO_UPDATE / CS | PB7 / PB8 / PB9 | J1-20 / J1-21 / J1-22 |

DRCTL和PWR已从PA19/PA20迁到PA8/PB13，PA19/PA20完整保留给SWD调试。DROVER和RESET使用无板载负载且已引出的PB1/PB4，避开PA0的板载LED/上拉以及PA6的40 MHz晶振网络；SDIO使用PB2，避开直接连接用户按键S2的PB21。

### AD5687与DAC8831

- 共享软件SPI：PB6 SCK（J1-19）、PA7 MOSI（J1-10）。
- AD5687 CS：PB22（J2-21）。
- DAC8831 CS：PB0（J1-8）。
- 两者可静态输出，但不要并行启动两个软件波形任务。

PB0/PB6均从MSP-LITO排针引出且无默认板载负载；不使用连接32.768 kHz/40 MHz晶振网络的PA4～PA6。

### ADS8363

- PB17 SCLK（J2-28）、PB18 MOSI（J2-27）、PB19 MISO（J2-26）。
- PB12 CS（J1-25）、PB23 RD/CONVST（J2-20）、PB24 BUSY（J2-19）。
- 使用20位GPIO软件SPI，不占用FPGA SPI0。

## 5. 初始化开关

`Inc/nuedc_port.h` 提供：

- `NUEDC_ENABLE_FPGA_MODULE_INIT`：默认1。
- `NUEDC_ENABLE_EXTERNAL_MODULE_INIT`：兼容用总开关，默认0。
- `NUEDC_ENABLE_AD5687`、`NUEDC_ENABLE_DAC8831`、`NUEDC_ENABLE_AD9910`、`NUEDC_ENABLE_ADS8363`、`NUEDC_ENABLE_SCREEN_PROCESS`：默认继承总开关。

建议在 Keil 的预处理宏中只把正在测试的模块设为1，不要为了测试一个模块同时初始化全部外设。

串口屏兼容层与 PC 调试输出共用 UART0 PA10/PA11，必须二选一。启用处理宏不会自动创建界面；业务层仍须提供 `Screen_ContextTypeDef`、调用 `Screen_Init()`，并按所选用途设置 XDS110 Backchannel 跳线。

## 6. 定时和资源边界

MSPM0G3507 Cortex-M0+没有DWT。兼容层以1 ms SysTick计数和当前递减值组合出亚毫秒软件周期计数，主循环不再使用阻塞LED延时。

| 能力 | STM32H743工程 | TI MSPM0G3507工程 | 移植结论 |
|---|---|---|---|
| AD9708/AD9280 FPGA协议与APP | 完整支持 | 复用同一高层源码 | 已完整移植，底层改为TI SPI0 |
| FPGA内部125/32 MSPS | FPGA执行 | FPGA执行 | 与MCU性能无关，功能一致 |
| FPGA ProMax实时DSP | FPGA并行执行 | MCU仅配置并读取压缩结果 | 支持8路DDC、4组32抽头匹配滤波，不支持当前档位之外的FFT |
| MCU片上ADC | 可使用DMA和更大缓存 | 512点、20 kSPS、软件定时单次采集 | 按真实TI能力缩减 |
| 大规模DSP缓存 | H743 RAM允许更大规模 | 32 KB SRAM，保持512点轻量实现 | 未复制4096点STM32缓存 |
| 软件DAC连续送点 | 较高主频和定时器资源 | 保守限制1000点/秒 | 不宣称不可达的高速波形 |
| FPGA QSPI烧写 | 由BX71工具完成 | 仍由BX71工具完成 | 不移植到MSPM0 |

- AD5687/DAC8831的软件送点上限限定为1000点/秒。
- ADS8363默认应用采样率1000 Hz，串口打印间隔200 ms。
- 片上ADC按配置的周期取512点；最高允许20 kSPS，只接受 `ADC_MODE_ONESHOT`。
- FPGA ADC/DAC时钟完全由FPGA产生，不受MSPM0软件定时精度影响。

链接文件给 MSPM0 分配128 KB Flash、32 KB SRAM和2 KB栈。默认构建约使用12 KB代码与6 KB零初始化RAM；AD9280的4096点缓存和AD9708的1024点波表仅在调用对应功能时进入最终链接，资源仍低于32 KB。

当前分项全功能测试固件为 Code 19720、RO 772、RW 32、ZI 18336 字节，仍低于128 KB Flash和32 KB SRAM边界。较大的ZI主要来自高速ADC缓存和测试结果，当前默认只运行板载ADC/FFT项目；外设不会同时启动。

## 7. 构建与实板验证

```powershell
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

必须得到 `0 Error(s), 0 Warning(s)`。生成的 `.hex` 用 XDS110/UniFlash 烧入 MSPM0G3507。数字编译、协议和资源验证不能替代模拟实测；接线、波形、ADC码值及故障定位按 `D:\NUEDC\fpga\集成工程\硬件测试指南.md` 执行。
