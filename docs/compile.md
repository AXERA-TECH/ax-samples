# 源码编译（Linux）

ax-samples 的源码编译目前有两种实现路径：

- 基于 AX-Pi 的本地编译，因为 AX-Pi 上集成的完成了软件开发环境，操作简单；
- 嵌入式 Linux 交叉编译。

# 目录
[本地编译](#本地编译)
- [本地编译](#本地编译)

[交叉编译](#交叉编译)

- [AX620A/U](compile.md#L80)

- [AX630A](compile.md#L146)

- [AX650](compile.md#L190)

## 本地编译

### 硬件需求

- AX-Pi（基于 AX620A，面向社区开发者的高性价比开发板）
- AX650

### 编译过程

git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

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

- 下载预编译好的 OpenCV 库文件 [[AX620A/U 匹配](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)] [[AX630A 匹配](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-aarch64-linux-gnu-gcc-7.5.0.zip)] [[AX650 匹配]()]；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中。

### 交叉编译 armv7a 版本

针对 **AX620A** 和 **AX620U** 硬件平台

#### 编译环境
- cmake 版本大于等于 3.13
- AX620A/U 配套的交叉编译工具链 `arm-linux-gnueabihf-gxx` 已添加到环境变量中

#### 安装交叉编译工具链

- Arm32 Linux 交叉编译工具链[获取地址](http://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz)

#### 依赖库准备

- 下载 **ax-samples** 交叉编译依赖库文件并解压到指定路径 `ax_bsp`, [依赖库获取地址](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.3/arm_axpi_r1.22.2801.zip)

```
wget https://github.com/AXERA-TECH/ax-samples/releases/download/v0.3/arm_axpi_r1.22.2801.zip
unzip arm_axpi_r1.22.2801.zip -d ax_bsp
```

#### 源码编译
git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ git clone https://github.com/AXERA-TECH/ax-samples.git
$ cd ax-samples
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake -DBSP_MSP_DIR=${ax_bsp}/ ..
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

- aarch64 Linux 交叉编译工具链[获取地址](http://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/aarch64-linux-gnu/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu.tar.xz)

#### BSP 准备

请联系 FAE 获取 AX630 BSP 开发包，执行如下操作
```
tar -zxvf AX630_SDK_XXX.tgz
cd AX630_SDK_XXX/package
tar -zxvf msp.tgz
```

#### 源码编译
git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

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

### 交叉编译 aarch64 版本

针对 **AX650** 硬件平台

#### 编译环境
- cmake 版本大于等于 3.13
- AX650 配套的交叉编译工具链 `aarch64-none-linux-gnu-gcc` 已添加到环境变量中

#### 安装交叉编译工具链

- aarch64 Linux 交叉编译工具链 [获取地址](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz)

#### 依赖库准备

请联系 FAE 获取 AX650 BSP 开发包，执行如下操作

#### 源码编译
git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
$ git clone https://github.com/AXERA-TECH/ax-samples.git
$ cd ax-samples
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-none-linux-gnu.toolchain.cmake -DBSP_MSP_DIR=${ax_bsp}/ -DAXERA_TARGET_CHIP=ax650 ..
$ make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/ax650/` 路径下：

```bash
ax-samples/build$ tree install
install
└── ax650
    ├── ax_classification
    ├── ax_pp_humanseg
    ├── ax_pp_liteseg_stdc2_cityscapes
    ├── ax_pp_ocr_rec
    ├── ax_pp_person_attribute
    ├── ax_pp_vehicle_attribute
    ├── ax_ppyoloe
    ├── ax_ppyoloe_obj365
    └── ax_yolov5s
```