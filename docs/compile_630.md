# 源码编译（AX630）

## 交叉编译

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
