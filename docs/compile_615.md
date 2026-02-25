# 源码编译（AX615）

ax-samples 的源码编译支持两种常见路径：

- **本地编译**：由于开发板集成了完整的Linux系统，可以预装必要的 gcc、cmake 等开发环境，因此可以在开发板上直接完成源码编译；
- **交叉编译**：在 x86 PC 的常规开发环境中，通过对应的交叉编译工具链完成对源码的编译

## 本地编译

- 适用对象：具有完整 rootfs、可联网的 AX615 开发板/模组；

### 环境搭建

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
```

若板端 BSP 已自带 OpenCV，可跳过 `libopencv-dev` 安装。确认 `/opt/axera`（或自行放置的 BSP）下包含 `msp/out/arm_glibc` 目录，用于提供头文件与库。

### 编译流程

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build_ax615_native && cd build_ax615_native
cmake -DBSP_MSP_DIR=/opt/axera/msp/out/arm_glibc -DAXERA_TARGET_CHIP=ax615 ..
make -j$(nproc)
make install
```

构建完成后，可执行示例位于 `ax-samples/build_ax615_native/install/ax615/`。

## 交叉编译

### 3rdparty / BSP 准备

1. 下载预编译好的 OpenCV arm-linux-gnueabihf 库（从 [Releases](https://github.com/GuoFM/ax-samples/releases) 获取 `opencv-arm-linux-gnueabihf.zip`）；
2. 在仓库根目录创建 `3rdparty`，解压 OpenCV 包到 `3rdparty/opencv-arm-linux`，目录示例：

```bash
ax-samples$ tree -L 2 3rdparty
3rdparty
└── opencv-arm-linux
    ├── bin
    ├── include
    ├── lib
    └── share
```

3. AX615 BSP/SDK 从 [Releases](https://github.com/GuoFM/ax-samples/releases) 下载 `ax615_bsp_sdk.tar.gz`，解压后：

```bash
tar -zxvf ax615_bsp_sdk.tar.gz
export AX615_BSP=$(pwd)/msp/out/arm_glibc
```

`AX615_BSP` 环境变量会被 CMake 中的 `BSP_MSP_DIR` 传递给 `cmake/ax615.cmake`，用于找到 `${BSP_MSP_DIR}/include`、`${BSP_MSP_DIR}/lib`。

### 编译环境

- cmake ≥ 3.13；
- 已安装 `arm-linux-gnueabihf-gcc/g++`（可通过 `apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf` 安装）；
- host 为常规 x86_64 Ubuntu / Debian 即可。

### 源码编译

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build_ax615 && cd build_ax615

cmake \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake \
  -DBSP_MSP_DIR=${AX615_BSP} \
  -DAXERA_TARGET_CHIP=ax615 \
  ..

make -j$(nproc)
make install
```

## 构建产物

交叉编译完成后，生成的可执行示例位于 `ax-samples/build_ax615/install/ax615/`：

```bash
build_ax615$ tree install/ax615 -L 1
install/ax615
├── ax_classification
├── ax_yolov5_face
├── ax_yolov5s
├── ax_yolov5s_seg
├── ax_yolov8
├── ax_yolov8_pose
├── ax_yolov8_seg
├── ax_yolo11
├── ax_yolo11_pose
└── ax_yolo11_seg
```

针对自定义示例，可在 `examples/ax615/CMakeLists.txt` 中新增 `axera_example(...)`，再次执行 `make install` 即可在同目录生成对应可执行文件。
