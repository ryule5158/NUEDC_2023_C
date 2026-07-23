# MSPM0G3507最小串口测试

## 硬件连接

1. XDS110-ETP通过USB连接电脑。
2. XDS110-ETP与最小板之间使用8芯排线直连，确保3V3、GND、SWDIO、SWCLK和RST方向一致。
3. 8芯接口使用UART0：PA10发送、PA11接收；排线内部将板卡TX接到XDS110 RXD，并将板卡RX接到XDS110 TXD。
4. 不需要外接USB-TTL，也不要额外给串口引脚接5 V电平。

## 烧录

1. 打开 `Keil/TI_MSPM0G3507.uvprojx`。
2. 在 `Options for Target > Debug` 选择 `CMSIS-DAP Debugger`。
3. 在 `Settings` 中选择 `XDS110 with CMSIS-DAP`。
4. 全量编译后点击 `Download`，成功信息应包含 `Erase Done`、`Programming Done` 和 `Verify OK`。

## 串口测试

1. 打开XDS110的Application/User UART对应COM口。
2. 设置为115200 bit/s、8数据位、无校验、1停止位、无流控和ASCII显示。
3. 复位后应收到启动提示，随后周期收到状态提示：

```text
MSPM0G3507 UART0 PA10/PA11 READY
MSPM0G3507 UART TEST OK
```

4. 发送 `abc123`，应立即原样收到 `abc123`，表示发送与接收均通过。

本机XDS110当前枚举为COM3和COM4。优先测试COM3；若被占用，先关闭其他串口工具和Keil调试会话。
