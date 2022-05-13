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

#include <string>
#include <vector>


namespace utilities
{
    std::vector<std::string> split_string(const std::string& content, const std::string& delimiter)
    {
        std::vector<std::string> result;

        std::string::size_type pos1 = 0;
        std::string::size_type pos2 = content.find(delimiter);

        while (std::string::npos != pos2)
        {
            result.push_back(content.substr(pos1, pos2 - pos1));

            pos1 = pos2 + delimiter.size();
            pos2 = content.find(delimiter, pos1);
        }

        if (pos1 != content.length())
        {
            result.push_back(content.substr(pos1));
        }

        return result;
    }
}
