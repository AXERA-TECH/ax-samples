[English](./AGENTS_EN.md) | 简体中文

# AGENTS.md

面向 AI Agent / 新贡献者的速查。人类阅读入口见 [README.md](README.md)。

## 项目是什么

爱芯元智（AXERA）各款 AI SoC 上的 NPU 推理示例（C++）。每个示例是一个独立可执行程序：读图 → 前处理 → `ax_engine` 推理 → 后处理 → 打印结果并保存结果图。没有库、没有单元测试，验证方式是 **交叉编译通过 + 板端运行输出正确**。

## 关键路径

| 路径 | 作用 |
| ---- | ---- |
| `CMakeLists.txt` | 顶层入口，`AXERA_TARGET_CHIP` 默认 `ax650` |
| `examples/CMakeLists.txt` | 按芯片设置宏 `AXERA_TARGET_CHIP_AXxxx`、OpenCV 路径，并 `add_subdirectory(examples/<目录>)` |
| `cmake/<chip>.cmake` | 定义 `axera_example(<程序名> <源文件>)`：链接 BSP 库并安装到 `install/<chip>/` |
| `examples/<chip>/` | 该芯片的全部示例：`ax_<模型>_steps.cc`、`CMakeLists.txt`、`README.md`、`middleware/io.hpp` |
| `examples/base/` | 跨芯片共用的后处理：`detection.hpp`（YOLO 系列解码、NMS、画框/mask/关键点）、`topk.hpp`、`pose.hpp` 等 |
| `examples/utilities/` | 命令行解析 `cmdline.hpp`、`args.hpp`、文件读取、计时器 |
| `docs/compile_<chip>.md` | 各芯片编译说明；`docs/<chip>/*.jpg` 为 README 中的结果图 |
| `benchmark/` | 各芯片模型耗时表 |
| `toolchains/` | 交叉编译工具链文件 |
| `.github/workflows/` | 各芯片 CI（仅编译） |

`3rdparty/`、`build*/` 在 `.gitignore` 中，不要提交。

## 芯片与编译参数

| 芯片 | `AXERA_TARGET_CHIP` | 示例目录 | 工具链文件 | OpenCV（解压到 `3rdparty/`） | 文档 |
| ---- | ---- | ---- | ---- | ---- | ---- |
| AX650A/AX650N/AX8850 | `ax650` | `examples/ax650` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_650](docs/compile_650.md) |
| AX637 | `ax637` | `examples/ax637` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_637](docs/compile_637.md) |
| AX630C | `ax630c` | `examples/ax620e` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_620e](docs/compile_620e.md) |
| AX620Q | `ax620q` | `examples/ax620e` | `arm-AX620E-linux-uclibcgnueabihf` | `opencv-arm-uclibc-linux` | [compile_620e](docs/compile_620e.md) |
| AX615 | `ax615` | `examples/ax615` | `arm-linux-gnueabihf` | `opencv-arm-uclibc-linux` | [compile_615](docs/compile_615.md) |
| AX8860 | `ax8860` | `examples/ax8860` | `aarch64-none-linux-gnu`（**须 Arm GNU 13.2，glibc ≥ 2.38**） | `opencv-aarch64-linux` | [compile_8860](docs/compile_8860.md) |

通用编译命令（禁止在源码目录内编译）：

```bash
mkdir build_<chip> && cd build_<chip>
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/<工具链>.toolchain.cmake \
      -DBSP_MSP_DIR=<BSP 目录，含 include/ 与 lib/> \
      -DAXERA_TARGET_CHIP=<chip> ..
make -j$(nproc) && make install      # 产物在 build_<chip>/install/<chip>/
```

## 示例程序约定

- 统一参数：`-m <axmodel>`（必填）、`-i <图片>`（必填）、`-g <h,w>` 模型输入尺寸（默认 640,640 或 224,224）、`-r <次数>` 重复推理次数用于测速。
- 输出：stdout 打印 `Repeat N times, avg time ...` 和检测结果（`类别id: 置信度, [x0, y0, x1, y1], 类名`）；结果图保存在 **当前目录** `<模型>_out.jpg`（分类示例只打印 top5）。
- 板端运行前：`export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH`（BSP 动态库位置以板子为准）。

## 常见任务

### 给已有芯片新增一个模型示例

1. 先在其他芯片目录找同名示例（如 `examples/ax650/ax_yolo11_steps.cc`），ax_engine API 在各芯片间基本一致，优先直接复用，保持代码风格一致。
2. 后处理函数放在 / 复用 `examples/base/`，不要在 steps.cc 里重复实现。
3. 在 `examples/<chip>/CMakeLists.txt` 加一行 `axera_example(ax_xxx ax_xxx_steps.cc)`。
4. 在 `examples/<chip>/README.md` 的目录列表和「运行示例」中加入命令、完整输出日志和结果图（图放 `docs/<chip>/`，宽度约 480px，控制体积）。
5. 若芯片编译文档里列出了 install 产物，同步更新。

### 新增一个芯片

1. `cmake/<chip>.cmake`：从相近芯片复制，修改 `install(... DESTINATION <chip>)`。
2. `examples/CMakeLists.txt`：两处 `elseif(AXERA_TARGET_CHIP MATCHES "<chip>")`（设置宏和 OpenCV、`add_subdirectory`）。注意 `MATCHES` 是正则子串匹配，新芯片名不要与已有名字互为子串。
3. `examples/<chip>/`：`CMakeLists.txt`、`middleware/io.hpp`、示例源码、`README.md`。
4. 文档：`docs/compile_<chip>.md`，并在 `docs/compile.md`、`README.md`、`README_EN.md` 的芯片列表中登记。
5. 可选：在 `.github/workflows/` 增加编译 CI。

## 已知坑

- **AX8860**：BSP 库依赖 glibc 2.38，gcc-arm 9.2 / 11.2 链接会报 `__isoc23_*@GLIBC_2.38`；`middleware/io.hpp` 必须为输入 buffer 填 `nSize`，否则推理报 `invalid input size=0`。
- **YOLOv8 系列后处理**（`generate_proposals_yolov8_native`）要求每个 stride 一个输出、box(64) 与 cls 在通道维拼接（1×H×W×144 / pose 为 65）；拆分输出的 onnx 需先在图中 Concat 再用 Pulsar2 转换。
- 输出 tensor 顺序由模型决定，后处理按 `pOutputs[i]` 下标读取；换模型时先确认输出顺序与 shape（部分 seg 示例会打印 io 信息）。
- 默认编译为 `release`（`-O1 -s`，已 strip）；板端调试可在 `examples/CMakeLists.txt` 打开 `-g -O0`。
- `docs/` 下图片较多，大仓库 clone 可用 `--filter=blob:none` + sparse-checkout 只取需要的目录。
