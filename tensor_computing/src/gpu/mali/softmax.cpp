// Copyright (C) 2019. Huawei Technologies Co., Ltd. All rights reserved.

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "sys.h"
#include "type.h"
#include "tensor_desc.h"
#include "error.h"
#include "gpu/mali/tensor_computing_mali.h"
#include "gpu/mali/fp16/softmax_mali_fp16.h"
#include "gpu/mali/infer_gclmem_desc_mali.h"

EE softmax_infer_output_size_mali(TensorDesc   inputDesc,
                                  TensorDesc*  outputDesc,
                                  GCLMemDesc_t gclmemInputDesc,
                                  GCLMemDesc_t gclmemOutputDesc){
    /*tensorDesc record cpu org data format info*/
    /*gclmemDesc record gpu trans data format info*/
    *outputDesc = inputDesc;

    DataType   idt;
    DataFormat idf;
    U32 iw, ih, ic, in;
    tensorSelectGet(inputDesc,  &idt, &idf, &in, &ic, &ih, &iw);

    if(idf == DF_NCHW) {
        CHECK_STATUS(infer_gclmem_desc_ncwhc4(iw, ih, ic, 0, 0, iw, ih, ic, idt, idt, gclmemInputDesc, gclmemOutputDesc));
        return SUCCESS;
    } 
    return NOT_SUPPORTED;
}

inline EE softmax_checkpara_mali(GCLHandle_t handle, 
                                 TensorDesc  inputDesc,
                                 GCLMem_t    input,
                                 TensorDesc  outputDesc,
                                 GCLMem_t    output) {

    if(handle == nullptr || nullptr == input || nullptr == output) return NULL_POINTER;
    if(input->desc.memFormat != output->desc.memFormat) return NOT_SUPPORTED;
    if(inputDesc.df != outputDesc.df)           return NOT_SUPPORTED;
    if(inputDesc.dims[0] != outputDesc.dims[0]) return NOT_SUPPORTED;
    if(inputDesc.dims[1] != outputDesc.dims[1]) return NOT_SUPPORTED;
    if(inputDesc.dims[2] != outputDesc.dims[2]) return NOT_SUPPORTED;
    if(inputDesc.dims[3] != outputDesc.dims[3]) return NOT_SUPPORTED;
    if(outputDesc.df != DF_NCHW)            return NOT_SUPPORTED;
    if(output->desc.memFormat != DF_NCWHC4) return NOT_SUPPORTED;
    return SUCCESS; 
}

EE softmax_mali(GCLHandle_t handle, 
                TensorDesc  inputDesc,
                GCLMem_t    input,
                TensorDesc  outputDesc,
                GCLMem_t    output) {
    EE ret = SUCCESS;
    CHECK_STATUS(softmax_checkpara_mali(handle, inputDesc, input, outputDesc, output));
    switch(inputDesc.dt){
        case DT_F16:{
            ret = softmax_mali_fp16(handle, inputDesc, input, outputDesc, output);
            break;
        }
        case DT_I8:{
            ret = NOT_SUPPORTED;
            break;
        }
        default:
            ret = NOT_SUPPORTED;
            break;
    }
    return ret;
}



