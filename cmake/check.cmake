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

# Compilers:
#
# - AXERA_COMPILER_GCC                          - GNU compiler (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
# - AXERA_COMPILER_CLANG                        - Clang-compatible compiler (CMAKE_CXX_COMPILER_ID MATCHES "Clang" - Clang or AppleClang, see CMP0025)
# - AXERA_COMPILER_ICC                          - Intel compiler
# - AXERA_COMPILER_MSVC                         - MSVC, Microsoft Visual Compiler (CMake variable)
# - AXERA_COMPILER_MINGW AXERA_COMPILER_CYGWIN  - MINGW / CYGWIN / CMAKE_COMPILER_IS_MINGW / CMAKE_COMPILER_IS_CYGWIN (CMake original variables)
#
# CPU Platforms String:
# - X86                                         - world wide architecture, u know who, x86 or amd64
# - ARM                                         - Advanced RISC Machine architecture, a.k.a ARM architecture
# - PPC64                                       - PowerPC
# - PPC64LE                                     - PowerPC Little Edition
# - MIPS                                        - Million Instructions Per Second architecture, a.k.a MIPS architecture
#
# OS:
# - AXERA_SYSTEM_WINDOWS                        - Windows |  MinGW
# - AXERA_SYSTEM_LINUX                          - Linux   | Android
# - AXERA_SYSTEM_ANDROID                        - Android
# - AXERA_SYSTEM_IOS                            - iOS
# - AXERA_SYSTEM_APPLE                          - MacOSX  | iOS
# - AXERA_SYSTEM_OHOS                           - Harmony OS


# check target cpu
if (AXERA_SKIP_TARGET_PROCESSOR_CHECK)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
    SET (AXERA_TARGET_PROCESSOR        "X86"   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE    CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*")
    SET (AXERA_TARGET_PROCESSOR        "X86"   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  TRUE    CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  FALSE   CACHE INTERNAL "" FORCE)
elseif ((IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
        OR (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*|ARM64.*)")
        OR (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*|ARM64.*)"))
    SET (AXERA_TARGET_PROCESSOR        "ARM"   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE    CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*)")
    SET (AXERA_TARGET_PROCESSOR        "ARM"   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  TRUE    CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  FALSE   CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64le")
    SET (AXERA_TARGET_PROCESSOR        "PPCLE" CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE    CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64")
    SET (AXERA_TARGET_PROCESSOR        "PPC"   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE    CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(mips.*|MIPS.*|mips64.*)")
    SET (AXERA_TARGET_PROCESSOR        "MIPS"  CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE   CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE    CACHE INTERNAL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(rv64.*|RV64.*|riscv64.*)")
    SET (AXERA_TARGET_PROCESSOR        "lp64dv" CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_32Bit  FALSE    CACHE INTERNAL "" FORCE)
    SET (AXERA_TARGET_PROCESSOR_64Bit  TRUE     CACHE INTERNAL "" FORCE)
else()
    if (NOT AXERA_SUPPRESS_TARGET_PROCESSOR_CHECK)
        MESSAGE (WARNING "AXERA: Unrecognized target processor configuration. " ${CMAKE_SYSTEM_PROCESSOR})
    endif()
endif()


# Workaround for 32-bit operating systems on x86_64
if ((CMAKE_SIZEOF_VOID_P EQUAL 4) AND (AXERA_TARGET_PROCESSOR MATCHES "X86") AND (NOT AXERA_FORCE_BUILD_X86_64))
    if (NOT AXERA_SUPPRESS_TARGET_PROCESSOR_CHECK)
        MESSAGE (WARNING "AXERA: 32Bit target OS is detected. Assume 32-bit compilation mode.")
    endif()
    if (AXERA_TARGET_PROCESSOR_64Bit)
        SET (AXERA_TARGET_PROCESSOR_64Bit  FALSE   CACHE INTERNAL "" FORCE)
        SET (AXERA_TARGET_PROCESSOR_32Bit  TRUE    CACHE INTERNAL "" FORCE)
    endif()
endif()

# Workaround for 32-bit operating systems on aarch64 processor
if ((CMAKE_SIZEOF_VOID_P EQUAL 4) AND (AXERA_TARGET_PROCESSOR MATCHES "ARM") AND (NOT AXERA_FORCE_BUILD_AARCH64))
    if (NOT AXERA_SUPPRESS_TARGET_PROCESSOR_CHECK)
        MESSAGE (STATUS "AXERA: 32Bit target OS is detected. Assume 32-bit compilation mode.")
    endif()
    if (AXERA_TARGET_PROCESSOR_64Bit)
        SET (AXERA_TARGET_PROCESSOR_32Bit  TRUE    CACHE INTERNAL "" FORCE)
        SET (AXERA_TARGET_PROCESSOR_64Bit  FALSE   CACHE INTERNAL "" FORCE)
    endif()
endif()



# Check which compiler
# GCC, the GNU Compiler Collection
if (NOT DEFINED AXERA_COMPILER_GCC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    SET (AXERA_COMPILER_GCC   TRUE CACHE INTERNAL "" FORCE)
endif()

# Clang, C Language Family Frontend for LLVM
if(NOT DEFINED AXERA_COMPILER_CLANG AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # Clang or AppleClang (see CMP0025)
    SET (AXERA_COMPILER_CLANG TRUE CACHE INTERNAL "" FORCE)
endif()

# ICC, Intel C++ Compiler
if (UNIX)
    if     (__ICL)
        SET (AXERA_COMPILER_ICC   __ICL                   CACHE INTERNAL "" FORCE)
    elseif (__ICC)
        SET (AXERA_COMPILER_ICC   __ICC                   CACHE INTERNAL "" FORCE)
    elseif (__ECL)
        SET (AXERA_COMPILER_ICC   __ECL                   CACHE INTERNAL "" FORCE)
    elseif (__ECC)
        SET (AXERA_COMPILER_ICC   __ECC                   CACHE INTERNAL "" FORCE)
    elseif (__INTEL_COMPILER)
        SET (AXERA_COMPILER_ICC   __INTEL_COMPILER)
    elseif (CMAKE_C_COMPILER MATCHES "icc")
        SET (AXERA_COMPILER_ICC   icc_matches_c_compiler  CACHE INTERNAL "" FORCE)
    endif()
endif()

if (MSVC AND CMAKE_C_COMPILER MATCHES "icc|icl")
    SET (AXERA_COMPILER_ICC  __INTEL_COMPILER_FOR_WINDOWS CACHE INTERNAL "" FORCE)
endif()

# MSVC, Microsoft Visual C++ Compiler
if (MSVC)
    SET (AXERA_COMPILER_MSVC      TRUE CACHE INTERNAL "" FORCE)
endif()

# MinGW, Minimalist GNU for Windows
if (MINGW OR CMAKE_COMPILER_IS_MINGW)
    SET (AXERA_COMPILER_GCC       TRUE CACHE INTERNAL "" FORCE)
    SET (AXERA_COMPILER_MINGW     TRUE CACHE INTERNAL "" FORCE)
endif()

# Cygwin
if (CYGWIN OR CMAKE_COMPILER_IS_CYGWIN)
    SET (AXERA_COMPILER_GCC       TRUE CACHE INTERNAL "" FORCE)
    SET (AXERA_COMPILER_CYGWIN    TRUE CACHE INTERNAL "" FORCE)
endif()



# check system
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET (AXERA_SYSTEM     "Windows"           CACHE INTERNAL "" FORCE)
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES "Android")
        SET (AXERA_SYSTEM "Android"           CACHE INTERNAL "" FORCE)
    elseif(OHOS)
        SET (AXERA_SYSTEM "Harmony OS"        CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
        SET (AXERA_SYSTEM "Linux"             CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*")
        SET (AXERA_SYSTEM "FreeBSD"           CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|FreeBSD")
        SET (AXERA_SYSTEM "FreeBSD"           CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
        SET (AXERA_SYSTEM "NetBSD"            CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
        SET (AXERA_SYSTEM "OpenBSD"           CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "SYSV5.*")
        SET (AXERA_SYSTEM "System V"          CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Solaris.*")
        SET (AXERA_SYSTEM "Solaris"           CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "HP-UX.*")
        SET (AXERA_SYSTEM "HP-UX"             CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "AIX.*")
        SET (AXERA_SYSTEM "AIX"               CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Minix.*")
        SET (AXERA_SYSTEM "Minix"             CACHE INTERNAL "" FORCE)
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
        SET (AXERA_SYSTEM "Darwin"            CACHE INTERNAL "" FORCE)
    elseif(CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
        SET (AXERA_SYSTEM "MacOS"             CACHE INTERNAL "" FORCE)
    else()
        SET (AXERA_SYSTEM "Apple"             CACHE INTERNAL "" FORCE)
    endif()
endif ()
