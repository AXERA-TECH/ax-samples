English | [简体中文](./AGENTS_ZH.md)

# AGENTS.md

Quick reference for AI agents and new contributors. For human-oriented docs see [README_EN.md](../README_EN.md).

## What this project is

NPU inference samples (C++) for AXERA AI SoCs. Each sample is a standalone executable: read image → preprocess → `ax_engine` inference → postprocess → print results and save a result image. There is no library and no unit test; a change is verified by **a successful cross-compile + correct output on the board**.

## Key paths

| Path | Purpose |
| ---- | ------- |
| `CMakeLists.txt` | Top-level entry; `AXERA_TARGET_CHIP` defaults to `ax650` |
| `examples/CMakeLists.txt` | Per-SoC macro `AXERA_TARGET_CHIP_AXxxx`, OpenCV path, and `add_subdirectory(examples/<dir>)` |
| `cmake/<chip>.cmake` | Defines `axera_example(<binary> <sources>)`: links BSP libs and installs to `install/<chip>/` |
| `examples/<chip>/` | All samples of the SoC: `ax_<model>_steps.cc`, `CMakeLists.txt`, `README.md`, `middleware/io.hpp` |
| `examples/base/` | Post-processing shared across SoCs: `detection.hpp` (YOLO decoding, NMS, drawing boxes/masks/keypoints), `topk.hpp`, `pose.hpp`, etc. |
| `examples/utilities/` | Command-line parsing `cmdline.hpp`, `args.hpp`, file io, timer |
| `docs/compile_<chip>.md` | Per-SoC build guide; `docs/<chip>/*.jpg` are result images used in READMEs |
| `benchmark/` | Model latency tables per SoC |
| `toolchains/` | Cross-compile toolchain files |
| `.github/workflows/` | Per-SoC CI (build only) |

`3rdparty/` and `build*/` are in `.gitignore`; do not commit them.

## SoCs and build parameters

| SoC | `AXERA_TARGET_CHIP` | Sample dir | Toolchain file | OpenCV (unpack into `3rdparty/`) | Guide |
| --- | ---- | ---- | ---- | ---- | ---- |
| AX650A/AX650N/AX8850 | `ax650` | `examples/ax650` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_650](./compile_650.md) |
| AX637 | `ax637` | `examples/ax637` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_637](./compile_637.md) |
| AX630C | `ax630c` | `examples/ax620e` | `aarch64-none-linux-gnu` | `opencv-aarch64-linux` | [compile_620e](./compile_620e.md) |
| AX620Q | `ax620q` | `examples/ax620e` | `arm-AX620E-linux-uclibcgnueabihf` | `opencv-arm-uclibc-linux` | [compile_620e](./compile_620e.md) |
| AX615 | `ax615` | `examples/ax615` | `arm-linux-gnueabihf` | `opencv-arm-uclibc-linux` | [compile_615](./compile_615.md) |
| AX8860 | `ax8860` | `examples/ax8860` | `aarch64-none-linux-gnu` (**requires Arm GNU 13.2, glibc ≥ 2.38**) | `opencv-aarch64-linux` | [compile_8860](./compile_8860.md) |

Generic build commands (in-source builds are rejected):

```bash
mkdir build_<chip> && cd build_<chip>
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/<toolchain>.toolchain.cmake \
      -DBSP_MSP_DIR=<BSP dir containing include/ and lib/> \
      -DAXERA_TARGET_CHIP=<chip> ..
make -j$(nproc) && make install      # binaries in build_<chip>/install/<chip>/
```

## Sample conventions

- Common options: `-m <axmodel>` (required), `-i <image>` (required), `-g <h,w>` model input size (default 640,640 or 224,224), `-r <count>` repeat inference for timing.
- Output: stdout prints `Repeat N times, avg time ...` and results (`class_id: score, [x0, y0, x1, y1], class_name`); the result image is saved to the **current directory** as `<model>_out.jpg` (classification only prints top5).
- Before running on the board: `export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH` (BSP library location depends on the board).

## Common tasks

### Add a model sample to an existing SoC

1. Look for the same sample under another SoC first (e.g. `examples/ax650/ax_yolo11_steps.cc`). The ax_engine API is largely the same across SoCs, so reuse it directly and keep the code style consistent.
2. Put / reuse post-processing in `examples/base/`; do not reimplement it inside steps.cc.
3. Add `axera_example(ax_xxx ax_xxx_steps.cc)` to `examples/<chip>/CMakeLists.txt`.
4. In `examples/<chip>/README.md`, add the sample to the list and to "运行示例" with the command, full output log and result image (images go to `docs/<chip>/`, about 480px wide to keep the repo small).
5. If the SoC's build guide lists the install outputs, update it as well.

### Add a new SoC

1. `cmake/<chip>.cmake`: copy from a similar SoC and change `install(... DESTINATION <chip>)`.
2. `examples/CMakeLists.txt`: add two `elseif(AXERA_TARGET_CHIP MATCHES "<chip>")` branches (macro + OpenCV, and `add_subdirectory`). `MATCHES` is a regex substring match, so the new name must not be a substring of an existing one or vice versa.
3. `examples/<chip>/`: `CMakeLists.txt`, `middleware/io.hpp`, sample sources, `README.md`.
4. Docs: `docs/compile_<chip>.md`, and register the SoC in `docs/compile.md`, `README.md` and `README_EN.md`.
5. Optional: add a build CI under `.github/workflows/`.

## Known pitfalls

- **AX8860**: BSP libraries depend on glibc 2.38; linking with gcc-arm 9.2 / 11.2 fails with `__isoc23_*@GLIBC_2.38`. `middleware/io.hpp` must set `nSize` for input buffers, otherwise inference fails with `invalid input size=0`.
- **YOLOv8 post-processing** (`generate_proposals_yolov8_native`) expects one output per stride with box(64) and cls concatenated on the channel axis (1×H×W×144, 65 for pose). ONNX models with split outputs need a Concat in the graph before converting with Pulsar2.
- Output tensor order is defined by the model and post-processing reads `pOutputs[i]` by index; check output order and shapes when switching models (some seg samples print io info).
- Default build type is `release` (`-O1 -s`, stripped); for on-board debugging enable `-g -O0` in `examples/CMakeLists.txt`.
- `docs/` holds many images; for a lighter clone use `--filter=blob:none` + sparse-checkout for the directories you need.
