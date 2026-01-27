# Benchmark(AX615)

Benchmark 是了解硬件平台网络模型运行速度的最佳途径。以下数据基于 AXera-Pi Pro 测试获取，**仅供社区参考，不代表商业交付最终性能**。

### 工具链版本
- Pulsar2 4.2

### 数据记录

| Models         | Input Size | Inference Time(ms)@1 Core | Inference Time(ms)@2 Cores |
| ------------     | ---------- | ---------- |----------  |
| mobilenetv2      | 224        | 5.411         | 2.896      |
| swin_t           | 224        | TBD           | 26.921     |
| resnet50         | 224        | 22.032        | 11.621     |
| yolov5s          | 640        | 36.058        | 19.807     |
| yolov6s          | 640        | 42.298        | 21.690     |
| yolov8s          | 640        | 49.131        | 27.500     |
| yolov9s          | 640        | 46.333        | 30.136     |
| yolov10s         | 640        | 49.965        | 27.148     |
| yolo11s          | 640        | 53.308        | 27.572     |
| yolo26n          | 640        | 26.995        | 10.599     |
| yolo26s          | 640        | 55.200        | 18.600     |
| yoloworldv2      | 640        | 71.009        | 40.472     |
| Depth-Anything-V2| 518        | TBD           | 354.809    |
