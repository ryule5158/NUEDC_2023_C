# TI四项联合实板测试

本指南保留四项联合测试的接线与验收资料，不对应当前临时UART测试入口。当前 `Src/main.c` 的操作方法见 `TI板卡串口测试.md`。

## 1. 测试固件

- TI工程：`Keil/TI_MSPM0G3507.uvprojx`
- 烧录文件：`output/TI_MSPM0G3507.hex`
- FPGA掉电启动镜像：`../FPGA_BX71_AD9708_AD9280/output/BOOT_PS7.bin`
- 串口：115200 bit/s、8数据位、无校验、1停止位、无流控
- AD9910：1 MHz正弦，幅度码8192
- AD9708：1 MHz、32点原始码正弦，峰值码96、中心码128
- AD9280：32 MSPS，每秒立即采集1024点，不抽取

AD9708测试使用原始码，不读取、不覆盖两点电压校准值；输出电压由模块模拟链路和电位器决定。

## 2. 断电接线

### 2.1 LaunchPad、串口和下载

1. J101的`GND、3V3、RXD、TXD、NRST、SWDIO、SWCLK`跳帽保持安装。
2. J21、J22均拨到XDS UART一侧，使PA10/PA11接入板载XDS110。
3. LaunchPad的`USB to PC`接口连接电脑。
4. 板载芯片`MSP432E401YT REV B`属于XDS110调试器部分，不是目标MSPM0G3507；使用它烧录时应选XDS110，不能在Keil中选ULINK。

若使用外部ULINK2/ULINK-ME，必须断电后拔掉J101的`NRST、SWDIO、SWCLK`跳帽，再把ULINK按SWD方式接J103；不要让板载XDS110和外部探针同时驱动SWD。

### 2.2 TI与BX71 FPGA

高速AD/DA模块与BX71的U3/P3尾端对齐直插，不翻面、不错位，并按模块要求稳定供电。TI与BX71只连接下列五根线：

| TI引脚 | LaunchPad位置 | BX71 |
|---|---|---|
| PA12 / SPI0_SCLK | BP32 | P2-31 |
| PA14 / SPI0_PICO（MOSI） | 板底J28-7 | P2-32 |
| PA13 / SPI0_POCI（MISO） | BP31 | P2-33 |
| PA15 / FPGA_CS | BP30 | P2-34 |
| GND | 任一GND | P2-30或P2-12 |

两板各自供电，不连接两板的3V3或5V。不得让原STM32同时连接并驱动同一组FPGA SPI线。PA14位于板底J28，必须按排针缺口和1脚方向确认J28-7，不能仅凭杜邦线颜色判断。

### 2.3 TI与AD9910

| AD9910信号 | TI引脚 | LaunchPad位置 |
|---|---|---|
| SCK | PB20 | BP36 |
| SDIO | PB2 | BP9 |
| PWR/POWERDOWN | PB13 | BP35 |
| DROVER | PB1 | BP39 |
| DRCTL | PA8 | BP4 |
| DRHOLD | PA16 | BP29，J15选择PA16 |
| RESET | PB4 | BP40 |
| PF0 | PB5 | 板底J27-1 |
| PF1 | PA17 | BP28 |
| PF2 | PA9 | 板卡排针PA9；避开J3上的PA18/BSL |
| OSK | PB7 | BP14 |
| IO_UPDATE | PB8 | BP15 |
| CS | PB9 | BP7 |
| GND | GND | GND |

AD9910模块还必须独立连接其规定的电源和参考时钟。当前CFR3配置按`40 MHz参考 × 25 = 1 GHz系统时钟`工作，因此参考时钟输入必须为40 MHz并符合模块电平要求。若实际只输入20 MHz，DDS输出会固定变成约500 kHz。SYNC_CLK是AD9910同步时钟输出，不可代替参考时钟输入。

所有控制线均按3.3 V逻辑使用，三块板和示波器/信号源必须共地。

## 3. 烧录

### 3.1 板载XDS110：推荐使用UniFlash

1. 关闭可能占用XDS110的CCS、Keil调试会话和串口工具。
2. 保持J101跳帽安装，用LaunchPad的Micro-USB连接电脑。
3. 打开TI UniFlash，自动检测或选择`MSPM0G3507 + XDS110`。
4. 在Program页面选择`output/TI_MSPM0G3507.hex`。
5. 执行Load Image/Program并开启写后校验。
6. 成功后按RESET。程序位于MSPM0G3507内部Flash，断电不会丢失。

### 3.2 外部ULINK：可在Keil直接下载

1. 按2.1节断开板载XDS110的三根SWD跳帽并接好外部ULINK。
2. 打开`Keil/TI_MSPM0G3507.uvprojx`。
3. Rebuild，确认`0 Error(s), 0 Warning(s)`。
4. 在`Options for Target -> Debug`选择`ULINK2/ME Debugger`和SWD；Utilities使用相同调试器。
5. 点击Download，完成后复位运行。

FPGA若已经烧入当前`BOOT_PS7.bin`且BX71处于QSPI档，上电后会自动配置，无需每次下载bit。先确认BX71的DONE/配置指示正常。

## 4. 测试顺序与预期现象

### 4.1 UART和数字通信

1. 打开Windows设备管理器，找到`XDS110 Class Application/User UART`对应COM口。
2. 串口工具设置为115200-8-N-1、无流控。
3. 按LaunchPad RESET。

应立即看到：

```text
[TI 4-LINK TEST] UART 115200-8-N-1 ready
[AD9910] status=0 output=1MHz sine
[AD9708] status=0 id=0xAD970802 version=0x00020003 output=1MHz sine-code
[AD9280] init=0 id=0xAD928001 version=0x00010001
```

随后约每秒一行：

```text
[ADC 1] status=0 count=1024 min=... max=... mean=... span=... otr=0 comm=PASS signal=...
```

`AD9708/AD9280`的状态、ID和版本完全符合时，才说明TI到FPGA的SCLK、MOSI、MISO、CS和FPGA固件协议真实通信成功。`AD9910 status=0`只代表GPIO写时序已执行，因为AD9910当前为单向SDIO控制，仍必须用示波器确认模拟输出。

### 4.2 高速DAC

1. 示波器探头接高速AD/DA模块S1，地接系统GND；建议先用1 MΩ输入观察，再根据模块输出网络决定是否使用50 Ω终端。
2. 复位TI板。
3. S1应连续输出约1 MHz、以原始码32～224变化的阶梯化正弦；经过模块滤波后应接近正弦。
4. 实际Vpp和直流中心由模块输出级、电位器、负载和耦合方式决定，本测试不改写校准参数，因此不规定绝对电压。

若S1无波形但串口ID正确，检查高速AD/DA模块供电、U3/P3方向、DAC时钟锁定和S1测量端；若ID错误，优先查五根TI-FPGA线和BX71当前固件。

### 4.3 高速ADC闭环

1. 确认S1输出正常且幅度未超过AD9280模块约-5～+5 V输入范围。
2. 用短SMA同轴线把S1接到S2。需要同时看S1时使用合适的50 Ω分配/T接，避免直接用长杜邦线传模拟信号。
3. 串口继续观察每秒的ADC报告。

预期：

- `status=0`、`count=1024`、`comm=PASS`；
- `min < max`且`span >= 8`，显示`signal=YES`；
- `mean`通常接近中点码128，但会随模拟链路偏置变化；
- `otr=0`；若非0，说明采集窗口出现过越界，应降低S1幅度或检查模块增益；
- 数字通信与有效输入均通过时，板载红灯约每500 ms翻转；失败时约每100 ms快闪。

不做闭环时，也可给S2输入`100 kHz、1 Vpp、0 V偏置`正弦进行独立ADC测试，必须先共地并确认信号始终在模块允许输入范围内。

### 4.4 AD9910

1. 示波器接AD9910模块的DAC输出SMA并共地。
2. 确认40 MHz参考时钟真实到达模块参考输入。
3. 复位TI板。

预期连续输出约1 MHz正弦。若约500 kHz，说明实际参考大约20 MHz；若完全无波形，依次测参考时钟、RESET、CS、SCK、SDIO和IO_UPDATE。SCK与IO_UPDATE只在复位后的初始化阶段出现脉冲，程序稳定运行后保持静态属于正常现象。

## 5. 反馈内容

请反馈以下四项：

1. 复位后的完整串口文本；
2. S1的频率和Vpp，以及S1接S2后的`min/max/mean/span/otr`；
3. AD9910输出频率和Vpp；
4. 若失败，说明红灯快慢，并拍下对应接线或示波器波形。
