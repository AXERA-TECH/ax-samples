# 源码编译（Linux）

## 交叉编译 Arm32 版本

### 编译环境
- cmake 版本大于等于 3.13
- AX620A 配套的交叉编译工具链 `arm-linux-gnueabihf-gxx` 已添加到环境变量中

### 下载源码

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
```

### 安装交叉编译工具链

Arm32 Linux 交叉编译工具链为：

```bash
sudo apt install g++-arm-linux-gnueabihf
```

### BSP 准备

请联系 FAE 获取 AX620 BSP 开发包，执行如下操作
```
tar -zxvf AX620_SDK_XXX.tgz
cd AX620_SDK_XXX/package
tar -zxvf msp.tgz
```

### 3rdparty 准备

- **[下载](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-arm-linux-gnueabihf-gcc-7.5.0.zip)** 预编译好的 OpenCV 库文件；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中。

### 源码编译
进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
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
