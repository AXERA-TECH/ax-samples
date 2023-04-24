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

if (AXERA_TARGET_CHIP MATCHES "ax650")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libopencv-4.5.5-aarch64/lib/cmake/opencv4)

    find_package(Threads)
    find_package(OpenCV REQUIRED)

    include("${CMAKE_SOURCE_DIR}/cmake/ax650.cmake")

    axera_example(ax_yolov5s ax_yolov5s_steps.cc)
    axera_example(ax_classification ax_classification_steps.cc)
    axera_example(ax_ppyoloe ax_ppyoloe_steps.cc)
    axera_example(ax_ppyoloe_obj365 ax_ppyoloe_obj365_steps.cc)
    axera_example(ax_pp_person_attribute ax_pp_person_attribute_steps.cc)
    axera_example(ax_pp_vehicle_attribute ax_pp_vehicle_attribute_steps.cc)
    axera_example(ax_pp_humanseg ax_pp_humanseg_steps.cc)
    axera_example(ax_pp_liteseg_stdc2_cityscapes ax_pp_liteseg_stdc2_cityscapes_steps.cc)
    axera_example(ax_pp_ocr_rec ax_pp_ocr_rec_steps.cc)
    axera_example(ax_realesrgan ax_realesrgan_steps.cc)
    axera_example(ax_yolov8 ax_yolov8s_steps.cc)
    axera_example(ax_detr ax_detr_steps.cc)
endif ()
