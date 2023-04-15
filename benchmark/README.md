# Benchmark

Benchmark 是了解硬件平台网络模型运行速度的最佳途径。以下数据基于 AXera-Pi 测试获取，不代表商业交付最终性能。

## 1. AX620A
### 1.1 vnpu 1_1, 1.8Tops@Int8
| Models       | Input Size | 耗时（ms） |
| ------------ | ---------- | ---------- |
| mobilenetv1  | 224        |            |
| mobilenetv2  | 224        | 3.3        |
| resnet18     | 224        |            |
| resnet50     | 224        |            |
| squeezenet10 | 224        |            |
| squeezenet11 | 224        |            |
| yolov3       | 416        |            |
| yolov5s      | 640        | 22.4       |
| yolov6s      | 640        | 32.3       |
| yolov7-tiny  | 416        | 9.6        |
| yolov8s      | 640        | 36.7       |
| yolox-s      | 640        | 41.6       |
| hrnet        | 256        | 14.1       |
