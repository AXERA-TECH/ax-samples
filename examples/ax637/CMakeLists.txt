# AXERA is pleased to support the open source community by making ax-samples available.
#
# Copyright (c) 2025, AXERA Semiconductor Co., Ltd. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
# in compliance with the License. You may obtain a copy of the License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
#
# Author: GUOFANGMING
#

find_package(Threads)
find_package(OpenCV REQUIRED)

include("${CMAKE_SOURCE_DIR}/cmake/ax637.cmake")

axera_example(ax_classification ax_classification_steps.cc)

axera_example(ax_yolov5s ax_yolov5s_steps.cc)
axera_example(ax_yolov5s_seg ax_yolov5s_seg_steps.cc)
axera_example(ax_yolov5_face ax_yolov5_face_steps.cc)

axera_example(ax_yolov8 ax_yolov8_steps.cc)
axera_example(ax_yolov8_seg ax_yolov8_seg_steps.cc)
axera_example(ax_yolov8_pose ax_yolov8_pose_steps.cc)

axera_example(ax_yolo11 ax_yolo11_steps.cc)
axera_example(ax_yolo11_seg ax_yolo11_seg_steps.cc)
axera_example(ax_yolo11_pose ax_yolo11_pose_steps.cc)
axera_example(ax_yolo11n_classification ax_yolo11n_classification_steps.cc)

axera_example(ax_depth_anything ax_depth_anythingv2_steps.cc)
axera_example(ax_deep_lab_v3_plus ax_deep_lab_v3_plus_steps.cc)

axera_example(ax_superpoint ax_superpoint_steps.cc)