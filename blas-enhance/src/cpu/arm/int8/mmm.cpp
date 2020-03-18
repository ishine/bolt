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
#include "error.h"
#include "cpu/arm/int8/blas_int8.h"
#include "cpu/arm/int8/mmm.h"


void matrix_matrix_multiply_tmp_bytes_int8(U32 row1, U32 col1, U32 row2, U32 col2, DataType dt, U32 *bytes) {
    *bytes = row1 * col1 + row2 * col2;
    *bytes *= bytesOf(dt);
    *bytes += 32;
}

EE mmm_int8(int M, int N, int K, INT8* matrix1, INT8* matrix2, INT8* tmp, I32* result, Arch arch) {
    EE ret = SUCCESS;
    switch (arch) {
       case ARM_A55:
           mmm_A55(M, N, K, matrix1, matrix2, tmp, result);
           break;
       case ARM_A76:
           mmm_A76(M, N, K, matrix1, matrix2, tmp, result);
           break;
       default:
           ret = NOT_SUPPORTED;
           break;
    }
    return ret;
}
