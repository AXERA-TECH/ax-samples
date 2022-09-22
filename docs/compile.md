# 源码编译（Linux）

ax-samples 的源码编译目前有两种实现路径：

- 基于 AX-Pi 的本地编译，因为 AX-Pi 上集成的完成了软件开发环境，操作简单；
- 嵌入式 Linux 交叉编译。

## 本地编译

### 硬件需求

- AX-Pi（基于 AX620A，面向社区开发者的高性价比开发板）

### 编译过程

gitclone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ git clone https://github.com/AXERA-TECH/ax-samples.git
$ cd ax-samples
$ mkdir build
$ cd build
$ cmake ..
$ make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/bin/` 路径下：

```bash
ax-samples/build$ tree install
install
└── bin
    ├── ax_classification
    ├── ax_classification_accuracy
    ├── ax_classification_nv12
    ├── ax_cv_test
    ├── ax_hrnet
    ├── ax_models_load_inspect
    ├── ax_monodlex
    ├── ax_nanodet
    ├── ax_paddle_mobilehumseg
    ├── ax_paddle_mobileseg
    ├── ax_paddle_yolov3
    ├── ax_robot_obstacle
    ├── ax_scrfd
    ├── ax_yolo_fastest
    ├── ax_yolo_fastest_body
    ├── ax_yolov3
    ├── ax_yolov3_accuracy
    ├── ax_yolov3_tiny
    ├── ax_yolov4
    ├── ax_yolov4_tiny
    ├── ax_yolov4_tiny_3l
    ├── ax_yolov5s
    ├── ax_yolov5s_620u
    ├── ax_yolov7
    └── ax_yoloxs
```

## 交叉编译

### 3rdparty 准备

- 下载预编译好的 OpenCV 库文件 [[AX620A/U 匹配](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)] [[AX630A 匹配](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-aarch64-linux-gnu-gcc-7.5.0.zip)]；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中。

### 交叉编译 armv7a 版本

针对 **AX620A** 和 **AX620U** 硬件平台

#### 编译环境
- cmake 版本大于等于 3.13
- AX620A/U 配套的交叉编译工具链 `arm-linux-gnueabihf-gxx` 已添加到环境变量中

#### 安装交叉编译工具链

- Arm32 Linux 交叉编译工具链建议使用同 BSP 一致的版本：

- [下载地址](http://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz)

#### BSP 准备

请联系 FAE 获取 AX620 BSP 开发包，执行如下操作
```
tar -zxvf AX620_SDK_XXX.tgz
cd AX620_SDK_XXX/package
tar -zxvf msp.tgz
```

#### 源码编译
gitclone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ git clone https://github.com/AXERA-TECH/ax-samples.git
$ cd ax-samples
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake -DBSP_MSP_DIR=${AX620_SDK_XXX}/msp/out/ ..
$ make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/bin/` 路径下：

```bash
ax-samples/build$ tree install
install
└── bin
    ├── ax_classification
    ├── ax_classification_accuracy
    ├── ax_classification_nv12
    ├── ax_cv_test
    ├── ax_hrnet
    ├── ax_models_load_inspect
    ├── ax_monodlex
    ├── ax_nanodet
    ├── ax_paddle_mobilehumseg
    ├── ax_paddle_mobileseg
    ├── ax_paddle_yolov3
    ├── ax_robot_obstacle
    ├── ax_scrfd
    ├── ax_yolo_fastest
    ├── ax_yolo_fastest_body
    ├── ax_yolov3
    ├── ax_yolov3_accuracy
    ├── ax_yolov3_tiny
    ├── ax_yolov4
    ├── ax_yolov4_tiny
    ├── ax_yolov4_tiny_3l
    ├── ax_yolov5s
    ├── ax_yolov5s_620u
    ├── ax_yolov7
    └── ax_yoloxs
```

### 交叉编译 aarch64 版本

针对 **AX630A** 的硬件平台

#### 编译环境
- cmake 版本大于等于 3.13
- AX630A 配套的交叉编译工具链 `aarch64-linux-gnu-gxx` 已添加到环境变量中

#### 安装交叉编译工具链

- aarch64 Linux 交叉编译工具链建议使用同 BSP 一致的版本：

- [下载地址](http://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/aarch64-linux-gnu/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz)

#### BSP 准备

请联系 FAE 获取 AX630 BSP 开发包，执行如下操作
```
tar -zxvf AX630_SDK_XXX.tgz
cd AX630_SDK_XXX/package
tar -zxvf msp.tgz
```

#### 源码编译
gitclone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ git clone https://github.com/AXERA-TECH/ax-samples.git
$ cd ax-samples
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-linux-gnu.toolchain.cmake -DBSP_MSP_DIR=${AX630_SDK_XXX}/msp/out/ -DAXERA_TARGET_CHIP=ax630a ..
$ make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/bin/` 路径下：

```bash
ax-samples/build$ tree install
install
└── bin
    ├── ax_classification
    ├── ax_yolo_fastest
    ├── ax_yolov3
    └── ax_yolov5s
```
