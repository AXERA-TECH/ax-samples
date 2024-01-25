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

# C/C++ Compiler report

MESSAGE (STATUS "")
MESSAGE (STATUS "")
MESSAGE (STATUS "Information Summary:")
MESSAGE (STATUS "")
# CMake information
MESSAGE (STATUS "CMake information:")
MESSAGE (STATUS "  - CMake version:              ${CMAKE_VERSION}")
MESSAGE (STATUS "  - CMake generator:            ${CMAKE_GENERATOR}")
MESSAGE (STATUS "  - CMake building tools:       ${CMAKE_BUILD_TOOL}")
MESSAGE (STATUS "  - Target System:              ${CMAKE_SYSTEM_NAME}")
MESSAGE (STATUS "  - Target CPU arch:            ${AXERA_TARGET_PROCESSOR}")
IF (AXERA_TARGET_PROCESSOR_32Bit)
    MESSAGE (STATUS "  - Target CPU bus width:       32 Bit")
ENDIF()
IF (AXERA_TARGET_PROCESSOR_64Bit)
    MESSAGE (STATUS "  - Target CPU bus width:       64 Bit")
ENDIF()
MESSAGE (STATUS "")


# C/C++ Compiler information
MESSAGE (STATUS "${PROJECT_NAME} toolchain information:")
MESSAGE (STATUS "  Cross compiling: ${CMAKE_CROSSCOMPILING}")
MESSAGE (STATUS "  C/C++ compiler:")
MESSAGE (STATUS "    - C   standard version:     C${CMAKE_C_STANDARD}")
MESSAGE (STATUS "    - C   standard required:    ${CMAKE_C_STANDARD_REQUIRED}")
MESSAGE (STATUS "    - C   standard extensions:  ${CMAKE_C_EXTENSIONS}")
MESSAGE (STATUS "    - C   compiler version:     ${CMAKE_C_COMPILER_VERSION}")
MESSAGE (STATUS "    - C   compiler:             ${CMAKE_C_COMPILER}")
MESSAGE (STATUS "    - C++ standard version:     C++${CMAKE_CXX_STANDARD}")
MESSAGE (STATUS "    - C++ standard required:    ${CMAKE_CXX_STANDARD_REQUIRED}")
MESSAGE (STATUS "    - C++ standard extensions:  ${CMAKE_CXX_EXTENSIONS}")
MESSAGE (STATUS "    - C++ compiler version:     ${CMAKE_CXX_COMPILER_VERSION}")
MESSAGE (STATUS "    - C++ compiler:             ${CMAKE_CXX_COMPILER}")
MESSAGE (STATUS "  C/C++ compiler flags:")
MESSAGE (STATUS "    - C   compiler flags:       ${CMAKE_C_FLAGS}")
MESSAGE (STATUS "    - C++ compiler flags:       ${CMAKE_CXX_FLAGS}")
MESSAGE (STATUS "  OpenMP:")
IF (OpenMP_FOUND)
MESSAGE (STATUS "    - OpenMP was found:         YES")
MESSAGE (STATUS "    - OpenMP version:           ${OpenMP_C_VERSION}")
ELSE()
MESSAGE (STATUS "    - OpenMP was found:         NO")
ENDIF()
MESSAGE (STATUS "")


# CMake project information
MESSAGE (STATUS "${PROJECT_NAME} building information:")
MESSAGE (STATUS "  - Project source path is:     ${PROJECT_SOURCE_DIR}")
MESSAGE (STATUS "  - Project building path is:   ${CMAKE_BINARY_DIR}")
MESSAGE (STATUS "")


MESSAGE (STATUS "${PROJECT_NAME} other information:")
# show building install path
MESSAGE (STATUS "  Package install path:         ${CMAKE_INSTALL_PREFIX}")
MESSAGE (STATUS "  Target platform:              ${AXERA_TARGET_CHIP}")
MESSAGE (STATUS "")
