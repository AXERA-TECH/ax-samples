English | [简体中文](./README.md)

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)

| Platform | Build Status |
| -------- | ------------ |
| AX650N   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_650.yml)|
| AX637    | ![Build](https://img.shields.io/badge/build-passing-brightgreen)|
| AX630C   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_630c_glibc.yaml)|
| AX620Q   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620q_uclibc.yaml)|

## Introduction

**AX-Samples** is developed by **[AXERA](https://www.axera-tech.com/)**. This project provides sample code for common **deep learning open-source algorithms** running on **AXERA AI SoCs**, enabling community developers to quickly evaluate and adapt.

### Supported SoCs

- [AX650A](https://www.axera-tech.com/zh-hans/product/2929.html) / [AX650N](https://www.axera-tech.com/zh-hans/product/2929.html)
- [AX8850](https://www.axera-tech.com/zh-hans/product/2896.html)
- [AX637](https://www.axera-tech.com/zh-hans/product/2826.html)
- [AX630C](https://www.axera-tech.com/zh-hans/product/2931.html) / [AX620Q](https://www.axera-tech.com/zh-hans/product/2932.html)

### Supported Boards

| Board | Image | SoC | Vendor | Link |
| ----- | ----- | --- | ------ | ---- |
| AXera-Pi Pro | <img src="docs/boards/Axera-Pi_pro.jpg" width="200"> | AX650N | Sipeed | [Wiki](https://wiki.sipeed.com/m4ndock) |
| AXera-Pi Zero | <img src="docs/boards/Axera-Pi_zero.jpg" width="200"> | AX620Q | Sipeed | [Docs](https://axera-pi-zero-docs-cn.readthedocs.io/zh-cn/latest/index.html) |
| LLM-8850 Card | <img src="docs/boards/AI-001_LLM-8850-main-pictures_01.jpg" width="200"> | AX650N / AX8850 | M5Stack | [Docs](https://docs.m5stack.com/en/ai_hardware/LLM-8850_Card) |
| LLM630 Compute Kit | <img src="docs/boards/LLM630_Compute_kit.jpg" width="200"> | AX630C | M5Stack | [Docs](https://docs.m5stack.com/en/core/LLM630%20Compute%20Kit) |
| Module LLM | <img src="docs/boards/module_LLM.jpg" width="200"> | AX630C | M5Stack | [Docs](https://docs.m5stack.com/en/module/Module-LLM) |

## Quick Start

### Compile

- [Compilation Guide](docs/compile.md) — Simple cross-platform compilation based on **cmake**.

### Samples

- [examples](examples/) — Common deep learning open-source algorithms including classification, detection, pose estimation, and traditional CV operations. Continuously updated based on community feedback.

### Resources

- **ModelZoo**, **pre-compiled binaries**, **test images** and more:
  - [Huggingface](https://huggingface.co/collections/AXERA-TECH/vision-models-67b0bce92ddc61229e8e94ed)
  - [Modelscope](https://modelscope.cn/organization/AXERA-TECH)

### Benchmark

- [Benchmark](benchmark/) — Inference time statistics for common open-source models, measured on *AXera-Pi Pro* and *AXera-Pi Zero*.

## Related Projects

- NPU toolchain online documentation with usage instructions and download links:
  - [Pulsar2 Online Docs](https://pulsar2-docs.readthedocs.io/zh_CN/latest/) (Supports AX650A/AX650N/AX630C/AX620Q)
- Recommended: download the latest Pulsar2 toolchain and documentation directly:
  - [Pulsar2 Download](https://huggingface.co/AXERA-TECH/Pulsar2)

## Technical Discussion

- Github issues
- QQ Group: 139953715
