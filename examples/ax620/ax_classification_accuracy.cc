/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* License); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

/*
* Copyright (c) 2022, AXERA TECH
* Author: hebing
*/

#include <cstdio>
#include <cstring>
#include <numeric>

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fcntl.h>
#include <sys/mman.h>

#include "base/topk.hpp"
#include "base/common.hpp"

#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "joint.h"
#include "joint_adv.h"

namespace ax
{
    namespace cls = classification;
    namespace mw = middleware;
    namespace utl = utilities;

    static void softmax(std::vector<cls::score>& input, int n, int stride)
    {
        int i;
        float sum = 0;
        float largest = input[0].score;
        for (i = 0; i < n; ++i)
        {
            if (input[i * stride].score > largest)
                largest = input[i * stride].score;
        }
        for (i = 0; i < n; ++i)
        {
            float e = exp(input[i * stride].score - largest);
            sum += e;
            input[i * stride].score = e;
        }
        for (i = 0; i < n; ++i)
        {
            input[i * stride].score /= sum;
        }
    }

    bool run_classification(const std::string& model, const std::string& image_dir, const std::string& val_file)
    {
        // 1. create a runtime handle and load the model
        AX_JOINT_HANDLE joint_handle;
        std::memset(&joint_handle, 0, sizeof(joint_handle));

        AX_JOINT_SDK_ATTR_T joint_attr;
        std::memset(&joint_attr, 0, sizeof(joint_attr));

        // 1.1 read model use
        auto* file_fp = fopen(model.c_str(), "r");
        if (!file_fp)
        {
            fprintf(stderr, "read model file fail \n");
        }

        fseek(file_fp, 0, SEEK_END);
        int model_size = ftell(file_fp);
        fclose(file_fp);

        int fd = open(model.c_str(), O_RDWR, 0644);
        void* mmap_add = mmap(NULL, model_size, PROT_WRITE, MAP_SHARED, fd, 0);

        // 1.2 parse model from buffer
        //   if the device do not have enough memory to create a buffer at step 3.1,
        //     consider using linux API 'mmap' to map the model file to a pointer,
        //     then use the pointer which returned by mmap to parse the run-joint
        //     model from the file.
        //   it will reduce the peak allocated memory(compared with creating a full
        //     size buffer).
        // auto ret = ax::mw::parse_npu_mode_from_joint((char*)mmap_add, model_size, &joint_attr.eNpuMode);

        joint_attr.eNpuMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_1_1;
        //   if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        //   {
        //      fprintf(stderr, "Load Run-Joint model(%s) failed.\n", model.c_str());
        //      return false;
        //   }

        // 1.3 init model
        auto ret = AX_JOINT_Adv_Init(&joint_attr);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Init Run-Joint model(%s) failed.\n", model.c_str());
            return false;
        }

        auto deinit_joint = [&joint_handle, &mmap_add, model_size]() {
            AX_JOINT_DestroyHandle(joint_handle);
            AX_JOINT_Adv_Deinit();
            munmap(mmap_add, model_size);
            return false;
        };

        // 1.4 the real init processing
        uint32_t duration_hdl_init_us = 0;
        {
            timer init_timer;
            ret = AX_JOINT_CreateHandle(&joint_handle, mmap_add, model_size);
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
        // std::vector<char>().swap(model_buffer);
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

        auto clear_and_exit = [&joint_io_arr, &joint_ctx, &joint_handle, &mmap_add, model_size]() {
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

            munmap(mmap_add, model_size);

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

        auto input_sizes = mw::io_get_input_size(io_info);
        fprintf(stderr, "[INFO] get input_size %d, %d\n", input_sizes[0], input_sizes[1]);
        auto image_size = 3 * input_sizes[0] * input_sizes[1];
        auto pBuf = mw::prepare_io_no_copy(image_size, joint_io_arr, io_info);

        // 4. run & benchmark
        uint32_t duration_neu_core_us = 0, duration_neu_total_us = 0;
        uint32_t duration_axe_core_us = 0, duration_axe_total_us = 0;

        std::ifstream val_file_1000(val_file);
        if (!val_file_1000.is_open())
        {
            fprintf(stderr, "[ERR] val_file_1000 open fail \n");
            clear_and_exit();
        }

        std::vector<float> time_costs;
        std::string val_file_1000_line_temp;
        std::vector<uint8_t> image(image_size);
        int top_1 = 0, top_5 = 0, total = 0;
        cv::Mat mat_input;
        cv::Mat img_new(input_sizes[0], input_sizes[1], CV_8UC3, image.data());
        std::vector<cls::score> result(1001);
        int cur_index = 0;

        // 5. loop the val dataset
        while (getline(val_file_1000, val_file_1000_line_temp))
        {
            // 1.0 decode file path
            std::stringstream val_1000_line_ss(val_file_1000_line_temp);
            std::string file_name, gt_index;
            getline(val_1000_line_ss, file_name, ' ');
            getline(val_1000_line_ss, gt_index, ' ');
            std::string image_file_path = image_dir + '/' + file_name;

            // 1.1 prepare image precess
            mat_input = cv::imread(image_file_path);
            if (mat_input.empty())
            {
                fprintf(stderr, "Read image failed.\n");
                clear_and_exit();
            }

            // 1.2 prepare resize, center crop & swapRB(default is OFF)
            common::get_input_data_centercrop(mat_input, image, input_sizes[0], input_sizes[1], false);

            mat_input.release();

            mw::copy_to_device(image.data(), image.size(), pBuf);
            joint_io_arr.pIoSetting = &joint_io_setting;

            // 1.3 run joint, inference the model
            timer tick;
            ret = AX_JOINT_RunSync(joint_handle, joint_ctx, &joint_io_arr);
            time_costs.push_back(tick.cost());

            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Inference failed(%d).\n", ret);
                return clear_and_exit();
            }

            // 1.4 get the result
            auto& output = io_info->pOutputs[0];
            auto& info = joint_io_arr.pOutputs[0];

            auto ptr = (float*)info.pVirAddr;
            auto actual_data_size = output.nSize / output.pShape[0] / sizeof(float);

            // 1.5 calculate the top1 & top5
            for (uint32_t id = 0; id < actual_data_size; id++)
            {
                result[id].id = id;
                result[id].score = ptr[id];
            }

            cls::sort_score(result);
            auto gt_index_1 = std::stoi(gt_index);
            if (result[0].id == gt_index_1)
            {
                top_1++;
            }
            for (int j = 0; j < 5; ++j)
            {
                if (result[j].id == gt_index_1)
                {
                    top_5++;
                    break;
                }
            }

            // cur_index++;
            // fprintf(stderr, "%d \n", cur_index);
            //            fprintf(stderr, "[标签]: %d \n", gt_index_1);
            //            fprintf(stderr, "top1 [分类号]:%d : [置信度]:%f\n", result[0].id, result[0].score);
            //            fprintf(stderr, "top2 [分类号]:%d : [置信度]:%f\n", result[1].id, result[1].score);
            //            fprintf(stderr, "top3 [分类号]:%d : [置信度]:%f\n", result[2].id, result[2].score);
            //            fprintf(stderr, "top4 [分类号]:%d : [置信度]:%f\n", result[3].id, result[3].score);
            //            fprintf(stderr, "top5 [分类号]:%d : [置信度]:%f\n", result[4].id, result[4].score);

            total++;
            float acc_top1 = ((float )top_1 / (float )total);
            float acc_top5 = ((float )top_5 / (float )total);

            if (total % 100 == 0)
            {
                fprintf(stdout, "gt_id %5d, total %5d, top1 %5d(%4.2f %%), top5 %5d(%4.2f %%) \t", gt_index_1, total, top_1, acc_top1*100, top_5, acc_top5*100);   
                fprintf(stdout, "[INFO] predict:%s top5:[%3d,%3d,%3d,%3d,%3d] gt:[%3s] \n", file_name.c_str(), result[0].id, result[1].id, result[2].id, result[3].id, result[4].id, gt_index.c_str());
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
                "run model %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                time_costs.size(),
                total_time / (float)time_costs.size(),
                *min_max_time.second,
                *min_max_time.first);

        clear_and_exit();
        return true;
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("images", 'i', "image file dir", true, "");
    cmd.add<std::string>("val", 'v', "val file", true, "");

    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("model");
    auto image_file = cmd.get<std::string>("images");
    auto val_file = cmd.get<std::string>("val");

    auto model_file_flag = utilities::file_exist(model_file);
    auto val_file_flag = utilities::file_exist(val_file);

    if (!model_file_flag | !val_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!val_file_flag) { show_error("val", image_file); }

        return -1;
    }

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");

    fprintf(stdout, "model file : %s\n", model_file.c_str());
    fprintf(stdout, "val file : %s\n", val_file.c_str());

    // 3. init ax system, if NOT INITED in other apps.
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

    // 4. show the version (optional)
    fprintf(stdout, "Run-Joint Runtime version: %s\n", AX_JOINT_GetVersion());
    fprintf(stdout, "--------------------------------------\n");

    // 5. run the processing
    auto flag
        = ax::run_classification(model_file, image_file, val_file);
    if (!flag)
    {
        fprintf(stderr, "Run classification failed.\n");
    }

    // 6. last de-init
    //   as step 1, if the device inited by another app, DO NOT de-init the
    //     device at this app.
    AX_SYS_Deinit();
}
