# 集成工程

这是从 2024 C 题工程独立整理出的通用库模板，原 `2024C` 目录没有被修改。顶层按硬件平台平级组织，不使用“主工程/从工程”关系。

| 子工程 | 路径 | 用途 |
|---|---|---|
| TI MSPM0G3507 | `TI_MSPM0G3507/Keil/TI_MSPM0G3507.uvprojx` | TI 板卡轻量 DSP、通用外设及独立 SPI0 FPGA 联动 |
| STM32H743 | `STM32H743/MDK-ARM/STM32H743.uvprojx` | STM32H743 通用库及高速 AD/DA APP 接口 |
| FPGA BX71 | `FPGA_BX71_AD9708_AD9280/Vivado/BX71_AD9708_AD9280_PS7/BX71_AD9708_AD9280_PS7.xpr` | AD9708、AD9280、ProMax 和 Zynq PS7 固化工程 |
| 第三方参考库 | `第三方参考库/` | Git 子模块形式的只读参考来源，不作为三个工程的编译路径 |

三个工程各自保存源码、工程配置、构建脚本和输出目录。TI 与 STM32 各有自己的 `Modules/`，任何 Keil 工程都不引用另一个板卡目录；它们仅在接口协议层与 FPGA 兼容。FPGA 的 Vivado 工程、启动支持和输出也全部位于 FPGA 自己的目录内。

首次克隆后如需第三方参考库，执行 `git submodule update --init --recursive`；三个正式工程不依赖该子模块即可编译。

工程已去除 C 题的 SD/SM、多径时延、AM 双 NCO、AD9910 同步启动、C 题业务参数以及串口屏页面逻辑。AD9708 固件版本为 `0x00020003`，ADC 与 ProMax 共用同一 SPI 链路。

构建和最小调用示例见各子工程的 `README.md`；完整接线、测试步骤和预期现象见 `硬件测试指南.md`。
最终全量编译、仿真、时序和镜像校验结果见 `验证结果.md`。

## DSP ProMax 实时档

本工程已从 `ryule5158/BX71-DSP-ProMax` 的 `3ff859aa888a7e77996f6fc3b32b92be8352f69c` 合并经过复核的实时数据面：8 路 DDC、每路 CIC64 滤波与功率统计，以及 4 组 32 抽头匹配滤波和峰值保持。AD9280 的原始 32 MSPS 样本通过异步 FIFO 送入 50 MHz DSP 域；原有触发采集和 DAC 波形发生不受影响。

当前能力字为 `0x09200408`。它真实声明 SPI 和实时档，不声明 ProMax UART、线级 CRC 或 FFT；两套 MCU 的 FFT 接口会返回 `FPGA_PROMAX_E_UNSUPPORTED`。MCU 只配置频率/模板并读取压缩结果，不搬运连续 32 MSPS 数据，也不在 MCU 上复制 160 个并行乘法器的工作。

STM32H743 与 MSPM0G3507 继续使用原有 6 字节物理寄存器链路，接线没有增加。公共 `FpgaPromax_*` 逻辑 API 在 MCU 本地校验 9 字节帧，再安全映射到现有寄存器；因此本地 CRC 不是 SPI 线上端到端 CRC。默认 `main` 只保留库，不自动启动 ProMax。

## 重要硬件约定

- 高速 AD/DA 模块与 BX71 P3 尾端对齐直插，模块供电为 +5 V，数字 IO 为 3.3 V。
- STM32 与 FPGA 的 SPI2 使用 PI1/PI3/PI2/PE8，必须共地。
- TI 与 FPGA 使用独占 SPI0：PA12（BP32）/PA14（板底 J28-7）/PA13（BP31）/PA15（BP30），必须共地；J28 无表面丝印，应按缺口和 1 脚方向定位；不能与 STM32 同时驱动 BX71 SPI。
- AD9910 若在新题目中使用，`IO_UPDATE` 应连 STM32 PB8；不应再连本 FPGA 的原 C 题同步引脚，否则可能出现输出争用。

## 代码约定

新增业务代码继续遵守：尽可能简洁、真实实现、优先可读性，并为全局变量、宏、函数、枚举和结构体添加简洁中文注释。APP 层只公开业务层直接调用的接口，私有辅助函数使用 `static`。
