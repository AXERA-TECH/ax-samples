# examples

AX-Samples 将不断更新最流行、实用、有趣的 AX8860 示例代码。以下列表覆盖当前 `examples/ax8860` 目录中已经提供的应用，后续会随着模型支持一起扩充。

- 图像分类
  - [MobileNetv2](#MobileNetv2)
- 物体检测
  - [YOLOv5s](#YOLOv5s)
  - [YOLOv8s](#YOLOv8s)
  - [YOLO11s](#YOLO11s)
  - [YOLO26s](#YOLO26s)
- 物体分割
  - [YOLOv8s-seg](#YOLOv8s-seg)
  - [YOLO11s-seg](#YOLO11s-seg)
  - [YOLO26s-seg](#YOLO26s-seg)
- 人体关键点
  - [YOLOv8s-pose](#YOLOv8s-pose)
  - [YOLO11s-pose](#YOLO11s-pose)
  - [YOLO26s-pose](#YOLO26s-pose)

编译方法见 [compile_8860.md](../../docs/compile_8860.md)。板端 BSP 动态库位于 `/opt/lib`，运行前请确认其在动态库搜索路径中：

```bash
export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH
```

### 模型转换

模型使用 Pulsar2 7.0 转换，目标平台为 AX8860：

```bash
pulsar2 build --config config.json --target_hardware AX8860 --npu_mode NPU4
```

YOLOv8 系列的 onnx 若为每个 stride 拆分输出（box 64 + cls 80 / 1），需先在 onnx 中将同一 stride 的 box 与 cls 在通道维 Concat，得到与 `ax_yolov8*` 后处理一致的输出：

- YOLOv8s：3 x (1 x H x W x 144)
- YOLOv8s-seg：3 x (1 x H x W x 144)，3 x (1 x H x W x 32)，1 x 32 x 160 x 160
- YOLOv8s-pose：3 x (1 x H x W x 65)，3 x (1 x H x W x 51)

以下示例模型均为 NPU4 模式，检测、分割、关键点示例统一使用 [bus.jpg](https://ultralytics.com/images/bus.jpg) 测试。

### 运行示例

#### MobileNetv2
```
# ./ax_classification -m models/mobilenetv2_npu4.axmodel -i images/cat.jpg -r 10
--------------------------------------
model file : models/mobilenetv2_npu4.axmodel
image file : images/cat.jpg
img_h, img_w : 224 224
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
topk cost time:0.07 ms
10.1433, 285
9.4579, 282
9.3208, 281
8.9096, 283
8.2243, 287
--------------------------------------
Repeat 10 times, avg time 0.32 ms, max_time 0.33 ms, min_time 0.32 ms
--------------------------------------
```

#### YOLOv5s
```
# ./ax_yolov5s -m models/yolov5s_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolov5s_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:4.89 ms
--------------------------------------
Repeat 10 times, avg time 0.81 ms, max_time 0.82 ms, min_time 0.80 ms
--------------------------------------
detection num: 5
 0:  85%, [  60,  408,  204,  909], person
 0:  79%, [ 666,  424,  808,  889], person
 0:  78%, [ 219,  408,  345,  872], person
 5:  72%, [   1,  210,  809,  811], bus
 0:  64%, [   1,  563,   66,  871], person
--------------------------------------
```
![YOLOv5s](../../docs/ax8860/yolov5s_out.jpg)

#### YOLOv8s
```
# ./ax_yolov8 -m models/yolov8s_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolov8s_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:4.14 ms
--------------------------------------
Repeat 10 times, avg time 0.96 ms, max_time 0.98 ms, min_time 0.94 ms
--------------------------------------
detection num: 5
 5:  89%, [  20,  229,  801,  740], bus
 0:  89%, [  48,  398,  243,  905], person
 0:  86%, [ 668,  394,  809,  874], person
 0:  83%, [ 223,  405,  348,  860], person
 0:  56%, [   0,  555,   65,  869], person
--------------------------------------
```
![YOLOv8s](../../docs/ax8860/yolov8_out.jpg)

#### YOLO11s
```
# ./ax_yolo11 -m models/yolo11s_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo11s_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:3.92 ms
--------------------------------------
Repeat 10 times, avg time 0.94 ms, max_time 0.96 ms, min_time 0.93 ms
--------------------------------------
detection num: 5
 5:  94%, [  32,  226,  796,  738], bus
 0:  91%, [ 222,  405,  346,  855], person
 0:  91%, [  47,  395,  246,  903], person
 0:  84%, [ 669,  395,  809,  881], person
 0:  67%, [   0,  547,   80,  871], person
--------------------------------------
```
![YOLO11s](../../docs/ax8860/yolo11_out.jpg)

#### YOLO26s
```
# ./ax_yolo26 -m models/yolo26s_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo26s_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:2.94 ms
--------------------------------------
Repeat 10 times, avg time 0.98 ms, max_time 1.00 ms, min_time 0.98 ms
--------------------------------------
detection num: 5
 5:  96%, [   6,  230,  806,  739], bus
 0:  94%, [  51,  396,  242,  898], person
 0:  92%, [ 668,  387,  809,  876], person
 0:  92%, [ 218,  404,  350,  866], person
 0:  73%, [   1,  554,   67,  874], person
--------------------------------------
```
![YOLO26s](../../docs/ax8860/yolo26_out.jpg)

#### YOLOv8s-seg
```
# ./ax_yolov8_seg -m models/yolov8s-seg_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolov8s-seg_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------

input size: 1
    name: [1;32m  images [1;34m[UINT8] [1;31m[unknown]
        [1;31m1 x 640 x 640 x 3[0m


output size: 7
    name: [1;32m stride8 [1;34m[FLOAT32][0m
        [1;31m1 x 80 x 80 x 144[0m

    name: [1;32mstride16 [1;34m[FLOAT32][0m
        [1;31m1 x 40 x 40 x 144[0m

    name: [1;32mstride32 [1;34m[FLOAT32][0m
        [1;31m1 x 20 x 20 x 144[0m

    name: [1;32m     402 [1;34m[FLOAT32][0m
        [1;31m1 x 80 x 80 x 32[0m

    name: [1;32m     468 [1;34m[FLOAT32][0m
        [1;31m1 x 40 x 40 x 32[0m

    name: [1;32m     534 [1;34m[FLOAT32][0m
        [1;31m1 x 20 x 20 x 32[0m

    name: [1;32m     564 [1;34m[FLOAT32][0m
        [1;31m1 x 32 x 160 x 160[0m

post process cost time:14.76 ms
--------------------------------------
Repeat 10 times, avg time 1.20 ms, max_time 1.22 ms, min_time 1.20 ms
--------------------------------------
detection num: 5
 0:  91%, [  51,  400,  248,  906], person
 5:  85%, [  15,  231,  801,  743], bus
 0:  85%, [ 221,  407,  344,  859], person
 0:  85%, [ 672,  392,  809,  878], person
 0:  55%, [   0,  553,   64,  871], person
--------------------------------------
```
![YOLOv8s-seg](../../docs/ax8860/yolov8_seg_out.jpg)

#### YOLO11s-seg
```
# ./ax_yolo11_seg -m models/yolo11s-seg_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo11s-seg_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------

input size: 1
    name: [1;32m  images [1;34m[UINT8] [1;31m[unknown]
        [1;31m1 x 640 x 640 x 3[0m


output size: 7
    name: [1;32m/model.23/Concat_1_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 80 x 80 x 144[0m

    name: [1;32m/model.23/Concat_2_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 40 x 40 x 144[0m

    name: [1;32m/model.23/Concat_3_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 20 x 20 x 144[0m

    name: [1;32m/model.23/cv4.0/cv4.0.2/Conv_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 80 x 80 x 32[0m

    name: [1;32m/model.23/cv4.1/cv4.1.2/Conv_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 40 x 40 x 32[0m

    name: [1;32m/model.23/cv4.2/cv4.2.2/Conv_output_0 [1;34m[FLOAT32][0m
        [1;31m1 x 20 x 20 x 32[0m

    name: [1;32m output1 [1;34m[FLOAT32][0m
        [1;31m1 x 32 x 160 x 160[0m

post process cost time:15.75 ms
--------------------------------------
Repeat 10 times, avg time 1.23 ms, max_time 1.27 ms, min_time 1.22 ms
--------------------------------------
detection num: 5
 0:  91%, [  50,  398,  249,  907], person
 0:  87%, [ 669,  400,  809,  879], person
 0:  84%, [ 222,  405,  343,  858], person
 5:  78%, [  24,  229,  802,  743], bus
 0:  55%, [   0,  548,   63,  870], person
--------------------------------------
```
![YOLO11s-seg](../../docs/ax8860/yolo11_seg_out.jpg)

#### YOLO26s-seg
```
# ./ax_yolo26_seg -m models/yolo26s-seg_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo26s-seg_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:14.10 ms
--------------------------------------
Repeat 10 times, avg time 1.36 ms, max_time 1.40 ms, min_time 1.35 ms
--------------------------------------
detection num: 4
 0:  96%, [  50,  410,  242,  904], person
 5:  96%, [  12,  234,  801,  736], bus
 0:  94%, [ 222,  406,  342,  864], person
 0:  90%, [ 664,  402,  809,  876], person
--------------------------------------
```
![YOLO26s-seg](../../docs/ax8860/yolo26_seg_out.jpg)

#### YOLOv8s-pose
```
# ./ax_yolov8_pose -m models/yolov8s-pose_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolov8s-pose_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:1.54 ms
--------------------------------------
Repeat 10 times, avg time 1.13 ms, max_time 1.16 ms, min_time 1.12 ms
--------------------------------------
detection num: 4
 0:  88%, [ 223,  405,  347,  857], person
 0:  88%, [  48,  395,  246,  903], person
 0:  87%, [ 668,  395,  809,  875], person
 0:  59%, [   1,  552,   68,  875], person
--------------------------------------
```
![YOLOv8s-pose](../../docs/ax8860/yolov8_pose_out.jpg)

#### YOLO11s-pose
```
# ./ax_yolo11_pose -m models/yolo11s-pose_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo11s-pose_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:1.54 ms
--------------------------------------
Repeat 10 times, avg time 1.15 ms, max_time 1.17 ms, min_time 1.13 ms
--------------------------------------
detection num: 4
 0:  93%, [ 225,  407,  347,  858], person
 0:  90%, [  50,  399,  245,  904], person
 0:  90%, [ 671,  394,  809,  878], person
 0:  54%, [   1,  556,   75,  877], person
--------------------------------------
```
![YOLO11s-pose](../../docs/ax8860/yolo11_pose_out.jpg)

#### YOLO26s-pose
```
# ./ax_yolo26_pose -m models/yolo26s-pose_npu4.axmodel -i images/bus.jpg -r 10
--------------------------------------
model file : models/yolo26s-pose_npu4.axmodel
image file : images/bus.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:0.22 ms
--------------------------------------
Repeat 10 times, avg time 1.17 ms, max_time 1.18 ms, min_time 1.16 ms
--------------------------------------
detection num: 4
 0:  93%, [  53,  401,  256,  899], person
 0:  90%, [ 669,  404,  809,  878], person
 0:  90%, [ 227,  409,  349,  861], person
 0:  41%, [   1,  547,   70,  896], person
--------------------------------------
```
![YOLO26s-pose](../../docs/ax8860/yolo26_pose_out.jpg)
