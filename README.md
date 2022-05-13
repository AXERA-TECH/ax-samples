# AX-Samples

## 简介

`AX-Samples` 由 **[爱芯元智](https://www.axera-tech.com/)** 主导开发。该项目实现了常见的 **深度学习开源算法** 在 **爱芯元智** 的 **AI SoC** 上的示例代码，方便社区开发者进行快速评估和适配。 

目前已支持以下芯片平台

- AX620A
- AX620U

## 示例分类

### NPU 模型推理：

- 分类模型

  - MobileNetv1
  - MobileNetv2
  - ResNet18
  - ResNet50

- 检测模型

  - YOLOv3
  - YOLOv3-Tiny
  - YOLOv4
  - YOLOv4-Tiny
  - YOLOv5s
  - YOLOX-S
  - YOLO-Fastest-XL
  - YOLO-Fastest-Body
  - NanoDet

- 姿态模型

  - HRNet

### NPU CV 

- CropResize

### Pipeline

- NV12 -> CropResize -> NN(Classification)


## 目录说明

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
│   ├── ax_classification_steps.cc              # 分类模型
│   ├── ax_crop_resize_nv12.cc
│   ├── ax_hrnet_steps.cc                       # 人体关键点
│   ├── ax_nanodet_steps.cc                     # 物体检测
│   ├── ax_paddle_yolov3_steps.cc               # PaddleDetection 物体检测
│   ├── ax_yolo_fastest_body_steps.cc           # 人体检测
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

## 使用指南

### 编译环境
- cmake 版本大于等于 3.13
- AX620A 配套的交叉编译工具链 `arm-linux-gnueabihf-gxx` 已添加到环境变量中

### BSP 准备

请联系 FAE 获取 AX620 BSP 开发包，执行如下操作
```
tar -zxvf AX620_SDK_XXX.tgz
cd AX620_SDK_XXX/package
tar -zxvf msp.tgz
```

### 3rdparty 准备

- **[下载](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)** 预编译好的 OpenCV 库文件；
- 在 ax-samaples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中。

### 源码编译
进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake -DBSP_MSP_DIR=${AX620_SDK_XXX}/msp/out/ ..
$ make install
```

编译完成后，生成的可执行示例存放在 build/install/bin/ 文件夹中：

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

## 运行示例

### 运行准备

登录 AX620A 开发板，在 `root` 路径下创建 `samples` 文件夹。

- 将上一章节 `build/install/bin/` 中编译生成的可执行示例拷贝到 `/root/samples/` 路径下；
- 将 **[ModelZoo](https://pan.baidu.com/s/1zm2M-vqiss4Rmk-uSoGO7w)** (*pwd: euy7*) 中相应的 **joint** 模型 `mobilenetv2.joint` 、 `yolov5s.joint` 拷贝到  `/root/samples/` 路径下；
- 将测试图片拷贝到 `/root/samples` 路径下。

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

### 运行示例

#### 分类模型

```
/root/sample # ./ax_classification -m mobilenetv2.joint -i cat.jpg -r 10
--------------------------------------
model file : mobilenetv2.joint
image file : cat.jpg
img_h, img_w : 224 224
Run-Joint Runtime version: 0.5.1
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.5.34.2
7ca3b9d5
10.6865, 285
10.3324, 283
9.1559, 281
9.1174, 356
9.0098, 282
--------------------------------------
Create handle took 229.16 ms (neu 6.39 ms, axe 11.45 ms, overhead 211.32 ms)
--------------------------------------
Repeat 10 times, avg time 5.35 ms, max_time 6.50 ms, min_time 5.17 ms
```

#### 检测模型

```
/root/qtang # ./ax_yolov5s -m yolov5s.joint -i dog.jpg -r 10
--------------------------------------
model file : yolov5s.joint
image file : dog.jpg
img_h, img_w : 640 640
Run-Joint Runtime version: 0.5.1
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.5.34.2
7ca3b9d5
run over: output len 3
--------------------------------------
Create handle took 3454.99 ms (neu 531.28 ms, axe 15.85 ms, overhead 2907.86 ms)
--------------------------------------
Repeat 10 times, avg time 123.89 ms, max_time 167.28 ms, min_time 118.25 ms
--------------------------------------
detection num: 3
16:  92%, [ 135,  209,  310,  545], dog
 2:  77%, [ 470,   78,  691,  174], car
 1:  54%, [ 156,  122,  572,  420], bicycle
```

## 技术讨论

- Github issues
- QQ 群: 139953715
- Email: Business@axera-tech.com (商务合作)
