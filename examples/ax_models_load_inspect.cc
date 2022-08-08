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

#include <fcntl.h>
#include <sys/mman.h>
#include <csignal>

#include "middleware/io.hpp"
#include "utilities/file.hpp"

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "joint.h"

#define mark(str, arg...)                 \
    do {                                  \
        printf("[INFO]" str "\n", ##arg); \
    } while (0)

namespace ax
{
    using namespace middleware;

    void free_io_index(AX_JOINT_IO_BUFFER_T* io_buf, size_t index)
    {
        for (int i = 0; i < index; ++i)
        {
            AX_JOINT_IO_BUFFER_T* pBuf = io_buf + i;
            free_joint_buffer(pBuf);
        }
    }

    void free_io(AX_JOINT_IO_T& io)
    {
        for (size_t j = 0; j < io.nInputSize; ++j)
        {
            AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs + j;
            free_joint_buffer(pBuf);
        }
        for (size_t j = 0; j < io.nOutputSize; ++j)
        {
            AX_JOINT_IO_BUFFER_T* pBuf = io.pOutputs + j;
            free_joint_buffer(pBuf);
        }
        delete[] io.pInputs;
        delete[] io.pOutputs;
    }

    bool prepare_io(AX_JOINT_IO_T& io, const AX_JOINT_IO_INFO_T* io_info, const char* mark)
    {
        std::memset(&io, 0, sizeof(io));
        // deal with input
        io.nInputSize = io_info->nInputSize;
        io.pInputs = new AX_JOINT_IO_BUFFER_T[io.nInputSize];
        auto ret = AX_ERR_NPU_JOINT_SUCCESS;
        for (size_t i = 0; i < io.nInputSize; ++i)
        {
            const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs + i;
            AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs + i;
            ret = alloc_joint_buffer(pMeta, pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "alloc_joint_buffer input fail %s", mark);
                free_io_index(io.pInputs, i);
                return false;
            }
        }

        // deal with output
        io.nOutputSize = io_info->nOutputSize;
        io.pOutputs = new AX_JOINT_IO_BUFFER_T[io.nOutputSize];
        for (size_t i = 0; i < io.nOutputSize; ++i)
        {
            const AX_JOINT_IOMETA_T* out_pMeta = io_info->pOutputs + i;
            AX_JOINT_IO_BUFFER_T* out_pBuf = io.pOutputs + i;
            ret = alloc_joint_buffer(out_pMeta, out_pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "alloc_joint_buffer output fail %s", mark);
                free_io_index(io.pOutputs, i);
                return false;
            }
        }
        return true;
    }

    void* load_model_mmap(const std::string& input_model, int& models_size)
    {
        auto* file_fp = fopen(input_model.c_str(), "r");
        if (!file_fp)
        {
            fprintf(stderr, "read model file fail \n");
            return nullptr;
        }

        fseek(file_fp, 0, SEEK_END);
        int model_size = ftell(file_fp);
        fclose(file_fp);

        int fd = open(input_model.c_str(), O_RDWR, 0644);
        void* mmap_add = mmap(nullptr, model_size, PROT_WRITE, MAP_SHARED, fd, 0);
        models_size = model_size;

        return mmap_add;
    }

    bool load_models(const std::vector<std::string>& input_models)
    {
        AX_JOINT_SDK_ATTR_T attr;
        std::memset(&attr, 0, sizeof(attr));
        attr.eNpuMode = AX_NPU_VIRTUAL_1_1; // force 1_1

        auto ret = AX_JOINT_Adv_Init(&attr);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "Joint Adv init failed. \n");
            return false;
        }
        mark("PROCESS_INFO_PRINT");

        auto model_nums = input_models.size();

        AX_JOINT_HANDLE joint_handles[model_nums];
        AX_JOINT_IO_T joint_io_arr[model_nums];
        AX_JOINT_IO_SETTING_T joint_io_setting[model_nums];
        AX_JOINT_EXECUTION_CONTEXT joint_ctx[model_nums];
        AX_JOINT_EXECUTION_CONTEXT_SETTING_T joint_ctx_settings[model_nums];

        std::memset(joint_handles, 0, sizeof(joint_handles));
        std::memset(joint_io_arr, 0, sizeof(joint_io_arr));
        std::memset(joint_io_setting, 0, sizeof(joint_io_setting));
        std::memset(joint_ctx, 0, sizeof(joint_ctx));
        std::memset(joint_ctx_settings, 0, sizeof(joint_ctx_settings));

        void* joint_mmap[model_nums];
        int models_size[model_nums];
        memset(models_size, 0, sizeof(models_size));

        auto de_init_handle = [&joint_handles, &joint_mmap, &models_size, &input_models](int fail_index) -> bool {
            for (int i = 0; i < fail_index; ++i)
            {
                AX_JOINT_DestroyHandle(joint_handles[i]);
                mark("%s AX_JOINT_DestroyHandle", input_models[i].c_str());
                munmap(joint_mmap[i], models_size[i]);
                mark("%s munmap", input_models[i].c_str());
            }
            AX_JOINT_Adv_Deinit();
            mark("AX_JOINT_Adv_Deinit");
            return false;
        };

        auto de_init_handle_context = [&joint_handles, &joint_mmap, &models_size, &joint_ctx, &input_models](int fail_index) -> bool {
            for (int i = 0; i < fail_index; ++i)
            {
                AX_JOINT_DestroyExecutionContext(joint_ctx[i]);
                mark("%s AX_JOINT_DestroyExecutionContext", input_models[i].c_str());
                AX_JOINT_DestroyHandle(joint_handles[i]);
                mark("%s AX_JOINT_DestroyHandle", input_models[i].c_str());
                munmap(joint_mmap[i], models_size[i]);
                mark("%s unmap", input_models[i].c_str());
            }
            AX_JOINT_Adv_Deinit();
            mark("AX_JOINT_Adv_Deinit");
            return false;
        };

        auto de_init_io_handle_context = [&joint_handles, &joint_mmap, &models_size, &joint_ctx, &input_models, &joint_io_arr](int fail_index) -> bool {
            for (int i = 0; i < fail_index; ++i)
            {
                free_io(joint_io_arr[i]);
                mark("%s free_io", input_models[i].c_str());
                AX_JOINT_DestroyExecutionContext(joint_ctx[i]);
                mark("%s AX_JOINT_DestroyExecutionContext", input_models[i].c_str());
                AX_JOINT_DestroyHandle(joint_handles[i]);
                mark("%s AX_JOINT_DestroyHandle", input_models[i].c_str());
                munmap(joint_mmap[i], models_size[i]);
                mark("%s unmap", input_models[i].c_str());
            }
            AX_JOINT_Adv_Deinit();
            mark("AX_JOINT_Adv_Deinit");
            return false;
        };

        for (int i = 0; i < model_nums; ++i)
        {
            joint_mmap[i] = load_model_mmap(input_models[i], models_size[i]);
            mark("%s load_model_mmap", input_models[i].c_str());
            if (!joint_mmap[i] || models_size[i] == 0)
            {
                return de_init_handle(i);
            }
            ret = AX_JOINT_CreateHandle(&joint_handles[i], joint_mmap[i], models_size[i]);
            mark("%s AX_JOINT_CreateHandle", input_models[i].c_str());
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Create Run-Joint handler from file(%s) failed.\n", input_models[i].c_str());
                return de_init_handle(i);
            }

            ret = AX_JOINT_CreateExecutionContextV2(joint_handles[i], &joint_ctx[i], &joint_ctx_settings[i]);
            mark("%s AX_JOINT_CreateExecutionContextV2", input_models[i].c_str());
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "Create Joint(%s) context failed.\n", input_models[i].c_str());
                return de_init_handle_context(i);
            }

            auto io_info = AX_JOINT_GetIOInfo(joint_handles[i]);
            ret = prepare_io(joint_io_arr[i], io_info, input_models[i].c_str());

            if (!ret)
            {
                fprintf(stderr, "prepare_io Joint(%s) failed.\n", input_models[i].c_str());
                return de_init_io_handle_context(i);
            }
        }

        de_init_io_handle_context((int)model_nums);

        // LD_PRELOAD=./inspect_mem.so ./ax_models_load_inspect model0.joint model1.joint model2.joint
        // raise sigusr2: report os cmm
        raise(SIGUSR2);

        return true;
    }

} // namespace ax

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage ./ax_models_pipe model_a.joint model_b.joint model_c.joint \n");
        return 0;
    }

    for (int i = 0; i < argc; ++i)
    {
        fprintf(stderr, "%s \n", argv[i]);
    }

    std::vector<std::string> model_files(argc - 1);
    for (int i = 1; i < argc; ++i)
    {
        model_files[i - 1] = argv[i];
    }

    AX_S32 ret = AX_SYS_Init();
    if (0 != ret)
    {
        fprintf(stderr, "Init system failed.\n");
        return ret;
    }

    fprintf(stdout, "joint_version: %s \n", AX_JOINT_GetVersion());

    auto flag = ax::load_models(model_files);
    if (!flag)
    {
        fprintf(stderr, "Run load_models failed.\n");
    }

    AX_SYS_Deinit();

    return 0;
}
