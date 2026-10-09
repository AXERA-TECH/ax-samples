/*
* AXERA is pleased to support the open source community by making ax-samples available.
*
* Copyright (c) 2024, AXERA Semiconductor Co., Ltd. All rights reserved.
*
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
* in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed
* under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
* CONDITIONS OF ANY KIND, either express or implied. See the License for the
* specific language governing permissions and limitations under the License.
*/

/*
* Note: For the YOLO-Master-EsMoE series exported by the ultralytics project (m2 split scheme).
*       The NPU subgraph outputs 4 tensors: dfl raw (1,64,8400) and three-scale cls raw
*       logits (1,80,6400/1600/400). dfl softmax decode + box decode + sigmoid + NMS are
*       re-implemented in C++ on the CPU, equivalent to the host post-ONNX output0 (1,84,8400).
*/

#include <cstdio>
#include <cstring>
#include <cfloat>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <array>

#include <opencv2/opencv.hpp>
#include "base/detection.hpp"
#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int DEFAULT_IMG_H = 640;
const int DEFAULT_IMG_W = 640;

const char* CLASS_NAMES[] = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"};

int NUM_CLASS = 80;

const int DEFAULT_LOOP_COUNT = 1;

// 与 infer_axmodel.py 单图模式一致：score-thresh 0.25 / iou 0.7
const float PROB_THRESHOLD = 0.25f;
const float NMS_THRESHOLD = 0.7f;

namespace ax
{
    // 复刻 host post-ONNX 的 dfl Softmax->box decode + Sigmoid，等价于 output0 (1,84,8400)。
    // 输出 tensor 均为 FP32 且保持 ONNX NCHW 语义（channel-major）：
    //   dfl_feat 内存布局为 [channel][anchor]，channel 0..63 = 4 边 x reg_max 个 bin；
    //   cls_feat 内存布局为 [class][anchor]。
    static void generate_proposals(int stride, const float* dfl_feat, int dfl_total, int dfl_anchor_offset,
                                   const float* cls_feat, float prob_threshold, std::vector<detection::Object>& objects,
                                   int letterbox_cols, int letterbox_rows, int cls_num)
    {
        const int feat_w = letterbox_cols / stride;
        const int feat_h = letterbox_rows / stride;
        const int num_anchors = feat_w * feat_h;
        const int reg_max = 16;

        const float p = std::min(std::max(prob_threshold, 1e-6f), 1.f - 1e-6f);
        const float conf_raw = std::log(p / (1.f - p));

        for (int i = 0; i < num_anchors; ++i)
        {
            // cls: 取最大 raw logit（channel-major：cls_feat[c * num_anchors + i]）
            int best_c = 0;
            float best_logit = -FLT_MAX;
            for (int c = 0; c < cls_num; ++c)
            {
                float v = cls_feat[c * num_anchors + i];
                if (v > best_logit)
                {
                    best_logit = v;
                    best_c = c;
                }
            }
            if (best_logit < conf_raw)
                continue;
            const float score = detection::sigmoid(best_logit);

            // dfl 解码：4 边各做 reg_max 个 bin 的 softmax，加权得到 ltrb 距离（格子单位），再乘 stride
            const int global_a = dfl_anchor_offset + i;
            float pred_ltrb[4];
            for (int k = 0; k < 4; ++k)
            {
                float bins[reg_max];
                float dis_after_sm[reg_max];
                for (int j = 0; j < reg_max; ++j)
                    bins[j] = dfl_feat[(k * reg_max + j) * dfl_total + global_a];
                const float dis = detection::softmax(bins, dis_after_sm, reg_max);
                pred_ltrb[k] = dis * stride;
            }

            const int w = i % feat_w;
            const int h = i / feat_w;
            const float cx = (w + 0.5f) * stride;
            const float cy = (h + 0.5f) * stride;
            float x0 = cx - pred_ltrb[0];
            float y0 = cy - pred_ltrb[1];
            float x1 = cx + pred_ltrb[2];
            float y1 = cy + pred_ltrb[3];

            x0 = std::max(0.f, std::min(x0, (float)(letterbox_cols - 1)));
            y0 = std::max(0.f, std::min(y0, (float)(letterbox_rows - 1)));
            x1 = std::max(0.f, std::min(x1, (float)(letterbox_cols - 1)));
            y1 = std::max(0.f, std::min(y1, (float)(letterbox_rows - 1)));

            detection::Object obj;
            obj.rect.x = x0;
            obj.rect.y = y0;
            obj.rect.width = x1 - x0;
            obj.rect.height = y1 - y0;
            obj.label = best_c;
            obj.prob = score;
            objects.push_back(obj);
        }
    }

    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, const cv::Mat& mat, int input_w, int input_h, const std::vector<float>& time_costs)
    {
        if (io_info->nOutputSize != 4)
        {
            fprintf(stderr, "expect 4 outputs (dfl + cls_p3/p4/p5), actual %d\n", io_info->nOutputSize);
            return;
        }

        // 按名称绑定 4 个输出，不依赖编译器输出顺序：
        //   Concat_output_0  (1,64,8400) dfl raw
        //   Reshape_3        (1,80,6400) cls P3 logit
        //   Reshape_4        (1,80,1600) cls P4 logit
        //   Reshape_5        (1,80,400)  cls P5 logit
        float* dfl_ptr = nullptr;
        float* cls_ptr[3] = {nullptr, nullptr, nullptr};
        int dfl_total = 0;
        const char* cls_keys[3] = {"Reshape_3", "Reshape_4", "Reshape_5"};
        for (int i = 0; i < io_info->nOutputSize; ++i)
        {
            const char* name = io_info->pOutputs[i].pName;
            if (strstr(name, "Concat_output_0"))
            {
                dfl_ptr = (float*)io_data->pOutputs[i].pVirAddr;
                dfl_total = io_info->pOutputs[i].pShape[io_info->pOutputs[i].nShapeSize - 1];
            }
            else
            {
                for (int k = 0; k < 3; ++k)
                {
                    if (strstr(name, cls_keys[k]))
                        cls_ptr[k] = (float*)io_data->pOutputs[i].pVirAddr;
                }
            }
        }
        if (!dfl_ptr || !cls_ptr[0] || !cls_ptr[1] || !cls_ptr[2])
        {
            fprintf(stderr, "failed to match outputs by name (expect Concat_output_0 + Reshape_3/4/5)\n");
            for (int i = 0; i < io_info->nOutputSize; ++i)
                fprintf(stderr, "  output[%d]: %s\n", i, io_info->pOutputs[i].pName);
            return;
        }

        std::vector<detection::Object> proposals;
        std::vector<detection::Object> objects;
        timer timer_postprocess;

        // dfl 的 anchor 维按 channel-major 读作最后一维（8400 = 6400 + 1600 + 400）
        int dfl_offset[3] = {0, 0, 0};
        for (int i = 1; i < 3; ++i)
        {
            int prev_stride = (1 << (i - 1)) * 8;
            dfl_offset[i] = dfl_offset[i - 1] + (input_h / prev_stride) * (input_w / prev_stride);
        }

        for (int i = 0; i < 3; ++i)
        {
            int32_t stride = (1 << i) * 8;
            generate_proposals(stride, dfl_ptr, dfl_total, dfl_offset[i], cls_ptr[i], PROB_THRESHOLD, proposals, input_w, input_h, NUM_CLASS);
        }

        detection::get_out_bbox(proposals, objects, NMS_THRESHOLD, input_h, input_w, mat.rows, mat.cols);
        fprintf(stdout, "post process cost time:%.2f ms \n", timer_postprocess.cost());
        fprintf(stdout, "--------------------------------------\n");
        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                (int)time_costs.size(),
                total_time / (float)time_costs.size(),
                *min_max_time.second,
                *min_max_time.first);
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "detection num: %zu\n", objects.size());

        detection::draw_objects(mat, objects, CLASS_NAMES, "yolo_master_esmoe_out");
    }

    bool run_model(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, int input_h, int input_w)
    {
        // 1. init engine
        AX_ENGINE_NPU_ATTR_T npu_attr;
        memset(&npu_attr, 0, sizeof(npu_attr));
        npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
        auto ret = AX_ENGINE_Init(&npu_attr);
        if (0 != ret)
        {
            return ret;
        }

        // 2. load model
        std::vector<char> model_buffer;
        if (!utilities::read_file(model, model_buffer))
        {
            fprintf(stderr, "Read Run-Joint model(%s) file failed.\n", model.c_str());
            return false;
        }

        // 3. create handle
        AX_ENGINE_HANDLE handle;
        ret = AX_ENGINE_CreateHandle(&handle, model_buffer.data(), model_buffer.size());
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine creating handle is done.\n");

        // 4. create context
        ret = AX_ENGINE_CreateContext(handle);
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine creating context is done.\n");

        // 5. set io
        AX_ENGINE_IO_INFO_T* io_info;
        ret = AX_ENGINE_GetIOInfo(handle, &io_info);
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine get io info is done. \n");
        middleware::print_io_info(io_info);

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. insert input
        ret = middleware::push_input(data, &io_data, io_info);
        SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
        fprintf(stdout, "Engine push input is done. \n");
        fprintf(stdout, "--------------------------------------\n");

        // 8. warm up
        for (int i = 0; i < 5; ++i)
        {
            AX_ENGINE_RunSync(handle, &io_data);
        }

        // 9. run model
        std::vector<float> time_costs(repeat, 0);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_ENGINE_RunSync(handle, &io_data);
            time_costs[i] = tick.cost();
            SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
        }

        // 10. get result
        post_process(io_info, &io_data, mat, input_w, input_h, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
    }
} // namespace ax

// 与 infer_axmodel.py 的 letterbox 一致：resize + 114 填充 + BGR->RGB，输出 NHWC uint8
static void get_input_data_letterbox_rgb(const cv::Mat& mat, std::vector<uint8_t>& image, int letterbox_rows, int letterbox_cols)
{
    float scale = std::min((float)letterbox_rows / mat.rows, (float)letterbox_cols / mat.cols);
    int resize_cols = (int)std::round(mat.cols * scale);
    int resize_rows = (int)std::round(mat.rows * scale);

    cv::Mat resized;
    cv::resize(mat, resized, cv::Size(resize_cols, resize_rows));

    float dw = (letterbox_cols - resize_cols) / 2.0f;
    float dh = (letterbox_rows - resize_rows) / 2.0f;
    int top = (int)std::round(dh - 0.1);
    int bottom = (int)std::round(dh + 0.1);
    int left = (int)std::round(dw - 0.1);
    int right = (int)std::round(dw + 0.1);

    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);
    memcpy(image.data(), rgb.data, letterbox_rows * letterbox_cols * 3);
}

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("image", 'i', "image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("model");
    auto image_file = cmd.get<std::string>("image");

    auto model_file_flag = utilities::file_exist(model_file);
    auto image_file_flag = utilities::file_exist(image_file);

    if (!model_file_flag | !image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!image_file_flag) { show_error("image", image_file); }

        return -1;
    }

    auto input_size_string = cmd.get<std::string>("size");

    std::array<int, 2> input_size = {DEFAULT_IMG_H, DEFAULT_IMG_W};

    auto input_size_flag = utilities::parse_string(input_size_string, input_size);

    if (!input_size_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input %s(%s) is not allowed, please check it.\n", kind.c_str(), value.c_str());
        };

        show_error("size", input_size_string);

        return -1;
    }

    auto repeat = cmd.get<int>("repeat");

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");
    fprintf(stdout, "model file : %s\n", model_file.c_str());
    fprintf(stdout, "image file : %s\n", image_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);
    fprintf(stdout, "--------------------------------------\n");

    // 2. read image & letterbox(114) & BGR->RGB
    std::vector<uint8_t> image(input_size[0] * input_size[1] * 3, 0);
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    get_input_data_letterbox_rgb(mat, image, input_size[0], input_size[1]);

    // 3. sys_init
    AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, image, repeat, mat, input_size[0], input_size[1]);

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}
