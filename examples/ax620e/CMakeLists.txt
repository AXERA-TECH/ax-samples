# AXERA is pleased to support the open source community by making ax-samples available.
#
# Copyright (c) 2022, AXERA Semiconductor (Shanghai) Co., Ltd. All rights reserved.
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
# Author:
#

find_package(Threads)
find_package(OpenCV REQUIRED)

include("${CMAKE_SOURCE_DIR}/cmake/ax650.cmake")

axera_example(ax_classification ax_classification_steps.cc)

axera_example(ax_yolov5s ax_yolov5s_steps.cc)
axera_example(ax_yolov5s_seg ax_yolov5s_seg_steps.cc)
axera_example(ax_yolov5_face ax_yolov5_face_steps.cc)
axera_example(ax_yolov6 ax_yolov6_steps.cc)
axera_example(ax_yolov7_tiny_face ax_yolov7_tiny_face_steps.cc)
axera_example(ax_yolov7 ax_yolov7_steps.cc)
axera_example(ax_yolov8 ax_yolov8s_steps.cc)
axera_example(ax_yolov8_seg ax_yolov8_seg_steps.cc)
axera_example(ax_yolov8_pose ax_yolov8s_pose_steps.cc)
axera_example(ax_yolox ax_yolox_steps.cc)

axera_example(ax_scrfd ax_scrfd_steps.cc)
axera_example(ax_simcc_pose ax_simcc_pose_steps.cc)

axera_example(ax_imgproc ax_imgproc_steps.cc)
axera_example(ax_model_info ax_model_info.cc)
