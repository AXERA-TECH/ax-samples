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

#include <cstdio>
#include <cstring>
#include <vector>

#include "ax_interpreter_external_api.h"
#include "ax_sys_api.h"
#include "joint.h"
#include "joint_adv.h"
#include "npu_cv_kit/npu_common.h"

namespace middleware
{
    AX_S32 parse_npu_mode_from_joint(const AX_CHAR* data, const AX_U32& data_size, AX_NPU_SDK_EX_HARD_MODE_T* pMode)
    {
        AX_NPU_SDK_EX_MODEL_TYPE_T npu_type;

        auto ret = AX_JOINT_GetJointModelType(data, data_size, &npu_type);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "[ERR]: Get joint model type failed. %X \n", ret);
            return -1;
        }

        if (AX_NPU_MODEL_TYPE_DEFUALT == npu_type)
        {
            fprintf(stdout, "[INFO]: Virtual npu was disabled!\n");
            *pMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_DISABLE;
        }
#ifdef AXERA_TARGET_CHIP_AX620
        else if (npu_type == AX_NPU_MODEL_TYPE_1_1_1 || npu_type == AX_NPU_MODEL_TYPE_1_1_2)
        {
            fprintf(stdout, "[INFO]: Virtual npu mode is 1_1\n\n");
            *pMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_1_1;
        }
#else
        else if (npu_type == AX_NPU_MODEL_TYPE_3_1_1 || npu_type == AX_NPU_MODEL_TYPE_3_1_2)
        {
            fprintf(stdout, "[INFO]: Virtual npu mode is 3_1\n\n");
            *pMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_3_1;
        }
        else if (npu_type == AX_NPU_MODEL_TYPE_2_2_1 || npu_type == AX_NPU_MODEL_TYPE_2_2_2)
        {
            fprintf(stdout, "[INFO]: Virtual npu mode is 2_2\n\n");
            *pMode = AX_NPU_SDK_EX_HARD_MODE_T::AX_NPU_VIRTUAL_2_2;
        }
#endif
        else
        {
            fprintf(stderr, "[ERR]: Unknown npu mode(%d).\n", (int)npu_type);
            return -1;
        }
        return ret;
    }

    AX_S32 alloc_joint_buffer(const AX_JOINT_IOMETA_T* pMeta, AX_JOINT_IO_BUFFER_T* pBuf, AX_JOINT_ALLOC_BUFFER_STRATEGY_T strategy = AX_JOINT_ABST_DEFAULT)
    {
        AX_JOINT_IOMETA_T meta = *pMeta;
        auto ret = AX_JOINT_AllocBuffer(&meta, pBuf, strategy);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "[ERR]: Cannot allocate memory.\n");
            return -1;
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }

    AX_S32 free_joint_buffer(AX_JOINT_IO_BUFFER_T* pBuf)
    {
        auto ret = AX_JOINT_FreeBuffer(pBuf);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "[ERR]: Free allocated memory failed.\n");
            return -1;
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }

    AX_S32 copy_to_device(const void* buf, void** vir_addr, long file_size)
    {
        memcpy(*vir_addr, buf, file_size);
        return 0;
    }

    AX_S32 copy_to_device(const void* buf, const size_t& size, AX_JOINT_IO_BUFFER_T* pBuf)
    {
        if (size > pBuf->nSize)
        {
            fprintf(stderr, "[ERR]: Target space is not large enough.\n");
            return -1;
        }

        std::memcpy(pBuf->pVirAddr, buf, size);
        return AX_ERR_NPU_JOINT_SUCCESS;
    }

    AX_S32 copy_to_device(const void* buf, const size_t& size, const AX_JOINT_IOMETA_T* pMeta, AX_JOINT_IO_BUFFER_T* pBuf)
    {
        if (size != pMeta->nSize)
        {
            fprintf(stderr, "[ERR]: Target space is not large enough.\n");
            return -1;
        }

        uint32_t write_len_wc = pMeta->pShape[3] * pMeta->pShape[2];
        uint32_t stride_len_wc = pBuf->pStride[1];
        for (int j = 0; j < pMeta->pShape[0]; j++)
        {
            uint32_t dst_offset = j * pBuf->pStride[0];
            uint32_t src_offset = pMeta->nSize * j / pMeta->pShape[0];

            for (int i = 0; i < pMeta->pShape[1]; i++)
            {
                auto src_ptr = (uint8_t*)buf + src_offset + i * write_len_wc;
                auto dst_ptr = (uint8_t*)pBuf->pVirAddr + dst_offset + i * stride_len_wc;
                memcpy(dst_ptr, src_ptr, write_len_wc);
            }
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }

    std::vector<int> io_get_input_size(const AX_JOINT_IO_INFO_T* io_info)
    {
        const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs;
        if (pMeta->nShapeSize <= 0)
        {
            fprintf(stderr, "[ERR] Dimension(%u) of shape is not allowed.\n", (uint32_t)pMeta->nShapeSize);
        }

        return {pMeta->pShape[1], pMeta->pShape[2]};
    }

    AX_JOINT_IO_BUFFER_T* prepare_io_no_copy(const size_t& size, AX_JOINT_IO_T& io, const AX_JOINT_IO_INFO_T* io_info, const uint32_t& batch = 1)
    {
        std::memset(&io, 0, sizeof(io));

        io.nInputSize = io_info->nInputSize;
        if (1 != io.nInputSize)
        {
            fprintf(stderr, "[ERR]: Only single input was accepted(got %u).\n", io.nInputSize);
            return nullptr;
        }
        io.pInputs = new AX_JOINT_IO_BUFFER_T[io.nInputSize];

        // fill input

        const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs;
        AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs;

        if (pMeta->nShapeSize <= 0)
        {
            fprintf(stderr, "[ERR]: Dimension(%u) of shape is not allowed.\n", (uint32_t)pMeta->nShapeSize);
            return nullptr;
        }

        auto actual_data_size = pMeta->nSize / pMeta->pShape[0] * batch;
        if (size != actual_data_size)
        {
            fprintf(stderr,
                    "[ERR]: The buffer size is not equal to model input(%s) size(%u vs %u).\n",
                    io_info->pInputs[0].pName,
                    (uint32_t)size,
                    actual_data_size);
            return nullptr;
        }

        auto ret = alloc_joint_buffer(pMeta, pBuf);
        if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        {
            fprintf(stderr, "[ERR]: Can not allocate memory for model input.\n");
            return nullptr;
        }

        //            ret = copy_to_device(buf, size, pBuf);
        //            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
        //            {
        //                fprintf(stderr, "[ERR]: Can not copy data to input.\n");
        //                return -1;
        //            }

        // deal with output
        io.nOutputSize = io_info->nOutputSize;
        io.pOutputs = new AX_JOINT_IO_BUFFER_T[io.nOutputSize];
        for (size_t i = 0; i < io.nOutputSize; ++i)
        {
            const AX_JOINT_IOMETA_T* pMeta = io_info->pOutputs + i;
            AX_JOINT_IO_BUFFER_T* pBuf = io.pOutputs + i;
            alloc_joint_buffer(pMeta, pBuf);
        }
        return pBuf;
    }

#ifdef AXERA_TARGET_CHIP_AX620
    AX_S32 prepare_io_npu_cv_image(AX_NPU_CV_Image* cv_image, AX_JOINT_IO_T& io, const AX_JOINT_IO_INFO_T* io_info, const uint32_t& batch = 1)
    {
        std::memset(&io, 0, sizeof(io));

        io.nInputSize = io_info->nInputSize;
        if (1 != io.nInputSize)
        {
            fprintf(stderr, "[ERR]: Only single input was accepted(got %u).\n", io.nInputSize);
            return -1;
        }
        io.pInputs = new AX_JOINT_IO_BUFFER_T[io.nInputSize];

        // fill input
        {
            const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs;
            AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs;

            if (pMeta->nShapeSize <= 0)
            {
                fprintf(stderr, "[ERR]: Dimension(%u) of shape is not allowed.\n", (uint32_t)pMeta->nShapeSize);
                return -1;
            }

            auto actual_data_size = pMeta->nSize / pMeta->pShape[0] * batch;
            if (cv_image->nSize != actual_data_size)
            {
                fprintf(stderr,
                        "[ERR]: The cv_image size is not equal to model input(%s) size(%u vs %u).\n",
                        io_info->pInputs[0].pName,
                        (uint32_t)cv_image->nSize,
                        actual_data_size);
                return -1;
            }

            pBuf->pVirAddr = cv_image->pVir;
            pBuf->phyAddr = cv_image->pPhy;
            pBuf->nSize = cv_image->nSize;
        }

        // deal with output
        {
            io.nOutputSize = io_info->nOutputSize;
            io.pOutputs = new AX_JOINT_IO_BUFFER_T[io.nOutputSize];
            for (size_t i = 0; i < io.nOutputSize; ++i)
            {
                const AX_JOINT_IOMETA_T* pMeta = io_info->pOutputs + i;
                AX_JOINT_IO_BUFFER_T* pBuf = io.pOutputs + i;
                alloc_joint_buffer(pMeta, pBuf);
            }
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }
#endif

    AX_S32 prepare_io_out_cache(const void* buf, const size_t& size, AX_JOINT_IO_T& io, const AX_JOINT_IO_INFO_T* io_info, const uint32_t& batch = 1)
    {
        std::memset(&io, 0, sizeof(io));

        io.nInputSize = io_info->nInputSize;
        if (1 != io.nInputSize)
        {
            fprintf(stderr, "[ERR]: Only single input was accepted(got %u).\n", io.nInputSize);
            return -1;
        }
        io.pInputs = new AX_JOINT_IO_BUFFER_T[io.nInputSize];

        // fill input
        {
            const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs;
            AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs;

            if (pMeta->nShapeSize <= 0)
            {
                fprintf(stderr, "[ERR]: Dimension(%u) of shape is not allowed.\n", (uint32_t)pMeta->nShapeSize);
                return -1;
            }

            auto actual_data_size = pMeta->nSize / pMeta->pShape[0] * batch;
            if (size != actual_data_size)
            {
                fprintf(stderr,
                        "[ERR]: The buffer size is not equal to model input(%s) size(%u vs %u).\n",
                        io_info->pInputs[0].pName,
                        (uint32_t)size,
                        actual_data_size);
                return -1;
            }

            auto ret = alloc_joint_buffer(pMeta, pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "[ERR]: Can not allocate memory for model input.\n");
                return -1;
            }

            ret = copy_to_device(buf, size, pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "[ERR]: Can not copy data to input.\n");
                return -1;
            }
        }

        // deal with output
        {
            io.nOutputSize = io_info->nOutputSize;
            io.pOutputs = new AX_JOINT_IO_BUFFER_T[io.nOutputSize];
            for (size_t i = 0; i < io.nOutputSize; ++i)
            {
                const AX_JOINT_IOMETA_T* pMeta = io_info->pOutputs + i;
                AX_JOINT_IO_BUFFER_T* pBuf = io.pOutputs + i;
                alloc_joint_buffer(pMeta, pBuf, AX_JOINT_ABST_CACHED);
            }
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }

    AX_S32 prepare_io(const void* buf, const size_t& size, AX_JOINT_IO_T& io, const AX_JOINT_IO_INFO_T* io_info, const uint32_t& batch = 1)
    {
        std::memset(&io, 0, sizeof(io));

        io.nInputSize = io_info->nInputSize;
        if (1 != io.nInputSize)
        {
            fprintf(stderr, "[ERR]: Only single input was accepted(got %u).\n", io.nInputSize);
            return -1;
        }
        io.pInputs = new AX_JOINT_IO_BUFFER_T[io.nInputSize];

        // fill input
        {
            const AX_JOINT_IOMETA_T* pMeta = io_info->pInputs;
            AX_JOINT_IO_BUFFER_T* pBuf = io.pInputs;

            if (pMeta->nShapeSize <= 0)
            {
                fprintf(stderr, "[ERR]: Dimension(%u) of shape is not allowed.\n", (uint32_t)pMeta->nShapeSize);
                return -1;
            }

            auto actual_data_size = pMeta->nSize / pMeta->pShape[0] * batch;
            if (size != actual_data_size)
            {
                fprintf(stderr,
                        "[ERR]: The buffer size is not equal to model input(%s) size(%u vs %u).\n",
                        io_info->pInputs[0].pName,
                        (uint32_t)size,
                        actual_data_size);
                return -1;
            }

            auto ret = alloc_joint_buffer(pMeta, pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "[ERR]: Can not allocate memory for model input.\n");
                return -1;
            }

            ret = copy_to_device(buf, size, pBuf);
            if (AX_ERR_NPU_JOINT_SUCCESS != ret)
            {
                fprintf(stderr, "[ERR]: Can not copy data to input.\n");
                return -1;
            }
        }

        // deal with output
        {
            io.nOutputSize = io_info->nOutputSize;
            io.pOutputs = new AX_JOINT_IO_BUFFER_T[io.nOutputSize];
            for (size_t i = 0; i < io.nOutputSize; ++i)
            {
                const AX_JOINT_IOMETA_T* pMeta = io_info->pOutputs + i;
                AX_JOINT_IO_BUFFER_T* pBuf = io.pOutputs + i;
                alloc_joint_buffer(pMeta, pBuf);
            }
        }
        return AX_ERR_NPU_JOINT_SUCCESS;
    }
} // namespace middleware
