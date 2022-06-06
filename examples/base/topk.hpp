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

#include <algorithm>
#include <cstdio>
#include <vector>

#include "base/score.hpp"


namespace classification
{
    void sort_score(std::vector<score>& array, bool reverse = false)
    {
        auto compare_func = [](const score& a, const score& b) -> bool
        {
            return a.score > b.score;
        };

        std::sort(array.begin(), array.end(), compare_func);

        if (reverse) std::reverse(array.begin(), array.end());
    }


    void print_score(const std::vector<score>& array, const size_t& n)
    {
        for (size_t i = 0; i < n; i++)
        {
            fprintf(stdout, "%.4f, %d\n", array[i].score, array[i].id);
        }
    }
}
