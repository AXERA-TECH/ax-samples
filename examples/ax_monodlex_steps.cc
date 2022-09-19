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
#include <numeric>

#include <opencv2/opencv.hpp>

#include "base/detection.hpp"
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

const int DEFAULT_IMG_H = 384;
const int DEFAULT_IMG_W = 1280;

const int DEFAULT_LOOP_COUNT = 1;

#define THRESHOLD     0.25

float kitti_P2[3][4] = {
    {721.5377, 0, 609.5593, 44.85728},
    {0, 721.5377, 172.854, 0.216379},
    {0, 0, 1, 0.002746}};

float box_3d_corner_map[8][3] = {
    {-0.5, -1, -0.5},
    {0.5, -1, -0.5},
    {0.5, 0, -0.5},
    {0.5, 0, 0.5},
    {0.5, -1, 0.5},
    {-0.5, -1, 0.5},
    {-0.5, 0, 0.5},
    {-0.5, 0, -0.5}};

int face_idx[][4] = {{5, 4, 3, 6}, {1, 2, 3, 4}, {1, 0, 7, 2}, {0, 5, 6, 7}};

const float angle_per_class = 2 * M_PI / 12;
namespace ax
{
    namespace det = detection;
    namespace mw = middleware;
    namespace utl = utilities;

    namespace mono_process
    {

        struct Calibrate
        {
            float P2[3][4]{};

            float cu = P2[0][2];
            float cv = P2[1][2];
            float fu = P2[0][0];
            float fv = P2[1][1];
            float tx = P2[0][3] / (-fu);
            float ty = P2[1][3] / (-fv);

            Calibrate()
            {
                memcpy(&P2[0][0], &kitti_P2[0][0], 3 * 4 * sizeof(float));
                cu = P2[0][2];
                cv = P2[1][2];
                fu = P2[0][0];
                fv = P2[1][1];
                tx = P2[0][3] / (-fu);
                ty = P2[1][3] / (-fv);
            }

            void img_to_rect(float u, float v, float depth, float* pt0, float* pt1, float* pt2) const
            {
                auto x = ((u - cu) * depth) / fu + tx;
                auto y = ((v - cv) * depth) / fv + ty;
                *pt0 = x;
                *pt1 = y;
                *pt2 = depth;
            }

            void alpha_ry(float alpha, float u, float* ry) const
            {
                *ry = alpha + std::atan2(u - cu, fu);
                if (*ry > M_PI)
                {
                    *ry -= 2 * M_PI;
                }
                if (*ry < -M_PI)
                {
                    *ry += 2 * M_PI;
                }
            }
        };

        struct hm_process_object
        {
            int pos;
            float score;
            int clas;
            float xs, ys;
        };

        struct reg_process_object
        {
            float val[35];
        };

        struct post_process_object
        {
            float score;
            int clas;
            float depth;

            float x3d, y3d;

            float x, y, z;
            float dim0, dim1, dim2;
            float ry;
        };

        struct box_3d_object
        {
            float coo[8][2];
            int clas;
        };

        void process_hm_message(std::vector<hm_process_object>& hm_process_objects,
                                int c,
                                int h,
                                int w,
                                const float* hm_max_data,
                                const float* hm_data)
        {
            for (int j = 0; j < h * w; ++j)
            {
                for (int i = 0; i < c; ++i)
                {
                    if ((hm_max_data[j * c + i] > THRESHOLD) && (std::abs(hm_max_data[j * c + i] - hm_data[j * c + i])) <= 0.0001)
                    {
                        hm_process_object object{};
                        object.pos = j;
                        object.score = hm_max_data[j * c + i];
                        object.clas = i;
                        object.xs = j % w;
                        object.ys = j / w;
                        hm_process_objects.push_back(object);
                    }
                }
            }

            std::sort(hm_process_objects.begin(),
                      hm_process_objects.end(),
                      [](const hm_process_object& a, const hm_process_object& b) {
                          return a.score > b.score;
                      });
        }

        void get_reg_data_object(const std::vector<hm_process_object>& hm_process_objects,
                                 std::vector<reg_process_object>& reg_process_objects,
                                 const float* depth_data,
                                 const float* heading_data,
                                 const float* size_3d,
                                 const float* offset_3d,
                                 const float* size_2d,
                                 const float* offset_2d)
        {
            for (const auto& hm_process_object : hm_process_objects)
            {
                reg_process_object object{};

                // nhwc
                int pos = hm_process_object.pos;

                object.val[0] = depth_data[pos * 2];
                object.val[1] = depth_data[pos * 2 + 1]; //sigma now do not use
                object.val[2] = size_3d[pos * 3];        // h
                object.val[3] = size_3d[pos * 3 + 1];    // w
                object.val[4] = size_3d[pos * 3 + 2];    // l

                object.val[5] = offset_3d[pos * 2];
                object.val[6] = offset_3d[pos * 2 + 1];

                object.val[7] = size_2d[pos * 2];
                object.val[8] = size_2d[pos * 2 + 1];
                object.val[9] = offset_2d[pos * 2];
                object.val[10] = offset_2d[pos * 2 + 1];

                memcpy(&object.val[11], &heading_data[pos * 24], 24 * sizeof(float));

                reg_process_objects.push_back(object);
            }
        }

        void post_process(const std::vector<hm_process_object>& hm_process_objects,
                          const std::vector<reg_process_object>& reg_process_objects,
                          std::vector<post_process_object>& post_process_objects,
                          const cv::Mat& input_mat, uint32_t input_h, uint32_t input_w)
        {
            Calibrate calibrate;
            for (int i = 0; i < hm_process_objects.size(); ++i)
            {
                hm_process_object hm_object = hm_process_objects[i];
                if (hm_object.score < 0.25)
                {
                    continue;
                }

                post_process_object object{};
                reg_process_object reg_object = reg_process_objects[i];

                object.score = hm_object.score;
                object.clas = hm_object.clas;

                object.depth = 1.0f / (detection::sigmoid(reg_object.val[0]) + 1e-6f) - 1;

                // center_xy 3d
                float temp_3d_x = hm_object.xs + reg_object.val[5];
                float temp_3d_y = hm_object.ys + reg_object.val[6];
                object.x3d = temp_3d_x * (float)input_mat.cols / (float)((float)input_w / 4);
                object.y3d = temp_3d_y * (float)input_mat.rows / (float)((float)input_h / 4);

                // heading_angle
                float heading_bin_max = -FLT_MAX;
                int heading_bin_index = 0;
                float heading_value = heading_bin_max;
                for (int j = 11; j < 23; ++j)
                {
                    if (reg_object.val[j] > heading_bin_max)
                    {
                        heading_bin_max = reg_object.val[j];
                        heading_value = reg_object.val[j + 12];
                        heading_bin_index = j - 11;
                    }
                }
                float angle_center = (float)heading_bin_index * angle_per_class;

                angle_center += heading_value;
                if (angle_center > M_PI)
                {
                    angle_center -= 2 * M_PI;
                }

                calibrate.img_to_rect(object.x3d, object.y3d, object.depth, &object.x, &object.y, &object.z);
                object.y += reg_object.val[2] / 2;

                calibrate.alpha_ry(angle_center, object.x3d, &object.ry);
                object.dim0 = reg_object.val[2], object.dim1 = reg_object.val[3], object.dim2 = reg_object.val[4];
                post_process_objects.push_back(object);
            }
        }

        void box_3d_process(const std::vector<post_process_object>& post_process_objects,
                            std::vector<box_3d_object>& box_3d_objects)
        {
            for (const auto& post_process_object : post_process_objects)
            {
                box_3d_object object{};
                // 8 points
                for (int j = 0; j < 8; ++j)
                {
                    float tmp_x = box_3d_corner_map[j][0] * post_process_object.dim2; // dim w
                    float tmp_y = box_3d_corner_map[j][1] * post_process_object.dim0; // dim0 -h
                    float tmp_z = box_3d_corner_map[j][2] * post_process_object.dim1; // dim l

                    float cos_value = std::cos(post_process_object.ry);
                    float sin_value = std::sin(post_process_object.ry);

                    float rotate_x = tmp_x * cos_value + tmp_z * sin_value + post_process_object.x;
                    float rotate_y = tmp_y + post_process_object.y;
                    float rotate_z = tmp_z * cos_value - tmp_x * sin_value + post_process_object.z;

                    float box3d_x = rotate_x * kitti_P2[0][0] + rotate_y * kitti_P2[0][1] + rotate_z * kitti_P2[0][2] + kitti_P2[0][3];
                    float box3d_y = rotate_x * kitti_P2[1][0] + rotate_y * kitti_P2[1][1] + rotate_z * kitti_P2[1][2] + kitti_P2[1][3];
                    float box3d_z = rotate_x * kitti_P2[2][0] + rotate_y * kitti_P2[2][1] + rotate_z * kitti_P2[2][2] + kitti_P2[2][3];

                    object.coo[j][0] = box3d_x / box3d_z;
                    object.coo[j][1] = box3d_y / box3d_z;
                }
                box_3d_objects.push_back(object);
            }
        }

        void draw_box_3d_object(const cv::Mat& input, const std::vector<box_3d_object>& box_3d_objects)
        {
            cv::Mat input_poly = input.clone();

            for (auto object : box_3d_objects)
            {
                for (int j = 3; j >= 0; j--)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        cv::line(input, cv::Point((int)object.coo[face_idx[j][k]][0], (int)object.coo[face_idx[j][k]][1]),
                                 cv::Point((int)object.coo[face_idx[j][(k + 1) % 4]][0], (int)object.coo[face_idx[j][(k + 1) % 4]][1]),
                                 cv::Scalar(0, 255, 0), 1, cv::LineTypes::LINE_AA);
                    }
                    if (j == 0)
                    {
                        // cv::Point poly_points[0][4];             // dimension can not be 0
                        cv::Point poly_points[1][4];
                        poly_points[0][0] = cv::Point((int)object.coo[face_idx[0][0]][0], (int)object.coo[face_idx[0][0]][1]);
                        poly_points[0][1] = cv::Point((int)object.coo[face_idx[0][1]][0], (int)object.coo[face_idx[0][1]][1]);
                        poly_points[0][2] = cv::Point((int)object.coo[face_idx[0][2]][0], (int)object.coo[face_idx[0][2]][1]);
                        poly_points[0][3] = cv::Point((int)object.coo[face_idx[0][3]][0], (int)object.coo[face_idx[0][3]][1]);
                        int npt[] = {4};
                        const cv::Point* ppt[1] = {poly_points[0]};
                        cv::fillPoly(input_poly, ppt, npt, 1, cv::Scalar(0, 0, 255));
                    }
                }
            }
            cv::addWeighted(input, 0.8, input_poly, 0.2, 10, input);
            cv::imwrite("./ax_monodlex.png", input);
        }

    } // namespace mono_process

    bool run_detection(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, uint32_t input_h, uint32_t input_w)
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

        ret = mw::prepare_io_out_cache(data.data(), data.size(), joint_io_arr, io_info);
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
        fprintf(stdout, "run over: output len %d\n", io_info->nOutputSize);

        // 5. get bbox
        auto& info = joint_io_arr.pOutputs[0];  // 274 - offset_2d
        auto& info1 = joint_io_arr.pOutputs[1]; // 277 - offset_3d
        auto& info2 = joint_io_arr.pOutputs[2]; // 280 - size_2d
        auto& info3 = joint_io_arr.pOutputs[3]; // 283 - depth_sigma
        auto& info4 = joint_io_arr.pOutputs[4]; // 286 - size_3d
        auto& info5 = joint_io_arr.pOutputs[5]; // 289 - heading
        auto& info6 = joint_io_arr.pOutputs[6]; // heatmap_out - maxpool_out
        auto& info7 = joint_io_arr.pOutputs[7]; // sigmoid_out

        auto data_offset2d = (float*)info.pVirAddr;
        auto data_offset3d = (float*)info1.pVirAddr;
        auto data_size2d = (float*)info2.pVirAddr;
        auto data_depth_sigma = (float*)info3.pVirAddr;
        auto data_size3d = (float*)info4.pVirAddr;
        auto data_heading = (float*)info5.pVirAddr;
        auto data_heatmap = (float*)info7.pVirAddr;
        auto data_heatmap_mp = (float*)info6.pVirAddr;

        using namespace mono_process;
        timer post_process_timer;
        // 5.1 process hm message get object score and position
        auto meta = io_info->pOutputs[7];
        std::vector<hm_process_object> hm_process_objects;
        process_hm_message(hm_process_objects, meta.pShape[3], meta.pShape[1], meta.pShape[2], data_heatmap_mp, data_heatmap);

        // 5.2. get regression data by hm position
        std::vector<reg_process_object> reg_process_objects;
        get_reg_data_object(hm_process_objects, reg_process_objects, data_depth_sigma,
                            data_heading, data_size3d, data_offset3d, data_size2d, data_offset2d);

        // 5.3. post process regression data
        std::vector<post_process_object> post_process_objects;
        post_process(hm_process_objects, reg_process_objects, post_process_objects, mat, input_h, input_w);

        // 5.4. get object 8 corner points
        std::vector<box_3d_object> box_3d_objects;
        box_3d_process(post_process_objects, box_3d_objects);
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "post_process cost %f ms \n", post_process_timer.cost());

        // 5.5. draw result
        draw_box_3d_object(mat, box_3d_objects);

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
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "detection num: %d\n", post_process_objects.size());

        clear_and_exit();
        return true;
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("image", 'i', "image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));

    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto model_file = cmd.get<std::string>("model");
    auto image_file = cmd.get<std::string>("image");

    auto model_file_flag = utilities::file_exist(model_file);
    auto image_file_flag = utilities::file_exist(image_file);

    if (!model_file_flag | !image_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
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

        show_error("size", input_size_string);

        return -1;
    }

    auto repeat = cmd.get<int>("repeat");

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");

    fprintf(stdout, "model file : %s\n", model_file.c_str());
    fprintf(stdout, "image file : %s\n", image_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);

    // 2. read image & resize & transpose
    std::vector<uint8_t> image(input_size[0] * input_size[1] * 3, 0);
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    common::get_input_data_no_letterbox(mat, image, input_size[0], input_size[1], true);

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
    auto flag = ax::run_detection(model_file, image, repeat, mat, input_size[0], input_size[1]);
    if (!flag)
    {
        fprintf(stderr, "Run classification failed.\n");
    }

    // 6. last de-init
    //   as step 1, if the device inited by another app, DO NOT de-init the
    //     device at this app.
    AX_SYS_Deinit();
}
