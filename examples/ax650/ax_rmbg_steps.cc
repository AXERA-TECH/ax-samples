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
* Note: For RMBG_1.4.
* Author: Yz
*/

// Usage: ./ax_rmbg -m /path/to/rmbg.axmodel -i /path/to/input.jpg -o /path/to/output.png
#include <cstdio>
#include <cstring>
#include <numeric>
#include <algorithm>
#include <vector>

#include <opencv2/opencv.hpp>
#include "base/common.hpp"
#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int MODEL_INPUT_H = 1024;
const int MODEL_INPUT_W = 1024;

const int DEFAULT_LOOP_COUNT = 1;

namespace ax
{
    void preprocess_image(const cv::Mat& src, std::vector<uint8_t>& image_data)
    {
        cv::Mat resized;
        cv::resize(src, resized, cv::Size(MODEL_INPUT_W, MODEL_INPUT_H), 0, 0, cv::INTER_LINEAR);
        
        if (resized.channels() == 1) {
            cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
        } else if (resized.channels() == 4) {
            cv::cvtColor(resized, resized, cv::COLOR_BGRA2BGR);
        }
        cv::Mat dst;
        cv::cvtColor(resized, dst, cv::COLOR_BGR2RGB);
        std::vector<cv::Mat> split_channels(3);
        cv::split(dst, split_channels);
    
        int channel_size = MODEL_INPUT_H * MODEL_INPUT_W;
        for (int i = 0; i < 3; i++) {
            memcpy(image_data.data() + i * channel_size, 
                split_channels[i].data, 
                channel_size * sizeof(uint8_t));
        }

    }

    void postprocess_mask(float* mask_data, int mask_h, int mask_w, 
                     const cv::Mat& original_image, cv::Mat& output_mask, cv::Mat& result_image)
    {
        timer timer_postprocess;
        
        cv::Mat mask(mask_h, mask_w, CV_32FC1, mask_data);
        cv::resize(mask, output_mask, cv::Size(original_image.cols, original_image.rows), 
                0, 0, cv::INTER_NEAREST);  
        
        cv::Mat mask_normalized;
        double minVal, maxVal;
        cv::minMaxLoc(output_mask, &minVal, &maxVal);
        
        if (maxVal - minVal < 1e-6) {
            mask_normalized = cv::Mat::ones(output_mask.size(), CV_8UC1) * 255;
        } else {
            output_mask.convertTo(mask_normalized, CV_8UC1, 
                                255.0 / (maxVal - minVal), 
                                -minVal * 255.0 / (maxVal - minVal));
        }
        
        if (original_image.channels() == 1) {
            cv::cvtColor(original_image, result_image, cv::COLOR_GRAY2BGRA);
        } else if (original_image.channels() == 3) {
            cv::cvtColor(original_image, result_image, cv::COLOR_BGR2BGRA);
        } else if (original_image.channels() == 4) {
            original_image.copyTo(result_image);
        } else {
            cv::cvtColor(original_image, result_image, cv::COLOR_BGR2BGRA);
        }
        
        int total_pixels = result_image.rows * result_image.cols;
        uchar* alpha_ptr = result_image.data + 3;  
        uchar* mask_ptr = mask_normalized.data;

        for (int i = 0; i < total_pixels; ++i) {
            *alpha_ptr = *mask_ptr;
            alpha_ptr += 4;  // 移动到下一个像素的alpha通道
            mask_ptr += 1;   // 移动到下一个mask值
        }
        
        mask_normalized.copyTo(output_mask);
    }

    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, 
                      const cv::Mat& original_image, const std::vector<float>& time_costs,
                      const std::string& output_path)
    {
        timer timer_postprocess;
        
        auto& output = io_data->pOutputs[0];
        auto& info = io_info->pOutputs[0];
        
        int mask_h = info.pShape[2];  // height
        int mask_w = info.pShape[3];  // width
        
        cv::Mat output_mask, result_image;
        postprocess_mask((float*)output.pVirAddr, mask_h, mask_w, 
                        original_image, output_mask, result_image);
        
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
        
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        compression_params.push_back(9);  
        
        cv::imwrite(output_path, result_image, compression_params);
        cv::imwrite("mask.png", output_mask);
        
        fprintf(stdout, "Saved result image: %s\n", output_path.c_str());
        fprintf(stdout, "Saved mask: mask.png\n");
    }

    bool run_model(const std::string& model, 
                   const std::vector<uint8_t>& image_data,
                   const int& repeat, 
                   cv::Mat& original_image,
                   const std::string& output_path)
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
            fprintf(stderr, "Read Run-ax model(%s) file failed.\n", model.c_str());
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
        
        // print
        fprintf(stdout, "Inputs:\n");
        for (uint32_t i = 0; i < io_info->nInputSize; ++i)
        {
            fprintf(stdout, "  [%d] name: %s, shape: [", i, io_info->pInputs[i].pName);
            for (uint32_t j = 0; j < io_info->pInputs[i].nShapeSize; ++j)
            {
                fprintf(stdout, "%d", io_info->pInputs[i].pShape[j]);
                if (j < io_info->pInputs[i].nShapeSize - 1) fprintf(stdout, ", ");
            }
            fprintf(stdout, "]\n");
        }
        
        fprintf(stdout, "Outputs:\n");
        for (uint32_t i = 0; i < io_info->nOutputSize; ++i)
        {
            fprintf(stdout, "  [%d] name: %s, shape: [", i, io_info->pOutputs[i].pName);
            for (uint32_t j = 0; j < io_info->pOutputs[i].nShapeSize; ++j)
            {
                fprintf(stdout, "%d", io_info->pOutputs[i].pShape[j]);
                if (j < io_info->pOutputs[i].nShapeSize - 1) fprintf(stdout, ", ");
            }
            fprintf(stdout, "]\n");
        }

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. push input
        int input_idx = 0;  
        if (image_data.size() != io_info->pInputs[input_idx].nSize)
        {
            fprintf(stderr, "Input size mismatch: expected %d, got %zu\n", 
                    io_info->pInputs[input_idx].nSize, image_data.size());
            middleware::free_io(&io_data);
            AX_ENGINE_DestroyHandle(handle);
            return false;
        }
        
        memcpy(io_data.pInputs[input_idx].pVirAddr, image_data.data(), image_data.size());
        
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

        // 10. post process
        post_process(io_info, &io_data, original_image, time_costs, output_path);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "axmodel file(a.k.a. axmodel)", true, "");
    cmd.add<std::string>("image", 'i', "input image file", true, "");
    cmd.add<std::string>("output", 'o', "output image file", false, "result.png");
    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    
    cmd.parse_check(argc, argv);

    auto model_file = cmd.get<std::string>("model");
    auto image_file = cmd.get<std::string>("image");
    auto output_file = cmd.get<std::string>("output");
    auto repeat = cmd.get<int>("repeat");

    auto model_file_flag = utilities::file_exist(model_file);
    auto image_file_flag = utilities::file_exist(image_file);

    if (!model_file_flag || !image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!image_file_flag) { show_error("image", image_file); }

        return -1;
    }

    fprintf(stdout, "--------------------------------------\n");
    fprintf(stdout, "Model file: %s\n", model_file.c_str());
    fprintf(stdout, "Input image: %s\n", image_file.c_str());
    fprintf(stdout, "Output image: %s\n", output_file.c_str());
    fprintf(stdout, "Model input size: %d x %d\n", MODEL_INPUT_H, MODEL_INPUT_W);
    fprintf(stdout, "Repeat count: %d\n", repeat);
    fprintf(stdout, "--------------------------------------\n");

    cv::Mat original_image = cv::imread(image_file, cv::IMREAD_UNCHANGED);
    if (original_image.empty())
    {
        fprintf(stderr, "Read image failed: %s\n", image_file.c_str());
        return -1;
    }
    
    fprintf(stdout, "Original image size: %d x %d, channels: %d\n", 
            original_image.cols, original_image.rows, original_image.channels());
    
    int input_size = MODEL_INPUT_H * MODEL_INPUT_W * 3;
    std::vector<uint8_t> image_data(input_size, 0);

    ax::preprocess_image(original_image, image_data);
    
    AX_SYS_Init();

    {
        ax::run_model(model_file, image_data, repeat, original_image, output_file);
        AX_ENGINE_Deinit();
    }

    AX_SYS_Deinit();
    
    return 0;
}