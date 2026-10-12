[English](./README_EN.md) | 简体中文

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)

| Platform | Build Status |
| -------- | ------------ |
| AX650/AX8850   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_650.yml)|
| AX637/AX8910    | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_637.yml)|
| AX630C   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_630c_glibc.yaml)|
| AX620Q   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620q_uclibc.yaml)|
| AX615    | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_615_glibc.yaml)|

## 简介

**AX-Samples** 由 **[爱芯元智](https://www.axera-tech.com/)** 主导开发。该项目实现了常见的 **深度学习开源算法** 在 **爱芯元智** 的 **AI SoC** 上的示例代码，方便社区开发者进行快速评估和适配。 

### 已支持芯片

**终端计算芯片**
- [AX650A](https://www.axera-tech.com/zh-hans/product/2929.html) / [AX650N](https://www.axera-tech.com/zh-hans/product/2929.html)
- AX637
- [AX630C](https://www.axera-tech.com/zh-hans/product/2931.html) / [AX620Q](https://www.axera-tech.com/zh-hans/product/2932.html)
- [AX615](https://www.axera-tech.com/zh-hans/product/2956.html)

**车载芯片**
- [M57](https://www.axera-tech.com/zh-hans/product/2826.html)

**边缘计算芯片**
- [AX8850N](https://www.axera-tech.com/zh-hans/product/2897.html)/[AX8850](https://www.axera-tech.com/zh-hans/product/2896.html)
- AX8860

### 已支持开发板

| 开发板 | 图片 | 芯片 | 厂商 | 链接 |
| ------ | ---- | ---- | ---- | ---- |
| AXera-Pi Pro | <img src="docs/boards/Axera-Pi_pro.jpg" width="200"> | AX8850 | Sipeed | [Wiki](https://wiki.sipeed.com/m4cdock) |
| AI Pyramid | <img src="docs/boards/AI_Pyramid.jpg" width="200"> | AX8850 | M5Stack | [文档](https://docs.m5stack.com/zh_CN/ai_hardware/AI_Pyramid) |
| LLM630 Compute Kit | <img src="docs/boards/LLM630_Compute_kit.jpg" width="200"> | AX630C | M5Stack | [文档](https://docs.m5stack.com/zh_CN/core/LLM630%20Compute%20Kit) |
| Module LLM | <img src="docs/boards/module_LLM.jpg" width="200"> | AX630C | M5Stack | [文档](https://docs.m5stack.com/zh_CN/module/Module-LLM) |

## 目录结构

```
ax-samples
├── 3rdparty/              # 第三方依赖，编译前将预编译 OpenCV 解压到此处
├── benchmark/             # 各芯片常见模型推理耗时统计
├── cmake/                 # 各芯片的编译配置（axXXX.cmake）与编译检查脚本
├── docs/                  # 编译文档（compile_*.md）、示例结果图、开发板图片
├── examples/              # 示例源码
│   ├── base/              #   通用后处理：检测、关键点、topk、坐标变换等
│   ├── utilities/         #   通用工具：命令行解析、文件读取、计时器等
│   ├── ax650/             #   AX650 / AX8850 示例
│   ├── ax637/             #   AX637 示例
│   ├── ax620e/            #   AX630C / AX620Q 示例
│   ├── ax615/             #   AX615 示例
│   └── ax8860/            #   AX8860 示例
├── toolchains/            # 交叉编译工具链 cmake 文件
├── CMakeLists.txt         # 顶层 cmake，通过 AXERA_TARGET_CHIP 选择芯片
├── AGENTS.md              # 面向 AI Agent 的速查（英文）：编译命令、新增模型/芯片步骤、常见坑
└── AGENTS_ZH.md           # AGENTS.md 中文版
```

每个芯片目录的组织方式相同：

```
examples/axXXX/
├── ax_<模型>_steps.cc     # 每个模型一个示例：前处理 → NPU 推理 → 后处理 → 保存结果图
├── middleware/io.hpp      # 该芯片 NPU 引擎的输入输出内存申请与释放
├── CMakeLists.txt         # 列出该芯片编译的示例程序
└── README.md              # 示例列表、运行命令与输出日志
```

## 快速上手

使用流程分三步：

1. **编译**：参考 [快速编译](docs/compile.md)，按芯片选择对应文档，基于 cmake 交叉编译出示例程序。
2. **准备模型**：从下方网盘资源获取 axmodel 和测试图片，或使用 [Pulsar2](https://huggingface.co/AXERA-TECH/Pulsar2) 自行转换。
3. **运行**：将示例程序、模型、图片拷贝到开发板，命令及输出参考各芯片示例目录下的 README（如 [examples/ax650](examples/ax650/README.md)）。

### 示例

- [examples](examples/) 提供常见分类、检测、分割、姿态等深度学习开源算法和传统 CV 操作用例，根据 issue 需求持续更新。

### 网盘资源

- 提供 **ModelZoo**, **预编译程序**, **测试图片** 等内容:
  - [Huggingface](https://huggingface.co/collections/AXERA-TECH/vision-models-67b0bce92ddc61229e8e94ed)
  - [Modelscope](https://modelscope.cn/organization/AXERA-TECH)

### 速度评估

- [Benchmark](benchmark/) 常见开源模型推理耗时统计，基于 *AXera-Pi Pro* 实测。

## 关联项目

- NPU工具链在线文档，其中提供了NPU工具链相关使用说明和获取方式（更新不及时）
  - [Pulsar2在线文档](https://pulsar2-docs.readthedocs.io/zh_CN/latest/)(Support AX650A/AX650N/AX630C/AX620Q)
- 更推荐直接从下面获取最新 Pulsar2 工具链及文档
  - [Pulsar2 下载](https://huggingface.co/AXERA-TECH/Pulsar2)

## 技术讨论

- Github issues
- QQ 群: 139953715
