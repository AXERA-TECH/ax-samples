简体中文 | [English](./README_EN.md)

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)
[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build.yml?branch=main)](https://github.com/AXERA-TECH/ax-samples/actions)

## 简介

**AX-Samples** 由 **[爱芯元智](https://www.axera-tech.com/)** 主导开发。该项目实现了常见的 **深度学习开源算法** 在 **爱芯元智** 的 **AI SoC** 上的示例代码，方便社区开发者进行快速评估和适配。 

已支持芯片

- [AX630A](docs/AX630A.md)
- [AX620A](docs/AX620A.md)
- [AX620U](docs/AX620U.md)

已支持开发板

- [AXera-Pi](https://wiki.sipeed.com/m3axpi)(AX620A)

## 快速上手

### 编译

- [快速编译](docs/compile.md) 基于 cmake 实现简单的跨平台编译。

### 示例

- [examples](examples/) 提供常见分类、检测、姿态等深度学习开源算法和传统 CV 操作用例，根据 issue 需求持续更新。

### 网盘资源

- 通过[百度网盘](https://pan.baidu.com/s/1ZHW2P6Y3lPf2odmj3fo8hA?pwd=sow9)获取，提供 **ModelZoo**, **预编译程序**, **测试图片** 等内容。

### 速度评估

- [Benchmark](benchmark/) 常见开源模型推理耗时统计，基于 *AXera-Pi* 实测。

## 联动项目

- [ax-pipeline](https://github.com/AXERA-TECH/ax-pipeline) 该项目基于 AXera-Pi 展示 ISP、图像处理、NPU、编码、显示 等功能模块软件调用方法，方便社区开发者进行快速评估和二次开发自己的多媒体应用
- [ax-models](https://github.com/AXERA-TECH/ax-models) examples 示例中部分预编译好的 NPU 模型
- [NPU 工具链在线文档](https://pulsar-docs.readthedocs.io/zh_CN/latest/)

## 技术讨论

- Github issues
- QQ 群: 139953715
