/*
* AXERA is pleased to support the open source community by making ax-samples available.
*
* Copyright (c) 2022, AXERA Semiconductor (Shanghai) Co., Ltd. All rights reserved.
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
* Author: ZHEQIUSHUI
*/

#include <cstdio>
#include <cstring>
#include <numeric>

#include <opencv2/opencv.hpp>
#include "base/common.hpp"
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
const int CLASS_NUM = 80;
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
const float ANCHORS[18] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};

const int DEFAULT_LOOP_COUNT = 1;

const float PROB_THRESHOLD = 0.45f;
const float NMS_THRESHOLD = 0.45f;
struct image_data_t
{
    std::string path;
    cv::Mat mat;
    std::vector<uint8_t> data;
};
namespace ax
{

    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, const std::vector<image_data_t>& batchdata, int input_w, int input_h, const std::vector<float>& time_costs)
    {
        float prob_threshold_u_sigmoid = -1.0f * (float)std::log((1.0f / PROB_THRESHOLD) - 1.0f);
        for (size_t b = 0; b < batchdata.size(); b++)
        {
            std::vector<detection::Object> proposals;
            std::vector<detection::Object> objects;

            timer timer_postprocess;
            for (uint32_t i = 0; i < io_info->nOutputSize; ++i)
            {
                auto& output = io_data->pOutputs[i];
                float* ptr = (float*)output.pVirAddr;
                int32_t stride = (1 << i) * 8;
                ptr += b * (input_w / stride) * (input_h / stride) * 3 * (CLASS_NUM + 5);
                detection::generate_proposals_yolov5(stride, ptr, PROB_THRESHOLD, proposals, input_w, input_h, ANCHORS, prob_threshold_u_sigmoid, CLASS_NUM);
            }

            detection::get_out_bbox(proposals, objects, NMS_THRESHOLD, input_h, input_w, batchdata[b].mat.rows, batchdata[b].mat.cols);
            fprintf(stdout, "post process cost time:%.2f ms \n", timer_postprocess.cost());
            fprintf(stdout, "--------------------------------------\n");
            fprintf(stdout, "detection num: %zu\n", objects.size());
            fprintf(stdout, "--------------------------------------\n");
            detection::draw_objects(batchdata[b].mat, objects, CLASS_NAMES, (batchdata[b].path + ".res").c_str());
        }
        fprintf(stdout, "--------------------------------------\n");
        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                (int)time_costs.size(),
                total_time / (float)time_costs.size(),
                *min_max_time.second,
                *min_max_time.first);
    }

    bool run_model(const std::string& model, const std::vector<image_data_t>& batchdata, const int& repeat, int input_h, int input_w)
    {
        // 1. init engine
#ifdef AXERA_TARGET_CHIP_AX620E
        auto ret = AX_ENGINE_Init();
#else
        AX_ENGINE_NPU_ATTR_T npu_attr;
        memset(&npu_attr, 0, sizeof(npu_attr));
        npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
        auto ret = AX_ENGINE_Init(&npu_attr);
#endif
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
        if (batchdata.size() > io_info->nMaxBatchSize)
        {
            fprintf(stderr, "The batch size is too large. %d > %d\n", batchdata.size(), io_info->nMaxBatchSize);
            return AX_ENGINE_DestroyHandle(handle);
        }
        middleware::print_io_info(io_info);

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. insert input
        io_data.nBatchSize = batchdata.size();
        int single_input_size = io_info->pInputs[0].nSize / io_info->nMaxBatchSize;
        printf("single input size %d \n", single_input_size);
        uint8_t* input_data = (uint8_t*)io_data.pInputs[0].pVirAddr;
        for (int i = 0; i < batchdata.size(); ++i)
        {
            memcpy(input_data + i * single_input_size, batchdata[i].data.data(), single_input_size);
        }
        // memcpy(io_data.pInputs[0].pVirAddr, data.data(), data.size());
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
        post_process(io_info, &io_data, batchdata, input_w, input_h, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("folder", 'f', "image folder", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("model");
    auto image_folder = cmd.get<std::string>("folder");

    auto model_file_flag = utilities::file_exist(model_file);

    if (!model_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }

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
    fprintf(stdout, "image folder : %s\n", image_folder.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);
    fprintf(stdout, "--------------------------------------\n");

    // 2. read image & resize & transpose

    if (image_folder.back() != '/')
    {
        image_folder += "/";
    }
    std::vector<std::string> image_list;
    cv::glob(image_folder + "*.jpg", image_list);

    std::vector<std::string> image_list_png;
    cv::glob(image_folder + "*.png", image_list_png);

    std::vector<std::string> image_list_jpeg;
    cv::glob(image_folder + "*.jpeg", image_list_jpeg);

    image_list.insert(image_list.end(), image_list_png.begin(), image_list_png.end());
    image_list.insert(image_list.end(), image_list_jpeg.begin(), image_list_jpeg.end());

    std::vector<image_data_t> batchdata(image_list.size());

    for (int i = 0; i < image_list.size(); ++i)
    {
        printf("read image : %s\n", image_list[i].c_str());
        batchdata[i].path = image_list[i];
        batchdata[i].mat = cv::imread(image_list[i]);
        if (batchdata[i].mat.empty())
        {
            fprintf(stderr, "Read image failed.\n");
            return -1;
        }
        batchdata[i].data.resize(input_size[0] * input_size[1] * 3, 0);
        common::get_input_data_letterbox(batchdata[i].mat, batchdata[i].data, input_size[0], input_size[1]);
    }
    // cv::Mat mat = cv::imread(image_file);
    // if (mat.empty())
    // {
    //     fprintf(stderr, "Read image failed.\n");
    //     return -1;
    // }
    // common::get_input_data_letterbox(mat, image, input_size[0], input_size[1]);

    // 3. sys_init
    AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, batchdata, repeat, input_size[0], input_size[1]);

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}
