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
 * Author: tianpengcheng
 */

#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "base/detection.hpp"
#include "base/pose.hpp"

#include "base/transform.hpp"
#include "middleware/io.hpp"
#include "middleware/common_ax620.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "joint.h"
#include "joint_adv.h"

#include "npu_cv_kit/ax_npu_imgproc.h"

#include "cv/utils.hpp"
#include "cv/cv.hpp"

#include <iostream>
#include <fstream>

const int DEFAULT_IMG_H = 512;
const int DEFAULT_IMG_W = 288;
const int HRNET_H = 256;
const int HRNET_W = 192;
const int HRNET_JOINTS = 17;

const char* CLASS_NAMES[] = {
    "person"};

const int DEFAULT_LOOP_COUNT = 1;

const float PROB_THRESHOLD = 0.5f;
const float NMS_THRESHOLD = 0.5f;
namespace ax
{
    namespace det = detection;
    namespace mw = middleware;
    namespace utl = utilities;

    // nv12 crop resize img
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

    int ax_crop_resize_nv12::init(int input_h, int input_w, int model_h, int model_w, int model_type)
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
        int ret = axcv::npu_crop_resize(m_input_image, input_data.data(), m_output_image, m_box, (AX_NPU_SDK_EX_MODEL_TYPE_T)m_model_type);
        if (ret != AX_NPU_DEV_STATUS_SUCCESS)
        {
            fprintf(stderr, "[ERR] npu_crop_resize err code:%x \n", ret);
            return ret;
        }
        else
        {
            memcpy(output_data.data(), m_output_image->pVir, cv::get_image_data_size(m_output_image));
            return 1;
        }
    }

    ax_crop_resize_nv12::~ax_crop_resize_nv12()
    {
        axcv::free_cv_image(m_input_image);
        axcv::free_cv_image(m_output_image);
        delete m_box;
    }

    // nv12 crop resize img

    static void generate_yolox_proposals(std::vector<det::GridAndStride> grid_strides, float* feat_ptr, float prob_threshold, std::vector<det::Object>& objects, int wxc)
    {
        const int num_grid = 3549;
        const int num_class = 1;
        const int num_anchors = grid_strides.size();

        for (int anchor_idx = 0; anchor_idx < num_anchors; anchor_idx++)
        {
            float box_objectness = feat_ptr[4 * wxc + anchor_idx];
            float box_cls_score = feat_ptr[5 * wxc + anchor_idx];
            float box_prob = box_objectness * box_cls_score;
            if (box_prob > prob_threshold)
            {
                det::Object obj;
                // printf("%d,%d\n",num_anchors,anchor_idx);
                const int grid0 = grid_strides[anchor_idx].grid0;   // 0
                const int grid1 = grid_strides[anchor_idx].grid1;   // 0
                const int stride = grid_strides[anchor_idx].stride; // 8
                // yolox/models/yolo_head.py decode logic
                //  outputs[..., :2] = (outputs[..., :2] + grids) * strides
                //  outputs[..., 2:4] = torch.exp(outputs[..., 2:4]) * strides
                float x_center = (feat_ptr[0 + anchor_idx] + grid0) * stride;
                float y_center = (feat_ptr[1 * wxc + anchor_idx] + grid1) * stride;
                float w = exp(feat_ptr[2 * wxc + anchor_idx]) * stride;
                float h = exp(feat_ptr[3 * wxc + anchor_idx]) * stride;
                float x0 = x_center - w * 0.5f;
                float y0 = y_center - h * 0.5f;
                obj.rect.x = x0;
                obj.rect.y = y0;
                obj.rect.width = w;
                obj.rect.height = h;
                obj.label = 0;
                obj.prob = box_prob;

                objects.push_back(obj);
            }
        } // point anchor loop
    }

    bool run_detection(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, uint32_t input_h, uint32_t input_w, std::vector<det::Object>& object_bbox, float conf_prob)
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
            fprintf(stderr, "relu_tiny25 p det Read Run-Joint model(%s) file failed.\n", model.c_str());
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
            fprintf(stderr, "relu_tiny25 p det Load Run-Joint model(%s) failed.\n", model.c_str());
            return false;
        }

        // 1.3 init model
        ret = AX_JOINT_Adv_Init(&joint_attr);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "relu_tiny25 p det Init Run-Joint model(%s) failed.\n", model.c_str());
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
                fprintf(stderr, "relu_tiny25 p det Create Run-Joint handler from file(%s) failed.\n", model.c_str());
                return deinit_joint();
            }
        }

        // 1.5 get the version of toolkit (optional)
        const AX_CHAR* version = AX_JOINT_GetModelToolsVersion(joint_handle);
        fprintf(stdout, "relu_tiny25 p det Tools version: %s\n", version);

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
            fprintf(stderr, "relu_tiny25 p det Create Run-Joint context failed.\n");
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
            fprintf(stderr, "relu_tiny25 p det Fill input failed.\n");
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
            fprintf(stderr, "relu_tiny25 p det Get components failed.\n");
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
                fprintf(stderr, "relu_tiny25 p det Unknown component type %d.\n", (int)comp.eType);
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
                fprintf(stderr, "relu_tiny25 p det Inference failed(%d).\n", ret);
                return clear_and_exit();
            }

            ret = AX_JOINT_ADV_GetComponents(joint_ctx, &joint_comps, &joint_comp_size);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "relu_tiny25 p det Get components after run failed.\n");
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
        fprintf(stdout, "relu_tiny25 p det run over: output len %d\n", io_info->nOutputSize);

        // 5. get bbox
        std::vector<det::Object> proporsel;
        std::vector<std::vector<int> > stride = {{8}, {16}, {32}};

        float post_time_costs = 0.f;
        timer tick;
        for (uint32_t i = 0; i < io_info->nOutputSize; ++i)
        {
            auto& output = io_info->pOutputs[i];
            auto& info = joint_io_arr.pOutputs[i];
            auto ptr = (float*)info.pVirAddr;
            std::vector<det::GridAndStride> grid_stride;
            int wxc = output.pShape[2] * output.pShape[3];
            det::generate_grids_and_stride(input_w, input_h, stride[i], grid_stride);
            ax::generate_yolox_proposals(grid_stride, ptr, conf_prob, proporsel, wxc);
        }
        det::get_out_bbox(proporsel, object_bbox, NMS_THRESHOLD, input_h, input_w, mat.rows, mat.cols);
        post_time_costs = tick.cost();
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "relu_tiny25 p det post_time_costs time cost: %.2f ms\n", post_time_costs);
        fprintf(stdout, "--------------------------------------\n");

        // 6. show time costs
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout,
                "relu_tiny25 p det create handle took %.2f ms (neu %.2f ms, axe %.2f ms, overhead %.2f ms)\n",
                duration_hdl_init_us / 1000.,
                duration_neu_init_us / 1000.,
                duration_axe_init_us / 1000.,
                (duration_hdl_init_us - duration_neu_init_us - duration_axe_init_us) / 1000.);

        fprintf(stdout, "--------------------------------------\n");

        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "relu_tiny25 p det Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                repeat,
                total_time / (float)repeat,
                *min_max_time.second,
                *min_max_time.first);
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "relu_tiny25 p det detection num: %d\n", object_bbox.size());

        //
        det::draw_objects(mat, object_bbox, CLASS_NAMES, "relu_tiny25");
        clear_and_exit();
        return true;
    }

    bool pose_run_detection(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, uint32_t input_h, uint32_t input_w, const detection::Object& obj, int offset_top, int offset_left, int offset_x, int offset_y, float ratio)
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
            fprintf(stderr, "pose Load Run-Joint model(%s) failed.\n", model.c_str());
            return false;
        }

        // 1.3 init model
        ret = AX_JOINT_Adv_Init(&joint_attr);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "pose Init Run-Joint model(%s) failed.\n", model.c_str());
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
                fprintf(stderr, "pose Create Run-Joint handler from file(%s) failed.\n", model.c_str());
                return deinit_joint();
            }
        }

        // 1.5 get the version of toolkit (optional)
        const AX_CHAR* version = AX_JOINT_GetModelToolsVersion(joint_handle);
        fprintf(stdout, "pose Tools version: %s\n", version);

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
            fprintf(stderr, "pose Create Run-Joint context failed.\n");
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
            fprintf(stderr, "pose Fill input failed.\n");
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
            fprintf(stderr, "pose Get components failed.\n");
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
                fprintf(stderr, "pose Unknown component type %d.\n", (int)comp.eType);
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
                fprintf(stderr, "pose Inference failed(%d).\n", ret);
                return clear_and_exit();
            }

            ret = AX_JOINT_ADV_GetComponents(joint_ctx, &joint_comps, &joint_comp_size);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "pose Get components after run failed.\n");
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
        fprintf(stdout, "pose run over: output len %d\n", io_info->nOutputSize);

        // 5. get result
        pose::ai_body_parts_s ai_point_result;
        auto& output = io_info->pOutputs[0];
        auto& info = joint_io_arr.pOutputs[0];
        auto ptr = (float*)info.pVirAddr;
        auto& info_index = joint_io_arr.pOutputs[1];
        auto ptr_index = (float*)info_index.pVirAddr;

        float pose_post_time_costs = 0.f;
        timer tick;
        pose::ppl_pose_post_process(ptr, ptr_index, ai_point_result, HRNET_JOINTS, HRNET_H, HRNET_W, offset_top, offset_left, offset_x, offset_y, ratio);
        pose_post_time_costs = tick.cost();
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "pose post_time_costs time cost: %.2f ms\n", pose_post_time_costs);
        fprintf(stdout, "--------------------------------------\n");

        // 6. show time costs
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout,
                "pose Create handle took %.2f ms (neu %.2f ms, axe %.2f ms, overhead %.2f ms)\n",
                duration_hdl_init_us / 1000.,
                duration_neu_init_us / 1000.,
                duration_axe_init_us / 1000.,
                (duration_hdl_init_us - duration_neu_init_us - duration_axe_init_us) / 1000.);

        fprintf(stdout, "--------------------------------------\n");

        auto total_time = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "pose Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                repeat,
                total_time / (float)repeat,
                *min_max_time.second,
                *min_max_time.first);
        fprintf(stdout, "--------------------------------------\n");

        pose::draw_result(mat, ai_point_result, HRNET_JOINTS, HRNET_W, HRNET_H, obj);

        clear_and_exit();
        return true;
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("dmodel", 'd', "det model joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("pmodel", 'p', "pose model joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("image", 'i', "image *.jpg file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));
    // cmd.add<std::string>("yuv", 'y', "yuv file", true, "");

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.add<float>("conf", 'c', "conf prob threshold", false, PROB_THRESHOLD);
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("dmodel");
    auto pose_model_file = cmd.get<std::string>("pmodel");
    auto image_file = cmd.get<std::string>("image");
    // auto yuv_file = cmd.get<std::string>("yuv");
    //1.获取不带路径的文件名
    std::string fnd = "jpg";
    std::string rep = "yuv";
    auto yuv_file = image_file;
    yuv_file = yuv_file.replace(yuv_file.find(fnd), fnd.length(), rep);
    // auto nv12_input_h = cmd.get<AX_U32>("nv12_input_h");
    // auto nv12_input_w = cmd.get<AX_U32>("nv12_input_w");

    auto model_file_flag = utilities::file_exist(model_file);
    auto pose_model_file_flag = utilities::file_exist(pose_model_file);
    auto image_file_flag = utilities::file_exist(image_file);

    if (!model_file_flag | !image_file_flag | !pose_model_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("dmodel", model_file); }
        if (!pose_model_file_flag) { show_error("pmodel", pose_model_file); }
        if (!image_file_flag) { show_error("image", image_file); }

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

        if (!input_size_flag) { show_error("size", input_size_string); }

        return -1;
    }

    auto repeat = cmd.get<int>("repeat");
    auto conf_prob = cmd.get<float>("conf");

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");

    fprintf(stdout, "relu_tiny25 p det model file : %s\n", model_file.c_str());
    fprintf(stdout, "relu_tiny25 p det image file : %s\n", image_file.c_str());
    fprintf(stdout, "relu_tiny25 p det img_h, img_w : %d %d\n", input_size[0], input_size[1]);

    std::vector<uint8_t> image(input_size[0] * input_size[1] * 3, 0);
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    AX_S32 ret = AX_SYS_Init();
    int src_w = mat.cols;
    int src_h = mat.rows;
    auto nv12_input_h = src_h;
    auto nv12_input_w = src_w;

    // 2. yuv & resize
    std::vector<char> yuvdata;
    std::fstream fs(yuv_file.c_str(), std::ios::in | std::ios::binary);
    if (!fs.is_open())
    {
        fprintf(stderr, "Read nv12 image [%s] failed.\n", yuv_file.c_str());
        return false;
    }

    fs.seekg(std::ios::end);
    auto fs_end = fs.tellg();
    fs.seekg(std::ios::beg);
    auto fs_beg = fs.tellg();

    auto file_size = static_cast<size_t>(fs_end - fs_beg);
    auto vector_size = yuvdata.size();

    yuvdata.reserve(vector_size + file_size);
    yuvdata.insert(yuvdata.end(), std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>());

    fs.close();
    // nv12 crop resize
    AX_NPU_SDK_EX_MODEL_TYPE_T virtual_npu_mode_type = AX_NPU_MODEL_TYPE_1_1_1;
    AX_NPU_SDK_EX_ATTR_T hard_mode = {common::get_hard_mode_by_model_type(virtual_npu_mode_type)};
    ret = AX_NPU_SDK_EX_Init_with_attr(&hard_mode);
    if (ret != AX_NPU_DEV_STATUS_SUCCESS)
    {
        fprintf(stderr, "[ERR] AX_NPU_SDK_EX_Init_with_attr err code:%x", ret);
        return ret;
    }

    std::vector<uint8_t> model_input(DEFAULT_IMG_W * DEFAULT_IMG_H * 3 / 2);
    {
        auto crop_resize_helper = std::unique_ptr<ax::ax_crop_resize_nv12>(new ax::ax_crop_resize_nv12);
        crop_resize_helper->init(nv12_input_h, nv12_input_w, DEFAULT_IMG_H, DEFAULT_IMG_W, AX_NPU_MODEL_TYPE_1_1_1);
        timer crop_resize_timer;
        for (size_t i = 0; i < 10; i++)
        {
            crop_resize_helper->run_crop_resize_nv12(yuvdata, model_input);
        }
        float crop_resize_cost = crop_resize_timer.cost();
        fprintf(stdout, "run crop_resize time cost avg: %f :ms \n", crop_resize_cost / 10);
    }

    // 3. init ax system, if NOT INITED in other apps.
    //   if other app init the device, DO NOT INIT DEVICE AGAIN.
    //   this init ONLY will be used in demo apps to avoid using a none inited
    //     device.
    //   for example, if another app(such as camera init app) has already init
    //     the system, then DO NOT call this api again, if it does, the system
    //     will be re-inited and loss the last configuration.
    if (0 != ret)
    {
        fprintf(stderr, "relu_tiny25 p det init system failed.\n");
        return ret;
    }

    // 4. show the version (optional)
    fprintf(stdout, "relu_tiny25 p det Run-Joint Runtime version: %s\n", AX_JOINT_GetVersion());
    fprintf(stdout, "--------------------------------------\n");

    // 5. run the processing
    std::vector<detection::Object> det_out_object;
    auto flag = ax::run_detection(model_file, model_input, repeat, mat, input_size[0], input_size[1], det_out_object, float(conf_prob));

    // ***** ========================================================================
    // *****
    // *****
    // *****                           start pose det
    // *****
    // *****
    // ***** ========================================================================

    std::array<int, 2> pose_input_size = {HRNET_H, HRNET_W};
    std::vector<uint8_t> pose_image(pose_input_size[0] * pose_input_size[1] * 3, 0);
    cv::Mat pose_mat(pose_input_size[0], pose_input_size[1], CV_8UC3, pose_image.data());
    for (size_t i = 0; i < det_out_object.size(); i++)
    {
        const detection::Object& obj = det_out_object[i];
        //
        float pose_pre_time_costs = 0.f;
        timer tick;

        int offset_x = 0, offset_y = 0;
        int offset_top = 0, offset_left = 0;
        int resize_w = 0;
        int resize_h = 0;
        float scale = 1.25f;
        float rmin = std::min(pose_input_size[0] / obj.rect.height, pose_input_size[1] / obj.rect.width);
        // expand bbox area
        int padded_bbox_h = (int)(std::round(pose_input_size[0] / rmin * scale));
        int padded_bbox_w = (int)(std::round(pose_input_size[1] / rmin * scale));
        int padded_bbox_x = (int)(std::round(obj.rect.x - (padded_bbox_w - obj.rect.width) / 2.0));  // may be < 0
        int padded_bbox_y = (int)(std::round(obj.rect.y - (padded_bbox_h - obj.rect.height) / 2.0)); // may be < 0
        float ratio = (float)pose_input_size[0] / (float)padded_bbox_h;
        std::cout << "px " << padded_bbox_x << "py " << padded_bbox_y << "pw " << padded_bbox_w << "ph " << padded_bbox_h << std::endl;
        // 判断expand区域是否越界
        if (padded_bbox_x < 0 || padded_bbox_y < 0 || (padded_bbox_x + padded_bbox_w) > src_w || (padded_bbox_y + padded_bbox_h) > src_h)
        {
            // crop bbox area
            int crop_bbox_x = std::max(0, padded_bbox_x);
            int crop_bbox_y = std::max(0, padded_bbox_y);
            int crop_bbox_w = 0, crop_bbox_h = 0;
            if (padded_bbox_y >= 0)
                crop_bbox_h = (padded_bbox_h + padded_bbox_y) > src_h ? (src_h - padded_bbox_y) : padded_bbox_h;
            else
                crop_bbox_h = (padded_bbox_h + padded_bbox_y) > src_h ? src_h : (padded_bbox_h + padded_bbox_y);
            if (padded_bbox_x >= 0)
                crop_bbox_w = (padded_bbox_w + padded_bbox_x) > src_w ? (src_w - padded_bbox_x) : padded_bbox_w;
            else
                crop_bbox_w = (padded_bbox_w + padded_bbox_x) > src_w ? src_w : (padded_bbox_w + padded_bbox_x);
            resize_w = crop_bbox_w * ratio;
            resize_h = crop_bbox_h * ratio;

            // black area
            int dt = padded_bbox_y >= 0 ? 0 : (-padded_bbox_y * ratio);
            int dl = padded_bbox_x >= 0 ? 0 : (-padded_bbox_x * ratio);
            //int db = pose_input_size[0] - resize_h - dt;
            //int dr = pose_input_size[1] - resize_w - dl;

            offset_top = dt;
            offset_left = dl;
            offset_x = crop_bbox_x;
            offset_y = crop_bbox_y;

            padded_bbox_x = crop_bbox_x;
            padded_bbox_y = crop_bbox_y;
            padded_bbox_w = crop_bbox_w;
            padded_bbox_h = crop_bbox_h;
            std::cout << "need black"
                      << " px " << padded_bbox_x << " py " << padded_bbox_y << " pw " << padded_bbox_w << " ph " << padded_bbox_h << std::endl;
        }
        else
        {
            offset_x = padded_bbox_x;
            offset_y = padded_bbox_y;

            resize_h = pose_input_size[0];
            resize_w = pose_input_size[1];

            std::cout << "no black"
                      << " px " << padded_bbox_x << " py " << padded_bbox_y << " pw " << padded_bbox_w << " ph " << padded_bbox_h << std::endl;
        }

        // // 2. alloc input image
        axcv::ax_image input;
        input.w = src_w;
        input.h = src_h;
        input.stride_w = input.w;
        input.color_space = cv::get_color_space("nv12");
        auto input_image = axcv::alloc_cv_image(input);
        if (!input_image)
        {
            fprintf(stderr, "[ERR] alloc_cv_image input falil \n");
            return false;
        }

        AX_F32 fX = padded_bbox_x;
        AX_F32 fY = padded_bbox_y;
        AX_F32 fW = (padded_bbox_w % 2) == 1 ? (padded_bbox_w - 1) : padded_bbox_w;
        AX_F32 fH = (padded_bbox_h % 2) == 1 ? (padded_bbox_h - 1) : padded_bbox_h;
        AX_NPU_CV_Box tbox = {fX, fY, fW, fH};
        std::cout << "fx " << fX << " "
                  << "fy " << fY << " "
                  << "fw " << fW << " "
                  << "fh " << fH << " ";

        offset_left = (offset_left % 2) == 1 ? (offset_left - 1) : offset_left;

        // resize_h = std::min(ALIGN_UP(resize_h, 2), pose_input_size[0]);
        // resize_w = std::min(ALIGN_UP(resize_w, 2), pose_input_size[1]);
        int align_h = (resize_h % 2) == 1 ? (resize_h + 1) : resize_h;
        int align_w = (resize_w % 2) == 1 ? (resize_w + 1) : resize_w;
        resize_h = std::min(align_h, pose_input_size[0]);
        resize_w = std::min(align_w, pose_input_size[1]);

        // 4. read input file
        std::vector<char> input_data;
        utilities::read_file(yuv_file.c_str(), input_data);
        middleware::copy_to_device(input_data.data(), (void**)&input_image->pVir, cv::get_image_data_size(input_image));

        // 5. alloc output image
        axcv::ax_image output;
        output.w = HRNET_W;
        output.h = HRNET_H;
        output.stride_w = output.w;
        output.color_space = cv::get_color_space("nv12");
        auto output_image = axcv::alloc_cv_image(output);
        if (!output_image)
        {
            fprintf(stderr, "[ERR] alloc_cv_image output falil \n");
            return false;
        }
        // 6. crop and resize
        // ret = axcv::npu_crop_resize(input_image, input_data.data(), output_image, out_box, cv::get_npu_mode_type(args.get<std::string>("mode-type")));
        ret = axcv::pose_npu_crop_resize(input_image, output_image, src_h, src_w, resize_w, resize_h, offset_left, offset_top, &tbox, AX_NPU_MODEL_TYPE_1_1_1, AX_NPU_CV_IMAGE_HORIZONTAL_CENTER, AX_NPU_CV_IMAGE_VERTICAL_CENTER);
        if (ret != AX_NPU_DEV_STATUS_SUCCESS)
        {
            fprintf(stderr, "[ERR] npu_crop_resize err code:%x \n", ret);
            return false;
        }
        std::vector<uint8_t> pose_input_yuv_data(output_image->nHeight * output_image->nWidth * 3 / 2, 0);
        memcpy(pose_input_yuv_data.data(), output_image->pVir, output_image->nHeight * output_image->nWidth * 3 / 2);

        cv::Mat output_nv12(output_image->nHeight * 3 / 2, output_image->nWidth, CV_8UC1, pose_input_yuv_data.data());
        int32_t yuv_size = output_image->nHeight * output_image->nWidth * 3 / 2;
        FILE* yuvFd = fopen("./output_cv_nv12.yuv", "w+");
        fwrite(output_nv12.ptr<uint8_t>(), 1, yuv_size, yuvFd);
        fclose(yuvFd);

        pose_pre_time_costs = tick.cost();
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "pose pre_process time cost: %.2f ms\n", pose_pre_time_costs);
        fprintf(stdout, "--------------------------------------\n");

        auto flag = ax::pose_run_detection(pose_model_file, pose_input_yuv_data, repeat, mat, pose_input_size[0], pose_input_size[1], obj,
                                           offset_top, offset_left, offset_x, offset_y, ratio);
    }

    std::string path = image_file;
    //1.获取不带路径的文件名
    std::string::size_type iPos = path.find_last_of('/') + 1;
    std::string filename = path.substr(iPos, path.length() - iPos);
    //2.获取不带后缀的文件名
    std::string name = filename.substr(0, filename.rfind("."));
    // std::string save_file_name = "/opt/data/tion/pose_out/" + name + ".png";
    //3.获取路径
    std::string ImgPath = path.substr(0, iPos); //获取文件路径
    std::string save_file_name = ImgPath + name + "_ret" + ".png";
    std::cout << save_file_name << std::endl;
    cv::imwrite(save_file_name, mat);
    // 6. last de-init
    //   as step 1, if the device inited by another app, DO NOT de-init the
    //     device at this app.
    AX_SYS_Deinit();
}
