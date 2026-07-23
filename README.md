# MSPM0G3507 Keil 集成工程

本工程把原 TI 板卡算法与外设库整理到 LP-MSPM0G3507，并增加了与 BX71 FPGA 高速 AD/DA 工程配套的控制链路。

Keil 工程：

`Keil/NUEDC_2023_D_MSPM0G3507.uvprojx`

命令行强制全量构建：

```powershell
cd D:\NUEDC\fpga\集成工程\NUEDC_2023_D_MSPM0G3507_TI
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

构建脚本会等待 uVision 完整结束；任何错误或警告都会使脚本返回失败。当前 ARMClang 6.23 结果为 `0 Error(s), 0 Warning(s)`。

## 默认运行行为

- PB26 状态灯每 500 ms 翻转一次，主循环没有阻塞延时。
- UART0 使用 PA10/PA11、115200-8-N-1。
- MSPM0 ADC0_CH4 使用 PB25，实板默认从 P1/Analog_In 经 OPA2365 缓冲输入；未断开缓冲链路时禁止从板底排针直接驱动 PB25。默认执行 512 点、20 kSPS 单次采集。
- `NUEDC_ENABLE_FPGA_MODULE_INIT=1`，上电核验 AD9708 与 AD9280 FPGA 标识。
- ProMax DSP 接口已编入工程，但默认不初始化、不启动数据面，避免改变现有功能。
- `NUEDC_ENABLE_EXTERNAL_MODULE_INIT=0`，默认不初始化未连接的 AD9910、AD5687、DAC8831、ADS8363 或串口屏协议；四个低有效片选保持安全高电平，不产生总线事务。

FPGA链路正确时串口输出：

```text
FPGA AD9708=0, AD9280=0
```

## TI 与 FPGA 接口

| MSPM0G3507 | LaunchPad位置 | BX71 P2 | 作用 |
|---|---|---:|---|
| PA12 / SPI0_SCLK | BoosterPack 32 | 31 | SPI时钟 |
| PA14 / SPI0_PICO | 板底J28-7 | 32 | MOSI |
| PA13 / SPI0_POCI | BoosterPack 31 | 33 | MISO |
| PA15 / GPIO | BoosterPack 30 | 34 | 低有效软件片选 |
| GND | 任一GND | 30 或 12 | 共地 |

SPI0 为独占硬件接口：模式0、8位、MSB优先、1 MHz。CS在完整6字节帧期间持续为低，响应头固定为 `A5 5A`。J28位于板底且无信号丝印，PA14必须按缺口和1脚方向定位。高速 DAC 的 125 MSPS 发生和高速 ADC 的 32 MSPS 采集均在 FPGA 内完成，TI只配置寄存器并读取BRAM。

AD9708/AD9280 的高层驱动直接复用 `../Keil/Modules`，TI底层链路由 `Port/fpga_link_ti.c` 实现，保证两块MCU使用同一套设备标识、寄存器和状态接口。

## FPGA ProMax 接口

`fpga_promax.c/.h` 是从最新版 BX71 DSP ProMax 合并的纯 C 控制核心；TI 适配层为 `Port/fpga_promax_link_ti.c`。调用方按需定义 `FpgaPromax` 句柄并执行 `FPGA_ProMax_Init(&句柄)`，随后可配置 8 路 DDC、装载 4 组 32 抽头匹配模板、启停/清理数据面，并读取 FPGA 生成的功率、匹配峰值和样本位置。初始化只校验设备标识、版本和能力，不会自动启动计算。

TI 与 FPGA 仍使用同一套四线、1 MHz、6 字节寄存器链路。适配层在本地生成并校验通用 9 字节逻辑帧，再映射到 FPGA 物理寄存器；稳定配置写入后会按硬件有效位读回确认。当前整合 bitstream 的能力字为 `0x09200408`：支持 SPI 和实时 DDC/匹配滤波，不支持 ProMax UART、线级 CRC 或 FFT profile，因此 FFT 高层接口会明确返回“不支持”。

ProMax 不应在中断中调用，也不能与 AD9708/AD9280 驱动并发访问 SPI0；本裸机工程应在主循环中串行调用。TI 只传配置和压缩结果，不搬运 32 MSPS 原始流。

## TI 能力边界

- 片上 ADC：单通道512点，软件定时最高20 kSPS，只实现单次采集。
- AD5687/DAC8831：软件送点上限1000点/秒，16点表最高约62 Hz。
- DSP：保留轻量数学层和512点规模，不复制STM32H743的大缓存/DMA实现。
- ProMax：MCU 仅承担控制和结果读取，8 路 DDC、4×32 匹配滤波由 FPGA 实时完成；当前档位不提供 FFT。
- AD9280：可读1～4096点、可抽取和触发，但不能连续接收32 MSPS，也没有预触发。
- 2 KB栈；默认固件RAM占用远低于32 KB。按需链接ADC/DAC大缓存后仍在器件边界内。

详细端口映射见 `Docs/MSPM0G3507_Keil_Port_Guide.md`，完整上板步骤和预期现象见上一级 `../硬件测试指南.md`。
