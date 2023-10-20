# 源码编译（AX620）

ax-samples 的源码编译目前有两种实现路径：

- 基于 AX-Pi 的本地编译，因为 AX-Pi 上集成的完成了软件开发环境，操作简单；
- 嵌入式 Linux 交叉编译。

## 本地编译

### 硬件需求

- 爱芯派（基于AX620A）

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

- 下载预编译好的 OpenCV 库文件 [[AX620A/U 匹配](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)]]；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中。

### 交叉编译 armv7a 版本

针对 **AX620A** 和 **AX620U** 硬件平台

#### 编译环境
- cmake 版本大于等于 3.13
- AX620A/U 配套的交叉编译工具链 `arm-linux-gnueabihf-gxx` 已添加到环境变量中

#### 安装交叉编译工具链

- Arm32 Linux 交叉编译工具链[获取地址](http://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz)

#### 依赖库准备

- 下载 **ax-samples** 交叉编译依赖库文件并解压到指定路径 `ax_bsp`, [依赖库获取地址](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.3/arm_axpi_r1.22.2801.zip)，并将其目录设置到临时环境变量 `$ax_bsp`

```
wget https://github.com/AXERA-TECH/ax-samples/releases/download/v0.3/arm_axpi_r1.22.2801.zip
unzip arm_axpi_r1.22.2801.zip -d ax_bsp
export ax_bsp=$PWD/ax_bsp
echo $ax_bsp
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
