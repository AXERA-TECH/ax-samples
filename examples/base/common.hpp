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
 * Author: ls.wang
 */

#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include "ax_interpreter_external_api.h"

namespace common
{
    static inline AX_NPU_SDK_EX_HARD_MODE_T get_hard_mode_by_model_type(int model_type)
    {
        if (AX_NPU_MODEL_TYPE_DEFUALT == model_type)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_DISABLE;
        }
        else if (model_type == AX_NPU_MODEL_TYPE_1_1_1 || model_type == AX_NPU_MODEL_TYPE_1_1_2)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_1_1;
        }
        else
        {
            fprintf(stderr, "[ERR]: get_hard_mode_by_model_type(int model_type) Unknown npu mode(%d). return default mode\n", model_type);
            return (AX_NPU_SDK_EX_HARD_MODE_T)0;
        }
    }
    // opencv mat(h, w)
    // resize cv::Size(dstw, dsth)
    void get_input_data_no_letterbox(cv::Mat mat, std::vector<uint8_t>& image, int model_h, int model_w)
    {
        cv::Mat img_new(model_h, model_w, CV_8UC3, image.data());
        cv::resize(mat, img_new, cv::Size(model_w, model_h));
    }

    void get_input_data_letterbox(cv::Mat mat, std::vector<uint8_t>& image, int letterbox_rows, int letterbox_cols)
    {
        /* letterbox process to support different letterbox size */
        float scale_letterbox;
        int resize_rows;
        int resize_cols;
        if ((letterbox_rows * 1.0 / mat.rows) < (letterbox_cols * 1.0 / mat.cols))
        {
            scale_letterbox = letterbox_rows * 1.0 / mat.rows;
        }
        else
        {
            scale_letterbox = letterbox_cols * 1.0 / mat.cols;
        }
        resize_cols = int(scale_letterbox * mat.cols);
        resize_rows = int(scale_letterbox * mat.rows);

        cv::Mat img_new(letterbox_rows, letterbox_cols, CV_8UC3, image.data());

        cv::resize(mat, mat, cv::Size(resize_cols, resize_rows));

        int top = (letterbox_rows - resize_rows) / 2;
        int bot = (letterbox_rows - resize_rows + 1) / 2;
        int left = (letterbox_cols - resize_cols) / 2;
        int right = (letterbox_cols - resize_cols + 1) / 2;

        // Letterbox filling
        cv::copyMakeBorder(mat, img_new, top, bot, left, right, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    }

    bool read_file(const char* fn, std::vector<uchar>& data)
    {
        FILE* fp = fopen(fn, "r");
        if (fp != nullptr)
        {
            fseek(fp, 0L, SEEK_END);
            auto len = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            data.clear();
            size_t read_size = 0;
            if (len > 0)
            {
                data.resize(len);
                read_size = fread(data.data(), 1, len, fp);
            }
            fclose(fp);
            return read_size == (size_t)len;
        }
        return false;
    }

    bool save_file(const char* fn, const void* data, size_t size)
    {
        FILE* fp = fopen(fn, "wb+");
        if (fp != nullptr)
        {
            auto save_size = fwrite(data, size, 1, fp);
            fclose(fp);
            return save_size == 1;
        }
        return false;
    }
} // namespace common