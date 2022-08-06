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

namespace yolo
{
    enum
    {
        YOLOV3 = 0,
        YOLOV3_TINY = 1,
        YOLOV4 = 2,
        YOLOV4_TINY = 3,
        YOLO_FASTEST = 4,
        YOLO_FASTEST_XL = 5,
        YOLO_FASTEST_BODY = 6,
        YOLOV4_TINY_3L = 7
    };

    struct BBoxRect
    {
        float score;
        float xmin;
        float ymin;
        float xmax;
        float ymax;
        float area;
        int label;
    };

    static inline float sigmoid(float x)
    {
        return (float)(1.f / (1.f + std::exp(-x)));
    }
    static inline float intersection_area(const BBoxRect& a, const BBoxRect& b)
    {
        if (a.xmin > b.xmax || a.xmax < b.xmin || a.ymin > b.ymax || a.ymax < b.ymin)
        {
            // no intersection
            return 0.f;
        }

        float inter_width = std::min(a.xmax, b.xmax) - std::max(a.xmin, b.xmin);
        float inter_height = std::min(a.ymax, b.ymax) - std::max(a.ymin, b.ymin);

        return inter_width * inter_height;
    }
    static void qsort_descent_inplace(std::vector<BBoxRect>& datas, int left, int right)
    {
        int i = left;
        int j = right;
        float p = datas[(left + right) / 2].score;

        while (i <= j)
        {
            while (datas[i].score > p)
                i++;

            while (datas[j].score < p)
                j--;

            if (i <= j)
            {
                // swap
                std::swap(datas[i], datas[j]);

                i++;
                j--;
            }
        }

        if (left < j)
            qsort_descent_inplace(datas, left, j);

        if (i < right)
            qsort_descent_inplace(datas, i, right);
    }

    static void qsort_descent_inplace(std::vector<BBoxRect>& datas)
    {
        if (datas.empty())
            return;

        qsort_descent_inplace(datas, 0, (int)(datas.size() - 1));
    }

    static void nms_sorted_bboxes(std::vector<BBoxRect>& bboxes, std::vector<size_t>& picked, float nms_threshold)
    {
        picked.clear();

        const size_t n = bboxes.size();

        for (size_t i = 0; i < n; i++)
        {
            const BBoxRect& a = bboxes[i];

            int keep = 1;
            for (unsigned int j : picked)
            {
                const BBoxRect& b = bboxes[j];

                // intersection over union
                float inter_area = intersection_area(a, b);
                float union_area = a.area + b.area - inter_area;
                // float IoU = inter_area / union_area
                if (inter_area > nms_threshold * union_area)
                {
                    keep = 0;
                    break;
                }
            }

            if (keep)
                picked.push_back(i);
        }
    }

    struct TMat
    {
        operator const float*() const
        {
            return (const float*)data;
        }

        float* row(int row) const
        {
            return (float*)data + w * row;
        }

        TMat channel_range(int start, int chn_num) const
        {
            TMat mat = {0};

            mat.batch = 1;
            mat.c = chn_num;
            mat.h = h;
            mat.w = w;
            mat.data = (float*)data + start * h * w;

            return mat;
        }

        TMat channel(int channel) const
        {
            return channel_range(channel, 1);
        }

        int batch, c, h, w;
        void* data;
    };

    class YoloDetectionOutput
    {
    public:
        int init(int version, float nms_threshold = 0.45f, float confidence_threshold = 0.48f, int class_num = 80);
        int forward(const std::vector<TMat>& bottom_blobs, std::vector<TMat>& top_blobs);
        int forward_nhwc(const std::vector<TMat>& bottom_blobs, std::vector<TMat>& top_blobs);

    private:
        int m_num_box;
        int m_num_class;
        int m_anchors_scale[32];
        float m_biases[32];
        int m_mask[32];
        float m_confidence_threshold;
        float m_confidence_threshold_unsigmoid;
        float m_nms_threshold;
    };

    int YoloDetectionOutput::init(int version, float nms_threshold, float confidence_threshold, int class_num)
    {
        memset(this, 0, sizeof(*this));
        m_num_box = 3;
        m_num_class = class_num;
        fprintf(stderr, "YoloDetectionOutput init param[%d]\n", version);

        if (version == YOLOV3)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;
            m_anchors_scale[2] = 8;

            float bias[] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};
            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 6;
            m_mask[1] = 7;
            m_mask[2] = 8;

            m_mask[3] = 3;
            m_mask[4] = 4;
            m_mask[5] = 5;

            m_mask[6] = 0;
            m_mask[7] = 1;
            m_mask[8] = 2;
        }
        else if (version == YOLOV3_TINY || version == YOLOV4_TINY)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;

            float bias[] = {10, 14, 23, 27, 37, 58, 81, 82, 135, 169, 344, 319};
            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 3;
            m_mask[1] = 4;
            m_mask[2] = 5;

            m_mask[3] = 0;
            m_mask[4] = 1;
            m_mask[5] = 2;
        }
        else if (version == YOLOV4)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;
            m_anchors_scale[2] = 8;

            float bias[] = {12, 16, 19, 36, 40, 28, 36, 75, 76, 55, 72, 146, 142, 110, 192, 243, 459, 401};
            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 6;
            m_mask[1] = 7;
            m_mask[2] = 8;

            m_mask[3] = 3;
            m_mask[4] = 4;
            m_mask[5] = 5;

            m_mask[6] = 0;
            m_mask[7] = 1;
            m_mask[8] = 2;
        }
        else if (version == YOLO_FASTEST || version == YOLO_FASTEST_XL)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;

            float bias[] = {12, 18, 37, 49, 52, 132, 115, 73, 119, 199, 242, 238};
            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 3;
            m_mask[1] = 4;
            m_mask[2] = 5;

            m_mask[3] = 0;
            m_mask[4] = 1;
            m_mask[5] = 2;
        }
        else if (version == YOLO_FASTEST_BODY)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;

            float bias[] = {7, 17, 20, 50, 45, 99, 64, 187, 123, 211, 227, 264};
            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 3;
            m_mask[1] = 4;
            m_mask[2] = 5;

            m_mask[3] = 0;
            m_mask[4] = 1;
            m_mask[5] = 2;
        }
        else if (version == YOLOV4_TINY_3L)
        {
            m_anchors_scale[0] = 32;
            m_anchors_scale[1] = 16;
            m_anchors_scale[2] = 8;

            //float bias_official[] = {12, 16, 19, 36, 40, 28,   36, 75, 76, 55, 72, 146,   142, 110, 192, 243, 459, 401};
            float bias[] = {10, 14, 23, 27, 37, 58, 36, 75, 76, 55, 72, 146, 81, 82, 135, 169, 344, 319};

            memcpy(m_biases, bias, sizeof(bias));

            m_mask[0] = 6;
            m_mask[1] = 7;
            m_mask[2] = 8;

            m_mask[3] = 3;
            m_mask[4] = 4;
            m_mask[5] = 5;

            m_mask[6] = 0;
            m_mask[7] = 1;
            m_mask[8] = 2;
        }

        m_confidence_threshold = confidence_threshold;
        m_nms_threshold = nms_threshold;
        m_confidence_threshold_unsigmoid = -1.0f * (float)std::log((1.0f / m_confidence_threshold) - 1.0f);

        return 0;
    }

    int YoloDetectionOutput::forward_nhwc(const std::vector<TMat>& bottom_blobs, std::vector<TMat>& top_blobs)
    {
        // gather all box
        std::vector<BBoxRect> all_bbox_rects;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const TMat& bottom_top_blobs = bottom_blobs[b];

            int w = bottom_top_blobs.w;
            int h = bottom_top_blobs.h;
            size_t mask_offset = b * m_num_box;
            int net_w = (int)(m_anchors_scale[b] * w);
            int net_h = (int)(m_anchors_scale[b] * h);

            auto feature_ptr = (float*)bottom_top_blobs.data;
            for (int i = 0; i < h; i++)
            {
                for (int j = 0; j < w; j++)
                {
                    for (int box = 0; box < m_num_box; ++box)
                    {
                        if (feature_ptr[4] < m_confidence_threshold_unsigmoid)
                        {
                            feature_ptr += (m_num_class + 5);
                            continue;
                        }

                        int class_index = 0;
                        float class_score = -FLT_MAX;
                        for (int k = 5; k < m_num_class + 5; ++k)
                        {
                            if (class_score < feature_ptr[k])
                            {
                                class_score = feature_ptr[k];
                                class_index = k - 5;
                            }
                        }

                        //sigmoid(box_score) * sigmoid(class_score)
                        float confidence_1 = 1.0f / ((1.f + std::exp(-feature_ptr[4])) * (1.f + std::exp(-class_score)));
                        if (confidence_1 >= m_confidence_threshold)
                        {
                            int biases_index = (int)(m_mask[box + mask_offset]);
                            const float bias_w = m_biases[biases_index * 2];
                            const float bias_h = m_biases[biases_index * 2 + 1];

                            // region box
                            // fprintf(stderr, "%f %f %d \n", class_score, feature_ptr[4], class_index);
                            float bbox_cx = ((float)j + sigmoid(feature_ptr[0])) / (float)w;
                            float bbox_cy = ((float)i + sigmoid(feature_ptr[1])) / (float)h;
                            auto bbox_w = (float)(std::exp(feature_ptr[2]) * bias_w / (float)net_w);
                            auto bbox_h = (float)(std::exp(feature_ptr[3]) * bias_h / (float)net_h);

                            float bbox_xmin = bbox_cx - bbox_w * 0.5f;
                            float bbox_ymin = bbox_cy - bbox_h * 0.5f;
                            float bbox_xmax = bbox_cx + bbox_w * 0.5f;
                            float bbox_ymax = bbox_cy + bbox_h * 0.5f;

                            float area = bbox_w * bbox_h;

                            BBoxRect c = {confidence_1, bbox_xmin, bbox_ymin, bbox_xmax, bbox_ymax, area, class_index};
                            all_bbox_rects.push_back(c);
                        }

                        feature_ptr += (m_num_class + 5);
                    }
                }
            }
        }

        // global sort inplace
        qsort_descent_inplace(all_bbox_rects);

        // apply nms
        std::vector<size_t> picked;
        nms_sorted_bboxes(all_bbox_rects, picked, m_nms_threshold);

        // select
        std::vector<BBoxRect> bbox_rects;

        for (unsigned int z : picked)
        {
            bbox_rects.push_back(all_bbox_rects[z]);
        }

        // fill result
        int num_detected = (int)(bbox_rects.size());
        if (num_detected == 0)
        {
            top_blobs[0].h = 0;
            return 0;
        }

        TMat& top_blob = top_blobs[0];

        for (int i = 0; i < num_detected; i++)
        {
            const BBoxRect& r = bbox_rects[i];
            float score = r.score;
            float* outptr = top_blob.row(i);

            outptr[0] = (float)r.label; // +1 for prepend background class
            outptr[1] = score;
            outptr[2] = r.xmin;
            outptr[3] = r.ymin;
            outptr[4] = r.xmax;
            outptr[5] = r.ymax;
        }
        top_blob.h = num_detected;
        return 0;
    }

    int YoloDetectionOutput::forward(const std::vector<TMat>& bottom_blobs, std::vector<TMat>& top_blobs)
    {
        // gather all box
        std::vector<BBoxRect> all_bbox_rects;

        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            std::vector<std::vector<BBoxRect> > all_box_bbox_rects;
            all_box_bbox_rects.resize(m_num_box);
            const TMat& bottom_top_blobs = bottom_blobs[b];

            int w = bottom_top_blobs.w;
            int h = bottom_top_blobs.h;
            int channels = bottom_top_blobs.c;
            //printf("%d %d %d\n", w, h, channels);
            const int channels_per_box = channels / m_num_box;

            // anchor coord + box score + num_class
            if (channels_per_box != 4 + 1 + m_num_class)
                return -1;
            size_t mask_offset = b * m_num_box;
            int net_w = (int)(m_anchors_scale[b] * w);
            int net_h = (int)(m_anchors_scale[b] * h);
            //printf("%d %d\n", net_w, net_h);

            //printf("%d %d %d\n", w, h, channels);
            for (int pp = 0; pp < m_num_box; pp++)
            {
                int p = pp * channels_per_box;
                int biases_index = (int)(m_mask[pp + mask_offset]);
                //printf("%d\n", biases_index);
                const float bias_w = m_biases[biases_index * 2];
                const float bias_h = m_biases[biases_index * 2 + 1];
                //printf("%f %f\n", bias_w, bias_h);
                const float* xptr = bottom_top_blobs.channel(p);
                const float* yptr = bottom_top_blobs.channel(p + 1);
                const float* wptr = bottom_top_blobs.channel(p + 2);
                const float* hptr = bottom_top_blobs.channel(p + 3);

                const float* box_score_ptr = bottom_top_blobs.channel(p + 4);

                // softmax class scores
                TMat scores = bottom_top_blobs.channel_range(p + 5, m_num_class);
                //softmax->forward_inplace(scores, opt);

                for (int i = 0; i < h; i++)
                {
                    for (int j = 0; j < w; j++)
                    {
                        // find class index with max class score
                        int class_index = 0;
                        float class_score = -FLT_MAX;
                        for (int q = 0; q < m_num_class; q++)
                        {
                            float score = scores.channel(q).row(i)[j];
                            if (score > class_score)
                            {
                                class_index = q;
                                class_score = score;
                            }
                        }

                        //sigmoid(box_score) * sigmoid(class_score)
                        float confidence = (float)1.f / ((1.f + std::exp(-box_score_ptr[0]) * (1.f + std::exp(-class_score))));
                        if (confidence >= m_confidence_threshold)
                        {
                            // fprintf(stderr, "%f %d \n", class_score, class_index);
                            // region box
                            float bbox_cx = ((float)j + sigmoid(xptr[0])) / (float)w;
                            float bbox_cy = ((float)i + sigmoid(yptr[0])) / (float)h;
                            auto bbox_w = (float)(std::exp(wptr[0]) * bias_w / (float)net_w);
                            auto bbox_h = (float)(std::exp(hptr[0]) * bias_h / (float)net_h);

                            float bbox_xmin = bbox_cx - bbox_w * 0.5f;
                            float bbox_ymin = bbox_cy - bbox_h * 0.5f;
                            float bbox_xmax = bbox_cx + bbox_w * 0.5f;
                            float bbox_ymax = bbox_cy + bbox_h * 0.5f;

                            float area = bbox_w * bbox_h;

                            BBoxRect c = {confidence, bbox_xmin, bbox_ymin, bbox_xmax, bbox_ymax, area, class_index};
                            all_box_bbox_rects[pp].push_back(c);
                        }

                        xptr++;
                        yptr++;
                        wptr++;
                        hptr++;

                        box_score_ptr++;
                    }
                }
            }

            for (int i = 0; i < m_num_box; i++)
            {
                const std::vector<BBoxRect>& box_bbox_rects = all_box_bbox_rects[i];

                all_bbox_rects.insert(all_bbox_rects.end(), box_bbox_rects.begin(), box_bbox_rects.end());
            }
        }

        // global sort inplace
        qsort_descent_inplace(all_bbox_rects);

        // apply nms
        std::vector<size_t> picked;
        nms_sorted_bboxes(all_bbox_rects, picked, m_nms_threshold);

        // select
        std::vector<BBoxRect> bbox_rects;

        for (unsigned int z : picked)
        {
            bbox_rects.push_back(all_bbox_rects[z]);
        }

        // fill result
        int num_detected = (int)(bbox_rects.size());
        if (num_detected == 0)
        {
            top_blobs[0].h = 0;
            return 0;
        }

        TMat& top_blob = top_blobs[0];

        for (int i = 0; i < num_detected; i++)
        {
            const BBoxRect& r = bbox_rects[i];
            float score = r.score;
            float* outptr = top_blob.row(i);

            outptr[0] = (float)r.label; // +1 for prepend background class
            outptr[1] = score;
            outptr[2] = r.xmin;
            outptr[3] = r.ymin;
            outptr[4] = r.xmax;
            outptr[5] = r.ymax;
        }
        top_blob.h = num_detected;

        return 0;
    }

} // namespace yolo
