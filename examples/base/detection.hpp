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
 * Author: ls.wang
 */

#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
namespace detection
{
    typedef struct
    {
        int grid0;
        int grid1;
        int stride;
    } GridAndStride;

    typedef struct Object
    {
        cv::Rect_<float> rect;
        int label;
        float prob;
        cv::Point2f landmark[5];
        /* for yolov5-seg */
        cv::Mat mask;
        std::vector<float> mask_feat;
        std::vector<float> kps_feat;
    } Object;

    /* for palm detection */
    typedef struct PalmObject
    {
        cv::Rect_<float> rect;
        float prob;
        cv::Point2f vertices[4];
        cv::Point2f landmarks[7];
        cv::Mat affine_trans_mat;
        cv::Mat affine_trans_mat_inv;
    } PalmObject;

    static inline float sigmoid(float x)
    {
        return static_cast<float>(1.f / (1.f + exp(-x)));
    }

    static float softmax(const float* src, float* dst, int length)
    {
        const float alpha = *std::max_element(src, src + length);
        float denominator = 0;
        float dis_sum = 0;
        for (int i = 0; i < length; ++i)
        {
            dst[i] = exp(src[i] - alpha);
            denominator += dst[i];
        }
        for (int i = 0; i < length; ++i)
        {
            dst[i] /= denominator;
            dis_sum += i * dst[i];
        }
        return dis_sum;
    }

    template<typename T>
    static inline float intersection_area(const T& a, const T& b)
    {
        cv::Rect_<float> inter = a.rect & b.rect;
        return inter.area();
    }

    template<typename T>
    static void qsort_descent_inplace(std::vector<T>& faceobjects, int left, int right)
    {
        int i = left;
        int j = right;
        float p = faceobjects[(left + right) / 2].prob;

        while (i <= j)
        {
            while (faceobjects[i].prob > p)
                i++;

            while (faceobjects[j].prob < p)
                j--;

            if (i <= j)
            {
                // swap
                std::swap(faceobjects[i], faceobjects[j]);

                i++;
                j--;
            }
        }
#pragma omp parallel sections
        {
#pragma omp section
            {
                if (left < j) qsort_descent_inplace(faceobjects, left, j);
            }
#pragma omp section
            {
                if (i < right) qsort_descent_inplace(faceobjects, i, right);
            }
        }
    }

    template<typename T>
    static void qsort_descent_inplace(std::vector<T>& faceobjects)
    {
        if (faceobjects.empty())
            return;

        qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
    }

    template<typename T>
    static void nms_sorted_bboxes(const std::vector<T>& faceobjects, std::vector<int>& picked, float nms_threshold)
    {
        picked.clear();

        const int n = faceobjects.size();

        std::vector<float> areas(n);
        for (int i = 0; i < n; i++)
        {
            areas[i] = faceobjects[i].rect.area();
        }

        for (int i = 0; i < n; i++)
        {
            const T& a = faceobjects[i];

            int keep = 1;
            for (int j = 0; j < (int)picked.size(); j++)
            {
                const T& b = faceobjects[picked[j]];

                // intersection over union
                float inter_area = intersection_area(a, b);
                float union_area = areas[i] + areas[picked[j]] - inter_area;
                // float IoU = inter_area / union_area
                if (inter_area / union_area > nms_threshold)
                    keep = 0;
            }

            if (keep)
                picked.push_back(i);
        }
    }

    static void generate_grids_and_stride(const int target_w, const int target_h, std::vector<int>& strides, std::vector<GridAndStride>& grid_strides)
    {
        for (auto stride : strides)
        {
            int num_grid_w = target_w / stride;
            int num_grid_h = target_h / stride;
            for (int g1 = 0; g1 < num_grid_h; g1++)
            {
                for (int g0 = 0; g0 < num_grid_w; g0++)
                {
                    GridAndStride gs;
                    gs.grid0 = g0;
                    gs.grid1 = g1;
                    gs.stride = stride;
                    grid_strides.push_back(gs);
                }
            }
        }
    }

    static void generate_proposals_scrfd(int feat_stride, const float* score_blob,
                                         const float* bbox_blob, const float* kps_blob,
                                         float prob_threshold, std::vector<detection::Object>& faceobjects, int letterbox_cols, int letterbox_rows)
    {
        static float anchors[] = {-8.f, -8.f, 8.f, 8.f, -16.f, -16.f, 16.f, 16.f, -32.f, -32.f, 32.f, 32.f, -64.f, -64.f, 64.f, 64.f, -128.f, -128.f, 128.f, 128.f, -256.f, -256.f, 256.f, 256.f};
        int feat_w = letterbox_cols / feat_stride;
        int feat_h = letterbox_rows / feat_stride;
        int feat_size = feat_w * feat_h;
        int anchor_group = 1;
        if (feat_stride == 8)
            anchor_group = 1;
        if (feat_stride == 16)
            anchor_group = 2;
        if (feat_stride == 32)
            anchor_group = 3;

        // generate face proposal from bbox deltas and shifted anchors
        const int num_anchors = 2;

        for (int q = 0; q < num_anchors; q++)
        {
            // shifted anchor
            float anchor_y = anchors[(anchor_group - 1) * 8 + q * 4 + 1];

            float anchor_w = anchors[(anchor_group - 1) * 8 + q * 4 + 2] - anchors[(anchor_group - 1) * 8 + q * 4 + 0];
            float anchor_h = anchors[(anchor_group - 1) * 8 + q * 4 + 3] - anchors[(anchor_group - 1) * 8 + q * 4 + 1];

            for (int i = 0; i < feat_h; i++)
            {
                float anchor_x = anchors[(anchor_group - 1) * 8 + q * 4 + 0];

                for (int j = 0; j < feat_w; j++)
                {
                    int index = i * feat_w + j;

                    float prob = sigmoid(score_blob[q * feat_size + index]);

                    if (prob >= prob_threshold)
                    {
                        // insightface/detection/scrfd/mmdet/models/dense_heads/scrfd_head.py _get_bboxes_single()
                        float dx = bbox_blob[(q * 4 + 0) * feat_size + index] * feat_stride;
                        float dy = bbox_blob[(q * 4 + 1) * feat_size + index] * feat_stride;
                        float dw = bbox_blob[(q * 4 + 2) * feat_size + index] * feat_stride;
                        float dh = bbox_blob[(q * 4 + 3) * feat_size + index] * feat_stride;
                        // insightface/detection/scrfd/mmdet/core/bbox/transforms.py distance2bbox()
                        float cx = anchor_x + anchor_w * 0.5f;
                        float cy = anchor_y + anchor_h * 0.5f;

                        float x0 = cx - dx;
                        float y0 = cy - dy;
                        float x1 = cx + dw;
                        float y1 = cy + dh;

                        Object obj;
                        obj.label = 0;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0 + 1;
                        obj.rect.height = y1 - y0 + 1;
                        obj.prob = prob;

                        if (kps_blob != 0)
                        {
                            obj.landmark[0].x = cx + kps_blob[index] * feat_stride;
                            obj.landmark[0].y = cy + kps_blob[1 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[1].x = cx + kps_blob[2 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[1].y = cy + kps_blob[3 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[2].x = cx + kps_blob[4 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[2].y = cy + kps_blob[5 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[3].x = cx + kps_blob[6 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[3].y = cy + kps_blob[7 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[4].x = cx + kps_blob[8 * feat_h * feat_w + index] * feat_stride;
                            obj.landmark[4].y = cy + kps_blob[9 * feat_h * feat_w + index] * feat_stride;
                        }

                        faceobjects.push_back(obj);
                    }

                    anchor_x += feat_stride;
                }

                anchor_y += feat_stride;
            }
        }
    }

    static void generate_proposals_mobilenet_ssd(const float* score, const float* boxes, const int head_count, const int* feature_map_size, const int* anchor_size, const int cls_num,
                                                 float prob_threshold, const float* strides, const float center_val, const float scale_val, const float* anchor_info, std::vector<detection::Object>& objects)
    {
        auto ptr_score = score;
        auto ptr_boxes = boxes;
        auto ptr_anchor_info = anchor_info;
        for (int head = 0; head < head_count; ++head)
        {
            for (int fea_h = 0; fea_h < feature_map_size[head]; ++fea_h)
            {
                for (int fea_w = 0; fea_w < feature_map_size[head]; ++fea_w)
                {
                    for (int anchor_i = 0; anchor_i < anchor_size[head]; ++anchor_i)
                    {
                        float softmax_sum = 0;
                        float class_score = -FLT_MAX;
                        int class_index = 0;
                        for (int s = 0; s < cls_num + 1; s++)
                        {
                            softmax_sum += std::exp(ptr_score[s]);
                        }
                        for (int i = 0; i < cls_num + 1; ++i)
                        {
                            float temp = std::exp(ptr_score[i]) / softmax_sum;
                            //                            if (temp > class_score)
                            //                            {
                            //                                class_index = i;
                            //                                class_score = temp;
                            //                            }

                            class_index = i;
                            class_score = temp;

                            if (temp >= prob_threshold and class_index != 0)
                            {
                                // fprintf(stderr, "class_score: %f %d \n", class_score, i);

                                float pred_x = (((float)fea_w + 0.5f) / (300.0f / strides[head]) + ptr_boxes[0] * center_val * ptr_anchor_info[anchor_i * 2] / 300.0f);
                                float pred_y = (((float)fea_h + 0.5f) / (300.0f / strides[head]) + ptr_boxes[1] * center_val * ptr_anchor_info[anchor_i * 2 + 1] / 300.0f);
                                float pred_w = std::exp(ptr_boxes[2] * scale_val) * ptr_anchor_info[anchor_i * 2] / 300.0f;
                                float pred_h = std::exp(ptr_boxes[3] * scale_val) * ptr_anchor_info[anchor_i * 2 + 1] / 300.0f;

                                float x0 = (pred_x - pred_w * 0.5f) * 300.0f;
                                float y0 = (pred_y - pred_h * 0.5f) * 300.0f;
                                float x1 = (pred_x + pred_w * 0.5f) * 300.0f;
                                float y1 = (pred_y + pred_h * 0.5f) * 300.0f;

                                Object obj;
                                obj.rect.x = x0;
                                obj.rect.y = y0;
                                obj.rect.width = x1 - x0;
                                obj.rect.height = y1 - y0;
                                obj.label = class_index;
                                obj.prob = class_score;

                                objects.push_back(obj);
                            }
                        }

                        ptr_score += cls_num + 1;
                        ptr_boxes += 4;
                    }
                }
            }
            ptr_anchor_info += anchor_size[head] * 2;
        }
    }

    static void generate_proposals_yolox(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                         int letterbox_cols, int letterbox_rows, int cls_num = 80)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;

        auto feat_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                float box_objectness = feat_ptr[4];
                if (box_objectness < prob_threshold)
                {
                    feat_ptr += cls_num + 5;
                    continue;
                }

                //process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s <= cls_num - 1; s++)
                {
                    float score = feat_ptr[s + 5];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }

                float box_prob = box_objectness * class_score;

                if (box_prob > prob_threshold)
                {
                    float x_center = (feat_ptr[0] + w) * stride;
                    float y_center = (feat_ptr[1] + h) * stride;
                    float w = exp(feat_ptr[2]) * stride;
                    float h = exp(feat_ptr[3]) * stride;
                    float x0 = x_center - w * 0.5f;
                    float y0 = y_center - h * 0.5f;

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = w;
                    obj.rect.height = h;
                    obj.label = class_index;
                    obj.prob = box_prob;

                    objects.push_back(obj);
                }

                feat_ptr += cls_num + 5;
            }
        }
    }

    static void generate_proposals_yolov7(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                          int letterbox_cols, int letterbox_rows, const float* anchors, int cls_num = 80)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;

        auto feat_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a_index = 0; a_index < 3; ++a_index)
                {
                    float box_objectness = feat_ptr[4];
                    if (box_objectness < prob_threshold)
                    {
                        feat_ptr += cls_num + 5;
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feat_ptr[s + 5];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }

                    float box_prob = box_objectness * class_score;

                    if (box_prob > prob_threshold)
                    {
                        float x_center = (feat_ptr[0] * 2 - 0.5f + (float)w) * (float)stride;
                        float y_center = (feat_ptr[1] * 2 - 0.5f + (float)h) * (float)stride;
                        float box_w = (feat_ptr[2] * 2) * (feat_ptr[2] * 2) * anchors[a_index * 2];
                        float box_h = (feat_ptr[3] * 2) * (feat_ptr[3] * 2) * anchors[a_index * 2 + 1];
                        float x0 = x_center - box_w * 0.5f;
                        float y0 = y_center - box_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = box_w;
                        obj.rect.height = box_h;
                        obj.label = class_index;
                        obj.prob = box_prob;

                        objects.push_back(obj);
                    }

                    feat_ptr += cls_num + 5;
                }
            }
        }
    }
    static void generate_proposals_yolov5_face(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                               int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int cls_num = 1;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5 + 10);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5 + 10];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;

                        const float* landmark_ptr = feature_ptr + 5;
                        for (int l = 0; l < 5; l++)
                        {
                            float lx = landmark_ptr[l * 2 + 0];
                            float ly = landmark_ptr[l * 2 + 1];
                            lx = lx * anchor_w + w * stride;
                            ly = ly * anchor_h + h * stride;
                            obj.landmark[l] = cv::Point2f(lx, ly);
                        }

                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5 + 10);
                }
            }
        }
    }

    static void generate_proposals_yolov5_license_plate(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                                        int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int cls_num = 1;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5 + 8);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5 + 8];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;

                        const float* landmark_ptr = feature_ptr + 5;
                        for (int l = 0; l < 4; l++)
                        {
                            float lx = landmark_ptr[l * 2 + 0];
                            float ly = landmark_ptr[l * 2 + 1];
                            lx = lx * anchor_w + w * stride;
                            ly = ly * anchor_h + h * stride;
                            obj.landmark[l] = cv::Point2f(lx, ly);
                        }

                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5 + 8);
                }
            }
        }
    }

    static void generate_proposals_yolov5(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                          int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid, int cls_num = 80)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;
                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5);
                }
            }
        }
    }

    static void generate_proposals_yolov5_seg(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                              int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid, int cls_num = 80, int mask_proto_dim = 32)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5 + mask_proto_dim);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;
                        obj.mask_feat.resize(mask_proto_dim);
                        for (int k = 0; k < mask_proto_dim; k++)
                        {
                            obj.mask_feat[k] = feature_ptr[cls_num + 5 + k];
                        }
                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5 + mask_proto_dim);
                }
            }
        }
    }

    static void generate_proposals_yolov5_visdrone(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                                   int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid, int cls_num = 10)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;
                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5);
                }
            }
        }
    }

    static void generate_proposals_yolov6(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                          int letterbox_cols, int letterbox_rows, int cls_num = 80)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;

        auto feat_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                //process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s <= cls_num - 1; s++)
                {
                    float score = feat_ptr[s + 4];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }

                float box_prob = class_score;

                if (box_prob > prob_threshold)
                {
                    float x0 = (w + 0.5f - feat_ptr[0]) * stride;
                    float y0 = (h + 0.5f - feat_ptr[1]) * stride;
                    float x1 = (w + 0.5f + feat_ptr[2]) * stride;
                    float y1 = (h + 0.5f + feat_ptr[3]) * stride;

                    float w = x1 - x0;
                    float h = y1 - y0;

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = w;
                    obj.rect.height = h;
                    obj.label = class_index;
                    obj.prob = box_prob;

                    objects.push_back(obj);
                }

                feat_ptr += 84;
            }
        }
    }

    static void generate_proposals_yolov7_face(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                               int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int cls_num = 1;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5 + 15);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5 + 15];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;

                        const float* landmark_ptr = feature_ptr + 6;
                        for (int l = 0; l < 5; l++)
                        {
                            float lx = (landmark_ptr[3 * l] * 2.0f - 0.5f + w) * stride;
                            float ly = (landmark_ptr[3 * l + 1] * 2.0f - 0.5f + h) * stride;
                            //float score = sigmoid(landmark_ptr[3 * l + 2]);
                            obj.landmark[l] = cv::Point2f(lx, ly);
                        }

                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5 + 15);
                }
            }
        }
    }

    static void generate_proposals_yolov7_palm(int stride, const float* feat, float prob_threshold, std::vector<PalmObject>& objects,
                                               int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int cls_num = 1;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        const int landmark_sort[7] = {0, 3, 4, 5, 6, 1, 2};
        auto feature_ptr = feat;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    if (feature_ptr[4] < prob_threshold_unsigmoid)
                    {
                        feature_ptr += (cls_num + 5 + 21);
                        continue;
                    }

                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feature_ptr[s + 5 + 21];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feature_ptr[4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        float dx = sigmoid(feature_ptr[0]);
                        float dy = sigmoid(feature_ptr[1]);
                        float dw = sigmoid(feature_ptr[2]);
                        float dh = sigmoid(feature_ptr[3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        PalmObject obj;
                        obj.rect.x = x0 / (float)letterbox_cols;
                        obj.rect.y = y0 / (float)letterbox_rows;
                        obj.rect.width = (x1 - x0) / (float)letterbox_cols;
                        obj.rect.height = (y1 - y0) / (float)letterbox_rows;
                        obj.prob = final_score;

                        const float* landmark_ptr = feature_ptr + 6;
                        std::vector<cv::Point2f> tmp(7);
                        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = 0, max_y = 0;
                        for (int l = 0; l < 7; l++)
                        {
                            float lx = (landmark_ptr[3 * l] * 2.0f - 0.5f + w) * stride;
                            float ly = (landmark_ptr[3 * l + 1] * 2.0f - 0.5f + h) * stride;
                            lx /= (float)letterbox_cols;
                            ly /= (float)letterbox_rows;

                            tmp[l] = cv::Point2f(lx, ly);
                            min_x = lx < min_x ? lx : min_x;
                            min_y = ly < min_y ? ly : min_y;
                            max_x = lx > max_x ? lx : max_x;
                            max_y = ly > max_y ? ly : max_y;
                        }
                        float w = max_x - min_x;
                        float h = max_y - min_y;
                        float long_side = h > w ? h : w;
                        long_side *= 1.1f;
                        obj.rect.x = min_x + w * 0.5f - long_side * 0.5f;
                        obj.rect.y = min_y + h * 0.5f - long_side * 0.5f;
                        obj.rect.width = long_side;
                        obj.rect.height = long_side;
                        for (int l = 0; l < 7; l++)
                        {
                            obj.landmarks[l] = tmp[landmark_sort[l]];
                        }

                        objects.push_back(obj);
                    }

                    feature_ptr += (cls_num + 5 + 21);
                }
            }
        }
    }

    static void generate_proposals_yolov8(int stride, const float* dfl_feat, const float* cls_feat, const float* cls_idx, float prob_threshold, std::vector<Object>& objects,
                                          int letterbox_cols, int letterbox_rows, int cls_num = 80)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        auto dfl_ptr = dfl_feat;
        auto cls_ptr = cls_feat;
        auto cls_idx_ptr = cls_idx;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                //process cls score
                int class_index = static_cast<int>(cls_idx_ptr[h * feat_w + w]);
                float class_score = cls_ptr[h * feat_w * cls_num + w * cls_num + class_index];

                float box_prob = sigmoid(class_score);

                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(dfl_ptr + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = box_prob;

                    objects.push_back(obj);
                }
                dfl_ptr += (4 * reg_max);
            }
        }
    }

    static void generate_proposals_yolov8_seg(int stride, const float* dfl_feat, const float* cls_feat, const float* cls_idx, float prob_threshold, std::vector<Object>& objects,
                                              int letterbox_cols, int letterbox_rows, int cls_num = 80, int mask_proto_dim = 32)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        auto dfl_ptr = dfl_feat;
        auto cls_ptr = cls_feat;
        auto cls_idx_ptr = cls_idx;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                //process cls score
                int class_index = static_cast<int>(cls_idx_ptr[h * feat_w + w]);
                float class_score = cls_ptr[h * feat_w * cls_num + w * cls_num + class_index];

                float box_prob = sigmoid(class_score);
                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(dfl_ptr + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = box_prob;
                    obj.mask_feat.resize(mask_proto_dim);
                    for (int k = 0; k < mask_proto_dim; k++)
                    {
                        obj.mask_feat[k] = dfl_ptr[4 * reg_max + k];
                    }
                    objects.push_back(obj);
                }

                dfl_ptr += (4 * reg_max + mask_proto_dim);
            }
        }
    }

    static void generate_proposals_yolov8_pose(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                               int letterbox_cols, int letterbox_rows, const int num_point = 17)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                //process cls score
                auto scores = feat;
                auto bboxes = feat + 1;
                auto kps = feat + 1 + 4 * reg_max;

                float box_prob = sigmoid(*scores);
                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(bboxes + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = 0;
                    obj.prob = box_prob;
                    obj.kps_feat.clear();
                    for (int k = 0; k < num_point; k++)
                    {
                        float kps_x = (kps[k * 3] * 2.f + w) * stride;
                        float kps_y = (kps[k * 3 + 1] * 2.f + h) * stride;
                        float kps_s = sigmoid(kps[k * 3 + 2]);
                        obj.kps_feat.push_back(kps_x);
                        obj.kps_feat.push_back(kps_y);
                        obj.kps_feat.push_back(kps_s);
                    }
                    objects.push_back(obj);
                }
                feat += (1 + 4 * reg_max + 3 * num_point);
            }
        }
    }

    static void generate_proposals_yolov8_native(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                                 int letterbox_cols, int letterbox_rows, int cls_num = 80)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        auto feat_ptr = feat;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                // process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s < cls_num; s++)
                {
                    float score = feat_ptr[s + 4 * reg_max];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }

                float box_prob = sigmoid(class_score);
                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(feat_ptr + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = box_prob;

                    objects.push_back(obj);
                }

                feat_ptr += (cls_num + 4 * reg_max);
            }
        }
    }

    static void generate_proposals_yolov8_seg_native(int stride, const float* feat, const float* feat_seg, float prob_threshold, std::vector<Object>& objects,
                                                     int letterbox_cols, int letterbox_rows, int cls_num = 80, int mask_proto_dim = 32)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        auto feat_ptr = feat;
        auto feat_seg_ptr = feat_seg;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                // process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s < cls_num; s++)
                {
                    float score = feat_ptr[s + 4 * reg_max];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }

                float box_prob = sigmoid(class_score);
                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(feat_ptr + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = box_prob;
                    obj.mask_feat.resize(mask_proto_dim);
                    memcpy(obj.mask_feat.data(), feat_seg_ptr, sizeof(float) * mask_proto_dim);
                    // for (int k = 0; k < mask_proto_dim; k++)
                    // {
                    //     obj.mask_feat[k] = feat_seg_ptr[k];
                    // }
                    objects.push_back(obj);
                }

                feat_ptr += cls_num + 4 * reg_max;
                feat_seg_ptr += mask_proto_dim;
            }
        }
    }

    static void generate_proposals_yolov8_pose_native(int stride, const float* feat, const float* feat_kps, float prob_threshold, std::vector<Object>& objects,
                                                      int letterbox_cols, int letterbox_rows, const int num_point = 17, int cls_num = 1)
    {
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int reg_max = 16;

        auto feat_ptr = feat;

        std::vector<float> dis_after_sm(reg_max, 0.f);
        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                // process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s < cls_num; s++)
                {
                    float score = feat_ptr[s + 4 * reg_max];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }

                float box_prob = sigmoid(class_score);
                if (box_prob > prob_threshold)
                {
                    float pred_ltrb[4];
                    for (int k = 0; k < 4; k++)
                    {
                        float dis = softmax(feat_ptr + k * reg_max, dis_after_sm.data(), reg_max);
                        pred_ltrb[k] = dis * stride;
                    }

                    float pb_cx = (w + 0.5f) * stride;
                    float pb_cy = (h + 0.5f) * stride;

                    float x0 = pb_cx - pred_ltrb[0];
                    float y0 = pb_cy - pred_ltrb[1];
                    float x1 = pb_cx + pred_ltrb[2];
                    float y1 = pb_cy + pred_ltrb[3];

                    x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
                    y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
                    x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
                    y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = box_prob;
                    obj.kps_feat.clear();
                    for (int k = 0; k < num_point; k++)
                    {
                        float kps_x = (feat_kps[k * 3] * 2.f + w) * stride;
                        float kps_y = (feat_kps[k * 3 + 1] * 2.f + h) * stride;
                        float kps_s = sigmoid(feat_kps[k * 3 + 2]);
                        obj.kps_feat.push_back(kps_x);
                        obj.kps_feat.push_back(kps_y);
                        obj.kps_feat.push_back(kps_s);
                    }
                    objects.push_back(obj);
                }
                feat_ptr += (cls_num + 4 * reg_max);
                feat_kps += 3 * num_point;
            }
        }
    }

    static void generate_proposals(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                                   int letterbox_cols, int letterbox_rows, const float* anchors, int cls_num = 80)
    {
        int anchor_num = 3;
        int feat_w = letterbox_cols / stride;
        int feat_h = letterbox_rows / stride;
        int anchor_group;
        if (stride == 8)
            anchor_group = 1;
        if (stride == 16)
            anchor_group = 2;
        if (stride == 32)
            anchor_group = 3;

        int w_stride = (cls_num + 5);
        int h_stride = feat_w * w_stride;
        int a_stride = feat_h * h_stride;

        for (int h = 0; h <= feat_h - 1; h++)
        {
            for (int w = 0; w <= feat_w - 1; w++)
            {
                for (int a = 0; a <= anchor_num - 1; a++)
                {
                    //process cls score
                    int class_index = 0;
                    float class_score = -FLT_MAX;
                    int offset = a * a_stride + h * h_stride + w * w_stride;
                    for (int s = 0; s <= cls_num - 1; s++)
                    {
                        float score = feat[offset + s + 5];
                        if (score > class_score)
                        {
                            class_index = s;
                            class_score = score;
                        }
                    }
                    //process box score
                    float box_score = feat[offset + 4];
                    float final_score = sigmoid(box_score) * sigmoid(class_score);

                    if (final_score >= prob_threshold)
                    {
                        int loc_idx = offset;
                        float dx = sigmoid(feat[loc_idx + 0]);
                        float dy = sigmoid(feat[loc_idx + 1]);
                        float dw = sigmoid(feat[loc_idx + 2]);
                        float dh = sigmoid(feat[loc_idx + 3]);
                        float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                        float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                        float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                        float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                        float pred_w = dw * dw * 4.0f * anchor_w;
                        float pred_h = dh * dh * 4.0f * anchor_h;
                        float x0 = pred_cx - pred_w * 0.5f;
                        float y0 = pred_cy - pred_h * 0.5f;
                        float x1 = pred_cx + pred_w * 0.5f;
                        float y1 = pred_cy + pred_h * 0.5f;

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = class_index;
                        obj.prob = final_score;
                        objects.push_back(obj);
                    }
                }
            }
        }
    }

    static void generate_proposals_palm(std::vector<PalmObject>& region_list, float score_thresh, int input_img_w, int input_img_h, float* scores_ptr, float* bboxes_ptr, int head_count, const int* strides, const int* anchor_size, const float* anchor_offset, const int* feature_map_size, float prob_threshold_unsigmoid)
    {
        int idx = 0;
        for (int i = 0; i < head_count; i++)
        {
            for (int y = 0; y < feature_map_size[i]; y++)
            {
                for (int x = 0; x < feature_map_size[i]; x++)
                {
                    for (int k = 0; k < anchor_size[i]; k++)
                    {
                        if (scores_ptr[idx] < prob_threshold_unsigmoid)
                        {
                            idx++;
                            continue;
                        }

                        const float x_center = (x + anchor_offset[i]) * 1.0f / feature_map_size[i];
                        const float y_center = (y + anchor_offset[i]) * 1.0f / feature_map_size[i];
                        float score = sigmoid(scores_ptr[idx]);

                        if (score > score_thresh)
                        {
                            float* p = bboxes_ptr + (idx * 18);

                            float cx = p[0] / input_img_w + x_center;
                            float cy = p[1] / input_img_h + y_center;
                            float w = p[2] / input_img_w;
                            float h = p[3] / input_img_h;

                            float x0 = cx - w * 0.5f;
                            float y0 = cy - h * 0.5f;
                            float x1 = cx + w * 0.5f;
                            float y1 = cy + h * 0.5f;

                            PalmObject region;
                            region.prob = score;
                            region.rect.x = x0;
                            region.rect.y = y0;
                            region.rect.width = x1 - x0;
                            region.rect.height = y1 - y0;

                            for (int j = 0; j < 7; j++)
                            {
                                float lx = p[4 + (2 * j) + 0];
                                float ly = p[4 + (2 * j) + 1];
                                lx += x_center * input_img_w;
                                ly += y_center * input_img_h;
                                lx /= (float)input_img_w;
                                ly /= (float)input_img_h;

                                region.landmarks[j].x = lx;
                                region.landmarks[j].y = ly;
                            }
                            region_list.push_back(region);
                        }
                        idx++;
                    }
                }
            }
        }
    }

    static void draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects, const char** class_names, const char* output_name, double fontScale = 0.5, int thickness = 1)
    {
        static const std::vector<cv::Scalar> COCO_COLORS = {
            {128, 56, 0, 255}, {128, 226, 255, 0}, {128, 0, 94, 255}, {128, 0, 37, 255}, {128, 0, 255, 94}, {128, 255, 226, 0}, {128, 0, 18, 255}, {128, 255, 151, 0}, {128, 170, 0, 255}, {128, 0, 255, 56}, {128, 255, 0, 75}, {128, 0, 75, 255}, {128, 0, 255, 169}, {128, 255, 0, 207}, {128, 75, 255, 0}, {128, 207, 0, 255}, {128, 37, 0, 255}, {128, 0, 207, 255}, {128, 94, 0, 255}, {128, 0, 255, 113}, {128, 255, 18, 0}, {128, 255, 0, 56}, {128, 18, 0, 255}, {128, 0, 255, 226}, {128, 170, 255, 0}, {128, 255, 0, 245}, {128, 151, 255, 0}, {128, 132, 255, 0}, {128, 75, 0, 255}, {128, 151, 0, 255}, {128, 0, 151, 255}, {128, 132, 0, 255}, {128, 0, 255, 245}, {128, 255, 132, 0}, {128, 226, 0, 255}, {128, 255, 37, 0}, {128, 207, 255, 0}, {128, 0, 255, 207}, {128, 94, 255, 0}, {128, 0, 226, 255}, {128, 56, 255, 0}, {128, 255, 94, 0}, {128, 255, 113, 0}, {128, 0, 132, 255}, {128, 255, 0, 132}, {128, 255, 170, 0}, {128, 255, 0, 188}, {128, 113, 255, 0}, {128, 245, 0, 255}, {128, 113, 0, 255}, {128, 255, 188, 0}, {128, 0, 113, 255}, {128, 255, 0, 0}, {128, 0, 56, 255}, {128, 255, 0, 113}, {128, 0, 255, 188}, {128, 255, 0, 94}, {128, 255, 0, 18}, {128, 18, 255, 0}, {128, 0, 255, 132}, {128, 0, 188, 255}, {128, 0, 245, 255}, {128, 0, 169, 255}, {128, 37, 255, 0}, {128, 255, 0, 151}, {128, 188, 0, 255}, {128, 0, 255, 37}, {128, 0, 255, 0}, {128, 255, 0, 170}, {128, 255, 0, 37}, {128, 255, 75, 0}, {128, 0, 0, 255}, {128, 255, 207, 0}, {128, 255, 0, 226}, {128, 255, 245, 0}, {128, 188, 255, 0}, {128, 0, 255, 18}, {128, 0, 255, 75}, {128, 0, 255, 151}, {128, 255, 56, 0}, {128, 245, 255, 0}};
        cv::Mat image = bgr.clone();

        for (size_t i = 0; i < objects.size(); i++)
        {
            const Object& obj = objects[i];

            fprintf(stdout, "%2d: %3.0f%%, [%4.0f, %4.0f, %4.0f, %4.0f], %s\n", obj.label, obj.prob * 100, obj.rect.x,
                    obj.rect.y, obj.rect.x + obj.rect.width, obj.rect.y + obj.rect.height, class_names[obj.label]);

            cv::rectangle(image, obj.rect, COCO_COLORS[obj.label], thickness);

            char text[256];
            sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);

            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseLine);

            int x = obj.rect.x;
            int y = obj.rect.y - label_size.height - baseLine;
            if (y < 0)
                y = 0;
            if (x + label_size.width > image.cols)
                x = image.cols - label_size.width;

            cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                          cv::Scalar(0, 0, 0), -1);

            cv::putText(image, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, fontScale,
                        cv::Scalar(255, 255, 255), thickness);
        }

        cv::imwrite(std::string(output_name) + ".jpg", image);
    }

    static void draw_keypoints(const cv::Mat& bgr, const std::vector<Object>& objects,
                               const std::vector<std::vector<uint8_t> >& kps_colors,
                               const std::vector<std::vector<uint8_t> >& limb_colors,
                               const std::vector<std::vector<uint8_t> >& skeleton,
                               const char* output_name)
    {
        cv::Mat image = bgr.clone();

        for (size_t i = 0; i < objects.size(); i++)
        {
            const Object& obj = objects[i];

            fprintf(stdout, "%2d: %3.0f%%, [%4.0f, %4.0f, %4.0f, %4.0f], person\n", obj.label, obj.prob * 100, obj.rect.x,
                    obj.rect.y, obj.rect.x + obj.rect.width, obj.rect.y + obj.rect.height);

            cv::rectangle(image, obj.rect, cv::Scalar(255, 0, 0));

            char text[256];
            sprintf(text, "person %.1f%%", obj.prob * 100);

            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

            int x = obj.rect.x;
            int y = obj.rect.y - label_size.height - baseLine;
            if (y < 0)
                y = 0;
            if (x + label_size.width > image.cols)
                x = image.cols - label_size.width;

            cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                          cv::Scalar(255, 255, 255), -1);

            cv::putText(image, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 0, 0));

            const int num_point = obj.kps_feat.size() / 3;
            for (int j = 0; j < num_point + 2; j++)
            {
                // draw circle
                if (j < num_point)
                {
                    int kps_x = std::round(obj.kps_feat[j * 3]);
                    int kps_y = std::round(obj.kps_feat[j * 3 + 1]);
                    float kps_s = obj.kps_feat[j * 3 + 2];
                    if (kps_s > 0.5f)
                    {
                        auto kps_color = cv::Scalar(kps_colors[j][0], kps_colors[j][1], kps_colors[j][2]);
                        cv::circle(image, {kps_x, kps_y}, 5, kps_color, -1);
                    }
                }
                // draw line
                auto& ske = skeleton[j];
                int pos1_x = obj.kps_feat[(ske[0] - 1) * 3];
                int pos1_y = obj.kps_feat[(ske[0] - 1) * 3 + 1];

                int pos2_x = obj.kps_feat[(ske[1] - 1) * 3];
                int pos2_y = obj.kps_feat[(ske[1] - 1) * 3 + 1];

                float pos1_s = obj.kps_feat[(ske[0] - 1) * 3 + 2];
                float pos2_s = obj.kps_feat[(ske[1] - 1) * 3 + 2];

                if (pos1_s > 0.5f && pos2_s > 0.5f)
                {
                    auto limb_color = cv::Scalar(limb_colors[j][0], limb_colors[j][1], limb_colors[j][2]);
                    cv::line(image, {pos1_x, pos1_y}, {pos2_x, pos2_y}, limb_color, 2);
                }
            }
        }
        cv::imwrite(std::string(output_name) + ".jpg", image);
    }

    static void draw_objects_mask(const cv::Mat& bgr, const std::vector<Object>& objects, const char** class_names, const std::vector<std::vector<uint8_t> >& colors, const char* output_name)
    {
        cv::Mat image = bgr.clone();
        cv::Mat mask = bgr.clone();
        int color_index = 0;
        for (size_t i = 0; i < objects.size(); i++)
        {
            const Object& obj = objects[i];

            const auto& color = colors[color_index % 80];
            color_index++;

            fprintf(stdout, "%2d: %3.0f%%, [%4.0f, %4.0f, %4.0f, %4.0f], %s\n", obj.label, obj.prob * 100, obj.rect.x,
                    obj.rect.y, obj.rect.x + obj.rect.width, obj.rect.y + obj.rect.height, class_names[obj.label]);

            mask(cv::Rect((int)obj.rect.x, (int)obj.rect.y, (int)objects[i].rect.width, (int)objects[i].rect.height)).setTo(color, objects[i].mask);

            cv::rectangle(image, obj.rect, cv::Scalar(255, 0, 0));

            char text[256];
            sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);

            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

            int x = obj.rect.x;
            int y = obj.rect.y - label_size.height - baseLine;
            if (y < 0)
                y = 0;
            if (x + label_size.width > image.cols)
                x = image.cols - label_size.width;

            cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                          cv::Scalar(255, 255, 255), -1);

            cv::putText(image, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 0, 0));
        }
        float blended_alpha = 0.5;
        image = (1 - blended_alpha) * mask + blended_alpha * image;

        cv::imwrite(std::string(output_name) + ".jpg", image);
    }

    static void draw_objects_palm(const cv::Mat& bgr, const std::vector<PalmObject>& objects, const char* output_name)
    {
        cv::Mat image = bgr.clone();
        for (size_t i = 0; i < objects.size(); i++)
        {
            const PalmObject& obj = objects[i];
            //fprintf(stdout, "prob:%.2f, x0:%.2f, y0:%.2f, x1:%.2f, y1:%.2f, x2:%.2f, y2:%.2f, x3:%.2f, y3:%.2f\n", obj.prob,
            //        obj.vertices[0].x, obj.vertices[0].y, obj.vertices[1].x, obj.vertices[1].y, obj.vertices[2].x,
            //        obj.vertices[2].y, obj.vertices[3].x, obj.vertices[3].y);
            cv::line(image, obj.vertices[0], obj.vertices[1], cv::Scalar(0, 0, 255), 2, 8, 0);
            cv::line(image, obj.vertices[1], obj.vertices[2], cv::Scalar(0, 0, 255), 2, 8, 0);
            cv::line(image, obj.vertices[2], obj.vertices[3], cv::Scalar(0, 0, 255), 2, 8, 0);
            cv::line(image, obj.vertices[3], obj.vertices[0], cv::Scalar(0, 0, 255), 2, 8, 0);
            for (auto ld : obj.landmarks)
            {
                cv::circle(image, ld, 2, cv::Scalar(0, 255, 0), -1, 8);
            }
        }
        cv::imwrite(std::string(output_name) + ".jpg", image);
    }

    static void draw_objects_yolopv2(const cv::Mat& bgr, const std::vector<Object>& objects, const cv::Mat& da_seg_mask, const cv::Mat& ll_seg_mask, const char* output_name)
    {
        cv::Mat image = bgr.clone();
        cv::Mat mask = bgr.clone();
        for (size_t i = 0; i < objects.size(); i++)
        {
            const Object& obj = objects[i];

            fprintf(stdout, "%2d: %3.0f%%, [%4.0f, %4.0f, %4.0f, %4.0f]\n", obj.label, obj.prob * 100, obj.rect.x,
                    obj.rect.y, obj.rect.x + obj.rect.width, obj.rect.y + obj.rect.height);

            cv::rectangle(image, obj.rect, cv::Scalar(0, 255, 255), 2, 8, 0);
        }

        mask.setTo(cv::Scalar(0, 255, 0), da_seg_mask);
        mask.setTo(cv::Scalar(0, 0, 255), ll_seg_mask);
        float blended_alpha = 0.5;
        image = (1 - blended_alpha) * mask + blended_alpha * image;

        cv::imwrite(std::string(output_name) + ".jpg", image);
    }

    void reverse_letterbox(std::vector<Object>& proposal, std::vector<Object>& objects, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
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

        int count = proposal.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposal[i];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
        }
    }

    void get_out_bbox_no_letterbox(std::vector<Object>& proposals, std::vector<Object>& objects, const float nms_threshold, int model_h, int model_w, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        /* yolov5 draw the result */
        float ratio_x = (float)src_cols / (float)model_w;
        float ratio_y = (float)src_rows / (float)model_h;

        int count = picked.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0)*ratio_x;
            y0 = (y0)*ratio_y;
            x1 = (x1)*ratio_x;
            y1 = (y1)*ratio_y;

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
        }
    }

    void get_out_bbox(std::vector<Object>& objects, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        /* yolov5 draw the result */
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

        int count = objects.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            for (int l = 0; l < 5; l++)
            {
                auto lx = objects[i].landmark[l].x;
                auto ly = objects[i].landmark[l].y;
                objects[i].landmark[l] = cv::Point2f((lx - tmp_w) * ratio_x, (ly - tmp_h) * ratio_y);
            }

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
        }
    }

    void get_out_bbox(std::vector<Object>& proposals, std::vector<Object>& objects, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        /* yolov5 draw the result */
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

        int count = picked.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            for (int l = 0; l < 5; l++)
            {
                auto lx = objects[i].landmark[l].x;
                auto ly = objects[i].landmark[l].y;
                objects[i].landmark[l] = cv::Point2f((lx - tmp_w) * ratio_x, (ly - tmp_h) * ratio_y);
            }

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
        }
    }

    void get_out_bbox_mask(std::vector<Object>& proposals, std::vector<Object>& objects, const float* mask_proto, int mask_proto_dim, int mask_stride, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        /* yolov5 draw the result */
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

        int mask_proto_h = int(letterbox_rows / mask_stride);
        int mask_proto_w = int(letterbox_cols / mask_stride);

        int count = picked.size();
        objects.resize(count);

        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);
            /* naive RoiAlign by opencv */
            int hstart = std::floor(objects[i].rect.y / mask_stride);
            int hend = std::ceil(objects[i].rect.y / mask_stride + objects[i].rect.height / mask_stride);
            int wstart = std::floor(objects[i].rect.x / mask_stride);
            int wend = std::ceil(objects[i].rect.x / mask_stride + objects[i].rect.width / mask_stride);

            hstart = std::min(std::max(hstart, 0), mask_proto_h);
            wstart = std::min(std::max(wstart, 0), mask_proto_w);
            hend = std::min(std::max(hend, 0), mask_proto_h);
            wend = std::min(std::max(wend, 0), mask_proto_w);

            int mask_w = wend - wstart;
            int mask_h = hend - hstart;

            cv::Mat mask = cv::Mat(mask_h, mask_w, CV_32FC1);
            if (mask_w > 0 && mask_h > 0)
            {
                std::vector<cv::Range> roi_ranges;
                roi_ranges.push_back(cv::Range(0, 1));
                roi_ranges.push_back(cv::Range::all());
                roi_ranges.push_back(cv::Range(hstart, hend));
                roi_ranges.push_back(cv::Range(wstart, wend));

                cv::Mat mask_protos = cv::Mat(mask_proto_dim, mask_proto_h * mask_proto_w, CV_32FC1, (float*)mask_proto);
                int sz[] = {1, mask_proto_dim, mask_proto_h, mask_proto_w};
                cv::Mat mask_protos_reshape = mask_protos.reshape(1, 4, sz);
                cv::Mat protos = mask_protos_reshape(roi_ranges).clone().reshape(0, {mask_proto_dim, mask_w * mask_h});
                cv::Mat mask_proposals = cv::Mat(1, mask_proto_dim, CV_32FC1, (float*)objects[i].mask_feat.data());
                cv::Mat masks_feature = (mask_proposals * protos);
                /* sigmoid */
                cv::exp(-masks_feature.reshape(1, {mask_h, mask_w}), mask);
                mask = 1.0 / (1.0 + mask);
            }

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
            cv::resize(mask, mask, cv::Size((int)objects[i].rect.width, (int)objects[i].rect.height));
            objects[i].mask = mask > 0.5;
        }
    }

    void get_out_bbox_kps(std::vector<Object>& proposals, std::vector<Object>& objects, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        /* yolov8 draw the result */
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

        int count = picked.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;

            for (int j = 0; j < objects[i].kps_feat.size() / 3; j++)
            {
                objects[i].kps_feat[j * 3] = std::max(
                    std::min((objects[i].kps_feat[j * 3] - tmp_w) * ratio_x, (float)(src_cols - 1)), 0.f);
                objects[i].kps_feat[j * 3 + 1] = std::max(
                    std::min((objects[i].kps_feat[j * 3 + 1] - tmp_h) * ratio_y, (float)(src_rows - 1)), 0.f);
            }
        }
    }

    static void transform_rects_palm(PalmObject& object)
    {
        float x0 = object.landmarks[0].x;
        float y0 = object.landmarks[0].y;
        float x1 = object.landmarks[2].x;
        float y1 = object.landmarks[2].y;
        float rotation = M_PI * 0.5f - std::atan2(-(y1 - y0), x1 - x0);

        float hand_cx;
        float hand_cy;
        float shift_x = 0.0f;
        float shift_y = -0.5f;
        if (rotation == 0)
        {
            hand_cx = object.rect.x + object.rect.width * 0.5f + (object.rect.width * shift_x);
            hand_cy = object.rect.y + object.rect.height * 0.5f + (object.rect.height * shift_y);
        }
        else
        {
            float dx = (object.rect.width * shift_x) * std::cos(rotation) - (object.rect.height * shift_y) * std::sin(rotation);
            float dy = (object.rect.width * shift_x) * std::sin(rotation) + (object.rect.height * shift_y) * std::cos(rotation);
            hand_cx = object.rect.x + object.rect.width * 0.5f + dx;
            hand_cy = object.rect.y + object.rect.height * 0.5f + dy;
        }

        float long_side = (std::max)(object.rect.width, object.rect.height);
        float dx = long_side * 1.3f;
        float dy = long_side * 1.3f;

        object.vertices[0].x = -dx;
        object.vertices[0].y = -dy;
        object.vertices[1].x = +dx;
        object.vertices[1].y = -dy;
        object.vertices[2].x = +dx;
        object.vertices[2].y = +dy;
        object.vertices[3].x = -dx;
        object.vertices[3].y = +dy;

        for (int i = 0; i < 4; i++)
        {
            float sx = object.vertices[i].x;
            float sy = object.vertices[i].y;
            object.vertices[i].x = sx * std::cos(rotation) - sy * std::sin(rotation);
            object.vertices[i].y = sx * std::sin(rotation) + sy * std::cos(rotation);
            object.vertices[i].x += hand_cx;
            object.vertices[i].y += hand_cy;
        }
    }

    static void get_out_bbox_palm(std::vector<PalmObject>& proposals, std::vector<PalmObject>& objects, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        int count = picked.size();
        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            transform_rects_palm(objects[i]);
        }

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

        float ratio_x = (float)src_cols / resize_cols;
        float ratio_y = (float)src_rows / resize_rows;

        for (auto& object : objects)
        {
            for (auto& vertice : object.vertices)
            {
                vertice.x = (vertice.x * letterbox_cols - tmp_w) * ratio_x;
                vertice.y = (vertice.y * letterbox_rows - tmp_h) * ratio_y;
            }

            for (auto& ld : object.landmarks)
            {
                ld.x = (ld.x * letterbox_cols - tmp_w) * ratio_x;
                ld.y = (ld.y * letterbox_rows - tmp_h) * ratio_y;
            }
            // get warpaffine transform mat to landmark detect
            cv::Point2f src_pts[4];
            src_pts[0] = object.vertices[0];
            src_pts[1] = object.vertices[1];
            src_pts[2] = object.vertices[2];
            src_pts[3] = object.vertices[3];

            cv::Point2f dst_pts[4];
            dst_pts[0] = cv::Point2f(0, 0);
            dst_pts[1] = cv::Point2f(224, 0);
            dst_pts[2] = cv::Point2f(224, 224);
            dst_pts[3] = cv::Point2f(0, 224);

            object.affine_trans_mat = cv::getAffineTransform(src_pts, dst_pts);
            cv::invertAffineTransform(object.affine_trans_mat, object.affine_trans_mat_inv);
        }
    }

    void get_out_bbox_yolopv2(std::vector<Object>& proposals, std::vector<Object>& objects, const float* da_ptr, const float* ll_ptr, cv::Mat& ll_seg_mask, cv::Mat& da_seg_mask, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
    {
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

        /* yolov5 draw the result */
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

        int count = picked.size();

        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
            objects[i] = proposals[picked[i]];
            float x0 = (objects[i].rect.x);
            float y0 = (objects[i].rect.y);
            float x1 = (objects[i].rect.x + objects[i].rect.width);
            float y1 = (objects[i].rect.y + objects[i].rect.height);

            x0 = (x0 - tmp_w) * ratio_x;
            y0 = (y0 - tmp_h) * ratio_y;
            x1 = (x1 - tmp_w) * ratio_x;
            y1 = (y1 - tmp_h) * ratio_y;

            x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
            y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
            x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
            y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

            objects[i].rect.x = x0;
            objects[i].rect.y = y0;
            objects[i].rect.width = x1 - x0;
            objects[i].rect.height = y1 - y0;
        }

        cv::Mat ll = cv::Mat(cv::Size(letterbox_cols, letterbox_rows), CV_32FC1, (float*)ll_ptr);
        ll = ll > 0.5;
        cv::resize(ll(cv::Rect(tmp_w, tmp_h, resize_cols, resize_rows)), ll_seg_mask, cv::Size(src_cols, src_rows), 0, 0, cv::INTER_LINEAR);

        cv::Mat da = cv::Mat(cv::Size(letterbox_cols, letterbox_rows), CV_32FC1, (float*)da_ptr);
        da = da > 0;
        cv::resize(da(cv::Rect(tmp_w, tmp_h, resize_cols, resize_rows)), da_seg_mask, cv::Size(src_cols, src_rows), 0, 0, cv::INTER_NEAREST);
    }

    namespace mmyolo
    {
        inline static float clamp(
            float val,
            float min = 0.f,
            float max = 1536.f)
        {
            return val > min ? (val < max ? val : max) : min;
        }

        inline float fast_exp(const float& x)
        {
            union
            {
                uint32_t i;
                float f;
            } v{};
            v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);
            return v.f;
        }

        inline float fast_sigmoid(const float& x)
        {
            return 1.0f / (1.0f + fast_exp(-x));
        }

        inline static float fast_softmax(
            const float* src,
            float* dst,
            int length)
        {
            const float alpha = *std::max_element(src, src + length);
            float denominator = 0;
            float dis_sum = 0;
            for (int i = 0; i < length; ++i)
            {
                dst[i] = fast_exp(src[i] - alpha);
                denominator += dst[i];
            }
            for (int i = 0; i < length; ++i)
            {
                dst[i] /= denominator;
                dis_sum += i * dst[i];
            }
            return dis_sum;
        }

        static void generate_proposals_ppyoloeplus(
            int stride,
            const float* cls_feat,
            const float* box_feat,
            float prob_threshold,
            std::vector<Object>& objects,
            int letterbox_cols,
            int letterbox_rows,
            int cls_num = 80)
        {
            int feat_w = letterbox_cols / stride;
            int feat_h = letterbox_rows / stride;
            auto cls_ptr = cls_feat;
            auto boxes_ptr = box_feat;
            int reg_max = 17;
            float dis_after_sm[reg_max];

            for (int h = 0; h < feat_h; h++)
            {
                for (int w = 0; w < feat_w; w++)
                {
                    auto max = std::max_element(cls_ptr, cls_ptr + cls_num);
                    float box_prob = fast_sigmoid(*max);

                    if (box_prob > prob_threshold)
                    {
                        float x0 = w + 0.5f - fast_softmax(boxes_ptr, dis_after_sm, reg_max);
                        float y0 = h + 0.5f - fast_softmax(boxes_ptr + reg_max, dis_after_sm, reg_max);
                        float x1 = w + 0.5f + fast_softmax(boxes_ptr + 2 * reg_max, dis_after_sm, reg_max);
                        float y1 = h + 0.5f + fast_softmax(boxes_ptr + 3 * reg_max, dis_after_sm, reg_max);

                        x0 *= stride;
                        y0 *= stride;
                        x1 *= stride;
                        y1 *= stride;

                        x0 = clamp(x0, 0.f, letterbox_cols - 1);
                        y0 = clamp(y0, 0.f, letterbox_rows - 1);
                        x1 = clamp(x1, 0.f, letterbox_cols - 1);
                        y1 = clamp(y1, 0.f, letterbox_rows - 1);

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = max - cls_ptr;
                        obj.prob = box_prob;

                        objects.push_back(obj);
                    }
                    cls_ptr += cls_num;
                    boxes_ptr += 4 * reg_max;
                }
            }
        }

        static void generate_proposals_yolox(
            int stride,
            const float* cls_feat,
            const float* box_feat,
            const float* conf_feat,
            float prob_threshold,
            std::vector<Object>& objects,
            int letterbox_cols,
            int letterbox_rows,
            int cls_num = 80)
        {
            int feat_w = letterbox_cols / stride;
            int feat_h = letterbox_rows / stride;
            auto cls_ptr = cls_feat;
            auto boxes_ptr = box_feat;
            auto conf_ptr = conf_feat;

            for (int h = 0; h < feat_h; h++)
            {
                for (int w = 0; w < feat_w; w++)
                {
                    //process cls score
                    auto max = std::max_element(cls_ptr, cls_ptr + cls_num);
                    float box_prob = fast_sigmoid(*max) * fast_sigmoid(*conf_ptr);

                    if (box_prob > prob_threshold)
                    {
                        float x = (w + boxes_ptr[0]) * stride;
                        float y = (h + boxes_ptr[1]) * stride;

                        float width = fast_exp(boxes_ptr[2]) * stride;
                        float height = fast_exp(boxes_ptr[3]) * stride;

                        float x0 = x - width * 0.5f;
                        float y0 = y - height * 0.5f;

                        x0 = clamp(x0, 0.f, letterbox_cols - 1);
                        y0 = clamp(y0, 0.f, letterbox_rows - 1);

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = width;
                        obj.rect.height = height;
                        obj.label = max - cls_ptr;
                        obj.prob = box_prob;

                        objects.push_back(obj);
                    }
                    cls_ptr += cls_num;
                    boxes_ptr += 4;
                    conf_ptr++;
                }
            }
        }

        static void generate_proposals_yolov6(
            int stride,
            const float* cls_feat,
            const float* box_feat,
            float prob_threshold,
            std::vector<Object>& objects,
            int letterbox_cols,
            int letterbox_rows,
            int cls_num = 80)
        {
            int feat_w = letterbox_cols / stride;
            int feat_h = letterbox_rows / stride;
            auto cls_ptr = cls_feat;
            auto boxes_ptr = box_feat;

            for (int h = 0; h < feat_h; h++)
            {
                for (int w = 0; w < feat_w; w++)
                {
                    //process cls score
                    auto max = std::max_element(cls_ptr, cls_ptr + cls_num);
                    float box_prob = fast_sigmoid(*max);

                    if (box_prob > prob_threshold)
                    {
                        float x0 = (w + 0.5f - boxes_ptr[0]) * stride;
                        float y0 = (h + 0.5f - boxes_ptr[1]) * stride;
                        float x1 = (w + 0.5f + boxes_ptr[2]) * stride;
                        float y1 = (h + 0.5f + boxes_ptr[3]) * stride;

                        x0 = clamp(x0, 0.f, letterbox_cols - 1);
                        y0 = clamp(y0, 0.f, letterbox_rows - 1);
                        x1 = clamp(x1, 0.f, letterbox_cols - 1);
                        y1 = clamp(y1, 0.f, letterbox_rows - 1);

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = max - cls_ptr;
                        obj.prob = box_prob;

                        objects.push_back(obj);
                    }
                    cls_ptr += cls_num;
                    boxes_ptr += 4;
                }
            }
        }

        static void generate_proposals_yolov8(
            int stride,
            const float* cls_feat,
            const float* box_feat,
            float prob_threshold,
            std::vector<Object>& objects,
            int letterbox_cols,
            int letterbox_rows,
            int cls_num = 80)
        {
            int feat_w = letterbox_cols / stride;
            int feat_h = letterbox_rows / stride;
            auto cls_ptr = cls_feat;
            auto boxes_ptr = box_feat;
            int reg_max = 16;
            float dis_after_sm[reg_max];

            for (int h = 0; h < feat_h; h++)
            {
                for (int w = 0; w < feat_w; w++)
                {
                    auto max = std::max_element(cls_ptr, cls_ptr + cls_num);
                    float box_prob = fast_sigmoid(*max);

                    if (box_prob > prob_threshold)
                    {
                        float x0 = w + 0.5f - fast_softmax(boxes_ptr, dis_after_sm, reg_max);
                        float y0 = h + 0.5f - fast_softmax(boxes_ptr + reg_max, dis_after_sm, reg_max);
                        float x1 = w + 0.5f + fast_softmax(boxes_ptr + 2 * reg_max, dis_after_sm, reg_max);
                        float y1 = h + 0.5f + fast_softmax(boxes_ptr + 3 * reg_max, dis_after_sm, reg_max);

                        x0 *= stride;
                        y0 *= stride;
                        x1 *= stride;
                        y1 *= stride;

                        x0 = clamp(x0, 0.f, letterbox_cols - 1);
                        y0 = clamp(y0, 0.f, letterbox_rows - 1);
                        x1 = clamp(x1, 0.f, letterbox_cols - 1);
                        y1 = clamp(y1, 0.f, letterbox_rows - 1);

                        Object obj;
                        obj.rect.x = x0;
                        obj.rect.y = y0;
                        obj.rect.width = x1 - x0;
                        obj.rect.height = y1 - y0;
                        obj.label = max - cls_ptr;
                        obj.prob = box_prob;

                        objects.push_back(obj);
                    }
                    cls_ptr += cls_num;
                    boxes_ptr += 4 * reg_max;
                }
            }
        }
    } // namespace mmyolo

} // namespace detection
