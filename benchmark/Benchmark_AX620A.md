# Benchmark(AX620A)

Benchmark 是了解硬件平台网络模型运行速度的最佳途径。以下数据基于 AXera-Pi 测试获取，不代表商业交付最终性能。

## vnpu 1_1, 1.8Tops@Int8
| Models       | Input Size | 耗时（ms） |
| ------------ | ---------- | ---------- |
| mobilenetv1  | 224        | 4.1        |
| mobilenetv2  | 224        | 3.3        |
| mobileone_s0 | 224        | 1.7        |
| resnet18     | 224        | 5.4        |
| resnet50     | 224        | 19.2       |
| squeezenet10 | 224        | 4.2        |
| squeezenet11 | 224        | 2.3        |
| yolov3       | 416        | 59.7       |
| yolov3-tiny  | 416        | 5.7        |
| yolov4-tiny  | 416        | 5.9        |
| yolov5s      | 640        | 22.4       |
| yolov6s      | 640        | 32.3       |
| yolov7-tiny  | 416        | 9.6        |
| yolov8s      | 640        | 36.7       |
| yolox-s      | 640        | 41.6       |
| yolopv2      | 228*480    | 100.5      |
| yolo-fastbody| 320        | 3.7        |
| yolo-fast-xl | 320        | 6.2        |
| hrnet        | 256        | 14.1       |
