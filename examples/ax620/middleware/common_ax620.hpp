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
 * Author: ls.wang
 */

#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include "ax_interpreter_external_api.h"
#include "base/common.hpp"

namespace common
{
    static inline AX_NPU_SDK_EX_HARD_MODE_T get_hard_mode_by_model_type(int model_type)
    {
        if (AX_NPU_MODEL_TYPE_DEFUALT == model_type)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_DISABLE;
        }
#ifdef AXERA_TARGET_CHIP_AX620
        else if (model_type == AX_NPU_MODEL_TYPE_1_1_1 || model_type == AX_NPU_MODEL_TYPE_1_1_2)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_1_1;
        }
#else
        else if (model_type == AX_NPU_MODEL_TYPE_3_1_1 || model_type == AX_NPU_MODEL_TYPE_3_1_2)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_3_1;
        }
        else if (model_type == AX_NPU_MODEL_TYPE_2_2_1 || model_type == AX_NPU_MODEL_TYPE_2_2_2)
        {
            return AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_2_2;
        }
#endif
        else
        {
            fprintf(stderr, "[ERR]: get_hard_mode_by_model_type(int model_type) Unknown npu mode(%d). return default mode\n", model_type);
            return (AX_NPU_SDK_EX_HARD_MODE_T)0;
        }
    }
} // namespace common