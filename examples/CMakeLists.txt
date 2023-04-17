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

if(NOT BSP_MSP_DIR)
    set(BSP_MSP_DIR ${CMAKE_SOURCE_DIR}/out)
endif()
message(STATUS "BSP_MSP_DIR = ${BSP_MSP_DIR}")

if(AXERA_TARGET_CHIP MATCHES "ax650")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libopencv-4.5.5-aarch64/lib/cmake/opencv4)
elseif(AXERA_TARGET_CHIP MATCHES "ax620a")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/opencv-arm-linux/lib/cmake/opencv4)
elseif(AXERA_TARGET_CHIP MATCHES "ax630a")
    set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/opencv-aarch64-linux/lib/cmake/opencv4)
endif()

find_package(OpenCV REQUIRED)

# open when need debug on board
# add_definitions("-g -O0")
message(STATUS "BUILD FOR ${AXERA_TARGET_CHIP}")
include_directories(.)

if(AXERA_TARGET_CHIP MATCHES "ax650")
    add_subdirectory(${CMAKE_SOURCE_DIR}/examples/ax650)
elseif(AXERA_TARGET_CHIP MATCHES "ax620a")
    add_subdirectory(${CMAKE_SOURCE_DIR}/examples/ax620)
elseif(AXERA_TARGET_CHIP MATCHES "ax630a")
    add_subdirectory(${CMAKE_SOURCE_DIR}/examples/ax620)
endif()