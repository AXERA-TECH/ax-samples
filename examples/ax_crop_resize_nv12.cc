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
 * Author: hebing
 */

#include <vector>
#include <cstdio>
#include <string>
#include <memory>

#include "utilities/cmdline.hpp"
#include "npu_cv_kit/ax_npu_imgproc.h"
#include "ax_sys_api.h"

#include "cv/utils.hpp"
#include "cv/cv.hpp"
#include "utilities/file.hpp"
#include "middleware/io.hpp"

#include <opencv2/opencv.hpp>

namespace ax
{
    int parse_args(cmdline::parser& args, int argc, char** argv)
    {
        args.add<std::string>("mode", '\0', "NPU hard mode: disable, 1_1", false, "disable");
        args.add<std::string>("mode-type", '\0', "Virtual NPU mode type: disable, 1_1_1, 1_1_2", false, "disable");

        args.add<std::string>("img_input", 'i', "input_img_path", true);
        args.add<std::string>("in_color_space", 'c', "input color space:rgb:nv21:nv12", true);
        args.add<AX_U32>("input_width", 'w', "input width", true);
        args.add<AX_U32>("input_height", 'h', "input height", true);
        args.add<AX_U32>("input_stride_width", 's', "input width stride", false, 0);

        args.add<std::string>("img_output", 'o', "output_img_path", true);
        args.add<std::string>("out_color_space", 'l', "output color space:rgb:nv21:nv12", true);
        args.add<AX_U32>("output_width", 'x', "output width", true);
        args.add<AX_U32>("output_height", 'y', "output height", true);
        args.add<AX_U32>("output_stride_width", 'd', "output width stride", false, 0);

        args.add<std::string>("box", 'b', "crop image box format is x,y,w,h", false);

        args.parse_check(argc, argv);

        return 0;
    }

    int save_result(AX_NPU_CV_Image* output_image)
    {
        std::vector<char> output_data(output_image->nHeight * output_image->nWidth * 3 / 2, 0);
        memcpy(output_data.data(), output_image->pVir, output_image->nHeight * output_image->nWidth * 3 / 2);

        cv::Mat output_nv12(output_image->nHeight * 3 / 2, output_image->nWidth, CV_8UC1, output_data.data());
        cv::Mat output_rgb(output_image->nHeight, output_image->nWidth, CV_8UC3);
        cv::cvtColor(output_nv12, output_rgb, cv::COLOR_YUV2BGR_NV12);

        cv::imwrite("./output_cv_nv12.png", output_nv12);
        cv::imwrite("./output_cv_nv12_rgb.png", output_rgb);

        return 0;
    }
}

// ./ax_cv_test -i 800x480car.nv12 -c nv12 -w 800 -h 480 -o 400x240.nv12 -l nv12 -x 400 -y 240 -b 0,0,800,480
int main(int argc, char** argv)
{
    cmdline::parser args;
    ax::parse_args(args, argc, argv);

    auto deinit = [](int code) {
        AX_NPU_SDK_EX_Deinit();
        AX_SYS_Deinit();
        return code;
    };
    // 1.init npu sdk
    int ret = AX_SYS_Init();
    if (ret != AX_NPU_DEV_STATUS_SUCCESS)
    {
        fprintf(stderr, "[ERR] AX_SYS_Init err code: %x", ret);
        return deinit(-1);
    }

    AX_NPU_SDK_EX_ATTR_T hard_mode = cv::get_npu_hard_mode(args.get<std::string>("mode"));
    ret = AX_NPU_SDK_EX_Init_with_attr(&hard_mode);
    if (ret != AX_NPU_DEV_STATUS_SUCCESS)
    {
        fprintf(stderr, "[ERR] AX_NPU_SDK_EX_Init_with_attr err code:%x", ret);
        return deinit(-1);
    }

    // 2. alloc input image
    axcv::ax_image input;
    input.w = args.get<AX_U32>("input_width");
    input.h = args.get<AX_U32>("input_height");
    input.stride_w = input.w;
    input.color_space = cv::get_color_space(args.get<std::string>("in_color_space"));
    auto input_image = axcv::alloc_cv_image(input);
    if (!input_image)
    {
        fprintf(stderr, "[ERR] alloc_cv_image input falil \n");
        return deinit(-1);
    }

    // 3. prepare box
    axcv::ax_box box;
    std::string box_str = args.get<std::string>("box");
    sscanf(box_str.c_str(), "%d,%d,%d,%d", &box.x, &box.y, &box.w, &box.h);
    auto out_box = axcv::filter_box(input, box);
    if (!out_box)
    {
        fprintf(stderr, "[ERR] box not legal \n");
        return deinit(-1);
    }

    // 4. read input file
    std::vector<char> input_data;
    utilities::read_file(args.get<std::string>("img_input"), input_data);

    // 5. alloc output image
    axcv::ax_image output;
    output.w = args.get<AX_U32>("output_width");
    output.h = args.get<AX_U32>("output_height");
    output.stride_w = output.w;
    output.color_space = cv::get_color_space(args.get<std::string>("out_color_space"));
    auto output_image = axcv::alloc_cv_image(output);
    if (!output_image)
    {
        fprintf(stderr, "[ERR] alloc_cv_image output falil \n");
        return deinit(-1);
    }

    // 6. crop and resize
    ret = axcv::npu_crop_resize(input_image, input_data.data(), output_image, out_box, cv::get_npu_mode_type(args.get<std::string>("mode-type")));
    if (ret != AX_NPU_DEV_STATUS_SUCCESS)
    {
        fprintf(stderr, "[ERR] npu_crop_resize err code:%x \n", ret);
        return deinit(-1);
    }

    // 7. save res
    ax::save_result(output_image);

    // 8. free resource
    axcv::free_cv_image(input_image);
    axcv::free_cv_image(output_image);

    return deinit(0);
}