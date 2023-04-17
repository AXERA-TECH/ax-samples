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
#include "utilities/file.hpp"
#include "middleware/io.hpp"
#include "utils.hpp"
#include <cstdio>
#include <vector>
#include <memory>

namespace axcv
{
    typedef struct ax_image
    {
        int h;
        int w;
        int stride_w = w;
        AX_NPU_CV_FrameDataType color_space;
    } ax_image;

    typedef struct ax_box
    {
        int x;
        int y;
        int w;
        int h;
    } ax_box;

    AX_NPU_CV_Box* filter_box(const ax_image& input, const ax_box& box)
    {
        if (box.x < 0 || box.y < 0 || box.w < 0 || box.h < 0)
        {
            fprintf(stderr, "[INFO] box err x or y or h or w < 0");
            return nullptr;
        }
        if (box.w % 2 == 1 || box.h % 2 == 1)
        {
            fprintf(stderr, "[INFO] box err w or h not even");
            return nullptr;
        }
        if ((box.w + box.x > input.w) || (box.h + box.y > input.h))
        {
            fprintf(stderr, "[INFO] boxs bottom right overflow input");
            return nullptr;
        }

        auto ax_cv_box = new AX_NPU_CV_Box;
        ax_cv_box->fX = box.x;
        ax_cv_box->fY = box.y;
        ax_cv_box->fW = box.w;
        ax_cv_box->fH = box.h;

        return ax_cv_box;
    }

    AX_NPU_CV_Image* alloc_cv_image(const ax_image& input)
    {
        auto dst_image = new AX_NPU_CV_Image;
        dst_image->nWidth = input.w;
        dst_image->nHeight = input.h;
        dst_image->tStride.nW = input.stride_w;
        dst_image->eDtype = input.color_space;
        auto ret = AX_SYS_MemAlloc((AX_U64*)&dst_image->pPhy, (void**)&dst_image->pVir, cv::get_image_data_size(dst_image), 128, (AX_S8*)"NPU-CV");
        if (ret != AX_ERR_NPU_JOINT_SUCCESS)
        {
            fprintf(stderr, "[ERR] error alloc image sys mem %x \n", ret);
            return nullptr;
        }
        return dst_image;
    }

    int free_cv_image(AX_NPU_CV_Image* image)
    {
        int ret = AX_SYS_MemFree((AX_U64)image->pPhy, (void*)image->pVir);
        if (ret != AX_ERR_NPU_JOINT_SUCCESS)
        {
            fprintf(stderr, "[ERR] error free %x \n", ret);
            return ret;
        }
        return 0;
    }

    int npu_crop_resize(AX_NPU_CV_Image* input_image, const char* input_data, AX_NPU_CV_Image* output_image, AX_NPU_CV_Box* box, AX_NPU_SDK_EX_MODEL_TYPE_T model_type)
    {
        middleware::copy_to_device(input_data, (void**)&input_image->pVir, cv::get_image_data_size(input_image));

        AX_NPU_CV_Color color;
        color.nYUVColorValue[0] = 0;
        color.nYUVColorValue[1] = 128;
        AX_NPU_SDK_EX_MODEL_TYPE_T virtual_npu_mode_type = model_type;
        auto horizontal = (AX_NPU_CV_ImageResizeAlignParam)0;
        auto vertical = (AX_NPU_CV_ImageResizeAlignParam)0;

        int ret = AX_NPU_CV_CropResizeImage(virtual_npu_mode_type, input_image, 1, &output_image, &box, horizontal, vertical, color);
        if (ret != AX_NPU_DEV_STATUS_SUCCESS)
        {
            fprintf(stderr, "[ERR] AX_NPU_CV_CropResizeImage err code: %X", ret);
            return ret;
        }

        return 0;
    }

    int pose_npu_crop_resize(AX_NPU_CV_Image* input_image, AX_NPU_CV_Image* output_image, int input_h, int input_w, int target_w, int target_h, int offset_left, int offset_top, AX_NPU_CV_Box* box,
                             AX_NPU_SDK_EX_MODEL_TYPE_T model_type, AX_NPU_CV_ImageResizeAlignParam horizontal,
                             AX_NPU_CV_ImageResizeAlignParam vertical)
    {
        // int npu_crop_resize(AX_NPU_CV_Image* input_image, AX_NPU_CV_Image* output_image, int target_w, int target_h, int offset_left, int offset_top, AX_NPU_CV_Box* box,
        //                 AX_NPU_SDK_EX_MODEL_TYPE_T model_type, AX_NPU_CV_ImageResizeAlignParam horizontal,
        //                 AX_NPU_CV_ImageResizeAlignParam vertical) {
        AX_NPU_CV_Color color;
        color.nYUVColorValue[0] = 0;
        color.nYUVColorValue[1] = 128;
        AX_NPU_SDK_EX_MODEL_TYPE_T virtual_npu_mode_type = model_type;

        AX_NPU_CV_Box* ppBox[1];
        ppBox[0] = box;

        auto ConvKitNV12Img = [](const AX_VIDEO_FRAME_S& tSrc, AX_BOOL bY) -> AX_NPU_CV_Image {
            AX_NPU_CV_Image t = {0};
            t.nWidth = bY ? (tSrc.u32Width) : (tSrc.u32Width / 2);
            t.nHeight = bY ? (tSrc.u32Height) : (tSrc.u32Height / 2);
            t.eDtype = bY ? AX_NPU_CV_FDT_GRAY : AX_NPU_CV_FDT_UV;
            t.tStride.nW = (0 == tSrc.u32PicStride[0]) ? t.nWidth : tSrc.u32PicStride[0];
            t.pPhy = tSrc.u64PhyAddr[0];
            t.pPhy = bY ? t.pPhy : (t.pPhy + t.tStride.nW * tSrc.u32Height);
            t.pVir = (AX_U8*)tSrc.u64VirAddr[0];
            t.pVir = bY ? (AX_U8*)tSrc.u64VirAddr[0] : ((AX_U8*)(tSrc.u64VirAddr[0] + t.tStride.nW * tSrc.u32Height));
            t.tStride.nW = bY ? t.tStride.nW : (t.tStride.nW / 2);
            t.nSize = bY ? (t.tStride.nW * t.nHeight) : (t.tStride.nW * t.nHeight * 2);
            return t;
        };

        auto ConvCvNV12Img = [](const AX_NPU_CV_Image* tSrc, int target_w, int target_h, int offset_left, int offset_top, AX_BOOL bY) -> AX_NPU_CV_Image {
            AX_NPU_CV_Image t = {0};
            int offset_x = offset_left;
            int offset_y = offset_top;
            if (offset_x < 0)
            {
                offset_x = 0;
            }
            if (offset_y < 0)
            {
                offset_y = 0;
            }
            offset_y = bY ? offset_y : (offset_y / 2);

            t.nWidth = bY ? (target_w) : (target_w / 2);
            t.nHeight = bY ? (target_h) : (target_h / 2);
            t.eDtype = bY ? AX_NPU_CV_FDT_GRAY : AX_NPU_CV_FDT_UV;
            t.tStride.nW = (0 == tSrc->tStride.nW) ? t.nWidth : tSrc->tStride.nW;
            t.pPhy = tSrc->pPhy + t.tStride.nW * offset_y + offset_x;
            t.pPhy = bY ? t.pPhy : (t.pPhy + t.tStride.nW * tSrc->nHeight);
            t.pVir = (AX_U8*)(tSrc->pVir + t.tStride.nW * offset_y + offset_x);
            t.pVir = bY ? (AX_U8*)t.pVir : ((AX_U8*)(t.pVir + t.tStride.nW * tSrc->nHeight));
            t.tStride.nW = bY ? t.tStride.nW : (t.tStride.nW / 2);
            t.nSize = bY ? (t.tStride.nW * t.nHeight) : (t.tStride.nW * t.nHeight * 2);
            return t;
        };

        AX_NPU_CV_Image tImgSrcY = ConvCvNV12Img(input_image, input_w, input_h, 0, 0, AX_TRUE);
        AX_NPU_CV_Image tImgSrcUV = ConvCvNV12Img(input_image, input_w, input_h, 0, 0, AX_FALSE);

        AX_NPU_CV_Image tImgDestY = ConvCvNV12Img(output_image, target_w, target_h, offset_left, offset_top, AX_TRUE);
        AX_NPU_CV_Image tImgDestUV = ConvCvNV12Img(output_image, target_w, target_h, offset_left, offset_top, AX_FALSE);

        AX_NPU_CV_Image* ppDstY[1];
        ppDstY[0] = &tImgDestY;
        AX_NPU_CV_Image* ppDstU[1];
        ppDstU[0] = &tImgDestUV;

        AX_U32 nStride = (0 == output_image->tStride.nW) ? output_image->nWidth : output_image->tStride.nW;
        memset((AX_VOID*)output_image->pVir, 0x00, nStride * output_image->nHeight);
        memset((AX_VOID*)(output_image->pVir + nStride * output_image->nHeight), 0x80, nStride * output_image->nHeight / 2);

        int ret = AX_NPU_CV_CropResizeImageForSplitYUV(virtual_npu_mode_type,
                                                       &tImgSrcY,
                                                       &tImgSrcUV,
                                                       1,
                                                       (AX_NPU_CV_Image**)&ppDstY,
                                                       (AX_NPU_CV_Image**)&ppDstU,
                                                       ppBox,
                                                       horizontal,
                                                       vertical,
                                                       color);

        if (ret != AX_NPU_DEV_STATUS_SUCCESS)
        {
            fprintf(stderr, "[ERR] AX_NPU_CV_CropResizeImage err code: %X\n", ret);
            return ret;
        }

        return 0;
    }

} // namespace axcv