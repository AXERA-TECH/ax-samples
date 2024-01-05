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

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include "ax_ivps_api.h"

const int DEFAULT_IMG_H = 1080;
const int DEFAULT_IMG_W = 1920;
const int DEFAULT_LOOP_COUNT = 1;
int cropresize_width = 640, cropresize_height = 640;

void save_file(const char* path, void* data, int len)
{
    FILE* fp = fopen(path, "wb");
    fwrite(data, 1, len, fp);
    fclose(fp);
}

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("image", 'i', "image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    auto image_file = cmd.get<std::string>("image");
    auto repeat = cmd.get<int>("repeat");
    auto input_size_string = cmd.get<std::string>("size");
    std::array<int, 2> input_size = {DEFAULT_IMG_H, DEFAULT_IMG_W};
    auto input_size_flag = utilities::parse_string(input_size_string, input_size);

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");
    fprintf(stdout, "image file : %s\n", image_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);
    fprintf(stdout, "--------------------------------------\n");

    // 2. read image and resize to target size
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    cv::resize(mat, mat, cv::Size(input_size[1], input_size[0]));

    // 3. allow cmm image
    AX_SYS_Init();
    int ret = AX_IVPS_Init();
    if (ret != 0)
    {
        printf("AX_IVPS_Init failed, s32Ret=0x%x!\n", ret);
    }
    AX_VIDEO_FRAME_T ive_bgr = {0}, ive_nv12 = {0}, ive_nv12_crop_resize = {0};
    unsigned char* tmp = 0;
    ret = AX_SYS_MemAlloc(&ive_bgr.u64PhyAddr[0], (void**)&tmp, mat.cols * mat.rows * 3, 128, (const AX_S8*)"NULL");
    if (ret != 0)
    {
        fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
        return -1;
    }
    ive_bgr.u64VirAddr[0] = (AX_U64)tmp;
    ive_bgr.enImgFormat = AX_FORMAT_BGR888;
    ive_bgr.u32PicStride[0] = mat.cols;
    ive_bgr.u32Width = mat.cols;
    ive_bgr.u32Height = mat.rows;
    memcpy(tmp, mat.data, mat.cols * mat.rows * 3);

    ret = AX_SYS_MemAlloc(&ive_nv12.u64PhyAddr[0], (void**)&tmp, mat.cols * mat.rows * 3 / 2, 128, (const AX_S8*)"NULL");
    if (ret != 0)
    {
        fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
        return -1;
    }
    ive_nv12.u64VirAddr[0] = (AX_U64)tmp;
    ive_nv12.u64VirAddr[1] = (AX_U64)(tmp + mat.cols * mat.rows);
    ive_nv12.u64PhyAddr[1] = ive_nv12.u64PhyAddr[0] + mat.cols * mat.rows;
    ive_nv12.enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
    ive_nv12.u32PicStride[0] = ive_nv12.u32PicStride[1] = mat.cols;
    ive_nv12.u32Width = mat.cols;
    ive_nv12.u32Height = mat.rows;

    ret = AX_SYS_MemAlloc(&ive_nv12_crop_resize.u64PhyAddr[0], (void**)&tmp, cropresize_width * cropresize_height * 3 / 2, 128, (const AX_S8*)"NULL");
    if (ret != 0)
    {
        fprintf(stderr, "AX_SYS_MemAlloc failed.\n");
        return -1;
    }
    ive_nv12_crop_resize.u64VirAddr[0] = (AX_U64)tmp;
    ive_nv12_crop_resize.u64VirAddr[1] = (AX_U64)(tmp + cropresize_width * cropresize_height);
    ive_nv12_crop_resize.u64PhyAddr[1] = ive_nv12_crop_resize.u64PhyAddr[0] + cropresize_width * cropresize_height;
    ive_nv12_crop_resize.enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
    ive_nv12_crop_resize.u32PicStride[0] = ive_nv12_crop_resize.u32PicStride[1] = cropresize_width;
    ive_nv12_crop_resize.u32Width = cropresize_width;
    ive_nv12_crop_resize.u32Height = cropresize_height;

    // 4. csc
    timer timer_csc;
    for (size_t i = 0; i < repeat; i++)
    {
        ret = AX_IVPS_CscTdp(&ive_bgr, &ive_nv12);
        if (ret != 0)
        {
            fprintf(stderr, "AX_IVPS_CSC failed. 0x%x\n", ret);
        }
    }
    printf("h:%d w:%d csc %d times cost time:%.2f ms \n", input_size[1], input_size[0], repeat, timer_csc.cost());

    // 5. crop resize
    {
        memset((void*)ive_nv12_crop_resize.u64VirAddr[0], 0, cropresize_width * cropresize_height * 3 / 2);
        timer timer_crop_resize;

        AX_IVPS_ASPECT_RATIO_T resize_ctrl;
        resize_ctrl.nBgColor = 0xFFFFFF;
        resize_ctrl.eMode = AX_IVPS_ASPECT_RATIO_AUTO;
        resize_ctrl.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
        resize_ctrl.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
        for (size_t i = 0; i < repeat; i++)
        {
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
        }
        printf("h:%d w:%d cropresize %d times cost time:%.2f ms \n", input_size[1], input_size[0], repeat, timer_crop_resize.cost());
        save_file("crop_resize.nv12", (void*)ive_nv12_crop_resize.u64VirAddr[0], ive_nv12_crop_resize.u32Width * ive_nv12_crop_resize.u32Height * 3 / 2);
    }

    {
        memset((void*)ive_nv12_crop_resize.u64VirAddr[0], 0, cropresize_width * cropresize_height * 3 / 2);
        timer timer_crop_resize;

        AX_IVPS_ASPECT_RATIO_T resize_ctrl;
        resize_ctrl.nBgColor = 0xFFFFFF;
        resize_ctrl.eMode = AX_IVPS_ASPECT_RATIO_AUTO;
        resize_ctrl.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
        resize_ctrl.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
        AX_IVPS_RECT_T pbox[1] = {{0, 0, (AX_U16)(ive_nv12.u32Width / 2), (AX_U16)(ive_nv12.u32Height / 2)}};
        for (size_t i = 0; i < repeat; i++)
        {
            auto temp = &ive_nv12_crop_resize;
#ifdef AXERA_TARGET_CHIP_AX620E
            AX_IVPS_CROP_RESIZE_ATTR_T crop_resize_attr;
            memset(&crop_resize_attr, 0, sizeof(crop_resize_attr));
            memcpy(&crop_resize_attr.tAspectRatio, &resize_ctrl, sizeof(resize_ctrl));
            ret = AX_IVPS_CropResizeV2Vpp(&ive_nv12, 1, pbox, temp, &crop_resize_attr);
#else
            ret = AX_IVPS_CropResizeV2Vpp(&ive_nv12, pbox, 1, &temp, &resize_ctrl);
#endif
            if (ret != 0)
            {
                fprintf(stderr, "AX_IVPS_CropResize failed. 0x%x\n", ret);
            }
        }
        printf("h:%d w:%d cropresize %d times cost time:%.2f ms \n", input_size[1], input_size[0], repeat, timer_crop_resize.cost());
        save_file("crop_resize_v2.nv12", (void*)ive_nv12_crop_resize.u64VirAddr[0], ive_nv12_crop_resize.u32Width * ive_nv12_crop_resize.u32Height * 3 / 2);
    }

    {
        memset((void*)ive_nv12_crop_resize.u64VirAddr[0], 0, cropresize_width * cropresize_height * 3 / 2);
        timer timer_crop_resize;

        AX_IVPS_ASPECT_RATIO_T resize_ctrl;
        resize_ctrl.nBgColor = 0xFFFFFF;
        resize_ctrl.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
        resize_ctrl.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
        resize_ctrl.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
        AX_IVPS_RECT_T pbox[1] = {{0, 0, (AX_U16)(ive_nv12.u32Width), (AX_U16)(ive_nv12.u32Height)}};
        for (size_t i = 0; i < repeat; i++)
        {
            auto temp = &ive_nv12_crop_resize;
#ifdef AXERA_TARGET_CHIP_AX620E
            AX_IVPS_CROP_RESIZE_ATTR_T crop_resize_attr;
            memset(&crop_resize_attr, 0, sizeof(crop_resize_attr));
            memcpy(&crop_resize_attr.tAspectRatio, &resize_ctrl, sizeof(resize_ctrl));
            ret = AX_IVPS_CropResizeV2Vpp(&ive_nv12, 1, pbox, temp, &crop_resize_attr);
#else
            ret = AX_IVPS_CropResizeV2Vpp(&ive_nv12, pbox, 1, &temp, &resize_ctrl);
#endif
            if (ret != 0)
            {
                fprintf(stderr, "AX_IVPS_CropResize failed. 0x%x\n", ret);
            }
        }
        printf("h:%d w:%d cropresize %d times cost time:%.2f ms \n", input_size[1], input_size[0], repeat, timer_crop_resize.cost());
        save_file("crop_resize_v2_force.nv12", (void*)ive_nv12_crop_resize.u64VirAddr[0], ive_nv12_crop_resize.u32Width * ive_nv12_crop_resize.u32Height * 3 / 2);
    }

    save_file("rgb.nv12", (void*)ive_bgr.u64VirAddr[0], ive_bgr.u32Width * ive_bgr.u32Height * 3);
    save_file("nv12.nv12", (void*)ive_nv12.u64VirAddr[0], ive_nv12.u32Width * ive_nv12.u32Height * 3 / 2);

    AX_IVPS_Deinit();
    AX_SYS_Deinit();
    return 0;
}
