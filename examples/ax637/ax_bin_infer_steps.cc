/*
 * AXERA is pleased to support the open source community by making ax-samples available.
 *
 * Copyright (c) 2026, AXERA Semiconductor Co., Ltd. All rights reserved.
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
 * Minimal raw-tensor axmodel inference utility.
 *
 * No pre/post processing: feed one raw binary file per model input, dump
 * each output tensor verbatim to <output-dir>/<sanitized-name>.bin plus a
 * small <output-dir>/manifest.txt describing shape/dtype/size.
 *
 * Mirrors ax_model_debug/ax_infer.py:
 *     ./ax_bin_infer --model compiled.axmodel --output-dir ./infer_out  \
 *                    1_left.bin 1_right.bin
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include <ax_engine_api.h>
#include <ax_sys_api.h>

#include "utilities/cmdline.hpp"
#include "utilities/file.hpp"
#include "utilities/timer.hpp"

namespace {

struct DtypeInfo
{
    const char* name;
    int         itemsize;  // bytes per element; 0 if packed / unknown
};

static DtypeInfo describe_dtype(AX_ENGINE_DATA_TYPE_T t)
{
    switch (t)
    {
    case AX_ENGINE_DT_UINT8:         return {"uint8",   1};
    case AX_ENGINE_DT_SINT8:         return {"int8",    1};
    case AX_ENGINE_DT_UINT16:        return {"uint16",  2};
    case AX_ENGINE_DT_SINT16:        return {"int16",   2};
    case AX_ENGINE_DT_FLOAT32:       return {"float32", 4};
    case AX_ENGINE_DT_UINT32:        return {"uint32",  4};
    case AX_ENGINE_DT_SINT32:        return {"int32",   4};
    case AX_ENGINE_DT_FLOAT64:       return {"float64", 8};
    case AX_ENGINE_DT_UINT10_PACKED: return {"uint10_packed", 0};
    case AX_ENGINE_DT_UINT12_PACKED: return {"uint12_packed", 0};
    case AX_ENGINE_DT_UINT14_PACKED: return {"uint14_packed", 0};
    case AX_ENGINE_DT_UINT16_PACKED: return {"uint16_packed", 0};
    default:                         return {"unknown", 0};
    }
}

static std::string shape_to_string(const AX_S32* shape, AX_U8 n)
{
    std::string s = "[";
    for (AX_U8 i = 0; i < n; ++i)
    {
        s += std::to_string(shape[i]);
        if (i + 1 < n) s += ",";
    }
    s += "]";
    return s;
}

static std::string sanitize(const std::string& name)
{
    std::string r; r.reserve(name.size());
    for (char c : name)
    {
        if (c == '/' || c == ':' || c == ' ') r.push_back('_');
        else r.push_back(c);
    }
    // trim leading / trailing underscores
    size_t b = r.find_first_not_of('_');
    size_t e = r.find_last_not_of('_');
    if (b == std::string::npos) return "tensor";
    return r.substr(b, e - b + 1);
}

static bool read_entire_file(const std::string& path, std::vector<char>& buf)
{
    std::ifstream fs(path, std::ios::binary | std::ios::ate);
    if (!fs.is_open()) return false;
    std::streamsize size = fs.tellg();
    fs.seekg(0, std::ios::beg);
    buf.resize(size);
    if (size > 0 && !fs.read(buf.data(), size)) return false;
    return true;
}

static bool write_entire_file(const std::string& path, const void* data, size_t n)
{
    std::ofstream fs(path, std::ios::binary | std::ios::trunc);
    if (!fs.is_open()) return false;
    fs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
    return fs.good();
}

static void mkdir_p(const std::string& path)
{
    if (path.empty()) return;
    if (::mkdir(path.c_str(), 0755) == 0) return;
    // ignore EEXIST, any other error will surface when we write into it
}

static int prepare_io(AX_ENGINE_IO_INFO_T* info, AX_ENGINE_IO_T* io)
{
    std::memset(io, 0, sizeof(AX_ENGINE_IO_T));
    io->nInputSize  = info->nInputSize;
    io->nOutputSize = info->nOutputSize;
    io->pInputs  = new AX_ENGINE_IO_BUFFER_T[io->nInputSize];
    io->pOutputs = new AX_ENGINE_IO_BUFFER_T[io->nOutputSize];
    std::memset(io->pInputs,  0, sizeof(AX_ENGINE_IO_BUFFER_T) * io->nInputSize);
    std::memset(io->pOutputs, 0, sizeof(AX_ENGINE_IO_BUFFER_T) * io->nOutputSize);

    for (AX_U32 i = 0; i < info->nInputSize; ++i)
    {
        io->pInputs[i].nSize = info->pInputs[i].nSize;
        AX_S32 ret = AX_SYS_MemAlloc((AX_U64*)&io->pInputs[i].phyAddr, &io->pInputs[i].pVirAddr,
                                     info->pInputs[i].nSize, 128,
                                     (const AX_S8*)"ax-samples-bin-infer");
        if (ret != 0) { std::fprintf(stderr, "alloc input[%u] failed: 0x%x\n", i, ret); return ret; }
    }
    for (AX_U32 i = 0; i < info->nOutputSize; ++i)
    {
        io->pOutputs[i].nSize = info->pOutputs[i].nSize;
        AX_S32 ret = AX_SYS_MemAlloc((AX_U64*)&io->pOutputs[i].phyAddr, &io->pOutputs[i].pVirAddr,
                                     info->pOutputs[i].nSize, 128,
                                     (const AX_S8*)"ax-samples-bin-infer");
        if (ret != 0) { std::fprintf(stderr, "alloc output[%u] failed: 0x%x\n", i, ret); return ret; }
    }
    return 0;
}

static void free_io(AX_ENGINE_IO_T* io)
{
    if (!io) return;
    for (AX_U32 i = 0; i < io->nInputSize; ++i)
        if (io->pInputs[i].pVirAddr)
            AX_SYS_MemFree(io->pInputs[i].phyAddr, io->pInputs[i].pVirAddr);
    for (AX_U32 i = 0; i < io->nOutputSize; ++i)
        if (io->pOutputs[i].pVirAddr)
            AX_SYS_MemFree(io->pOutputs[i].phyAddr, io->pOutputs[i].pVirAddr);
    delete[] io->pInputs;  io->pInputs  = nullptr;
    delete[] io->pOutputs; io->pOutputs = nullptr;
}

} // namespace

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add<std::string>("model",      'm', "compiled .axmodel path", true);
    cmd.add<std::string>("output-dir", 'o', "directory to drop output tensors",
                         false, "./infer_out");
    cmd.add<int>        ("repeat",     'r', "forward repetitions (for timing)",
                         false, 1);
    cmd.footer("<input0.bin> <input1.bin> ...  (one binary file per model input)");
    cmd.parse_check(argc, argv);

    const std::string model_file = cmd.get<std::string>("model");
    const std::string output_dir = cmd.get<std::string>("output-dir");
    const int         repeat     = std::max(1, cmd.get<int>("repeat"));
    const auto&       input_files = cmd.rest();

    if (input_files.empty())
    {
        std::fprintf(stderr, "no input .bin files given; see --help\n");
        return -1;
    }

    std::vector<char> model_buf;
    if (!read_entire_file(model_file, model_buf))
    {
        std::fprintf(stderr, "failed to read model: %s\n", model_file.c_str());
        return -1;
    }

    AX_ENGINE_NPU_ATTR_T npu_attr; std::memset(&npu_attr, 0, sizeof(npu_attr));
    npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
    AX_S32 ret = AX_SYS_Init();
    if (ret != 0) { std::fprintf(stderr, "AX_SYS_Init failed: 0x%x\n", ret); return ret; }
    ret = AX_ENGINE_Init(&npu_attr);
    if (ret != 0) { std::fprintf(stderr, "AX_ENGINE_Init failed: 0x%x\n", ret); AX_SYS_Deinit(); return ret; }

    AX_ENGINE_HANDLE handle = nullptr;
    {
        timer t;
        ret = AX_ENGINE_CreateHandle(&handle, model_buf.data(), model_buf.size());
        if (ret != 0) { std::fprintf(stderr, "CreateHandle failed: 0x%x\n", ret); AX_ENGINE_Deinit(); AX_SYS_Deinit(); return ret; }
        ret = AX_ENGINE_CreateContext(handle);
        if (ret != 0) { std::fprintf(stderr, "CreateContext failed: 0x%x\n", ret); AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit(); return ret; }
        std::printf("load: %.2f ms\n", t.cost());
    }

    AX_ENGINE_IO_INFO_T* io_info = nullptr;
    ret = AX_ENGINE_GetIOInfo(handle, &io_info);
    if (ret != 0) { std::fprintf(stderr, "GetIOInfo failed: 0x%x\n", ret); AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit(); return ret; }

    if (io_info->nInputSize != input_files.size())
    {
        std::fprintf(stderr, "model expects %u inputs, got %zu .bin file(s). expected order:\n",
                     io_info->nInputSize, input_files.size());
        for (AX_U32 i = 0; i < io_info->nInputSize; ++i)
            std::fprintf(stderr, "  %u: %s\n", i, io_info->pInputs[i].pName);
        AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit();
        return -1;
    }

    AX_ENGINE_IO_T io_data; std::memset(&io_data, 0, sizeof(io_data));
    ret = prepare_io(io_info, &io_data);
    if (ret != 0) { free_io(&io_data); AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit(); return ret; }

    // load and copy inputs
    for (AX_U32 i = 0; i < io_info->nInputSize; ++i)
    {
        const auto& meta = io_info->pInputs[i];
        auto dt = describe_dtype(meta.eDataType);
        std::vector<char> raw;
        if (!read_entire_file(input_files[i], raw))
        {
            std::fprintf(stderr, "failed to read input[%u]: %s\n", i, input_files[i].c_str());
            free_io(&io_data); AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit();
            return -1;
        }
        std::printf("in  %u %-24s shape=%s dtype=%s size=%u bytes  <- %s (%zu bytes)\n",
                    i, meta.pName,
                    shape_to_string(meta.pShape, meta.nShapeSize).c_str(),
                    dt.name, meta.nSize, input_files[i].c_str(), raw.size());
        if (raw.size() != meta.nSize)
        {
            std::fprintf(stderr, "  input[%u] size mismatch: model expects %u, file has %zu\n",
                         i, meta.nSize, raw.size());
            free_io(&io_data); AX_ENGINE_DestroyHandle(handle); AX_ENGINE_Deinit(); AX_SYS_Deinit();
            return -1;
        }
        std::memcpy(io_data.pInputs[i].pVirAddr, raw.data(), raw.size());
    }

    // warmup + timed forward
    for (int i = 0; i < 3; ++i) AX_ENGINE_RunSync(handle, &io_data);
    std::vector<float> times(repeat);
    for (int i = 0; i < repeat; ++i)
    {
        timer t;
        ret = AX_ENGINE_RunSync(handle, &io_data);
        times[i] = t.cost();
        if (ret != 0) { std::fprintf(stderr, "RunSync failed at iter %d: 0x%x\n", i, ret); break; }
    }
    if (ret == 0)
    {
        float sum = 0.f, mn = 1e9f, mx = 0.f;
        for (float t : times) { sum += t; mn = std::min(mn, t); mx = std::max(mx, t); }
        std::printf("forward: loop=%d  min=%.2fms  max=%.2fms  avg=%.2fms\n",
                    repeat, mn, mx, sum / repeat);
    }

    // dump outputs
    mkdir_p(output_dir);
    const std::string manifest_path = output_dir + "/manifest.txt";
    std::ofstream mf(manifest_path);
    if (mf.is_open())
        mf << "# index\tname\tdtype\tshape\tbytes\tfile\n";

    for (AX_U32 i = 0; i < io_info->nOutputSize; ++i)
    {
        const auto& meta = io_info->pOutputs[i];
        auto dt = describe_dtype(meta.eDataType);
        const std::string fname = sanitize(meta.pName) + ".bin";
        const std::string fpath = output_dir + "/" + fname;

        const void* vptr = io_data.pOutputs[i].pVirAddr;
        if (!write_entire_file(fpath, vptr, meta.nSize))
        {
            std::fprintf(stderr, "failed to write output[%u] -> %s\n", i, fpath.c_str());
            continue;
        }
        std::printf("out %u %-24s shape=%s dtype=%s size=%u bytes  -> %s\n",
                    i, meta.pName,
                    shape_to_string(meta.pShape, meta.nShapeSize).c_str(),
                    dt.name, meta.nSize, fpath.c_str());

        if (mf.is_open())
        {
            mf << i << '\t' << meta.pName << '\t' << dt.name << '\t'
               << shape_to_string(meta.pShape, meta.nShapeSize) << '\t'
               << meta.nSize << '\t' << fname << '\n';
        }
    }

    free_io(&io_data);
    AX_ENGINE_DestroyHandle(handle);
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
    return 0;
}
