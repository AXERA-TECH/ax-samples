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

function(axera_example name)
    add_executable(${name})

    # add srcs
    foreach(file IN LISTS ARGN)
        target_sources(${name} PRIVATE ${file})
    endforeach()

    # headers
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(${name} PRIVATE ${BSP_MSP_DIR}/include)

    if(${AXERA_TARGET_PROCESSOR} MATCHES "ARM" AND AXERA_TARGET_PROCESSOR_32Bit)
        target_include_directories(${name} PRIVATE /opt/include)
    endif()

    # libs
    if(AXERA_TARGET_CHIP MATCHES "ax620a") # ax620 support
        target_link_libraries(${name} PRIVATE pthread dl) # ax620a use this

        # target_link_libraries (${name} PRIVATE pthread dl stdc++fs) # ax620u use this
        target_link_libraries(${name} PRIVATE ax_run_joint ax_interpreter_external ax_interpreter ax_sys axsyslog stdc++fs)
    else() # ax630a support
        target_link_libraries(${name} PRIVATE pthread dl stdc++fs)
        target_link_libraries(${name} PRIVATE ax_run_joint ax_interpreter_external ax_interpreter ax_sys)
    endif()

    target_link_libraries(${name} PRIVATE ax_ivps ax_npu_cv_kit)

    # folders
    target_link_directories(${name} PRIVATE ${BSP_MSP_DIR}/lib)

    if(${AXERA_TARGET_PROCESSOR} MATCHES "ARM" AND AXERA_TARGET_PROCESSOR_32Bit)
        target_link_libraries(${name} PRIVATE dl)
        target_link_directories(${name} PRIVATE /opt/lib)
    endif()

    # opencv
    target_include_directories(${name} PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE ${OpenCV_LIBS})

    # install (TARGETS ${name} DESTINATION bin)
    if(AXERA_TARGET_CHIP MATCHES "ax620a")
        install(TARGETS ${name} DESTINATION ax620)
    elseif(AXERA_TARGET_CHIP MATCHES "ax630a")
        install(TARGETS ${name} DESTINATION ax630)
    endif()
endfunction()
