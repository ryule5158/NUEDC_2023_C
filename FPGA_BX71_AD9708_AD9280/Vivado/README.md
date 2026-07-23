# Vivado 工程入口

正式掉电启动工程：

`BX71_AD9708_AD9280_PS7/BX71_AD9708_AD9280_PS7.xpr`

该工程包含 Zynq PS7、AD9708、AD9280 和 ProMax。源文件仍按相对路径引用本 FPGA 目录内的 `rtl/`、`rtl_ps7/`、`ps7/` 与 `constraints/`，不引用 TI 或 STM32 工程。

推荐通过上一级 `scripts/Build-QspiBoot.ps1` 全量构建。脚本仅在运行 Vivado 2018.3 时建立临时英文目录联接，以规避旧版本工具对中文路径的兼容问题；`.xpr` 和全部正式输出仍保存在本 FPGA 工程内。

如只需纯 PL 调试，可运行 `scripts/build_project.tcl`，工程会固定生成到 `BX71_AD9708_AD9280_PL/`。

行为仿真临时工程统一生成到 `Simulation/`，该目录可随时重建且不会提交到 Git。
