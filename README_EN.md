English | [简体中文](./README.md)

# AX-Samples

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](https://raw.githubusercontent.com/AXERA-TECH/ax-samples/main/LICENSE)

## Introduction

**AX-Samples** is developed by **[Axera](https://www.axera-tech.com/)** . This project implements the sample code of common **deep learning open source algorithms** on **Axera's** **AI SoC** , which is convenient for community developers to quickly evaluate and adapt.

The following chip platforms are currently supported

- AX620A
- AX620U

## Example Classification

### NPU Inference Models：

- Classification Models:

  - MobileNetv1
  - MobileNetv2
  - ResNet18
  - ResNet50

- Detection Models:

  - YOLOv3
  - YOLOv3-Tiny
  - YOLOv4
  - YOLOv4-Tiny
  - YOLOv5s
  - YOLOX-S
  - YOLO-Fastest-XL
  - YOLO-Fastest-Body
  - NanoDet

- Pose Models:

  - HRNet

### NPU CV 

- CropResize

### Pipeline

- NV12 -> CropResize -> NN(Classification)


## Directory Description

```bash
$ tree -L 2
.
├── CMakeLists.txt
├── README.md
├── cmake
│   ├── check.cmake
│   └── summary.cmake
├── examples
│   ├── CMakeLists.txt
│   ├── ax_classification_nv12_resize_steps.cc
│   ├── ax_classification_steps.cc              # Classification Model
│   ├── ax_crop_resize_nv12.cc
│   ├── ax_hrnet_steps.cc                       # Human key points
│   ├── ax_nanodet_steps.cc                     # Object detection
│   ├── ax_paddle_yolov3_steps.cc               # PaddleDetection object detection
│   ├── ax_yolo_fastest_body_steps.cc           # Human detection
│   ├── ax_yolo_fastest_steps.cc
│   ├── ax_yolov3_steps.cc                      # YOLOv3
│   ├── ax_yolov3_tiny_steps.cc
│   ├── ax_yolov4_steps.cc                      # YOLOv4
│   ├── ax_yolov4_tiny_steps.cc
│   ├── ax_yolov5s_steps.cc                     # YOLOv5
│   ├── ax_yoloxs_steps.cc                      # YOLOX
│   ├── base
│   ├── cv
│   ├── middleware
│   └── utilities
└── toolchains
    └── arm-linux-gnueabihf.toolchain.cmake
```

## User Guide

### Compiler Environment
- cmake version must be greater than 3.13
- The cross-compilation toolchain `arm-linux-gnueabihf-gxx` for AX620A has been added to the environment variable

### BSP preparation

Please contact FAE to obtain the AX620 BSP development kit, do the following
```
tar -zxvf AX620_SDK_XXX.tgz
cd AX620_SDK_XXX/package
tar -zxvf msp.tgz
```

### 3rdparty preparation

- **[Download](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)** Precompiled OpenCV library files；
- Create a 3rdparty file in ax-samaples and unzip the downloaded OpenCV library file into this folder.

### Source code compilation
Go to the ax-samples root directory and create a cmake compilation task:

```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake -DBSP_MSP_DIR=${AX620_SDK_XXX}/msp/out/ ..
$ make install
```

After compiling, the generated executable examples are stored in the build/install/bin/ folder:

```bash
/console/build$ tree install
install
└── bin
    ├── ax_classification
    ├── ax_classification_nv12
    ├── ax_cv_test
    ├── ax_hrnet
    ├── ax_nanodet
    ├── ax_yolo_fastest
    ├── ax_yolo_fastest_body
    ├── ax_yolov3
    ├── ax_yolov3_tiny
    ├── ax_yolov4
    ├── ax_yolov4_tiny
    ├── ax_yolov5s
    └── ax_yoloxs
```

## Running Case Example

### Running preparation

Log in to the AX620A development board and create a `samples` folder in the `root` path.

- Copy the executable samples compiled and generated in the previous chapter `build/install/bin/` to the `/root/samples/` path;
- Copy the corresponding **joint** models `mobilenetv2.joint`, `yolov5s.joint` in **[ModelZoo](https://pan.baidu.com/s/1zm2M-vqiss4Rmk-uSoGO7w)** (*pwd: euy7*)  to `/root/samples/` path;
- Copy the test images to the `/root/samples` path.

```
/root/sample # ls -l
total 40644
-rwx--x--x    1 root     root       3805332 Mar 22 14:01 ax_classification
-rwx--x--x    1 root     root       3979652 Mar 22 14:01 ax_yolov5s
-rw-------    1 root     root        140391 Mar 22 10:39 cat.jpg
-rw-------    1 root     root        163759 Mar 22 14:01 dog.jpg
-rw-------    1 root     root       4299243 Mar 22 14:00 mobilenetv2.joint
-rw-------    1 root     root      29217004 Mar 22 14:04 yolov5s.joint
```

### Running Case Example

#### Classification model

```
/root/samples # ./ax_classification -m mobilenetv2.joint -i cat.jpg -r 100
--------------------------------------
model file : mobilenetv2.joint
image file : cat.jpg
img_h, img_w : 224 224
Run-Joint Runtime version: 0.5.6
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.22
2ed4ac96
10.6865, 285
10.3324, 283
9.1559, 281
9.1174, 356
9.0098, 282
--------------------------------------
Create handle took 198.25 ms (neu 6.09 ms, axe 0.00 ms, overhead 192.15 ms)
--------------------------------------
Repeat 100 times, avg time 4.33 ms, max_time 4.88 ms, min_time 4.31 ms
```

#### Detection model

```
/root/samples # ./ax_yolov5s -m yolov5s.joint -i dog.jpg -r 100
--------------------------------------
model file : yolov5s.joint
image file : dog.jpg
img_h, img_w : 640 640
Run-Joint Runtime version: 0.5.6
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.22
2ed4ac96
run over: output len 3
--------------------------------------
Create handle took 408.67 ms (neu 16.94 ms, axe 19.86 ms, overhead 371.88 ms)
--------------------------------------
Repeat 100 times, avg time 61.90 ms, max_time 237.61 ms, min_time 59.92 ms
--------------------------------------
detection num: 3
16:  92%, [ 135,  209,  310,  545], dog
 2:  77%, [ 470,   78,  691,  174], car
 1:  54%, [ 156,  122,  572,  420], bicycle
```

## Technical discussions

- Github issues
- QQ Group: 139953715
- Email: Business@axera-tech.com (Business Cooperation)
