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

function(axera_example example_name)
    add_executable(${example_name})

    foreach (file IN LISTS ARGN)
        target_sources(${example_name} PRIVATE ${file})
    endforeach ()

    target_include_directories(${example_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${BSP_MSP_DIR}/include)

    # opencv
    target_include_directories(${example_name} PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${example_name} PRIVATE ${OpenCV_LIBS})

    # ax_engine
    target_link_libraries(${example_name} PRIVATE ax_engine)

    # sdk
    target_link_libraries(${example_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT} ax_interpreter ax_sys ax_ivps  ax_hsm)
    target_link_directories(${example_name} PRIVATE ${BSP_MSP_DIR}/lib)

    target_compile_options (${example_name} PUBLIC $<$<COMPILE_LANGUAGE:C,CXX>: -O3>)
    
    if (AXERA_TARGET_CHIP MATCHES "ax637")
        install(TARGETS ${example_name} DESTINATION ax637)
    endif()
endfunction()

