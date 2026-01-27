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
#include <opencv2/opencv.hpp>

#include "base/detection.hpp"
#include "base/transform.hpp"
#include "base/common.hpp"
#include "base/pose.hpp"
#include "middleware/io.hpp"
#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

#include <iostream>
#include <fstream>

const int IMG_H = 224;
const int IMG_W = 224;
const int HAND_JOINTS = 21;
const int DEFAULT_LOOP_COUNT = 1;

namespace ax
{
    namespace mw = middleware;
    namespace utl = utilities;

    void post_process(AX_ENGINE_IO_INFO_T* io_info,
                      AX_ENGINE_IO_T* io_data,
                      const cv::Mat& mat,
                      int input_w,
                      int input_h,
                      const std::vector<float>& time_costs)
    {
        timer timer_postprocess;

        auto& info_point = io_data->pOutputs[0];
        auto& info_score = io_data->pOutputs[1];

        auto point_ptr = (float*)info_point.pVirAddr;
        auto score_ptr = (float*)info_score.pVirAddr;

        pose::ai_hand_parts_s ai_point_result;
        pose::post_process_hand(point_ptr, score_ptr, ai_point_result, HAND_JOINTS, IMG_H, IMG_W);

        fprintf(stdout, "post process cost time: %.2f ms \n", timer_postprocess.cost());
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

        pose::draw_result_hand(mat, ai_point_result, HAND_JOINTS);
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
        std::memset(&npu_attr, 0, sizeof(npu_attr));
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
            fprintf(stderr, "Read Run-Joint model(%s) file failed.\n", model.c_str());
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
        AX_ENGINE_IO_INFO_T* io_info = nullptr;
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
        std::memset(&io_data, 0, sizeof(io_data));
        ret = mw::prepare_io(io_info, &io_data,
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
            fprintf(stderr, "push_input failed, ret = 0x%x\n", ret);
            mw::free_io(&io_data);
            AX_ENGINE_DestroyHandle(handle);
            AX_ENGINE_Deinit();
            return false;
        }
        fprintf(stdout, "Engine push input is done.\n");
        fprintf(stdout, "--------------------------------------\n");

        // 8. warmup
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

        fprintf(stdout, "run over: output len %d\n", io_info->nOutputSize);

        // 10. post process
        post_process(io_info, &io_data, mat, input_w, input_h, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        // 11. free io & destroy handle
        mw::free_io(&io_data);
        AX_ENGINE_DestroyHandle(handle);
        return true;
    }

} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("image", 'i', "image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false,
                         std::to_string(IMG_H) + "," + std::to_string(IMG_W));
    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    // 0. get app args
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
    std::array<int, 2> input_size = {IMG_H, IMG_W};
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

    common::get_input_data_no_letterbox(mat, image, input_size[0], input_size[1], true);

    // 3. init ax system
    AX_S32 ret = AX_SYS_Init();
    if (0 != ret)
    {
        fprintf(stderr, "Init system failed.\n");
        return ret;
    }

    // 4. run engine model
    {
        bool ok = ax::run_model(model_file, image, repeat, mat, input_size[0], input_size[1]);
        if (!ok)
        {
            fprintf(stderr, "Run hand pose model failed.\n");
        }

        AX_ENGINE_Deinit();
    }

    // 5. last de-init
    AX_SYS_Deinit();
    return 0;
}