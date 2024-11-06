English | [简体中文](./README.md)

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)

| Platform | Build Status |
| -------- | ------------ |
| AX650N   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_650.yml)|
| AX630C   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_630c_glibc.yaml)|
| AX620Q   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620q_uclibc.yaml)|
| AX620A   | ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/AXERA-TECH/ax-samples/build_620a.yml)|

## Intro

**AX-Samples** by **[Axera](https://www.axera-tech.com/)** dominate the development. The project has implemented the sample code of the most common **deep learning open source algorithm** on the **AI SoC** of **Axera**, which is convenient for community developers to quickly evaluate and adapt.

Support SoC

- [AX630C](docs/AX630C.md)/[AX620Q](docs/AX620Q.md)
- [AX650A](docs/AX650A.md)/[AX650N](docs/AX650N.md)
- [AX620A](docs/AX620A.md)/[AX620U](docs/AX620U.md)
- [AX630A](docs/AX630A.md)

Support Board

- [AXera-Pi](https://wiki.sipeed.com/m3axpi)(AX620A)
- [AXera-Pi Pro](https://wiki.sipeed.com/m4ndock)(AX650N)
- [AXera-Pi Zero](https://axera-pi-zero-docs-cn.readthedocs.io/zh-cn/latest/index.html)(AX620Q)

## Quick Start

### Compile

- [easy compile](docs/compile.md) Simple cross-platform compilation based on **cmake**.

### Samples

- [examples](examples/) It provides open source deep learning algorithms such as common classification, detection, posture and traditional CV operation use cases, which are continuously updated according to the needs of the issue.

### Online Disk

- Provide **ModelZoo**, **pre-compiler bin**, **test pictures** and so on:
  - [BaiDu Disk](https://pan.baidu.com/s/1cnMeqsD-hErlRZlBDDvuoA?pwd=oey4)
  - [Google Drive](https://drive.google.com/drive/folders/1JY59vOFS2qxI8TkVIZ0pHfxHMfKPW5PS?usp=sharing)

### Benchmark

- [Benchmark](benchmark/) Common open source model inference time statistics, based on *AXera-Pi*, *AXera-Pi Pro*, *AXera-Pi Zero* measured.

## Related project

- NPU Tool Chain Online document, which provides instructions for using the NPU tool chain and how to obtain it.
  - [Pulsar](https://pulsar-docs.readthedocs.io/zh_CN/latest/)(Support AX630A/AX620A/AX620U)
  - [Pulsar2](https://pulsar2-docs.readthedocs.io/zh_CN/latest/)(Support AX650A/AX650N/AX630C/AX620Q)

## Technical discussion

- Github issues
- QQ Group: 139953715
