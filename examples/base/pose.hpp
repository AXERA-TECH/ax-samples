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

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

namespace pose
{
    typedef struct
    {
        float x;
        float y;
        float score;
    } ai_point_t;

    struct skeleton
    {
        int connection[2];
        int left_right_neutral;
    };

    std::vector<skeleton> pairs = {{15, 13, 0},
                                   {13, 11, 0},
                                   {16, 14, 0},
                                   {14, 12, 0},
                                   {11, 12, 0},
                                   {5, 11, 0},
                                   {6, 12, 0},
                                   {5, 6, 0},
                                   {5, 7, 0},
                                   {6, 8, 0},
                                   {7, 9, 0},
                                   {8, 10, 0},
                                   {1, 2, 0},
                                   {0, 1, 0},
                                   {0, 2, 0},
                                   {1, 3, 0},
                                   {2, 4, 0},
                                   {0, 5, 0},
                                   {0, 6, 0}};

    typedef struct ai_body_parts_s
    {
        std::vector<ai_point_t> keypoints;
        int32_t img_width = 0;
        int32_t img_heigh = 0;
        uint64_t timestamp = 0;
    } ai_body_parts_s;

    static inline void find_max_2d(float* buf, int width, int height, int* max_idx_width, int* max_idx_height, float* max_value, int c)
    {
        float* ptr = buf;
        *max_value = -10.f;
        *max_idx_width = 0;
        *max_idx_height = 0;
        for (int h = 0; h < height; h++)
        {
            for (int w = 0; w < width; w++)
            {
                float score = ptr[c * height * width + h * width + w];
                if (score > *max_value)
                {
                    *max_value = score;
                    *max_idx_height = h;
                    *max_idx_width = w;
                }
            }
        }
    }

    static inline void draw_result(cv::Mat img, ai_body_parts_s& pose, int joints_num, int model_w, int model_h)
    {
        for (int i = 0; i < joints_num; i++)
        {
            int x = (int)(pose.keypoints[i].x * img.cols);
            int y = (int)(pose.keypoints[i].y * img.rows);

            x = std::max(std::min(x, (img.cols - 1)), 0);
            y = std::max(std::min(y, (img.rows - 1)), 0);

            cv::circle(img, cv::Point(x, y), 4, cv::Scalar(0, 255, 0), cv::FILLED);
        }

        cv::Scalar color;
        cv::Point pt1;
        cv::Point pt2;
        for (auto& element : pairs)
        {
            switch (element.left_right_neutral)
            {
            case 0:
                color = cv::Scalar(255, 0, 0);
                break;
            case 1:
                color = cv::Scalar(0, 0, 255);
                break;
            default:
                color = cv::Scalar(0, 255, 0);
            }

            int x1 = (int)(pose.keypoints[element.connection[0]].x * img.cols);
            int y1 = (int)(pose.keypoints[element.connection[0]].y * img.rows);
            int x2 = (int)(pose.keypoints[element.connection[1]].x * img.cols);
            int y2 = (int)(pose.keypoints[element.connection[1]].y * img.rows);

            x1 = std::max(std::min(x1, (img.cols - 1)), 0);
            y1 = std::max(std::min(y1, (img.rows - 1)), 0);
            x2 = std::max(std::min(x2, (img.cols - 1)), 0);
            y2 = std::max(std::min(y2, (img.rows - 1)), 0);

            pt1 = cv::Point(x1, y1);
            pt2 = cv::Point(x2, y2);
            cv::line(img, pt1, pt2, color, 2);
        }

        cv::imwrite("./pose_out.png", img);
    }

    static inline void post_process(float* data, ai_body_parts_s& pose, int joint_num, int img_h, int img_w)
    {
        int heatmap_width = img_w / 4;
        int heatmap_height = img_h / 4;
        int max_idx_width, max_idx_height;
        float max_score;

        ai_point_t kp;
        for (int c = 0; c < joint_num; ++c)
        {
            find_max_2d(data, heatmap_width, heatmap_height, &max_idx_width, &max_idx_height, &max_score, c);
            kp.x = (float)max_idx_width / (float)heatmap_width;
            kp.y = (float)max_idx_height / (float)heatmap_height;
            kp.score = max_score;
            pose.keypoints.push_back(kp);

//            std::cout << "x: " << pose.keypoints[c].x << ", y: " << pose.keypoints[c].y << ", score: "
//                      << pose.keypoints[c].score << std::endl;
        }
    }

} // namespace pose