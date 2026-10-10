# examples

AX-Samples 将不断更新最流行、实用、有趣的 AX8860 示例代码。以下列表覆盖当前 `examples/ax8860` 目录中已经提供的应用，后续会随着模型支持一起扩充。

- 图像分类
  - [MobileNetv2](#MobileNetv2)
- 物体检测
  - [YOLOv5s](#YOLOv5s)

编译方法见 [compile_8860.md](../../docs/compile_8860.md)。板端 BSP 动态库位于 `/opt/lib`，运行前请确认其在动态库搜索路径中：

```bash
export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH
```

### 运行示例

#### MobileNetv2
```
# ./ax_classification -m models/mobilenetv2.axmodel -i images/cat.jpg -r 10
--------------------------------------
model file : models/mobilenetv2.axmodel
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
9.2452, 285
9.1132, 282
9.1132, 281
8.4528, 283
7.3962, 463
--------------------------------------
Repeat 10 times, avg time 0.36 ms, max_time 0.38 ms, min_time 0.35 ms
--------------------------------------
```

#### YOLOv5s
```
# ./ax_yolov5s -m models/yolov5s.axmodel -i images/dog.jpg -r 10
--------------------------------------
model file : models/yolov5s.axmodel
image file : images/dog.jpg
img_h, img_w : 640 640
--------------------------------------
Engine creating handle is done.
Engine creating context is done.
Engine get io info is done.
Engine alloc io is done.
Engine push input is done.
--------------------------------------
post process cost time:3.41 ms
--------------------------------------
Repeat 10 times, avg time 1.87 ms, max_time 1.89 ms, min_time 1.87 ms
--------------------------------------
detection num: 3
16:  91%, [ 134,  215,  310,  543], dog
 2:  67%, [ 469,   76,  689,  173], car
 1:  56%, [ 159,  117,  570,  417], bicycle
--------------------------------------
```
