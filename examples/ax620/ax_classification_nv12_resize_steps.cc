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

#include <cstdio>
#include <cstring>
#include <memory>
#include <numeric>

#include <opencv2/opencv.hpp>

#include "base/topk.hpp"
#include "middleware/common_ax620.hpp"

#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include "cv/cv.hpp"
#include "cv/utils.hpp"

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "joint.h"
#include "joint_adv.h"

const int MODEL_BGR_INPUT_H = 224;
const int MODEL_BGR_INPUT_W = 224;

const int DEFAULT_LOOP_COUNT = 1;

namespace ax
{
    namespace cls = classification;
    namespace mw = middleware;
    namespace utl = utilities;

    class ax_crop_resize_nv12
    {
    public:
        ax_crop_resize_nv12() = default;
        ~ax_crop_resize_nv12();
        int init(int input_h, int input_w, int model_h, int model_w, int model_type);
        int run_crop_resize_nv12(std::vector<char>& input_data, std::vector<uint8_t>& output_data);

    private:
        AX_NPU_CV_Image* m_input_image;
        AX_NPU_CV_Image* m_output_image;
        AX_NPU_CV_Box* m_box;

        int m_model_type = AX_NPU_MODEL_TYPE_1_1_1;
    };

    int
    ax_crop_resize_nv12::init(int input_h, int input_w, int model_h, int model_w, int model_type)
    {
        axcv::ax_image input;
        input.w = input_w;
        input.h = input_h;
        input.stride_w = input.w;
        input.color_space = AX_NPU_CV_FDT_NV12;
        m_input_image = axcv::alloc_cv_image(input);
        if (!m_input_image)
        {
            fprintf(stderr, "[ERR] alloc_cv_image input falil \n");
            return -1;
        }

        axcv::ax_box box;
        box.h = input_h;
        box.w = input_w;
        box.x = 0;
        box.y = 0;
        m_box = axcv::filter_box(input, box);
        if (!m_box)
        {
            fprintf(stderr, "[ERR] box not legal \n");
            return -1;
        }

        axcv::ax_image output;
        output.w = model_w;
        output.h = model_h;
        output.stride_w = output.w;
        output.color_space = AX_NPU_CV_FDT_NV12;
        m_output_image = axcv::alloc_cv_image(output);
        if (!m_output_image)
        {
            fprintf(stderr, "[ERR] alloc_cv_image output falil \n");
            return -1;
        }

        m_model_type = model_type;

        return 0;
    }

    int ax_crop_resize_nv12::run_crop_resize_nv12(std::vector<char>& input_data, std::vector<uint8_t>& output_data)
    {
        /**
         *  when copy_to_device or memcpy slow
         *  try:
         *      1. set model_input and crop_resize.dst_image as same block address: ref ax_classification_nv12_resize_opt.cc
         *      2. use npu_cv_crop implement dma copy
         *      3. use ivps_cmm_copy but require 32k align
         */

        int ret = axcv::npu_crop_resize(m_input_image, input_data.data(), m_output_image, m_box, (AX_NPU_SDK_EX_MODEL_TYPE_T)m_model_type);
        if (ret != AX_NPU_DEV_STATUS_SUCCESS)
        {
            fprintf(stderr, "[ERR] npu_crop_resize err code:%x \n", ret);
            return ret;
        }

        memcpy(output_data.data(), m_output_image->pVir, cv::get_image_data_size(m_output_image));
        return 0;
    }

    ax_crop_resize_nv12::~ax_crop_resize_nv12()
    {
        axcv::free_cv_image(m_input_image);
        axcv::free_cv_image(m_output_image);
        delete m_box;
    }

    bool
    run_classification(const std::string& model, const std::vector<uint8_t>& data, const int& repeat)
    {
        // 1. create a runtime handle and load the model
        AX_JOINT_HANDLE joint_handle;
        std::memset(&joint_handle, 0, sizeof(joint_handle));

        AX_JOINT_SDK_ATTR_T joint_attr;
        std::memset(&joint_attr, 0, sizeof(joint_attr));

        // 1.1 read model file to buffer
        std::vector<char> model_buffer;
        if (!ax::utl::read_file(model, model_buffer))
        {
            fprintf(stderr, "Read Run-Joint model(%s) file failed.\n", model.c_str());
            return false;
        }

        // 1.2 parse model from buffer
        //   if the device do not have enough memory to create a buffer at step 3.1,
        //     consider using linux API 'mmap' to map the model file to a pointer,
        //     then use the pointer which returned by mmap to parse the run-joint
        //     model from the file.
        //   it will reduce the peak allocated memory(compared with creating a full
        //     size buffer).
        auto ret = ax::mw::parse_npu_mode_from_joint(model_buffer.data(), model_buffer.size(), &joint_attr.eNpuMode);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Load Run-Joint model(%s) failed.\n", model.c_str());
            return false;
        }

        // 1.3 init model
        ret = AX_JOINT_Adv_Init(&joint_attr);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Init Run-Joint model(%s) failed.\n", model.c_str());
            return false;
        }

        auto deinit_joint = [&joint_handle]() {
            AX_JOINT_DestroyHandle(joint_handle);
            AX_JOINT_Adv_Deinit();
            return false;
        };

        // 1.4 the real init processing
        uint32_t duration_hdl_init_us = 0;
        {
            timer init_timer;
            ret = AX_JOINT_CreateHandle(&joint_handle, model_buffer.data(), model_buffer.size());
            duration_hdl_init_us = (uint32_t)(init_timer.cost() * 1000);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Create Run-Joint handler from file(%s) failed.\n", model.c_str());
                return deinit_joint();
            }
        }

        // 1.5 get the version of toolkit (optional)
        const AX_CHAR* version = AX_JOINT_GetModelToolsVersion(joint_handle);
        fprintf(stdout, "Tools version: %s\n", version);

        // 1.6 drop the model buffer
        std::vector<char>().swap(model_buffer);
        auto io_info = AX_JOINT_GetIOInfo(joint_handle);

        // 1.7 create context
        AX_JOINT_EXECUTION_CONTEXT joint_ctx;
        AX_JOINT_EXECUTION_CONTEXT_SETTING_T joint_ctx_settings;
        std::memset(&joint_ctx, 0, sizeof(joint_ctx));
        std::memset(&joint_ctx_settings, 0, sizeof(joint_ctx_settings));
        ret = AX_JOINT_CreateExecutionContextV2(joint_handle, &joint_ctx, &joint_ctx_settings);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Create Run-Joint context failed.\n");
            return deinit_joint();
        }

        // 2. fill input & prepare to inference
        AX_JOINT_IO_T joint_io_arr;
        AX_JOINT_IO_SETTING_T joint_io_setting;
        std::memset(&joint_io_arr, 0, sizeof(joint_io_arr));
        std::memset(&joint_io_setting, 0, sizeof(joint_io_setting));

        ret = mw::prepare_io(data.data(), data.size(), joint_io_arr, io_info);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Fill input failed.\n");
            AX_JOINT_DestroyExecutionContext(joint_ctx);
            return deinit_joint();
        }
        joint_io_arr.pIoSetting = &joint_io_setting;

        auto clear_and_exit = [&joint_io_arr, &joint_ctx, &joint_handle]() {
            for (size_t i = 0; i < joint_io_arr.nInputSize; ++i)
            {
                AX_JOINT_IO_BUFFER_T* pBuf = joint_io_arr.pInputs + i;
                mw::free_joint_buffer(pBuf);
            }
            for (size_t i = 0; i < joint_io_arr.nOutputSize; ++i)
            {
                AX_JOINT_IO_BUFFER_T* pBuf = joint_io_arr.pOutputs + i;
                mw::free_joint_buffer(pBuf);
            }
            delete[] joint_io_arr.pInputs;
            delete[] joint_io_arr.pOutputs;

            AX_JOINT_DestroyExecutionContext(joint_ctx);
            AX_JOINT_DestroyHandle(joint_handle);
            AX_JOINT_Adv_Deinit();

            return false;
        };

        // 3. get the init profile info.
        AX_JOINT_COMPONENT_T* joint_comps;
        uint32_t joint_comp_size;

        ret = AX_JOINT_ADV_GetComponents(joint_ctx, &joint_comps, &joint_comp_size);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Get components failed.\n");
            return clear_and_exit();
        }

        uint32_t duration_neu_init_us = 0;
        uint32_t duration_axe_init_us = 0;
        for (uint32_t j = 0; j < joint_comp_size; ++j)
        {
            auto& comp = joint_comps[j];
            switch (comp.eType)
            {
            case AX_JOINT_COMPONENT_TYPE_T::AX_JOINT_COMPONENT_TYPE_NEU:
            {
                duration_neu_init_us += comp.tProfile.nInitUs;
                break;
            }
            case AX_JOINT_COMPONENT_TYPE_T::AX_JOINT_COMPONENT_TYPE_AXE:
            {
                duration_axe_init_us += comp.tProfile.nInitUs;
                break;
            }
            default:
                fprintf(stderr, "Unknown component type %d.\n", (int)comp.eType);
            }
        }

        // 4. run & benchmark
        uint32_t duration_neu_core_us = 0, duration_neu_total_us = 0;
        uint32_t duration_axe_core_us = 0, duration_axe_total_us = 0;

        std::vector<float> time_costs(repeat, 0.f);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_JOINT_RunSync(joint_handle, joint_ctx, &joint_io_arr);
            time_costs[i] = tick.cost();
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Inference failed(%d).\n", ret);
                return clear_and_exit();
            }

            ret = AX_JOINT_ADV_GetComponents(joint_ctx, &joint_comps, &joint_comp_size);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Get components after run failed.\n");
                return clear_and_exit();
            }

            for (uint32_t j = 0; j < joint_comp_size; ++j)
            {
                auto& comp = joint_comps[j];

                if (comp.eType == AX_JOINT_COMPONENT_TYPE_T::AX_JOINT_COMPONENT_TYPE_NEU)
                {
                    duration_neu_core_us += comp.tProfile.nCoreUs;
                    duration_neu_total_us += comp.tProfile.nTotalUs;
                }

                if (comp.eType == AX_JOINT_COMPONENT_TYPE_T::AX_JOINT_COMPONENT_TYPE_AXE)
                {
                    duration_axe_core_us += comp.tProfile.nCoreUs;
                    duration_axe_total_us += comp.tProfile.nTotalUs;
                }
            }
        }

        // 5. get top K
        for (uint32_t i = 0; i < io_info->nOutputSize; ++i)
        {
            auto& output = io_info->pOutputs[i];
            auto& info = joint_io_arr.pOutputs[i];

            auto ptr = (float*)info.pVirAddr;
            auto actual_data_size = output.nSize / output.pShape[0] / sizeof(float);

            std::vector<cls::score> result(actual_data_size);
            for (uint32_t id = 0; id < actual_data_size; id++)
            {
                result[id].id = id;
                result[id].score = ptr[id];
            }

            cls::sort_score(result);
            cls::print_score(result, 5);
        }

        // 6. show time costs
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout,
                "Create handle took %.2f ms (neu %.2f ms, axe %.2f ms, overhead %.2f ms)\n",
                duration_hdl_init_us / 1000.,
                duration_neu_init_us / 1000.,
                duration_axe_init_us / 1000.,
                (duration_hdl_init_us - duration_neu_init_us - duration_axe_init_us) / 1000.);

        fprintf(stdout, "--------------------------------------\n");

        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                repeat,
                total_time / (float)repeat,
                *min_max_time.second,
                *min_max_time.first);

        clear_and_exit();
        return true;
    }
} // namespace ax

// ./ax_classification_nv12 -m mobilenetv2-7.joint -i 800x480car.nv12 -h 480 -w 800
// ./ax_classification_nv12 -m mobilenetv2-7.joint -i 480x360cat.yuv -h 360 -w 480
int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("input", 'i', "input file", true, "");
    cmd.add<AX_U32>("input_h", 'h', "input_h", true, 0);
    cmd.add<AX_U32>("input_w", 'w', "input_w", true, 0);

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.add<int>("mode_type", 'n', "model_type", false, AX_NPU_MODEL_TYPE_1_1_1);
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("model");
    auto input_file = cmd.get<std::string>("input");

    auto model_file_flag = utilities::file_exist(model_file);
    auto image_file_flag = utilities::file_exist(input_file);

    auto input_h = cmd.get<AX_U32>("input_h");
    auto input_w = cmd.get<AX_U32>("input_w");

    auto model_type = (AX_NPU_SDK_EX_MODEL_TYPE_T)cmd.get<int>("mode_type");

    if (!model_file_flag | !image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!image_file_flag) { show_error("image", input_file); }

        return -1;
    }

    auto repeat = cmd.get<int>("repeat");

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");

    fprintf(stdout, "model file : %s\n", model_file.c_str());
    fprintf(stdout, "input file : %s\n", input_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_h, input_w);

    // 2. init ax system, if NOT INITED in other apps.
    //   if other app init the device, DO NOT INIT DEVICE AGAIN.
    //   this init ONLY will be used in demo apps to avoid using a none inited
    //     device.
    //   for example, if another app(such as camera init app) has already init
    //     the system, then DO NOT call this api again, if it does, the system
    //     will be re-inited and loss the last configuration.
    AX_S32 ret = AX_SYS_Init();
    if (0 != ret)
    {
        fprintf(stderr, "Init system failed.\n");
        return ret;
    }

    AX_NPU_SDK_EX_ATTR_T hard_mode = {common::get_hard_mode_by_model_type(model_type)};
    ret = AX_NPU_SDK_EX_Init_with_attr(&hard_mode);
    if (ret != AX_NPU_DEV_STATUS_SUCCESS)
    {
        fprintf(stderr, "[ERR] AX_NPU_SDK_EX_Init_with_attr err code:%x", ret);
        return ret;
    }

    // 3. read nv12 image
    std::vector<char> input;
    utilities::read_file(input_file, input);

    // 4. crop_resize
    //    use sys_npu_api so sys_init before process nv12
    std::vector<uint8_t> model_input(MODEL_BGR_INPUT_W * MODEL_BGR_INPUT_H * 3 / 2);
    {
        auto crop_resize_helper = std::make_unique<ax::ax_crop_resize_nv12>();
        crop_resize_helper->init((int)input_h, (int)input_w, MODEL_BGR_INPUT_H, MODEL_BGR_INPUT_W, model_type);
        timer crop_resize_timer;
        for (size_t i = 0; i < 10; i++)
        {
            crop_resize_helper->run_crop_resize_nv12(input, model_input);
        }
        float crop_resize_cost = crop_resize_timer.cost();
        fprintf(stdout, "run crop_resize time cost avg: %f :ms \n", crop_resize_cost / 10);
    }

    // 5. show the version (optional)
    fprintf(stdout, "Run-Joint Runtime version: %s\n", AX_JOINT_GetVersion());
    fprintf(stdout, "--------------------------------------\n");

    // 6. run the processing
    auto flag = ax::run_classification(model_file, model_input, repeat);
    if (!flag)
    {
        fprintf(stderr, "Run classification failed.\n");
    }

    // 7. last de-init
    //   as step 1, if the device inited by another app, DO NOT de-init the
    //     device at this app.
    AX_NPU_SDK_EX_Deinit();
    AX_SYS_Deinit();
}
