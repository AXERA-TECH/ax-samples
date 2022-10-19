# examples

AX-Samples 将不断更新最流行的、实用的、有趣的示例代码。

- 物体分类
  - MobileNetv1
  - MobileNetv2
  - MobileOne-s0
  - ResNet18
  - ResNet50
  - Others......
- 物体检测
  - [PP-YOLOv3](#yolov3paddle)
  - YOLOv3
  - YOLOv3-Tiny
  - YOLOv4
  - YOLOv4-Tiny
  - YOLOv4-Tiny-3l
  - YOLOv5s
  - YOLOv7-Tiny
  - YOLOX-S
  - YOLO-Fastest-XL
  - NanoDet
- 人型检测
  - YOLO-Fastest-Body
- 人脸检测
  - scrfd
  - yolov5-face([original model](https://github.com/deepcam-cn/yolov5-face))
- 障碍物检测 (扫地机场景)
  - Robot-Obstacle-Detect
- 3D单目车辆检测
  - Monodlex
- 人体关键点
  - HRNet
- 人体分割
  - [PP-HumanSeg](#MobileSeg)
- 语义分割
  - [PP-Seg](#PP-HumanSeg)
- 传统 CV 操作
  - CropResize
- Pipeline 示例
  - NV12 -> CropResize -> NN(Classification)

## 运行示例

下面以物体分类、物体检测 两个常见任务说明如何在 AX620A 开发板上运行相关示例。

### 运行准备

登录 AX620A 开发板，在 `root` 路径下创建 `samples` 文件夹。

- 将 [快速编译](../docs/compile.md) 中编译生成的可执行示例拷贝到 `/root/samples/` 路径下；
- 将 **[ModelZoo](https://pan.baidu.com/s/1ZHW2P6Y3lPf2odmj3fo8hA?pwd=sow9)** 中相应的 **joint** 模型 `mobilenetv2.joint` 、 `yolov5s.joint` 拷贝到  `/root/samples/` 路径下；
- 将测试图片拷贝到 `/root/samples` 路径下。

```
/root/samples # ls -l
total 40644
-rwx--x--x    1 root     root       3805332 Mar 22 14:01 ax_classification
-rwx--x--x    1 root     root       3979652 Mar 22 14:01 ax_yolov5s
-rw-------    1 root     root        140391 Mar 22 10:39 cat.jpg
-rw-------    1 root     root        163759 Mar 22 14:01 dog.jpg
-rw-------    1 root     root       4299243 Mar 22 14:00 mobilenetv2.joint
-rw-------    1 root     root      29217004 Mar 22 14:04 yolov5s.joint
```

### 运行示例
- 物体分类：MobileNetv2 

```
/root/samples # ./ax_classification -m mobilenetv2.joint -i cat.jpg -r 10
--------------------------------------
model file : mobilenetv2.joint
image file : cat.jpg
img_h, img_w : 224 224
Run-Joint Runtime version: 0.5.6
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.32
8a011dfa
10.6865, 285
10.3324, 283
9.1559, 281
9.1174, 356
9.0098, 282
--------------------------------------
Create handle took 201.32 ms (neu 6.36 ms, axe 0.00 ms, overhead 194.96 ms)
--------------------------------------
Repeat 10 times, avg time 3.43 ms, max_time 3.75 ms, min_time 3.37 ms
```

- 物体检测：YOLOv5s

```
root@AXERA:~/samples# ./ax_yolov5s -m yolov5s.joint -i ssd_dog.jpg -r 10
--------------------------------------
model file : models/yolov5s.joint
image file : images/ssd_dog.jpg
img_h, img_w : 640 640
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: d696ee2f
run over: output len 3
--------------------------------------
Create handle took 488.94 ms (neu 22.83 ms, axe 0.00 ms, overhead 466.11 ms)
--------------------------------------
Repeat 10 times, avg time 22.54 ms, max_time 22.91 ms, min_time 22.47 ms
--------------------------------------
detection num: 3
16:  92%, [ 133,  221,  312,  541], dog
 2:  77%, [ 468,   76,  692,  171], car
 1:  65%, [ 167,  120,  564,  417], bicycle
```
![YOLOv5s](../docs/yolov5s.jpg)

- 物体检测：YOLOv7-Tiny
```
root@AXERA:~/samples# ./ax_yolov7 -m yolov7-tiny.joint -i ssd_dog.jpg -r 10
--------------------------------------
model file : yolov7-tiny-cut-sim-sigmoid-dfs.joint
image file : images/ssd_dog.jpg
img_h, img_w : 416 416
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.1.4
59588c54
run over: output len 3
--------------------------------------
Create handle took 376.32 ms (neu 15.51 ms, axe 0.00 ms, overhead 360.81 ms)
--------------------------------------
Repeat 10 times, avg time 9.68 ms, max_time 10.01 ms, min_time 9.63 ms
--------------------------------------
detection num: 3
16:  88%, [ 133,  221,  316,  543], dog
 1:  86%, [ 139,  130,  571,  422], bicycle
 2:  63%, [ 468,   76,  691,  169], car
```
![YOLOv7-Tiny](../docs/yolov7-tiny.jpg)

- 物体检测：YOLOX-S
```
/tmp/samples # ./ax_yoloxs -m yolox_s_cut.joint -i dog.jpg -r 10
--------------------------------------
model file : yolox_s_cut.joint
image file : dog.jpg
img_h, img_w : 640 640
Run-Joint Runtime version: 0.5.8
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.32
8a011dfa
run over: output len 3
--------------------------------------
Create handle took 497.16 ms (neu 23.64 ms, axe 0.00 ms, overhead 473.52 ms)
--------------------------------------
Repeat 10 times, avg time 41.65 ms, max_time 42.37 ms, min_time 41.55 ms
--------------------------------------
detection num: 4
 1:  97%, [ 123,  119,  569,  417], bicycle
16:  95%, [ 136,  222,  307,  540], dog
 7:  72%, [ 470,   75,  688,  171], truck
58:  53%, [ 685,  111,  716,  154], potted plant
```

- 人脸检测：Scrfd
```
root@AXERA:~/samples# ./ax_scrfd -m scrfd_500m_bnkps_shape640x640.joint -i selfie.jpg -r 10
--------------------------------------
model file : models/scrfd_500m_bnkps_shape640x640.joint
image file : images/selfie.jpg
img_h, img_w : 640 640
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.34
9c2b134d
run over: output len 9
--------------------------------------
Create handle took 68.89 ms (neu 4.43 ms, axe 0.00 ms, overhead 64.46 ms)
--------------------------------------
Repeat 10 times, avg time 5.76 ms, max_time 6.09 ms, min_time 5.71 ms
--------------------------------------
detection num: 111
```
![Scrfd](../docs/scrfd.jpg)

- 人脸检测：Yolov5-face
```
root@AXERA:~/samples# ./ax_yolov5s_face -m yolov5s-face.joint -i selfie.jpg -r 10
--------------------------------------
model file : yolov5s-face_sub.joint
image file : selfie.jpg
img_h, img_w : 640 640
[AX_SYS_LOG] AX_SYS_Log2ConsoleThread_Start
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.1.20
07305a6
run over: output len 3
--------------------------------------
Create handle took 498.81 ms (neu 25.10 ms, axe 0.00 ms, overhead 473.71 ms)
--------------------------------------
Repeat 10 times, avg time 21.89 ms, max_time 22.31 ms, min_time 21.83 ms
--------------------------------------
detection num: 310
```
![yolov5s-face](../docs/yolov5s_face.jpg)

- 3D单目车辆检测：Monodlex
```
root@AXERA:~/samples# ./ax_monodlex -m monodlex_sigmoid_max.joint -i cityscape.png -r 10
--------------------------------------
model file : models/monodlex_sigmoid_max.joint
image file : images/cityscape.png
img_h, img_w : 384 1280
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: Unknown
run over: output len 8
--------------------------------------
Create handle took 628.84 ms (neu 29.80 ms, axe 0.00 ms, overhead 599.03 ms)
--------------------------------------
Repeat 10 times, avg time 111.77 ms, max_time 112.34 ms, min_time 111.66 ms
--------------------------------------
detection num: 7
```
![Monodlex](../docs/monodlex.png)

- 人体关键点：HRNet
```
root@AXERA:~/samples# ./ax_hrnet -m hrnet_256x192.joint -i pose-1.jpeg -r 10
--------------------------------------
model file : models/hrnet_256x192.joint
image file : images/pose-1.jpeg
img_h, img_w : 256 192
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.30
100b6396
run over: output len 1
--------------------------------------
Create handle took 1385.15 ms (neu 25.64 ms, axe 0.00 ms, overhead 1359.51 ms)
--------------------------------------
Repeat 10 times, avg time 14.11 ms, max_time 14.64 ms, min_time 14.04 ms
--------------------------------------
```
![HRNet](../docs/hrnet.png)

## 模型说明
### YOLOv3(Paddle)
YOLOv3(Paddle) 源自国内产业级深度学习开源框架飞桨的目标检测开发套件 [PaddleDetection](https://github.com/PaddlePaddle/PaddleDetection)，通过速度与精度权衡，我们选择基于 416尺度的 [YOLOv3-Res34](https://github.com/PaddlePaddle/PaddleDetection/tree/develop/configs/yolov3/) 进行功能展示。

#### Paddle2ONNX

- git clone https://github.com/PaddlePaddle/PaddleDetection.git
- 参考 PaddleDetection/deploy/EXPORT_ONNX_MODEL.md 导出 onnx 模型

#### ONNX2Joint

- 目前需通过 FAE 获取AI工具链进行尝试
- 可通过 ModelZoo 中预先转换好的 `yolov3-paddle-416.joint` 进行体验

#### Sample

```
root@AXERA:~/samples# ./ax_paddle_yolov3 -m yolov3_paddle.joint -i dog.jpg -r 10
--------------------------------------
model file : yolov3_paddle.joint
image file : dog.jpg
img_h, img_w : 416 416
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: 0.6.0.17
run over: output len 3
--------------------------------------
Create handle took 2079.12 ms (neu 38.63 ms, axe 0.00 ms, overhead 2040.49 ms)
--------------------------------------
Repeat 10 times, avg time 35.94 ms, max_time 36.59 ms, min_time 35.85 ms
--------------------------------------
detection num: 3
 1:  89%, [ 120,  126,  568,  432], bicycle
16:  57%, [ 126,  193,  312,  536], dog
 2:  54%, [ 453,   78,  686,  174], car
```
![yolov3_paddle](../docs/yolov3_paddle.jpg)

### MobileSeg
MobileSeg 源自国内产业级深度学习开源框架飞桨的高性能图像分割开发套件 [PaddleSeg](https://github.com/PaddlePaddle/PaddleSeg)，通过速度与精度权衡，我们选择基于 *MobileNetV2*  Backbone 的 [MobileSeg](https://github.com/PaddlePaddle/PaddleSeg/tree/release/2.6/configs/mobileseg/) 进行功能展示。

#### Paddle2ONNX

- git clone https://github.com/PaddlePaddle/PaddleDetection.git
- 参考 PaddleDetection/deploy/EXPORT_ONNX_MODEL.md 导出 onnx 模型

#### ONNX2Joint

- 目前需通过 FAE 获取AI工具链进行尝试
- 可通过 ModelZoo 中预先转换好的 `model_mv2seg_sim_cut_infer_argmax.joint` 进行体验

#### Sample

```
root@AXERA:~/samples# ./ax_paddle_mobileseg -m model_mv2seg_sim_cut_infer_argmax.joint -i mv2seg.png -r 10
--------------------------------------
model file : model_mv2seg_sim_cut_infer_argmax.joint
image file : mv2seg.png
img_h, img_w : 512 1024
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: Unknown
--------------------------------------
Create handle took 303.45 ms (neu 26.27 ms, axe 0.00 ms, overhead 277.19 ms)
--------------------------------------
Repeat 10 times, avg time 74.90 ms, max_time 76.26 ms, min_time 74.66 ms
```
![paddle_mobileseg](../docs/seg_res.jpg)

### PP-HumanSeg
PP-HumanSeg 源自国内产业级深度学习开源框架飞桨的高性能图像分割开发套件 [PaddleSeg](https://github.com/PaddlePaddle/PaddleSeg)，通过速度与精度权衡，我们选择  [PP-HumanSegV1-Mobile](https://github.com/PaddlePaddle/PaddleSeg/tree/release/2.6/contrib/PP-HumanSeg/) 进行功能展示。

#### Paddle2ONNX

- git clone https://github.com/PaddlePaddle/PaddleDetection.git
- 参考 PaddleDetection/deploy/EXPORT_ONNX_MODEL.md 导出 onnx 模型

#### ONNX2Joint

- 目前需通过 FAE 获取AI工具链进行尝试
- 可通过 ModelZoo 中预先转换好的 `pp_human_seg_mobile_sim.joint` 进行体验

#### Sample

```
root@AXERA:~/samples# ./ax_paddle_mobilehumseg -m pp_human_seg_mobile_sim.joint -i pose-1.jpeg -r 10
--------------------------------------
model file : pp_human_seg_mobile_sim.joint
image file : pose-1.jpeg
img_h, img_w : 192 192
Run-Joint Runtime version: 0.5.10
--------------------------------------
[INFO]: Virtual npu mode is 1_1

Tools version: Unknown
--------------------------------------
Create handle took 97.94 ms (neu 3.85 ms, axe 0.00 ms, overhead 94.09 ms)
--------------------------------------
Repeat 10 times, avg time 2.58 ms, max_time 2.82 ms, min_time 2.53 ms
```
![PP-HumanSeg](../docs/body_seg_bg_res.jpg)
