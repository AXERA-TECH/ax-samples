/*
* AXERA is pleased to support the open source community by making ax-samples available.
*
* Copyright (c) 2025, AXERA Semiconductor Co., Ltd. All rights reserved.
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
* Note: For SuperPoint feature detection and matching model.
* Author: GUOFANGMING
*/

#include <cstdio>
#include <cstring>
#include <numeric>
#include <vector>
#include <algorithm>
#include <cmath>

#include <opencv2/opencv.hpp>
#include "base/common.hpp"
#include "middleware/io.hpp"

#include "utilities/args.hpp"
#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

#include <ax_sys_api.h>
#include <ax_engine_api.h>

const int DEFAULT_IMG_H = 480;
const int DEFAULT_IMG_W = 640;

const int DEFAULT_LOOP_COUNT = 1;
const float DEFAULT_THRESHOLD = 0.005f;
const int DEFAULT_MAX_POINTS = 100;

struct KeyPoint
{
    float x, y;
    float score;
};

namespace ax
{
    void get_keypoints(const float* score_map, int h, int w, float threshold, 
                      std::vector<KeyPoint>& keypoints)
    {
        keypoints.clear();
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                float score = score_map[y * w + x];
                if (score > threshold)
                {
                    KeyPoint kp;
                    kp.x = (float)x;
                    kp.y = (float)y;
                    kp.score = score;
                    keypoints.push_back(kp);
                }
            }
        }
    }

    void get_descriptors(const std::vector<KeyPoint>& keypoints, 
                        const float* desc_map, int desc_c, int desc_h, int desc_w,
                        std::vector<std::vector<float>>& descriptors)
    {
        descriptors.clear();
        if (keypoints.empty())
        {
            return;
        }

        for (const auto& kp : keypoints)
        {
            float x = kp.x / 8.0f;
            float y = kp.y / 8.0f;
            
            int x0 = std::floor(x);
            int x1 = x0 + 1;
            int y0 = std::floor(y);
            int y1 = y0 + 1;
            
            x0 = std::max(0, std::min(x0, desc_w - 1));
            x1 = std::max(0, std::min(x1, desc_w - 1));
            y0 = std::max(0, std::min(y0, desc_h - 1));
            y1 = std::max(0, std::min(y1, desc_h - 1));
            
            float wa = (x1 - x) * (y1 - y);
            float wb = (x1 - x) * (y - y0);
            float wc = (x - x0) * (y1 - y);
            float wd = (x - x0) * (y - y0);
            
            std::vector<float> desc(desc_c, 0.0f);
            for (int c = 0; c < desc_c; ++c)
            {
                float Q_tl = desc_map[c * desc_h * desc_w + y0 * desc_w + x0];
                float Q_bl = desc_map[c * desc_h * desc_w + y1 * desc_w + x0];
                float Q_tr = desc_map[c * desc_h * desc_w + y0 * desc_w + x1];
                float Q_br = desc_map[c * desc_h * desc_w + y1 * desc_w + x1];
                
                desc[c] = Q_tl * wa + Q_bl * wb + Q_tr * wc + Q_br * wd;
            }
            
            // Normalize
            float norm = 0.0f;
            for (int c = 0; c < desc_c; ++c)
            {
                norm += desc[c] * desc[c];
            }
            norm = std::sqrt(norm + 1e-6f);
            for (int c = 0; c < desc_c; ++c)
            {
                desc[c] /= norm;
            }
            
            descriptors.push_back(desc);
        }
    }

    void extract_features(AX_ENGINE_IO_INFO_T* io_info, AX_ENGINE_IO_T* io_data,
                          float threshold, int max_points,
                          std::vector<KeyPoint>& keypoints,
                          std::vector<std::vector<float>>& descriptors)
    {
        // Get outputs: score_map and descriptor_map
        auto& score_output = io_data->pOutputs[0];
        auto& desc_output = io_data->pOutputs[1];
        auto& score_info = io_info->pOutputs[0];
        auto& desc_info = io_info->pOutputs[1];
        
        int score_h = score_info.pShape[1];
        int score_w = score_info.pShape[2];
        float* score_map = (float*)score_output.pVirAddr;
        
        int desc_c = desc_info.pShape[1];
        int desc_h = desc_info.pShape[2];
        int desc_w = desc_info.pShape[3];
        float* desc_map = (float*)desc_output.pVirAddr;
        
        // Extract keypoints
        get_keypoints(score_map, score_h, score_w, threshold, keypoints);
        
        // Limit keypoints
        if ((int)keypoints.size() > max_points)
        {
            std::sort(keypoints.begin(), keypoints.end(), 
                     [](const KeyPoint& a, const KeyPoint& b) { return a.score > b.score; });
            keypoints.resize(max_points);
        }
        
        // Extract descriptors
        get_descriptors(keypoints, desc_map, desc_c, desc_h, desc_w, descriptors);
    }

    void match_and_visualize(const cv::Mat& img1, const cv::Mat& img2,
                             const std::vector<KeyPoint>& kp1, const std::vector<std::vector<float>>& desc1,
                             const std::vector<KeyPoint>& kp2, const std::vector<std::vector<float>>& desc2,
                             const std::string& output_file)
    {
        if (kp1.empty() || kp2.empty() || desc1.empty() || desc2.empty())
        {
            fprintf(stderr, "No keypoints or descriptors to match\n");
            return;
        }
        
        // Convert descriptors to cv::Mat for matching
        cv::Mat desc_mat1(desc1.size(), desc1[0].size(), CV_32F);
        cv::Mat desc_mat2(desc2.size(), desc2[0].size(), CV_32F);
        
        for (size_t i = 0; i < desc1.size(); ++i)
        {
            memcpy(desc_mat1.ptr<float>(i), desc1[i].data(), desc1[i].size() * sizeof(float));
        }
        
        for (size_t i = 0; i < desc2.size(); ++i)
        {
            memcpy(desc_mat2.ptr<float>(i), desc2[i].data(), desc2[i].size() * sizeof(float));
        }
        
        // Manual brute force matching (L2 distance with cross-check)
        struct Match {
            int queryIdx;
            int trainIdx;
            float distance;
        };
        std::vector<Match> matches;
        
        // Cross-check matching: for each descriptor in img1, find best match in img2
        // and verify that the reverse match is also the best
        for (int i = 0; i < desc_mat1.rows; ++i)
        {
            float best_dist = std::numeric_limits<float>::max();
            int best_idx = -1;
            
            // Find best match in img2
            for (int j = 0; j < desc_mat2.rows; ++j)
            {
                float dist = 0.0f;
                for (int k = 0; k < desc_mat1.cols; ++k)
                {
                    float diff = desc_mat1.at<float>(i, k) - desc_mat2.at<float>(j, k);
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);
                
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_idx = j;
                }
            }
            
            // Cross-check: verify reverse match
            if (best_idx >= 0)
            {
                float reverse_best_dist = std::numeric_limits<float>::max();
                int reverse_best_idx = -1;
                
                for (int k = 0; k < desc_mat1.rows; ++k)
                {
                    float dist = 0.0f;
                    for (int l = 0; l < desc_mat1.cols; ++l)
                    {
                        float diff = desc_mat1.at<float>(k, l) - desc_mat2.at<float>(best_idx, l);
                        dist += diff * diff;
                    }
                    dist = std::sqrt(dist);
                    
                    if (dist < reverse_best_dist)
                    {
                        reverse_best_dist = dist;
                        reverse_best_idx = k;
                    }
                }
                
                // If cross-check passes, add match
                if (reverse_best_idx == i)
                {
                    matches.push_back({i, best_idx, best_dist});
                }
            }
        }
        
        // Sort matches by distance
        std::sort(matches.begin(), matches.end(), 
                 [](const Match& a, const Match& b) { return a.distance < b.distance; });
        
        fprintf(stdout, "Found %zu matches\n", matches.size());
        
        // Draw matches manually
        int img1_w = img1.cols;
        int img2_w = img2.cols;
        int max_h = std::max(img1.rows, img2.rows);
        cv::Mat match_img(max_h, img1_w + img2_w, CV_8UC3);
        match_img.setTo(cv::Scalar(0, 0, 0));
        
        // Copy images side by side
        cv::Mat roi1 = match_img(cv::Rect(0, 0, img1_w, img1.rows));
        img1.copyTo(roi1);
        cv::Mat roi2 = match_img(cv::Rect(img1_w, 0, img2_w, img2.rows));
        img2.copyTo(roi2);
        
        // Draw matches
        cv::RNG rng(12345);
        for (size_t i = 0; i < matches.size() && i < 100; ++i) // Limit to 100 matches for visualization
        {
            const Match& m = matches[i];
            cv::Scalar color(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
            
            cv::Point2f pt1(kp1[m.queryIdx].x, kp1[m.queryIdx].y);
            cv::Point2f pt2(kp2[m.trainIdx].x + img1_w, kp2[m.trainIdx].y);
            
            cv::circle(match_img, pt1, 3, color, -1);
            cv::circle(match_img, pt2, 3, color, -1);
            cv::line(match_img, pt1, pt2, color, 1);
        }
        
        cv::imwrite(output_file, match_img);
        fprintf(stdout, "Match result saved to %s\n", output_file.c_str());
    }

    bool run_model(const std::string& model, 
                   const std::vector<float>& input_data1,
                   const std::vector<float>& input_data2,
                   const int& repeat,
                   cv::Mat& img1, cv::Mat& img2,
                   float threshold, int max_points,
                   std::vector<KeyPoint>& kp1, std::vector<std::vector<float>>& desc1,
                   std::vector<KeyPoint>& kp2, std::vector<std::vector<float>>& desc2)
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
        middleware::print_io_info(io_info);

        // 6. alloc io
        AX_ENGINE_IO_T io_data;
        ret = middleware::prepare_io(io_info, &io_data, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
        SAMPLE_AX_ENGINE_DEAL_HANDLE
        fprintf(stdout, "Engine alloc io is done. \n");

        // 7. Process first image
        if (input_data1.size() != io_info->pInputs[0].nSize / sizeof(float))
        {
            fprintf(stderr, "Input data1 size mismatch: expected %zu, got %zu\n",
                    io_info->pInputs[0].nSize / sizeof(float), input_data1.size());
            middleware::free_io(&io_data);
            return AX_ENGINE_DestroyHandle(handle);
        }
        
        memcpy(io_data.pInputs[0].pVirAddr, input_data1.data(), input_data1.size() * sizeof(float));
        fprintf(stdout, "Engine push input1 is done. \n");
        fprintf(stdout, "--------------------------------------\n");

        // 8. warm up
        for (int i = 0; i < 5; ++i)
        {
            AX_ENGINE_RunSync(handle, &io_data);
        }

        // 9. run model for image 1
        std::vector<float> time_costs(repeat, 0);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_ENGINE_RunSync(handle, &io_data);
            time_costs[i] = tick.cost();
            SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
        }

        // 10. get result for image 1
        fprintf(stdout, "Processing image 1:\n");
        extract_features(io_info, &io_data, threshold, max_points, kp1, desc1);
        fprintf(stdout, "Found %zu keypoints in image 1\n", kp1.size());
        fprintf(stdout, "Extracted %zu descriptors from image 1\n", desc1.size());
        fprintf(stdout, "--------------------------------------\n");
        auto total_time1 = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time1 = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Image 1 - Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                (int)time_costs.size(),
                total_time1 / (float)time_costs.size(),
                *min_max_time1.second,
                *min_max_time1.first);
        fprintf(stdout, "--------------------------------------\n");

        // 11. Process second image
        if (input_data2.size() != io_info->pInputs[0].nSize / sizeof(float))
        {
            fprintf(stderr, "Input data2 size mismatch: expected %zu, got %zu\n",
                    io_info->pInputs[0].nSize / sizeof(float), input_data2.size());
            middleware::free_io(&io_data);
            return AX_ENGINE_DestroyHandle(handle);
        }
        
        memcpy(io_data.pInputs[0].pVirAddr, input_data2.data(), input_data2.size() * sizeof(float));
        fprintf(stdout, "Engine push input2 is done. \n");
        fprintf(stdout, "--------------------------------------\n");

        // 12. run model for image 2
        time_costs.assign(repeat, 0);
        for (int i = 0; i < repeat; ++i)
        {
            timer tick;
            ret = AX_ENGINE_RunSync(handle, &io_data);
            time_costs[i] = tick.cost();
            SAMPLE_AX_ENGINE_DEAL_HANDLE_IO
        }

        // 13. get result for image 2
        fprintf(stdout, "Processing image 2:\n");
        extract_features(io_info, &io_data, threshold, max_points, kp2, desc2);
        fprintf(stdout, "Found %zu keypoints in image 2\n", kp2.size());
        fprintf(stdout, "Extracted %zu descriptors from image 2\n", desc2.size());
        fprintf(stdout, "--------------------------------------\n");
        auto total_time2 = std::accumulate(time_costs.begin(), time_costs.end(), 0.f);
        auto min_max_time2 = std::minmax_element(time_costs.begin(), time_costs.end());
        fprintf(stdout,
                "Image 2 - Repeat %d times, avg time %.2f ms, max_time %.2f ms, min_time %.2f ms\n",
                (int)time_costs.size(),
                total_time2 / (float)time_costs.size(),
                *min_max_time2.second,
                *min_max_time2.first);
        fprintf(stdout, "--------------------------------------\n");

        middleware::free_io(&io_data);
        return AX_ENGINE_DestroyHandle(handle);
    }
} // namespace ax

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model", 'm', "joint file(a.k.a. joint model)", true, "");
    cmd.add<std::string>("img1", '1', "first image file", true, "");
    cmd.add<std::string>("img2", '2', "second image file", true, "");
    cmd.add<std::string>("size", 'g', "input_h, input_w", false, std::to_string(DEFAULT_IMG_H) + "," + std::to_string(DEFAULT_IMG_W));
    cmd.add<float>("threshold", 't', "keypoint threshold", false, DEFAULT_THRESHOLD);
    cmd.add<int>("max_points", 'p', "max number of keypoints", false, DEFAULT_MAX_POINTS);
    cmd.add<int>("repeat", 'r', "repeat count", false, DEFAULT_LOOP_COUNT);

    cmd.parse_check(argc, argv);

    // 0. get app args
    auto model_file = cmd.get<std::string>("model");
    auto img1_file = cmd.get<std::string>("img1");
    auto img2_file = cmd.get<std::string>("img2");

    auto model_file_flag = utilities::file_exist(model_file);
    auto img1_file_flag = utilities::file_exist(img1_file);
    auto img2_file_flag = utilities::file_exist(img2_file);

    if (!model_file_flag | !img1_file_flag | !img2_file_flag)
    {
        auto show_error = [](const std::string& kind, const std::string& value) {
            fprintf(stderr, "Input file %s(%s) is not exist, please check it.\n", kind.c_str(), value.c_str());
        };

        if (!model_file_flag) { show_error("model", model_file); }
        if (!img1_file_flag) { show_error("img1", img1_file); }
        if (!img2_file_flag) { show_error("img2", img2_file); }

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

    auto threshold = cmd.get<float>("threshold");
    auto max_points = cmd.get<int>("max_points");
    auto repeat = cmd.get<int>("repeat");

    // 1. print args
    fprintf(stdout, "--------------------------------------\n");
    fprintf(stdout, "model file : %s\n", model_file.c_str());
    fprintf(stdout, "image1 file : %s\n", img1_file.c_str());
    fprintf(stdout, "image2 file : %s\n", img2_file.c_str());
    fprintf(stdout, "img_h, img_w : %d %d\n", input_size[0], input_size[1]);
    fprintf(stdout, "threshold : %.4f\n", threshold);
    fprintf(stdout, "max_points : %d\n", max_points);
    fprintf(stdout, "--------------------------------------\n");

    // 2. read images & preprocess
    cv::Mat img1 = cv::imread(img1_file);
    cv::Mat img2 = cv::imread(img2_file);
    
    if (img1.empty())
    {
        fprintf(stderr, "Read image1 failed.\n");
        return -1;
    }
    
    if (img2.empty())
    {
        fprintf(stderr, "Read image2 failed.\n");
        return -1;
    }
    
    // Convert to grayscale and resize
    cv::Mat gray1, gray2;
    cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    
    cv::Mat resized1, resized2;
    cv::resize(gray1, resized1, cv::Size(input_size[1], input_size[0]));
    cv::resize(gray2, resized2, cv::Size(input_size[1], input_size[0]));
    
    // Normalize to [0, 1] and convert to float
    std::vector<float> input_data1(input_size[0] * input_size[1], 0.0f);
    std::vector<float> input_data2(input_size[0] * input_size[1], 0.0f);
    
    for (int y = 0; y < input_size[0]; ++y)
    {
        for (int x = 0; x < input_size[1]; ++x)
        {
            input_data1[y * input_size[1] + x] = resized1.at<uchar>(y, x) / 255.0f;
            input_data2[y * input_size[1] + x] = resized2.at<uchar>(y, x) / 255.0f;
        }
    }

    // 3. sys_init
    AX_SYS_Init();

    // 4. -  engine model  -  can only use AX_ENGINE** inside
    {
        std::vector<KeyPoint> kp1, kp2;
        std::vector<std::vector<float>> desc1, desc2;
        
        // AX_ENGINE_NPUReset(); // todo ??
        ax::run_model(model_file, input_data1, input_data2, repeat, img1, img2, threshold, max_points,
                     kp1, desc1, kp2, desc2);

        // Match and visualize
        ax::match_and_visualize(img1, img2, kp1, desc1, kp2, desc2, "superpoint_matches.jpg");

        // 4.3 engine de init
        AX_ENGINE_Deinit();
        // AX_ENGINE_NPUReset();
    }
    // 4. -  engine model  -

    AX_SYS_Deinit();
    return 0;
}

