# 源码编译

所有芯片均使用 cmake 编译（主机交叉编译，部分芯片也支持板端本地编译），通过 `-DAXERA_TARGET_CHIP` 指定芯片、`-DBSP_MSP_DIR` 指定 BSP 目录。各芯片所需工具链、OpenCV 与 BSP 不同，请按芯片阅读对应文档：

## [AX650 系列编译详细说明](./compile_650.md)
支持以下芯片
- AX650A
- AX650N

## [AX637 系列编译详细说明](./compile_637.md)
支持以下芯片
- AX637

## [AX620E 系列编译详细说明](./compile_620e.md)
支持以下芯片
- AX630C
- AX620Q

## [AX615 系列编译详细说明](./compile_615.md)
支持以下芯片
- AX615

## [AX8860 编译详细说明](./compile_8860.md)
支持以下芯片
- AX8860
