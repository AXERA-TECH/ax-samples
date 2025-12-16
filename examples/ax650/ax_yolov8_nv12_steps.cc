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
#include <ax_ivps_api.h>
#include <ax_engine_api.h>

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) ((((x) + ((align)-1)) / (align)) * (align))
#endif

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

const float PROB_THRESHOLD = 0.45f;
const float NMS_THRESHOLD = 0.45f;
namespace ax
{

    bool rgb2nv12(cv::Mat mat, int cropresize_width, int cropresize_height, std::vector<uint8_t>& image_nv12)
    {
        // resolution align 16
        printf("image size: %d %d\n", mat.cols, mat.rows);
        cv::resize(mat, mat, cv::Size(ALIGN_UP(mat.cols, 16), ALIGN_UP(mat.rows, 16)));
        printf("align image size: %d %d\n", mat.cols, mat.rows);
        int ret = AX_SYS_Init();
        if (ret != 0)
        {
            printf("AX_SYS_Init failed, s32Ret=0x%x!\n", ret);
            return false;
        }
        ret = AX_IVPS_Init();
        if (ret != 0)
        {
            printf("AX_IVPS_Init failed, s32Ret=0x%x!\n", ret);
            return false;
        }
        AX_VIDEO_FRAME_T ive_bgr = {0}, ive_nv12 = {0}, ive_nv12_crop_resize = {0};
        unsigned char* tmp = 0;
        ret = AX_SYS_MemAlloc(&ive_bgr.u64PhyAddr[0], (void**)&tmp, mat.cols * mat.rows * 3, 16, (const AX_S8*)"NULL");
        if (ret != 0)
        {
            fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
            return false;
        }
        ive_bgr.u64VirAddr[0] = (AX_U64)tmp;
        ive_bgr.enImgFormat = AX_FORMAT_BGR888;
        ive_bgr.u32PicStride[0] = mat.cols;
        ive_bgr.u32Width = mat.cols;
        ive_bgr.u32Height = mat.rows;
        memcpy(tmp, mat.data, mat.cols * mat.rows * 3);

        ret = AX_SYS_MemAlloc(&ive_nv12.u64PhyAddr[0], (void**)&tmp, mat.cols * mat.rows * 3 / 2, 16, (const AX_S8*)"NULL");
        if (ret != 0)
        {
            fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
            return false;
        }
        ive_nv12.u64VirAddr[0] = (AX_U64)tmp;
        ive_nv12.u64VirAddr[1] = (AX_U64)(tmp + mat.cols * mat.rows);
        ive_nv12.u64PhyAddr[1] = ive_nv12.u64PhyAddr[0] + mat.cols * mat.rows;
        ive_nv12.enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
        ive_nv12.u32PicStride[0] = ive_nv12.u32PicStride[1] = mat.cols;
        ive_nv12.u32Width = mat.cols;
        ive_nv12.u32Height = mat.rows;

        ret = AX_SYS_MemAlloc(&ive_nv12_crop_resize.u64PhyAddr[0], (void**)&tmp, cropresize_width * cropresize_height * 3 / 2, 16, (const AX_S8*)"NULL");
        if (ret != 0)
        {
            fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
            return false;
        }
        ive_nv12_crop_resize.u64VirAddr[0] = (AX_U64)tmp;
        ive_nv12_crop_resize.u64VirAddr[1] = (AX_U64)(tmp + cropresize_width * cropresize_height);
        ive_nv12_crop_resize.u64PhyAddr[1] = ive_nv12_crop_resize.u64PhyAddr[0] + cropresize_width * cropresize_height;
        ive_nv12_crop_resize.enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
        ive_nv12_crop_resize.u32PicStride[0] = ive_nv12_crop_resize.u32PicStride[1] = cropresize_width;
        ive_nv12_crop_resize.u32Width = cropresize_width;
        ive_nv12_crop_resize.u32Height = cropresize_height;

        ret = AX_IVPS_CscTdp(&ive_bgr, &ive_nv12);
        if (ret != 0)
        {
            fprintf(stderr, "AX_IVPS_CSC failed. 0x%x\n", ret);
            return false;
        }

        memset((void*)ive_nv12_crop_resize.u64VirAddr[0], 0, cropresize_width * cropresize_height * 3 / 2);
        timer timer_crop_resize;

        AX_IVPS_ASPECT_RATIO_T resize_ctrl;
        resize_ctrl.nBgColor = 0xFFFFFF;
        resize_ctrl.eMode = AX_IVPS_ASPECT_RATIO_AUTO;
        resize_ctrl.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
        resize_ctrl.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;

#ifdef AXERA_TARGET_CHIP_AX620E
        AX_IVPS_CROP_RESIZE_ATTR_T crop_resize_attr;
        memset(&crop_resize_attr, 0, sizeof(crop_resize_attr));
        memcpy(&crop_resize_attr.tAspectRatio, &resize_ctrl, sizeof(resize_ctrl));
        ret = AX_IVPS_CropResizeVpp(&ive_nv12, &ive_nv12_crop_resize, &crop_resize_attr);
#else
        ret = AX_IVPS_CropResizeVpp(&ive_nv12, &ive_nv12_crop_resize, &resize_ctrl);
#endif
        if (ret != 0)
        {
            fprintf(stderr, "AX_IVPS_CropResize failed. 0x%x\n", ret);
        }

        image_nv12.resize(cropresize_width * cropresize_height * 3 / 2);
        memcpy(image_nv12.data(), (void*)ive_nv12_crop_resize.u64VirAddr[0], cropresize_width * cropresize_height * 3 / 2);

        AX_SYS_MemFree(ive_bgr.u64PhyAddr[0], (void*)ive_bgr.u64VirAddr[0]);
        AX_SYS_MemFree(ive_nv12.u64PhyAddr[0], (void*)ive_nv12.u64VirAddr[0]);
        AX_SYS_MemFree(ive_nv12_crop_resize.u64PhyAddr[0], (void*)ive_nv12_crop_resize.u64VirAddr[0]);
        return true;
    }

    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, const cv::Mat& mat, int input_w, int input_h, const std::vector<float>& time_costs)
    {
        std::vector<detection::Object> proposals;
        std::vector<detection::Object> objects;
        timer timer_postprocess;
        for (int i = 0; i < 3; ++i)
        {
            auto feat_ptr = (float*)io_data->pOutputs[i].pVirAddr;
            int32_t stride = (1 << i) * 8;
            detection::generate_proposals_yolov8_native(stride, feat_ptr, PROB_THRESHOLD, proposals, input_w, input_h, NUM_CLASS);
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

        detection::draw_objects(mat, objects, CLASS_NAMES, "yolov8s_out", 1, 3);
    }

    bool run_model(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, int input_h, int input_w)
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

    // 2. read image & resize & transpose
    std::vector<uint8_t> image_nv12;
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }

    if (!ax::rgb2nv12(mat, input_size[0], input_size[1], image_nv12))
    {
        fprintf(stderr, "rgb2nv12 failed.\n");
        return -1;
    }

    // 3. sys_init
    // AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, image_nv12, repeat, mat, input_size[0], input_size[1]);

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}
