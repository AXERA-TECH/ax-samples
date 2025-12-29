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
#include "base/common.hpp"
#include "base/detection.hpp"
#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int DEFAULT_IMG_H = 384;
const int DEFAULT_IMG_W = 640;

const int DEFAULT_LOOP_COUNT = 1;

const float PROB_THRESHOLD = 0.5f;

namespace ax
{
    static void shift(int w, int h, int stride, std::vector<float> anchor_points, std::vector<float>& shifted_anchor_points)
    {
        std::vector<float> x_, y_;
        for (int i = 0; i < w; i++)
        {
            float x = (i + 0.5) * stride;
            x_.push_back(x);
        }
        for (int i = 0; i < h; i++)
        {
            float y = (i + 0.5) * stride;
            y_.push_back(y);
        }

        std::vector<float> shift_x(w * h, 0), shift_y(w * h, 0);
        for (int i = 0; i < h; i++)
        {
            for (int j = 0; j < w; j++)
            {
                shift_x[i * w + j] = x_[j];
            }
        }
        for (int i = 0; i < h; i++)
        {
            for (int j = 0; j < w; j++)
            {
                shift_y[i * w + j] = y_[i];
            }
        }

        std::vector<float> shifts(w * h * 2, 0);
        for (int i = 0; i < w * h; i++)
        {
            shifts[i * 2] = shift_x[i];
            shifts[i * 2 + 1] = shift_y[i];
        }

        shifted_anchor_points.resize(2 * w * h * anchor_points.size() / 2, 0);
        for (int i = 0; i < w * h; i++)
        {
            for (int j = 0; j < (int)anchor_points.size() / 2; j++)
            {
                float x = anchor_points[j * 2] + shifts[i * 2];
                float y = anchor_points[j * 2 + 1] + shifts[i * 2 + 1];
                shifted_anchor_points[i * anchor_points.size() / 2 * 2 + j * 2] = x;
                shifted_anchor_points[i * anchor_points.size() / 2 * 2 + j * 2 + 1] = y;
            }
        }
    }
    static void generate_anchor_points(int stride, int row, int line, std::vector<float>& anchor_points)
    {
        float row_step = (float)stride / row;
        float line_step = (float)stride / line;

        std::vector<float> x_, y_;
        for (int i = 1; i < line + 1; i++)
        {
            float x = (i - 0.5) * line_step - stride / 2;
            x_.push_back(x);
        }
        for (int i = 1; i < row + 1; i++)
        {
            float y = (i - 0.5) * row_step - stride / 2;
            y_.push_back(y);
        }
        std::vector<float> shift_x(row * line, 0), shift_y(row * line, 0);
        for (int i = 0; i < row; i++)
        {
            for (int j = 0; j < line; j++)
            {
                shift_x[i * line + j] = x_[j];
            }
        }
        for (int i = 0; i < row; i++)
        {
            for (int j = 0; j < line; j++)
            {
                shift_y[i * line + j] = y_[i];
            }
        }
        anchor_points.resize(row * line * 2, 0);
        for (int i = 0; i < row * line; i++)
        {
            float x = shift_x[i];
            float y = shift_y[i];
            anchor_points[i * 2] = x;
            anchor_points[i * 2 + 1] = y;
        }
    }
    static void generate_anchor_points(int img_w, int img_h, std::vector<int> pyramid_levels, int row, int line, std::vector<float>& all_anchor_points)
    {
        std::vector<std::pair<int, int> > image_shapes;
        std::vector<int> strides;
        for (int i = 0; i < (int)pyramid_levels.size(); i++)
        {
            int new_h = std::floor((img_h + std::pow(2, pyramid_levels[i]) - 1) / std::pow(2, pyramid_levels[i]));
            int new_w = std::floor((img_w + std::pow(2, pyramid_levels[i]) - 1) / std::pow(2, pyramid_levels[i]));
            image_shapes.push_back(std::make_pair(new_w, new_h));
            strides.push_back(std::pow(2, pyramid_levels[i]));
        }

        all_anchor_points.clear();
        for (int i = 0; i < (int)pyramid_levels.size(); i++)
        {
            std::vector<float> anchor_points;
            generate_anchor_points(std::pow(2, pyramid_levels[i]), row, line, anchor_points);
            std::vector<float> shifted_anchor_points;
            shift(image_shapes[i].first, image_shapes[i].second, strides[i], anchor_points, shifted_anchor_points);
            all_anchor_points.insert(all_anchor_points.end(), shifted_anchor_points.begin(), shifted_anchor_points.end());
        }
    }

    void post_process(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data, const cv::Mat& mat, int input_w, int input_h, const std::vector<float>& time_costs)
    {
        std::vector<int> pyramid_levels(1, 3);
        std::vector<float> all_anchor_points;
        generate_anchor_points(input_w, input_h, pyramid_levels, 2, 2, all_anchor_points);

        timer timer_postprocess;

        int letterbox_rows = input_h;
        int letterbox_cols = input_w;
        int src_rows = mat.rows;
        int src_cols = mat.cols;
        float scale_letterbox;
        int resize_rows;
        int resize_cols;
        if ((letterbox_rows * 1.0 / src_rows) < (letterbox_cols * 1.0 / src_cols))
        {
            scale_letterbox = letterbox_rows * 1.0 / src_rows;
        }
        else
        {
            scale_letterbox = letterbox_cols * 1.0 / src_cols;
        }
        resize_cols = int(scale_letterbox * src_cols);
        resize_rows = int(scale_letterbox * src_rows);

        int tmp_h = (letterbox_rows - resize_rows) / 2;
        int tmp_w = (letterbox_cols - resize_cols) / 2;

        float ratio_x = (float)src_rows / resize_rows;
        float ratio_y = (float)src_cols / resize_cols;

        struct crowd_point_t
        {
            float x, y;
        };

        crowd_point_t* pred_points_ptr = (crowd_point_t*)(float*)io_data->pOutputs[0].pVirAddr;
        crowd_point_t* pred_scores_ptr = (crowd_point_t*)(float*)io_data->pOutputs[1].pVirAddr;

        int len = io_data->pOutputs[0].nSize / sizeof(float) / 2;

        crowd_point_t* anchor_points_ptr = (crowd_point_t*)all_anchor_points.data();

        std::vector<float> _softmax_result(2, 0);
        std::vector<cv::Point> points;
        for (int i = 0; i < len; i++)
        {
            if (pred_scores_ptr[i].x < pred_scores_ptr[i].y)
            {
                detection::softmax(&pred_scores_ptr[i].x, _softmax_result.data(), 2);
                if (_softmax_result[1] > PROB_THRESHOLD)
                {
                    cv::Point p;
                    p.x = pred_points_ptr[i].x * 100 + anchor_points_ptr[i].x;
                    p.y = pred_points_ptr[i].y * 100 + anchor_points_ptr[i].y;

                    p.x = (p.x - tmp_w) * ratio_x;
                    p.y = (p.y - tmp_h) * ratio_y;
                    points.push_back(p);
                }
            }
        }

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

        // draw points to mat
        printf("there are %ld points\n", (long)points.size());
        for (int i = 0; i < (int)points.size(); i++)
        {
            cv::circle(mat, points[i], 2, cv::Scalar(0, 0, 255), 2);
        }
        cv::imwrite("crowdcount_out.jpg", mat);
    }

    bool run_model(const std::string& model, const std::vector<uint8_t>& data, const int& repeat, cv::Mat& mat, int input_h, int input_w)
    {
        // 1. init engine
        AX_ENGINE_NPU_ATTR_T npu_attr;
        memset(&npu_attr, 0, sizeof(npu_attr));
        npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
        auto ret = AX_ENGINE_Init(&npu_attr);

        if (0 != ret)
        {
            return ret;
        }

        // 2. load model
        std::vector<char> model_buffer;
        if (!utilities::read_file(model, model_buffer))
        {
            fprintf(stderr, "Read Run-Joint model(%s) file failed.\n", model.c_str());
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

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. insert input
        ret = middleware::push_input(data, &io_data, io_info);
        SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
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

        // 10. get result
        post_process(io_info, &io_data, mat, input_w, input_h, time_costs);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
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
    fprintf(stdout, "--------------------------------------\n");

    // 2. read image & resize & transpose
    std::vector<uint8_t> image(input_size[0] * input_size[1] * 3, 0);
    cv::Mat mat = cv::imread(image_file);
    if (mat.empty())
    {
        fprintf(stderr, "Read image failed.\n");
        return -1;
    }
    common::get_input_data_letterbox(mat, image, input_size[0], input_size[1], true);

    // 3. sys_init
    AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, image, repeat, mat, input_size[0], input_size[1]);

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}
