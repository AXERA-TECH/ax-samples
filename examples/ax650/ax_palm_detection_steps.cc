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
 * Author: LittleMouse
 */

#include <cstdio>
#include <cstring>
#include <numeric>
#include <list>
#include <opencv2/opencv.hpp>

#include "base/detection.hpp"
#include "base/common.hpp"
#include "middleware/io.hpp"
#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int DEFAULT_IMG_H = 192;
const int DEFAULT_IMG_W = 192;
const int DEFAULT_LOOP_COUNT = 1;
const float PROB_THRESHOLD = 0.45f;
const float NMS_THRESHOLD = 0.45f;

// palm anchor configs
const int map_size[2] = {24, 12};
const int strides[2] = {8, 16};
const int anchor_size[2] = {2, 6};
const float anchor_offset[2] = {0.5f, 0.5f};

namespace ax
{
    namespace det = detection;
    namespace mw = middleware;
    namespace utl = utilities;

    void post_process(AX_ENGINE_IO_INFO_T* io_info,
                      AX_ENGINE_IO_T* io_data,
                      const cv::Mat& mat,
                      int input_w,
                      int input_h,
                      const std::vector<float>& time_costs)
    {
        std::vector<det::PalmObject> proposals;
        std::vector<det::PalmObject> objects;

        timer timer_postprocess;

        auto& bboxes_info = io_data->pOutputs[0];
        auto bboxes_ptr = (float*)bboxes_info.pVirAddr;

        auto& scores_info = io_data->pOutputs[1];
        auto scores_ptr = (float*)scores_info.pVirAddr;

        float prob_threshold_unsigmoid = -1.0f * (float)std::log((1.0f / PROB_THRESHOLD) - 1.0f);

        det::generate_proposals_palm(proposals,
                                     PROB_THRESHOLD,
                                     DEFAULT_IMG_W,
                                     DEFAULT_IMG_H,
                                     scores_ptr,
                                     bboxes_ptr,
                                     2,
                                     strides,
                                     anchor_size,
                                     anchor_offset,
                                     map_size,
                                     prob_threshold_unsigmoid);

        det::get_out_bbox_palm(proposals,
                               objects,
                               NMS_THRESHOLD,
                               input_h,
                               input_w,
                               mat.rows,
                               mat.cols);

        fprintf(stdout, "post process cost time: %.2f ms\n", timer_postprocess.cost());
        fprintf(stdout, "--------------------------------------\n");

        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Repeat %zu times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                time_costs.size(),
                total_time / (float)time_costs.size(),
                *min_max_time.second,
                *min_max_time.first);

        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "detection num: %zu\n", objects.size());

        det::draw_objects_palm(mat, objects, "palm_detection");
    }

    bool run_model(const std::string& model,
                   const std::vector<uint8_t>& data,
                   const int& repeat,
                   cv::Mat& mat,
                   int input_h,
                   int input_w)
    {
        // 1. init engine
        AX_ENGINE_NPU_ATTR_T npu_attr;
        memset(&npu_attr, 0, sizeof(npu_attr));
        npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;

        auto ret = AX_ENGINE_Init(&npu_attr);
        if (0 != ret)
        {
            fprintf(stderr, "AX_ENGINE_Init failed, ret = 0x%x\n", ret);
            return false;
        }

        // 2. load model
        std::vector<char> model_buffer;
        if (!utl::read_file(model, model_buffer))
        {
            fprintf(stderr, "Read AX-Engine model(%s) file failed.\n", model.c_str());
            AX_ENGINE_Deinit();
            return false;
        }

        // 3. create handle
        AX_ENGINE_HANDLE handle;
        ret = AX_ENGINE_CreateHandle(&handle, model_buffer.data(), model_buffer.size());
        if (0 != ret)
        {
            fprintf(stderr, "AX_ENGINE_CreateHandle failed, ret = 0x%x\n", ret);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine creating handle is done.\n");

        // 4. create context
        ret = AX_ENGINE_CreateContext(handle);
        if (0 != ret)
        {
            fprintf(stderr, "AX_ENGINE_CreateContext failed, ret = 0x%x\n", ret);
            AX_ENGINE_DestroyHandle(handle);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine creating context is done.\n");

        // 5. get io info
        AX_ENGINE_IO_INFO_T* io_info;
        ret = AX_ENGINE_GetIOInfo(handle, &io_info);
        if (0 != ret)
        {
            fprintf(stderr, "AX_ENGINE_GetIOInfo failed, ret = 0x%x\n", ret);
            AX_ENGINE_DestroyHandle(handle);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine get io info is done.\n");

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = mw::prepare_io(io_info,
                             &io_data,
                             std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        if (0 != ret)
        {
            fprintf(stderr, "prepare_io failed, ret = 0x%x\n", ret);
            AX_ENGINE_DestroyHandle(handle);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine alloc io is done.\n");

        // 7. push input
        ret = mw::push_input(data, &io_data, io_info);
        if (0 != ret)
        {
            fprintf(stderr, "Engine push input failed, ret = 0x%x\n", ret);
            mw::free_io(&io_data);
            AX_ENGINE_DestroyHandle(handle);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine push input is done.\n");
        fprintf(stdout, "--------------------------------------\n");

        // 8. warm up
        for (int i = 0; i < 5; ++i)
        {
            AX_ENGINE_RunSync(handle, &io_data);
        }

        // 9. run model
        std::vector<float> time_costs(repeat, 0.f);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_ENGINE_RunSync(handle, &io_data);
            time_costs[i] = tick.cost();
            if (0 != ret)
            {
                fprintf(stderr, "AX_ENGINE_RunSync failed, ret = 0x%x\n", ret);
                mw::free_io(&io_data);
                AX_ENGINE_DestroyHandle(handle);
                AX_ENGINE_Deinit();
                return false;
            }
        }

        // 10. post process
        post_process(io_info, &io_data, mat, input_w, input_h, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        mw::free_io(&io_data);
        AX_ENGINE_DestroyHandle(handle);
        AX_ENGINE_Deinit();
        return true;
    }

} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("image", 'i', "image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false,
                         std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));
    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    auto model_file = cmd.get<std::string>("model");
    auto image_file = cmd.get<std::string>("image");

    auto model_file_flag = utilities::file_exist(model_file);
    auto image_file_flag = utilities::file_exist(image_file);
    if (!model_file_flag | !image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n",
                    kind.c_str(), value.c_str());
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
            fprintf(stderr, "Input %s(%s) is not allowed, please check it.\n",
                    kind.c_str(), value.c_str());
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
    std::vector<uint8_t> image(input_size[0] * input_size[1] * 3, 0);
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    common::get_input_data_letterbox(mat, image, input_size[0], input_size[1], true);

    // 3. sys_init
    AX_S32 ret = AX_SYS_Init();
    if (0 != ret)
    {
        fprintf(stderr, "AX_SYS_Init failed, ret = 0x%x\n", ret);
        return ret;
    }

    // 4. engine model
    {
        ax::run_model(model_file, image, repeat, mat, input_size[0], input_size[1]);
    }

    AX_SYS_Deinit();
    return 0;
}