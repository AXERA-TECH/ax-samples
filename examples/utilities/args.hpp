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

#include <array>
#include <vector>
#include <type_traits>

#include "utilities/split.hpp"


namespace utilities
{
    template <typename T, size_t N>
    bool parse_string(const std::string& argument_string, std::array<T, N>& arguments, const std::string& delimiter = ",")
    {
        std::vector<std::string> result = split_string(argument_string, delimiter);

        if (N != result.size())
        {
            return false;
        }

        for (size_t i = 0; i < N; i++)
        {
            if (std::is_integral<T>::value)
            {
                arguments[i] = std::stoi(result[i]);
            }

            if (std::is_floating_point<T>::value)
            {
                arguments[i] = std::stof(result[i]);
            }
        }

        return true;
    }
}
