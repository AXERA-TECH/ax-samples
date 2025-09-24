[English](./README_EN.md) | 简体中文

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)

| Platform | Build Status |
| -------- | ------------ |
| AX650N   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_650.yml)|
| AX630C   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_630c_glibc.yaml)|
| AX620Q   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620q_uclibc.yaml)|
| AX620A   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620a.yml)|

## 简介

**AX-Samples** 由 **[爱芯元智](https://www.axera-tech.com/)** 主导开发。该项目实现了常见的 **深度学习开源算法** 在 **爱芯元智** 的 **AI SoC** 上的示例代码，方便社区开发者进行快速评估和适配。 

已支持芯片

- [AX630C](docs/AX630C.md)/[AX620Q](docs/AX620Q.md)
- [AX650A](docs/AX650A.md)/[AX650N](docs/AX650N.md)
- [AX620A](docs/AX620A.md)/[AX620U](docs/AX620U.md)
- [AX630A](docs/AX630A.md)

已支持开发板

- [AXera-Pi](https://wiki.sipeed.com/m3axpi)(AX620A)
- [AXera-Pi Pro](https://wiki.sipeed.com/m4ndock)(AX650N)
- [AXera-Pi Zero](https://axera-pi-zero-docs-cn.readthedocs.io/zh-cn/latest/index.html)(AX620Q)

## 快速上手

### 编译

- [快速编译](docs/compile.md) 基于 cmake 实现简单的跨平台编译。

### 示例

- [examples](examples/) 提供常见分类、检测、姿态等深度学习开源算法和传统 CV 操作用例，根据 issue 需求持续更新。

### 网盘资源

- 提供 **ModelZoo**, **预编译程序**, **测试图片** 等内容:
  - [huggingface](https://huggingface.co/collections/AXERA-TECH/vision-models-67b0bce92ddc61229e8e94ed)

### 速度评估

- [Benchmark](benchmark/) 常见开源模型推理耗时统计，基于 *AXera-Pi* 、 *AXera-Pi Pro* 、*AXera-Pi Zero* 实测。

## 关联项目

- NPU工具链在线文档，其中提供了NPU工具链相关使用说明和获取方式（更新不及时）
  - [Pulsar2在线文档](https://pulsar2-docs.readthedocs.io/zh_CN/latest/)(Support AX650A/AX650N/AX630C/AX620Q)
- 更推荐直接从下面获取最新 Pulsar2 工具链及文档
 - [Pulsar2 下载](https://huggingface.co/AXERA-TECH/Pulsar2)

## 技术讨论

- Github issues
- QQ 群: 139953715
