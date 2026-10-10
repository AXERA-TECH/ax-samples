# 源码编译（AX8860）

AX8860 板端为 Buildroot 精简系统，不带 gcc / cmake，因此采用 **交叉编译**：在 x86_64 Linux 主机上编译，再将可执行文件拷贝到开发板运行。

## 1. 准备

### 1.1 交叉编译工具链

AX8860 BSP 中的 `libax_*.so` 基于 **glibc 2.38** 构建，工具链自带的 glibc 必须 **≥ 2.38**，否则链接时会报
`undefined reference to '__isoc23_strtol@GLIBC_2.38'`。推荐使用 Arm GNU Toolchain 13.2.Rel1（glibc 2.38，与板端一致）：

```bash
mkdir -p ~/toolchains && cd ~/toolchains
wget https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-aarch64-none-linux-gnu.tar.xz
tar -xf arm-gnu-toolchain-13.2.rel1-x86_64-aarch64-none-linux-gnu.tar.xz
export PATH=$PWD/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
```

> gcc-arm-9.2 / 11.2 等旧工具链（glibc ≤ 2.34）无法链接 AX8860 BSP。

### 1.2 OpenCV

下载预编译好的 [OpenCV 库](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-aarch64-linux-gnu-gcc-7.5.0.zip)，解压到仓库根目录的 `3rdparty/opencv-aarch64-linux`：

```bash
ax-samples$ tree -L 2 3rdparty
3rdparty
└── opencv-aarch64-linux
    ├── bin
    ├── include
    ├── lib
    └── share
```

### 1.3 BSP

使用 AX8860 的 `arm64_glibc` BSP 包，解压后目录下应包含 `include/`（`ax_engine_api.h`、`ax_sys_api.h` 等）与 `lib/`（`libax_engine.so`、`libax_sys.so` 等）：

```bash
unzip arm64_glibc.zip
export AX8860_BSP=$(pwd)/arm64_glibc
```

## 2. 编译

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build_ax8860 && cd build_ax8860
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-none-linux-gnu.toolchain.cmake \
      -DBSP_MSP_DIR=${AX8860_BSP} \
      -DAXERA_TARGET_CHIP=ax8860 ..
make -j$(nproc)
make install
```

编译完成后，可执行示例位于 `build_ax8860/install/ax8860/`：

```bash
build_ax8860$ tree install
install
└── ax8860
    ├── ax_classification
    └── ax_yolov5s
```

## 3. 运行

将 `install/ax8860/` 下的可执行文件与模型、测试图片拷贝到开发板，运行方式见 [examples/ax8860/README.md](../examples/ax8860/README.md)。板端 BSP 库位于 `/opt/lib`，一般已在动态库搜索路径中；若提示找不到 `libax_engine.so`，执行：

```bash
export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH
```
