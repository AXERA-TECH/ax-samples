# 源码编译（AX615）

ax-samples 的源码编译目前有一种实现路径：

- **交叉编译**：在 x86 PC 的常规开发环境中，通过对应的交叉编译工具链完成对源码的编译。

> **注意**：由于 AX615 是 AX620Q 的 costdown 版本，采用 SIP DDR 设计，当前阶段并无完整 Linux 文件系统的产品支持，因此不会存在本地编译的需求。

## 交叉编译

### 3rdparty / BSP 准备

1. 下载预编译好的 OpenCV uclibc arm 库（从 [Releases](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.6/opencv-arm-uclibc-linux.zip) 获取 `opencv-arm-uclibc-linux.zip`）；
2. 在仓库根目录创建 `3rdparty`，解压 OpenCV 包到 `3rdparty/opencv-arm-uclibc-linux`，目录示例：

```bash
ax-samples$ tree -L 2 3rdparty
3rdparty
└── opencv-arm-uclibc-linux
    ├── bin
    ├── include
    ├── lib
    └── share
```

3. AX615 BSP/SDK 从 [Releases](https://github.com/AXERA-TECH/ax-samples/releases) 下载 `ax615_bsp_sdk.tar.gz`，解压后：

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
  -DOpenCV_DIR=../3rdparty/opencv-arm-uclibc-linux/lib/cmake/opencv4 \
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
