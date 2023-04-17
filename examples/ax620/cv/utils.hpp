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

#pragma once

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "npu_cv_kit/ax_npu_imgproc.h"
#include <cstdio>
#include <string>

namespace cv
{
    static inline AX_NPU_SDK_EX_ATTR_T get_npu_hard_mode(const std::string& mode)
    {
        AX_NPU_SDK_EX_ATTR_T hard_mode;
        if (mode == "1_1")
        {
            printf("\nVirtual npu mode is 1_1\n\n");
            hard_mode.eHardMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_1_1;
        }
        else
        {
            printf("\nVirtual npu disable!\n\n");
            hard_mode.eHardMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_DISABLE;
        }
        return hard_mode;
    }

    static inline AX_NPU_SDK_EX_MODEL_TYPE_T get_npu_mode_type(const std::string& mode_type)
    {
        if (mode_type == "1_1_1")
        {
            printf("\nVirtual npu mode type is 1_1_1\n\n");
            return AX_NPU_MODEL_TYPE_1_1_1;
        }
        else if (mode_type == "1_1_2")
        {
            printf("\nVirtual npu mode type is 1_1_2\n\n");
            return AX_NPU_MODEL_TYPE_1_1_2;
        }
        else
        {
            printf("\nVirtual npu mode type is disable!\n\n");
            return AX_NPU_MODEL_TYPE_DEFUALT;
        }
    }

    AX_NPU_CV_FrameDataType get_color_space(const std::string& color_space)
    {
        if (color_space == "NV12" || color_space == "nv12")
        {
            return AX_NPU_CV_FDT_NV12;
        }
        else if (color_space == "NV21" || color_space == "nv21")
        {
            return AX_NPU_CV_FDT_NV21;
        }
        else if (color_space == "RGB" || color_space == "rgb")
        {
            return AX_NPU_CV_FDT_RGB;
        }
        else if (color_space == "BGR" || color_space == "bgr")
        {
            return AX_NPU_CV_FDT_BGR;
        }
        else if (color_space == "RGBA" || color_space == "rgba")
        {
            return AX_NPU_CV_FDT_RGBA;
        }
        else if (color_space == "GRAY" || color_space == "gray")
        {
            return AX_NPU_CV_FDT_GRAY;
        }
        else
        {
            printf("color space support error! \n");
        }
    }

    uint32_t get_image_stride_w(const AX_NPU_CV_Image* pImg)
    {
        // 内部使用接口，不再冗余校验pImg指针
        if (pImg->tStride.nW == 0)
        {
            return pImg->nWidth;
        }
        else if (pImg->tStride.nW >= pImg->nWidth)
        {
            if ((pImg->eDtype == AX_NPU_CV_FDT_NV12 || pImg->eDtype == AX_NPU_CV_FDT_NV21) && pImg->tStride.nW % 2 != 0)
            {
                fprintf(stderr, "[ERR] Invalid param: image stride_w %d should be even for NV12/NV21", pImg->tStride.nW);
            }
            return pImg->tStride.nW;
        }
        else
        {
            fprintf(stderr, "Invalid param: image stride_w %d not less than image width %d", pImg->tStride.nW, pImg->nWidth);
        }
        return pImg->nWidth;
    }

    int get_image_data_size(const AX_NPU_CV_Image* img)
    {
        int stride_w = get_image_stride_w(img);
        switch (img->eDtype)
        {
        case AX_NPU_CV_FDT_NV12: // FIXME
        case AX_NPU_CV_FDT_NV21:
            return int(stride_w * img->nHeight * 3 / 2);

        case AX_NPU_CV_FDT_RGB:
        case AX_NPU_CV_FDT_BGR:
        case AX_NPU_CV_FDT_YUV444:
            return int(stride_w * img->nHeight * 3);

        case AX_NPU_CV_FDT_RGBA:
            return int(stride_w * img->nHeight * 4);

        case AX_NPU_CV_FDT_GRAY:
            return int(stride_w * img->nHeight * 1);

        default:
            fprintf(stderr, "[ERR] unsupported color space %d to calculate image data size\n", (int)img->eDtype);
            return 0;
        }
    }

} // namespace cv