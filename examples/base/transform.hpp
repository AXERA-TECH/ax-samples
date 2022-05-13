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
 * Author: hebing
 */

#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
namespace transform
{
    static void nhwc2nchw(const float* input, float* output, int h, int w, int c)
    {
        int output_index = 0;
        for (int i = 0; i < c; ++i)
        {
            for (int j = 0; j < h; ++j)
            {
                for (int k = 0; k < w; ++k)
                {
                    int input_index = j * w * c + k * c + i;
                    output[output_index++] = input[input_index];
                }
            }
        }
    }

} // namespace transform
