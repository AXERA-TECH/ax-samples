# 源码编译（AX620E）

ax-samples 的源码编译目前有两种实现路径：

- **本地编译**：由于开发板集成了完整的Linux系统，可以预装必要的 gcc、cmake 等开发环境，因此可以在开发板上直接完成源码编译；
- **交叉编译**：在 x86 PC 的常规开发环境中，通过对应的交叉编译工具链完成对源码的编译。

## 本地编译

- 由于 AX630C/AX620Q 的产品定位是嵌入式智能硬件的主控芯片，当前阶段并无完整 Linux 文件系统的产品支持，因此不会存在本地编译的需求；
- AX620Q 有且仅有 SIP 256MiB 内存，因此不会存在本地编译的需求。  

## 交叉编译

当前SDK版本(v2.0.0_P7)中支持 64位、32位 两种不同的编译器版本，下面示例分别基于 AX630C 的 64 位环境与 AX620Q 的 32 位环境进行说明。

- [AX630C](#AX630C(64位,aarch64-linux))
- [AX620Q](#AX620Q(32位,arm-uclibc-linux))

### AX630C(64位,aarch64-linux)

#### 3rdparty 准备

- 下载预编译好的 [OpenCV 库文件](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.1/opencv-aarch64-linux-gnu-gcc-7.5.0.zip)；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中，文件夹目录结构如下所示

```bash
ax-samples$ tree -L 3
.
├── 3rdparty
│   └── opencv-aarch64-linux
│       ├── bin
│       ├── build_opencv_aarch64.sh
│       ├── include
│       ├── lib
│       └── share
```

#### 编译环境
- cmake 版本大于等于 3.13
- AX620E 配套的交叉编译工具链 `aarch64-none-linux-gnu-gcc` 已添加到环境变量中

#### 安装交叉编译工具链

- Arm64 Linux 交叉编译工具链 [获取地址](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz)

#### 依赖库准备

请联系 FAE 获取 AX620E BSP 开发包，执行如下操作

```
tar -zxvf AX620E_SDK_XXX.tgz
cd AX620E_SDK_XXX/package
tar -zxvf msp.tgz
```

解压完成后将文件夹内此 `msp/out` 目录设置到临时环境变量，具体操作如下：

```
export ax_bsp=${ax620e_bsp_sdk PATH}/msp/out/arm64_glibc
```

#### 源码编译
git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-none-linux-gnu.toolchain.cmake -DBSP_MSP_DIR=${ax_bsp}/ -DAXERA_TARGET_CHIP=ax630c ..
make -j6
make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/ax630c/` 路径下：

```bash
ax-samples/build$ tree install
install
└── ax630c
    ├── ax_classification
    ├── ax_crowdcount
    ├── ax_depth_anything
    ├── ax_imgproc
    ├── ax_model_info
    ├── ax_rtdetr
    ├── ax_scrfd
    ├── ax_simcc_pose
    ├── ax_yolov5_face
    ├── ax_yolov5s
    ├── ax_yolov5s_seg
    ├── ax_yolov6
    ├── ax_yolov7
    ├── ax_yolov7_tiny_face
    ├── ax_yolov8
    ├── ax_yolov8_pose
    ├── ax_yolov8_seg
    ├── ax_yolov9
    └── ax_yolox
```

### AX620Q(32位,arm-uclibc-linux)

#### 3rdparty 准备

- 下载预编译好的 [OpenCV 库文件](https://github.com/AXERA-TECH/ax-samples/releases/download/v0.6/opencv-arm-uclibc-linux.zip)；
- 在 ax-samples 创建 3rdparty 文件，并将下载好的 OpenCV 库文件压缩包解压到该文件夹中，文件夹目录结构如下所示

```bash
ax-samples$ tree -L 3
.
├── 3rdparty
│   └── opencv-arm-uclibc-linux
│       ├── bin
│       ├── include
│       ├── lib
│       └── share
```

#### 编译环境
- cmake 版本大于等于 3.13
- AX620E 配套的交叉编译工具链 `arm-AX620E-linux-uclibcgnueabihf-gxx` 已添加到环境变量中

#### 安装交叉编译工具链

- Arm32 Linux 交叉编译工具链 [获取地址](https://github.com/AXERA-TECH/ax620q_bsp_sdk/releases/download/v2.0.0/arm-AX620E-linux-uclibcgnueabihf_V3_20240320.tgz)

#### 依赖库准备

请联系 FAE 获取 AX620E BSP 开发包，执行如下操作

```
tar -zxvf AX620E_SDK_XXX.tgz
cd AX620E_SDK_XXX/package
tar -zxvf msp.tgz
```

解压完成后将文件夹内此 `msp/out` 目录设置到临时环境变量，具体操作如下：

```
export ax_bsp=${ax620e_bsp_sdk PATH}/msp/out/arm_uclibc
```

#### 源码编译
git clone 下载源码，进入 ax-samples 根目录，创建 cmake 编译任务：

```bash
git clone https://github.com/AXERA-TECH/ax-samples.git
cd ax-samples
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-AX620E-linux-uclibcgnueabihf.toolchain.cmake -DBSP_MSP_DIR=${ax_bsp}/ -DAXERA_TARGET_CHIP=ax620q ..
make -j6
make install
```

编译完成后，生成的可执行示例存放在 `ax-samples/build/install/ax620q/` 路径下：

```bash
ax-samples/build$ tree install
install
└── ax620q
    ├── ax_classification
    ├── ax_crowdcount
    ├── ax_depth_anything
    ├── ax_imgproc
    ├── ax_model_info
    ├── ax_rtdetr
    ├── ax_scrfd
    ├── ax_simcc_pose
    ├── ax_yolov5_face
    ├── ax_yolov5s
    ├── ax_yolov5s_seg
    ├── ax_yolov6
    ├── ax_yolov7
    ├── ax_yolov7_tiny_face
    ├── ax_yolov8
    ├── ax_yolov8_pose
    ├── ax_yolov8_seg
    ├── ax_yolov9
    └── ax_yolox
```
