# 源码编译（AX637）

ax-samples 的源码编译支持两种常见路径：

- **本地编译**：由于开发板集成了完整的Linux系统，可以预装必要的 gcc、cmake 等开发环境，因此可以在开发板上直接完成源码编译；
- **交叉编译**：在 x86 PC 的常规开发环境中，通过对应的交叉编译工具链完成对源码的编译

## 本地编译

- 适用对象：具有完整 rootfs、可联网的 AX637 开发板/模组；

### 环境搭建

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
```

若板端 BSP 已自带 OpenCV，可跳过 `libopencv-dev` 安装。确认 `/opt/axera`（或自行放置的 BSP）下包含 `msp/out` 目录，用于提供头文件与库。

### 编译流程

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build_ax637_native && cd build_ax637_native
cmake -DBSP_MSP_DIR=/opt/axera/msp/out -DAXERA_TARGET_CHIP=ax637 ..
make -j$(nproc)
make install
```

构建完成后，可执行示例位于 `ax-samples/build_ax637_native/install/ax637/`。

## 交叉编译

### 3rdparty / BSP 准备

1. 下载预编译好的 [OpenCV 库](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-aarch64-linux-gnu-gcc-7.5.0.zip)；
2. 在仓库根目录创建 `3rdparty`，解压 OpenCV 包到 `3rdparty/opencv-aarch64-linux`，目录示例：

```bash
ax-samples$ tree -L 2 3rdparty
3rdparty
└── opencv-aarch64-linux
    ├── bin
    ├── include
    ├── lib
    └── share
```

3. AX637 BSP/SDK 请联系FAE获取，获取后执行：

```bash
tar -zxvf AX637_SDK_XXX.tgz
cd AX637_SDK_XXX/package
tar -zxvf msp.tgz
export AX637_BSP=$(pwd)/msp/out
```

`AX637_BSP` 环境变量会被 CMake 中的 `BSP_MSP_DIR` 传递给 `cmake/ax637.cmake`，用于找到 `${BSP_MSP_DIR}/include`、`${BSP_MSP_DIR}/lib`。

### 编译环境

- cmake ≥ 3.13；
- 已将 `aarch64-none-linux-gnu-gcc/g++` 添加到 `PATH`；
- host 为常规 x86_64 Ubuntu / Debian 即可。

#### 工具链获取

```bash
mkdir -p ~/toolchains && cd ~/toolchains
wget https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
tar -xf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
export PATH=$PWD/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin:$PATH
```

### 源码编译

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build_ax637 && cd build_ax637

cmake \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-none-linux-gnu.toolchain.cmake \
  -DBSP_MSP_DIR=${AX637_BSP} \
  -DAXERA_TARGET_CHIP=ax637 \
  ..

make -j$(nproc)
make install
```


## 构建产物

交叉编译完成后，生成的可执行示例位于 `ax-samples/build_ax637/install/ax637/`：

```bash
build_ax637$ tree install/ax637 -L 1
install/ax637
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

针对自定义示例，可在 `examples/ax637/CMakeLists.txt` 中新增 `axera_example(...)`，再次执行 `make install` 即可在同目录生成对应可执行文件。

