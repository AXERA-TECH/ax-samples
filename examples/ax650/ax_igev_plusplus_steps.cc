/*
* AXERA is pleased to support the open source community by making ax-samples available.
*
* Copyright (c) 2025, AXERA Semiconductor Co., Ltd. All rights reserved.
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
* Note: For IGEV-plusplus stereo matching model.
* Author: GUOFANGMING
*/

// Usage: ./ax_igev_plusplus_steps -m /path/to/igev_plusplus.axmodel -l /path/to/left.jpg -R /path/to/right.jpg -r 10
#include <cstdio>
#include <cstring>
#include <numeric>
#include <algorithm>

#include <opencv2/opencv.hpp>
#include "base/common.hpp"
#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int DEFAULT_IMG_H = 384;
const int DEFAULT_IMG_W = 512;

const int DEFAULT_LOOP_COUNT = 1;

namespace ax
{
    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, 
                      const cv::Mat& left_mat, const cv::Mat& right_mat,
                      const std::vector<float>& time_costs)
    {
        timer timer_postprocess;
        
        // Get output disparity map
        auto& output = io_data->pOutputs[0];
        auto& info = io_info->pOutputs[0];
        
        // Create disparity map from output
        int disp_h = info.pShape[2];  // height
        int disp_w = info.pShape[3];   // width
        cv::Mat disparity_map(disp_h, disp_w, CV_32FC1, output.pVirAddr);
        
        // Normalize disparity for visualization
        double minVal, maxVal;
        cv::minMaxLoc(disparity_map, &minVal, &maxVal);
        
        cv::Mat disparity_normalized;
        if (maxVal > minVal)
        {
            disparity_map.convertTo(disparity_normalized, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
        }
        else
        {
            disparity_map.convertTo(disparity_normalized, CV_8UC1);
        }
        
        // Apply color map (JET colormap similar to Python's 'jet')
        cv::Mat disparity_color;
        cv::applyColorMap(disparity_normalized, disparity_color, cv::ColormapTypes::COLORMAP_JET);
        
        // Resize to original image size if needed
        cv::Mat disparity_resized;
        cv::resize(disparity_color, disparity_resized, cv::Size(left_mat.cols, left_mat.rows));
        
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
        fprintf(stdout, "Disparity range: [%.2f, %.2f]\n", minVal, maxVal);
        fprintf(stdout, "--------------------------------------\n");
        
        // Save results
        cv::imwrite("igev_plusplus_disparity.jpg", disparity_resized);
        
        // Create side-by-side visualization
        cv::Mat combined;
        cv::hconcat(std::vector<cv::Mat>{left_mat, disparity_resized}, combined);
        cv::imwrite("igev_plusplus_result.jpg", combined);
        
        fprintf(stdout, "Saved disparity map: igev_plusplus_disparity.jpg\n");
        fprintf(stdout, "Saved combined result: igev_plusplus_result.jpg\n");
    }

    bool run_model(const std::string& model, 
                   const std::vector<uint8_t>& left_data, 
                   const std::vector<uint8_t>& right_data,
                   const int& repeat, 
                   cv::Mat& left_mat, 
                   cv::Mat& right_mat)
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
        middleware::print_io_info(io_info);

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. Find input indices by name
        int left_input_idx = -1;
        int right_input_idx = -1;
        for (uint32_t i = 0; i < io_info->nInputSize; ++i)
        {
            if (strcmp(io_info->pInputs[i].pName, "left") == 0)
            {
                left_input_idx = i;
            }
            else if (strcmp(io_info->pInputs[i].pName, "right") == 0)
            {
                right_input_idx = i;
            }
        }
        
        if (left_input_idx < 0 || right_input_idx < 0)
        {
            fprintf(stderr, "Failed to find 'left' or 'right' input in model. Available inputs:\n");
            for (uint32_t i = 0; i < io_info->nInputSize; ++i)
            {
                fprintf(stderr, "  Input[%d]: %s\n", i, io_info->pInputs[i].pName);
            }
            middleware::free_io(&io_data);
            return AX_ENGINE_DestroyHandle(handle);
        }

        // 8. insert input
        if (left_data.size() != io_info->pInputs[left_input_idx].nSize)
        {
            fprintf(stderr, "Left input size mismatch: expected %d, got %zu\n", 
                    io_info->pInputs[left_input_idx].nSize, left_data.size());
            middleware::free_io(&io_data);
            return AX_ENGINE_DestroyHandle(handle);
        }
        
        if (right_data.size() != io_info->pInputs[right_input_idx].nSize)
        {
            fprintf(stderr, "Right input size mismatch: expected %d, got %zu\n", 
                    io_info->pInputs[right_input_idx].nSize, right_data.size());
            middleware::free_io(&io_data);
            return AX_ENGINE_DestroyHandle(handle);
        }
        
        memcpy(io_data.pInputs[left_input_idx].pVirAddr, left_data.data(), left_data.size());
        memcpy(io_data.pInputs[right_input_idx].pVirAddr, right_data.data(), right_data.size());
        
        fprintf(stdout, "Engine push input is done. \n");
        fprintf(stdout, "--------------------------------------\n");

        // 9. warm up
        for (int i = 0; i < 5; ++i)
        {
            AX_ENGINE_RunSync(handle, &io_data);
        }

        // 10. run model
        std::vector<float> time_costs(repeat, 0);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_ENGINE_RunSync(handle, &io_data);
            time_costs[i] = tick.cost();
            SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
        }

        // 11. get result
        post_process(io_info, &io_data, left_mat, right_mat, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("left", 'l', "left image file", true, "");
    cmd.add<std::string>("right", 'R', "right image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    // 0. get app args
    auto model_file = cmd.get<std::string>("model");
    auto left_image_file = cmd.get<std::string>("left");
    auto right_image_file = cmd.get<std::string>("right");

    auto model_file_flag = utilities::file_exist(model_file);
    auto left_image_file_flag = utilities::file_exist(left_image_file);
    auto right_image_file_flag = utilities::file_exist(right_image_file);

    if (!model_file_flag | !left_image_file_flag | !right_image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!left_image_file_flag) { show_error("left image", left_image_file); }
        if (!right_image_file_flag) { show_error("right image", right_image_file); }

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
    fprintf(stdout, "left image file : %s\n", left_image_file.c_str());
    fprintf(stdout, "right image file : %s\n", right_image_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);
    fprintf(stdout, "--------------------------------------\n");

    // 2. read images & resize & convert to RGB
    std::vector<uint8_t> left_image(input_size[0] * input_size[1] * 3, 0);
    std::vector<uint8_t> right_image(input_size[0] * input_size[1] * 3, 0);
    
    cv::Mat left_mat = cv::imread(left_image_file);
    cv::Mat right_mat = cv::imread(right_image_file);
    
    if (left_mat.empty())
    {
        fprintf(stderr, "Read left image failed.\n");
        return -1;
    }
    
    if (right_mat.empty())
    {
        fprintf(stderr, "Read right image failed.\n");
        return -1;
    }
    
    // Resize and convert BGR to RGB (matching Python implementation)
    common::get_input_data_no_letterbox(left_mat, left_image, input_size[0], input_size[1], true);
    common::get_input_data_no_letterbox(right_mat, right_image, input_size[0], input_size[1], true);

    // 3. sys_init
    AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, left_image, right_image, repeat, left_mat, right_mat);

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}

