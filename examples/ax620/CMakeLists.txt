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
# Author: ls.wang
#

# example generation function
# function (axera_example name)
# add_executable(${name})

# # add srcs
# foreach (file IN LISTS ARGN)
# target_sources (${name} PRIVATE ${file})
# endforeach()

# # headers
# target_include_directories (${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# target_include_directories (${name} PRIVATE ${BSP_MSP_DIR}/include)
# if (${AXERA_TARGET_PROCESSOR} MATCHES "ARM" AND AXERA_TARGET_PROCESSOR_32Bit)
# target_include_directories (${name} PRIVATE /opt/include)
# endif()

# # libs
# if (AXERA_TARGET_CHIP MATCHES "ax620a")   # ax620 support
# target_link_libraries (${name} PRIVATE pthread dl) # ax620a use this
# # target_link_libraries (${name} PRIVATE pthread dl stdc++fs) # ax620u use this
# target_link_libraries (${name} PRIVATE ax_run_joint ax_interpreter_external ax_interpreter ax_sys axsyslog stdc++fs)
# else()  # ax630a support
# target_link_libraries (${name} PRIVATE pthread dl stdc++fs)
# target_link_libraries (${name} PRIVATE ax_run_joint ax_interpreter_external ax_interpreter ax_sys)
# endif()
# target_link_libraries (${name} PRIVATE ax_ivps ax_npu_cv_kit)

# # folders
# target_link_directories (${name} PRIVATE ${BSP_MSP_DIR}/lib)
# if (${AXERA_TARGET_PROCESSOR} MATCHES "ARM" AND AXERA_TARGET_PROCESSOR_32Bit)
# target_link_libraries (${name} PRIVATE dl)
# target_link_directories (${name} PRIVATE /opt/lib)
# endif()

# # opencv
# target_include_directories (${name} PRIVATE ${OpenCV_INCLUDE_DIRS})
# target_link_libraries(${name} PRIVATE ${OpenCV_LIBS})

# install (TARGETS ${name} DESTINATION bin)
# endfunction()

if(AXERA_TARGET_CHIP MATCHES "ax620a") # ax620 support
    include("${CMAKE_SOURCE_DIR}/cmake/ax620.cmake")

    axera_example(ax_classification ax_classification_steps.cc)
    axera_example(ax_classification_nv12 ax_classification_nv12_resize_steps.cc)
    axera_example(ax_classification_nv12_opt ax_classification_nv12_resize_opt.cc)
    axera_example(ax_yolov5s ax_yolov5s_steps.cc)
    axera_example(ax_yolov5_lite ax_yolov5_lite_steps.cc)
    axera_example(ax_yolov5s_face ax_yolov5s_face_steps.cc)
    axera_example(ax_yolov5s_visdrone ax_yolov5s_visdrone_steps.cc)
    axera_example(ax_yoloxs ax_yoloxs_steps.cc)
    axera_example(ax_yolo_fastest ax_yolo_fastest_steps.cc)
    axera_example(ax_yolo_fastest_body ax_yolo_fastest_body_steps.cc)
    axera_example(ax_hrnet ax_hrnet_steps.cc)
    axera_example(ax_hrnet_animal ax_hrnet_animal_steps.cc)
    axera_example(ax_yolov3 ax_yolov3_steps.cc)
    axera_example(ax_yolov3_tiny ax_yolov3_tiny_steps.cc)
    axera_example(ax_yolov4 ax_yolov4_steps.cc)
    axera_example(ax_yolov4_tiny ax_yolov4_tiny_steps.cc)
    axera_example(ax_yolov4_tiny_3l ax_yolov4_tiny_3l_steps.cc)
    axera_example(ax_nanodet ax_nanodet_steps.cc)
    axera_example(ax_paddle_yolov3 ax_paddle_yolov3_steps.cc)
    axera_example(ax_scrfd ax_scrfd_steps.cc)
    axera_example(ax_pose_ppl ax_pose_ppl_steps.cc)
    axera_example(ax_yolov5s_license_plate ax_yolov5s_license_plate_steps.cc)

    axera_example(ax_classification_accuracy ax_classification_accuracy.cc)
    axera_example(ax_yolov3_accuracy ax_yolov3_accuracy.cc)

    axera_example(ax_yolov5s_620u ax_yolov5s_620u_steps.cc)
    axera_example(ax_robot_obstacle ax_robot_obstacle_detect_steps.cc)
    axera_example(ax_yolov7 ax_yolov7_steps.cc)
    axera_example(ax_models_load_inspect ax_models_load_inspect.cc)
    axera_example(ax_paddle_mobilehumseg ax_paddle_mobilehumseg_steps.cc)
    axera_example(ax_monodlex ax_monodlex_steps.cc)
    axera_example(ax_paddle_mobileseg ax_paddle_mobileseg.cc)
    axera_example(ax_crnn ax_crnn_steps.cc)
    axera_example(ax_face_parsing ax_face_parsing_steps.cc)
    axera_example(ax_handpose ax_handpose_steps.cc)
    axera_example(ax_yolov5s_seg ax_yolov5s_seg_steps.cc)
    axera_example(ax_palm_detection ax_palm_detection_steps.cc)
    axera_example(ax_yolopv2 ax_yolopv2_steps.cc)
    axera_example(ax_yolov7s_face ax_yolov7s_face_steps.cc)
    axera_example(ax_yolov7s_palm ax_yolov7s_palm_steps.cc)
    axera_example(ax_yolov6s ax_yolov6s_steps.cc)
    axera_example(ax_yolov8s ax_yolov8s_steps.cc)
    axera_example(ax_yolov8s_seg ax_yolov8s_seg_steps.cc)
    axera_example(ax_yolov8s_pose ax_yolov8s_pose_steps.cc)

    # ******************************** MMYOLO model ********************************#
    # axera_example (ax_mmyolov5s             mmyolo/ax_mmyolov5s.cc)
    axera_example(ax_mmyoloxs mmyolo/ax_mmyoloxs.cc)
    axera_example(ax_mmppyoloeps mmyolo/ax_mmppyoloeps.cc)
    axera_example(ax_mmyolov6s mmyolo/ax_mmyolov6s.cc)

    # axera_example (ax_mmyolov7t             mmyolo/ax_mmyolov7t.cc)
    # axera_example (ax_mmrtmdets             mmyolo/ax_mmrtmdets.cc)
    axera_example(ax_mmyolov8s mmyolo/ax_mmyolov8s.cc)

else() # ax630a support
    axera_example(ax_classification ax_classification_steps.cc)
    axera_example(ax_yolov5s ax_yolov5s_steps.cc)
    axera_example(ax_yolo_fastest ax_yolo_fastest_steps.cc)
    axera_example(ax_yolov3 ax_yolov3_steps.cc)
endif()
