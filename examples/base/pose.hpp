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
    std::vector<skeleton> hand_pairs = {{0, 1, 0},
                                        {1, 2, 0},
                                        {2, 3, 0},
                                        {3, 4, 0},
                                        {0, 5, 1},
                                        {5, 6, 1},
                                        {6, 7, 1},
                                        {7, 8, 1},
                                        {0, 9, 2},
                                        {9, 10, 2},
                                        {10, 11, 2},
                                        {11, 12, 2},
                                        {0, 13, 3},
                                        {13, 14, 3},
                                        {14, 15, 3},
                                        {15, 16, 3},
                                        {0, 17, 4},
                                        {17, 18, 4},
                                        {18, 19, 4},
                                        {19, 20, 4}};
    std::vector<skeleton> animal_pairs = {{19, 15, 0},
                                          {18, 14, 0},
                                          {17, 13, 0},
                                          {16, 12, 0},
                                          {15, 11, 0},
                                          {14, 10, 0},
                                          {13, 9, 0},
                                          {12, 8, 0},
                                          {11, 6, 0},
                                          {10, 6, 0},
                                          {9, 7, 0},
                                          {8, 7, 0},
                                          {6, 7, 0},
                                          {7, 5, 0},
                                          {5, 4, 0},
                                          {0, 2, 0},
                                          {1, 3, 0},
                                          {0, 1, 0},
                                          {0, 4, 0},
                                          {1, 4, 0}};

    typedef struct ai_body_parts_s
    {
        std::vector<ai_point_t> keypoints;
        int32_t img_width = 0;
        int32_t img_heigh = 0;
        uint64_t timestamp = 0;
    } ai_body_parts_s;

    typedef struct ai_hand_parts_s
    {
        std::vector<ai_point_t> keypoints;
        int32_t hand_side = 0; //0-left hand,1-right hand
        int32_t img_width = 0;
        int32_t img_heigh = 0;
        uint64_t timestamp = 0;
    } ai_hand_parts_s;

    typedef struct ai_animal_parts_s
    {
        std::vector<ai_point_t> keypoints;
        int32_t img_width = 0;
        int32_t img_heigh = 0;
        uint64_t timestamp = 0;
    } ai_animal_parts_s;

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
    }

    static inline void draw_animal_result(cv::Mat img, ai_animal_parts_s& pose, int joints_num, int model_w, int model_h)
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
        for (auto& element : animal_pairs)
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
    }

    static inline void draw_result(cv::Mat img, ai_body_parts_s& pose, int joints_num, int model_w, int model_h, const detection::Object& obj)
    {
        for (int i = 0; i < joints_num; i++)
        {
            int x = (int)(pose.keypoints[i].x);
            int y = (int)(pose.keypoints[i].y);
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

            int x1 = (int)(pose.keypoints[element.connection[0]].x);
            int y1 = (int)(pose.keypoints[element.connection[0]].y);
            int x2 = (int)(pose.keypoints[element.connection[1]].x);
            int y2 = (int)(pose.keypoints[element.connection[1]].y);

            x1 = std::max(std::min(x1, (img.cols - 1)), 0);
            y1 = std::max(std::min(y1, (img.rows - 1)), 0);
            x2 = std::max(std::min(x2, (img.cols - 1)), 0);
            y2 = std::max(std::min(y2, (img.rows - 1)), 0);

            pt1 = cv::Point(x1, y1);
            pt2 = cv::Point(x2, y2);
            cv::line(img, pt1, pt2, color, 2);
        }
        // 画框
        cv::rectangle(img, obj.rect, cv::Scalar(255, 0, 0));
        char text[256];
        sprintf(text, "%s %.1f%%", "person", obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > img.cols)
            x = img.cols - label_size.width;

        cv::rectangle(img, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255), -1);

        cv::putText(img, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 0, 0));
        cv::imwrite("./pose_ppl_out.png", img);
    }

    static inline void draw_result_hand(cv::Mat img, ai_hand_parts_s& pose, int joints_num)
    {
        for (int i = 0; i < joints_num; i++)
        {
            int x = (int)(pose.keypoints[i].x * img.cols);
            int y = (int)(pose.keypoints[i].y * img.rows);

            x = std::max(std::min(x, (img.cols - 1)), 0);
            y = std::max(std::min(y, (img.rows - 1)), 0);

            cv::circle(img, cv::Point(x, y), 4, cv::Scalar(0, 0, 255), cv::FILLED);
        }

        cv::Scalar color;
        cv::Point pt1;
        cv::Point pt2;
        for (auto& element : hand_pairs)
        {
            switch (element.left_right_neutral)
            {
            case 0:
                color = cv::Scalar(10, 215, 255);
                break;
            case 1:
                color = cv::Scalar(255, 115, 55);
                break;
            case 2:
                color = cv::Scalar(5, 255, 55);
                break;
            case 3:
                color = cv::Scalar(25, 15, 255);
                break;
            default:
                color = cv::Scalar(225, 15, 55);
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

        cv::imwrite("./hand_pose_out.png", img);
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

    static inline void animal_post_process(float* data, ai_animal_parts_s& pose, int joint_num, int img_h, int img_w)
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

    static inline void ppl_pose_post_process(float* data1, float* data2, ai_body_parts_s& pose, int joint_num, int img_h, int img_w, int offset_top, int offset_left, int offset_x, int offset_y, float ratio)
    {
        ai_point_t kp;

        for (int c = 0; c < joint_num; ++c)
        {
            kp.x = (data1[c] / 2 - offset_left) / ratio + offset_x;
            kp.y = (data2[c] / 2 - offset_top) / ratio + offset_y;
            std::cout << "x1: " << kp.x << ", y1: " << kp.y << std::endl;
            pose.keypoints.push_back(kp);
        }
    }

    static inline void post_process_hand(float* point_data, float* score_data, ai_hand_parts_s& pose, int joint_num, int img_h, int img_w)
    {
        ai_point_t kp;
        for (int c = 0; c < joint_num; ++c)
        {
            kp.x = (float)point_data[c * 3] / img_w;
            kp.y = (float)point_data[c * 3 + 1] / img_h;
            pose.keypoints.push_back(kp);
        }
        if (score_data[0] > 0.5)
            pose.hand_side = 1;
    }

} // namespace pose