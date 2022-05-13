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
#include <string>
#include <vector>
#include <fstream>

namespace utilities
{
    bool file_exist(const std::string& path)
    {
        auto flag = false;

        std::fstream fs(path, std::ios::in | std::ios::binary);
        flag = fs.is_open();
        fs.close();

        return flag;
    }

    bool read_file(const std::string& path, std::vector<char>& data)
    {
        std::fstream fs(path, std::ios::in | std::ios::binary);

        if (!fs.is_open())
        {
            return false;
        }

        fs.seekg(std::ios::end);
        auto fs_end = fs.tellg();
        fs.seekg(std::ios::beg);
        auto fs_beg = fs.tellg();

        auto file_size = static_cast<size_t>(fs_end - fs_beg);
        auto vector_size = data.size();

        data.reserve(vector_size + file_size);
        data.insert(data.end(), std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>());

        fs.close();

        return true;
    }

    bool dump_file(const std::string& path, std::vector<char>& data)
    {
        std::fstream fs(path, std::ios::out | std::ios::binary);

        if (!fs.is_open() || fs.fail())
        {
            fprintf(stderr, "[ERR] cannot open file %s \n", path.c_str());
        }

        fs.write(data.data(), data.size());

        return true;
    }

    bool dump_file(const std::string& path, char* data, int size)
    {
        std::fstream fs(path, std::ios::out | std::ios::binary);

        if (!fs.is_open() || fs.fail())
        {
            fprintf(stderr, "[ERR] cannot open file %s \n", path.c_str());
        }

        fs.write(data, size);

        return true;
    }
} // namespace utilities
