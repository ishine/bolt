// Copyright (C) 2019. Huawei Technologies Co., Ltd. All rights reserved.

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "cpu/arm/fp16/convolution_gemm_icnchw.h"

EE convolution_gemm_icnchw_A55(TensorDesc inputDesc, F16* inArray,
    TensorDesc filterDesc, const F16* filterArray,
    ConvolutionDesc convDesc,
    TensorDesc biasDesc, const F16* biasArray,
    U32 tmpBytes, void* tmp,
    TensorDesc outputDesc, F16* outArray,
    ActivationMode activationMode)
{
    UNUSED(biasDesc);
    UNUSED(tmpBytes);

    DataType idt, fdt, odt;
    DataFormat idf, fdf, odf;
    U32 in, ic, ih, iw;
    U32 fn, fc, fh, fw;
    U32 on, oc, oh, ow;
    CHECK_STATUS(tensor4dGet(inputDesc, &idt, &idf, &in, &ic, &ih, &iw));
    CHECK_STATUS(tensor4dGet(filterDesc, &fdt, &fdf, &fn, &fc, &fh, &fw));
    CHECK_STATUS(tensor4dGet(outputDesc, &odt, &odf, &on, &oc, &oh, &ow));
    U32 strideH = convDesc.stride_h;
    U32 strideW = convDesc.stride_w;
    U32 paddingT = convDesc.padding_top;
    U32 paddingB = convDesc.padding_bottom;
    U32 paddingL = convDesc.padding_left;
    U32 paddingR = convDesc.padding_right;
    U32 dilateH = convDesc.dilatedRate_h;
    U32 dilateW = convDesc.dilatedRate_w;

    if (fdf != DF_NHWCN16)
        CHECK_STATUS(NOT_MATCH);

    I64 activation = 0;
    switch (activationMode) {
        case ACTIVATION_NULL:
            activation = 0;
            break;
        case ACTIVATION_RELU:
            activation = 1;
            break;
        default:
            return NOT_SUPPORTED;
    }

    oc /= 8;
    U32 ih_pad = ih + paddingT + paddingB;
    U32 iw_pad = iw + paddingL + paddingR;
    I32 ohow = oh*ow;
    U32 ihiw = ih_pad*iw_pad;
    F16 *inArray_pad;
    EE ret = SUCCESS;
    for (U32 n = 0; n < in; n++) {
        if (paddingT == 0 && paddingB == 0 && paddingL == 0 && paddingR == 0) {
            inArray_pad = inArray + n*ic*ih*iw;
        } else {
            // copy input into a input with padding
            inArray_pad = (F16*)tmp;
            F16 *inArray_pad_mov = inArray_pad;
            F16 *inArray_mov = inArray + n*ic*ih*iw;
            for (U32 c = 0; c < ic; c++) {
                for (U32 h = 0; h < paddingT; h++) {
                    memset(inArray_pad_mov, 0, iw_pad*bytesOf(idt));
                    inArray_pad_mov += iw_pad;
                }
                for (U32 h = paddingT; h < ih_pad - paddingB; h++) {
                    memset(inArray_pad_mov, 0, paddingL*bytesOf(idt));
                    inArray_pad_mov += paddingL;
                    memcpy(inArray_pad_mov, inArray_mov, iw*bytesOf(idt));
                    inArray_pad_mov += iw;
                    inArray_mov += iw;
                    memset(inArray_pad_mov, 0, paddingR*bytesOf(idt));
                    inArray_pad_mov += paddingR;
                }
                for (U32 h = ih_pad - paddingB; h < ih_pad; h++) {
                    memset(inArray_pad_mov, 0, iw_pad*bytesOf(idt));
                    inArray_pad_mov += iw_pad;
                }
            }
        }

        // ohow / 8
        for (I32 hw = 0; hw < ohow-7; hw+=8) {
            const F16 *b0 = biasArray;
            const F16 *b1 = biasArray + 8;
            const F16 *f_o0c0 = filterArray;
            F16 *in_pack = ((F16*)tmp) + ic*ih_pad*iw_pad;
            // pack input
            // NCHW => NHWChw8 + im2col
            U32 in_h_0 = (hw/ow)*strideH;
            U32 in_w_0 = (hw%ow)*strideW;
            U32 in_h_1 = ((hw+1)/ow)*strideH;
            U32 in_w_1 = ((hw+1)%ow)*strideW;
            U32 in_h_2 = ((hw+2)/ow)*strideH;
            U32 in_w_2 = ((hw+2)%ow)*strideW;
            U32 in_h_3 = ((hw+3)/ow)*strideH;
            U32 in_w_3 = ((hw+3)%ow)*strideW;
            U32 in_h_4 = ((hw+4)/ow)*strideH;
            U32 in_w_4 = ((hw+4)%ow)*strideW;
            U32 in_h_5 = ((hw+5)/ow)*strideH;
            U32 in_w_5 = ((hw+5)%ow)*strideW;
            U32 in_h_6 = ((hw+6)/ow)*strideH;
            U32 in_w_6 = ((hw+6)%ow)*strideW;
            U32 in_h_7 = ((hw+7)/ow)*strideH;
            U32 in_w_7 = ((hw+7)%ow)*strideW;
            for (U32 c = 0; c < ic; c++) {
                for (U32 fh_idx = 0; fh_idx < fh; fh_idx++) {
                    for (U32 fw_idx = 0; fw_idx < fw; fw_idx++) {
                        F16 *in_hw = inArray_pad + c*ihiw + fh_idx*dilateH*iw_pad+ dilateW*fw_idx;
                        F16 *in_0 = in_hw + in_h_0*iw_pad + in_w_0;
                        F16 *in_1 = in_hw + in_h_1*iw_pad + in_w_1;
                        F16 *in_2 = in_hw + in_h_2*iw_pad + in_w_2;
                        F16 *in_3 = in_hw + in_h_3*iw_pad + in_w_3;
                        F16 *in_4 = in_hw + in_h_4*iw_pad + in_w_4;
                        F16 *in_5 = in_hw + in_h_5*iw_pad + in_w_5;
                        F16 *in_6 = in_hw + in_h_6*iw_pad + in_w_6;
                        F16 *in_7 = in_hw + in_h_7*iw_pad + in_w_7;
                        F16 *in_pack_hw8 = in_pack + fh_idx*fw*ic*8 + fw_idx*ic*8 + c*8;
                        *in_pack_hw8 = *in_0;
                        *(in_pack_hw8+1) = *in_1;
                        *(in_pack_hw8+2) = *in_2;
                        *(in_pack_hw8+3) = *in_3;
                        *(in_pack_hw8+4) = *in_4;
                        *(in_pack_hw8+5) = *in_5;
                        *(in_pack_hw8+6) = *in_6;
                        *(in_pack_hw8+7) = *in_7;
                    }
                }
            }

            // compute
            for (I32 o = 0; o < I32(oc-1); o+=2) {
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + o*ohow*8 + hw*8;
                F16 *out_o1hw0 = out_o0hw0 + ohow*8;
                // bias
                const F16 *b_o0 = b0;
                const F16 *b_o1 = b1;
                __asm__ __volatile__(
                    "ldr d22, [%[b_0]]\n"       //b_o0
                    "ldr  x1, [%[b_0], #8]\n"
                    "ins v22.d[1], x1\n"
                    "ldr d23, [%[b_1]]\n"       //b_o1
                    "ldr  x2, [%[b_1], #8]\n"
                    "ins v23.d[1], x2\n"
                    "mov  x0, %[ic]\n"             //ic_blk
                    "mov  v2.16b, v22.16b\n"      //out_o0hw0
                    "ldr  d0, [%[in_0]]\n"           //in_hw0
                    "mov  v3.16b, v22.16b\n"      //out_o0hw1
                    "ldr  x1, [%[in_0], #8]\n"
                    "mov  v4.16b, v22.16b\n"      //out_o0hw2
                    "ins  v0.d[1], x1\n"
                    "mov  v5.16b, v22.16b\n"      //out_o0hw3
                    "ldr d18, [%[f_0]]\n"            //f_o0c0
                    "mov  v6.16b, v22.16b\n"      //out_o0hw4
                    "ldr  x2, [%[f_0], #8]\n"
                    "mov  v7.16b, v22.16b\n"      //out_o0hw5
                    "ins v18.d[1], x2\n"
                    "mov  v8.16b, v22.16b\n"      //out_o0hw6
                    "ldr d19, [%[f_0], #16]\n"            //f_o1c0
                    "mov  v9.16b, v22.16b\n"      //out_o0hw7
                    "ldr  x3, [%[f_0], #24]\n"
                    "mov v10.16b, v23.16b\n"      //out_o1hw0
                    "ins v19.d[1], x3\n"
                    "mov v11.16b, v23.16b\n"      //out_o1hw1
                    "mov v12.16b, v23.16b\n"      //out_o1hw2
                    "mov v13.16b, v23.16b\n"      //out_o1hw3
                    "mov v14.16b, v23.16b\n"      //out_o1hw4
                    "mov v15.16b, v23.16b\n"      //out_o1hw5
                    "mov v16.16b, v23.16b\n"      //out_o1hw6
                    "mov v17.16b, v23.16b\n"      //out_o1hw7

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr  d1, [%[in_0], #16]\n"           //in_hw0
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "ldr  x1, [%[in_0], #24]\n"
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "ins  v1.d[1], x1\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "ldr d20, [%[f_0], #32]\n"            //f_o0c0
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"
                    "ldr  x2, [%[f_0], #40]\n"
                    "fmla  v6.8h, v18.8h, v0.h[4]\n"
                    "ins v20.d[1], x2\n"
                    "fmla  v7.8h, v18.8h, v0.h[5]\n"
                    "ldr d21, [%[f_0], #48]\n"            //f_o1c0
                    "fmla  v8.8h, v18.8h, v0.h[6]\n"
                    "ldr  x3, [%[f_0], #56]\n"
                    "fmla  v9.8h, v18.8h, v0.h[7]\n"
                    "ins v21.d[1], x3\n"
                    "fmla v10.8h, v19.8h, v0.h[0]\n"
                    "fmla v11.8h, v19.8h, v0.h[1]\n"
                    "fmla v12.8h, v19.8h, v0.h[2]\n"
                    "fmla v13.8h, v19.8h, v0.h[3]\n"
                    "fmla v14.8h, v19.8h, v0.h[4]\n"
                    "fmla v15.8h, v19.8h, v0.h[5]\n"
                    "fmla v16.8h, v19.8h, v0.h[6]\n"
                    "fmla v17.8h, v19.8h, v0.h[7]\n"

                    "ldr  d0, [%[in_0], #32]\n"           //in_hw0
                    "fmla  v2.8h, v20.8h, v1.h[0]\n"
                    "ldr  x1, [%[in_0], #40]\n"
                    "fmla  v3.8h, v20.8h, v1.h[1]\n"
                    "ins  v0.d[1], x1\n"
                    "fmla  v4.8h, v20.8h, v1.h[2]\n"
                    "ldr d18, [%[f_0], #64]\n"            //f_o0c0
                    "fmla  v5.8h, v20.8h, v1.h[3]\n"
                    "ldr  x2, [%[f_0], #72]\n"
                    "fmla  v6.8h, v20.8h, v1.h[4]\n"
                    "ins v18.d[1], x2\n"
                    "fmla  v7.8h, v20.8h, v1.h[5]\n"
                    "ldr d19, [%[f_0], #80]\n"            //f_o1c0
                    "fmla  v8.8h, v20.8h, v1.h[6]\n"
                    "ldr  x3, [%[f_0], #88]\n"
                    "fmla  v9.8h, v20.8h, v1.h[7]\n"
                    "ins v19.d[1], x3\n"
                    "fmla v10.8h, v21.8h, v1.h[0]\n"
                    "add %[in_0], %[in_0], #32\n"
                    "fmla v11.8h, v21.8h, v1.h[1]\n"
                    "add %[f_0], %[f_0], #64\n"
                    "fmla v12.8h, v21.8h, v1.h[2]\n"
                    "sub x0, x0, #2\n"
                    "fmla v13.8h, v21.8h, v1.h[3]\n"
                    "fmla v14.8h, v21.8h, v1.h[4]\n"
                    "fmla v15.8h, v21.8h, v1.h[5]\n"
                    "fmla v16.8h, v21.8h, v1.h[6]\n"
                    "fmla v17.8h, v21.8h, v1.h[7]\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"
                    "fmla  v6.8h, v18.8h, v0.h[4]\n"
                    "fmla  v7.8h, v18.8h, v0.h[5]\n"
                    "fmla  v8.8h, v18.8h, v0.h[6]\n"
                    "fmla  v9.8h, v18.8h, v0.h[7]\n"
                    "add %[f_0], %[f_0], #32\n"
                    "fmla v10.8h, v19.8h, v0.h[0]\n"
                    "fmla v11.8h, v19.8h, v0.h[1]\n"
                    "fmla v12.8h, v19.8h, v0.h[2]\n"
                    "fmla v13.8h, v19.8h, v0.h[3]\n"
                    "fmla v14.8h, v19.8h, v0.h[4]\n"
                    "fmla v15.8h, v19.8h, v0.h[5]\n"
                    "fmla v16.8h, v19.8h, v0.h[6]\n"
                    "fmla v17.8h, v19.8h, v0.h[7]\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "fmax  v3.8h,  v3.8h, v1.8h\n"
                    "fmax  v4.8h,  v4.8h, v1.8h\n"
                    "fmax  v5.8h,  v5.8h, v1.8h\n"
                    "fmax  v6.8h,  v6.8h, v1.8h\n"
                    "fmax  v7.8h,  v7.8h, v1.8h\n"
                    "fmax  v8.8h,  v8.8h, v1.8h\n"
                    "fmax  v9.8h,  v9.8h, v1.8h\n"
                    "fmax v10.8h, v10.8h, v1.8h\n"
                    "fmax v11.8h, v11.8h, v1.8h\n"
                    "fmax v12.8h, v12.8h, v1.8h\n"
                    "fmax v13.8h, v13.8h, v1.8h\n"
                    "fmax v14.8h, v14.8h, v1.8h\n"
                    "fmax v15.8h, v15.8h, v1.8h\n"
                    "fmax v16.8h, v16.8h, v1.8h\n"
                    "fmax v17.8h, v17.8h, v1.8h\n"

                    "3:\n"
                    "str   q2, [%[out_0]]\n"           //out_o0hw0
                    "str   q3, [%[out_0], #16]\n"      //out_o0hw1
                    "str   q4, [%[out_0], #32]\n"      //out_o0hw2
                    "str   q5, [%[out_0], #48]\n"      //out_o0hw3
                    "str   q6, [%[out_0], #64]\n"      //out_o0hw4
                    "str   q7, [%[out_0], #80]\n"      //out_o0hw5
                    "str   q8, [%[out_0], #96]\n"      //out_o0hw6
                    "str   q9, [%[out_0], #112]\n"     //out_o0hw7
                    "str  q10, [%[out_1]]\n"           //out_o1hw0
                    "str  q11, [%[out_1], #16]\n"      //out_o1hw1
                    "str  q12, [%[out_1], #32]\n"      //out_o1hw2
                    "str  q13, [%[out_1], #48]\n"      //out_o1hw3
                    "str  q14, [%[out_1], #64]\n"      //out_o1hw4
                    "str  q15, [%[out_1], #80]\n"      //out_o1hw5
                    "str  q16, [%[out_1], #96]\n"      //out_o1hw6
                    "str  q17, [%[out_1], #112]\n"     //out_o1hw7
                    :[out_0]"+r"(out_o0hw0),
                     [out_1]"+r"(out_o1hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_o0c0)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [b_1]"r"(b_o1),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
                        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
                        "v21", "v22", "v23", "x0", "x1", "x2", "x3"
                );
                b0 += 16;
                b1 += 16;
            }
            if (oc & 1) {
                // oc%2 != 0
                const F16 *f_r = filterArray + (oc-1)*8*fh*fw*ic;
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + (oc-1)*ohow*8 + hw*8;
                // bias
                const F16 *b_o0 = biasArray + (oc-1)*8;
                __asm__ __volatile__(
                    "ldr q12, [%[b_0]]\n"       //b_o0
                    "mov x0, %[ic]\n" // ic_blk
                    "ldr d0, [%[in_0]]\n"   //in_hw0
                    "mov v2.16b, v12.16b\n" //out_o0hw0
                    "ldr x1, [%[in_0], #8]\n"
                    "mov v3.16b, v12.16b\n" //out_o0hw1
                    "ins v0.d[1], x1\n"
                    "mov v4.16b, v12.16b\n" //out_o0hw2
                    "ldr d10, [%[f_0]]\n"   //f_o0c0
                    "mov v5.16b, v12.16b\n" //out_o0hw3
                    "ldr x2, [%[f_0], #8]\n"
                    "mov v6.16b, v12.16b\n" //out_o0hw4
                    "ins v10.d[1], x2\n"
                    "mov v7.16b, v12.16b\n" //out_o0hw5
                    "mov v8.16b, v12.16b\n" //out_o0hw6
                    "mov v9.16b, v12.16b\n" //out_o0hw7

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr d1, [%[in_0], #16]\n" //in_hw0
                    "fmla v2.8h, v10.8h, v0.h[0]\n"
                    "ldr x1, [%[in_0], #24]\n"
                    "fmla v3.8h, v10.8h, v0.h[1]\n"
                    "ins v1.d[1], x1\n"
                    "fmla v4.8h, v10.8h, v0.h[2]\n"
                    "ldr d11, [%[f_0], #16]\n" //f_o0c0
                    "fmla v5.8h, v10.8h, v0.h[3]\n"
                    "ldr x2, [%[f_0], #24]\n"
                    "fmla v6.8h, v10.8h, v0.h[4]\n"
                    "ins v11.d[1], x2\n"
                    "fmla v7.8h, v10.8h, v0.h[5]\n"
                    "sub x0, x0, #2\n"
                    "fmla v8.8h, v10.8h, v0.h[6]\n"
                    "fmla v9.8h, v10.8h, v0.h[7]\n"

                    "ldr d0, [%[in_0], #32]\n" //in_hw0
                    "fmla v2.8h, v11.8h, v1.h[0]\n"
                    "ldr x1, [%[in_0], #40]\n"
                    "fmla v3.8h, v11.8h, v1.h[1]\n"
                    "ins v0.d[1], x1\n"
                    "fmla v4.8h, v11.8h, v1.h[2]\n"
                    "ldr d10, [%[f_0], #32]\n" //f_o0c0
                    "fmla v5.8h, v11.8h, v1.h[3]\n"
                    "ldr x2, [%[f_0], #40]\n"
                    "fmla v6.8h, v11.8h, v1.h[4]\n"
                    "ins v10.d[1], x2\n"
                    "fmla v7.8h, v11.8h, v1.h[5]\n"
                    "add %[in_0], %[in_0], #32\n"
                    "fmla v8.8h, v11.8h, v1.h[6]\n"
                    "add %[f_0], %[f_0], #32\n"
                    "fmla v9.8h, v11.8h, v1.h[7]\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v10.8h, v0.h[0]\n"
                    "fmla  v3.8h, v10.8h, v0.h[1]\n"
                    "fmla  v4.8h, v10.8h, v0.h[2]\n"
                    "fmla  v5.8h, v10.8h, v0.h[3]\n"
                    "add %[f_0], %[f_0], #16\n"
                    "fmla  v6.8h, v10.8h, v0.h[4]\n"
                    "fmla  v7.8h, v10.8h, v0.h[5]\n"
                    "fmla  v8.8h, v10.8h, v0.h[6]\n"
                    "fmla  v9.8h, v10.8h, v0.h[7]\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "fmax  v3.8h,  v3.8h, v1.8h\n"
                    "fmax  v4.8h,  v4.8h, v1.8h\n"
                    "fmax  v5.8h,  v5.8h, v1.8h\n"
                    "fmax  v6.8h,  v6.8h, v1.8h\n"
                    "fmax  v7.8h,  v7.8h, v1.8h\n"
                    "fmax  v8.8h,  v8.8h, v1.8h\n"
                    "fmax  v9.8h,  v9.8h, v1.8h\n"

                    "3:\n"
                    "str q2, [%[out_0]]\n" //out_o0hw0
                    "str q3, [%[out_0], #16]\n" //out_o0hw0
                    "str q4, [%[out_0], #32]\n" //out_o0hw0
                    "str q5, [%[out_0], #48]\n" //out_o0hw0
                    "str q6, [%[out_0], #64]\n" //out_o0hw0
                    "str q7, [%[out_0], #80]\n" //out_o0hw0
                    "str q8, [%[out_0], #96]\n" //out_o0hw0
                    "str q9, [%[out_0], #112]\n" //out_o0hw0
                    :[out_0]"+r"(out_o0hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_r)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "x0", "x1", "x2"
                );
            }
        }
        // ohow_reminder % 8 / 4
        U32 ohow_s = (ohow / 8) * 8;
        //U32 ohow_s = (ohow/8)*8;
        for (I32 hw = ohow_s; hw < ohow-3; hw+=4) {
            const F16 *b0 = biasArray;
            const F16 *b1 = biasArray + 8;
            const F16 *f_o0c0 = filterArray;
            F16 *in_pack = ((F16*)tmp) + ic*ih_pad*iw_pad;

            // pack input
            // NCHWc8 => NHWChw4 + im2col
            U32 in_h_0 = (hw/ow)*strideH;
            U32 in_w_0 = (hw%ow)*strideW;
            U32 in_h_1 = ((hw+1)/ow)*strideH;
            U32 in_w_1 = ((hw+1)%ow)*strideW;
            U32 in_h_2 = ((hw+2)/ow)*strideH;
            U32 in_w_2 = ((hw+2)%ow)*strideW;
            U32 in_h_3 = ((hw+3)/ow)*strideH;
            U32 in_w_3 = ((hw+3)%ow)*strideW;
            for (U32 c = 0; c < ic; c++) {
                for (U32 fh_idx = 0; fh_idx < fh; fh_idx++) {
                    for (U32 fw_idx = 0; fw_idx < fw; fw_idx++) {
                        F16 *in_hw = inArray_pad + c*ihiw + fh_idx*dilateH*iw_pad + dilateW*fw_idx;
                        F16 *in_0 = in_hw + in_h_0*iw_pad + in_w_0;
                        F16 *in_1 = in_hw + in_h_1*iw_pad + in_w_1;
                        F16 *in_2 = in_hw + in_h_2*iw_pad + in_w_2;
                        F16 *in_3 = in_hw + in_h_3*iw_pad + in_w_3;
                        F16 *in_pack_hw4 = in_pack + fh_idx*fw*ic*4 + fw_idx*ic*4 + c*4;
                        *in_pack_hw4 = *in_0;
                        *(in_pack_hw4+1) = *in_1;
                        *(in_pack_hw4+2) = *in_2;
                        *(in_pack_hw4+3) = *in_3;
                    }
                }
            }

            // compute
            for (I32 o = 0; o < I32(oc-1); o+=2) {
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + o*ohow*8 + hw*8;
                F16 *out_o1hw0 = out_o0hw0 + ohow*8;
                // bias
                const F16 *b_o0 = b0;
                const F16 *b_o1 = b1;
                __asm__ __volatile__(
                    "ldr d22, [%[b_0]]\n"       //b_o0
                    "ldr  x1, [%[b_0], #8]\n"
                    "ins v22.d[1], x1\n"
                    "ldr d23, [%[b_1]]\n"       //b_o1
                    "ldr  x2, [%[b_1], #8]\n"
                    "ins v23.d[1], x2\n"
                    "mov  x0, %[ic]\n"             //ic_blk
                    "mov  v2.16b, v22.16b\n"      //out_o0hw0
                    "ldr  d0, [%[in_0]]\n"           //in_hw0
                    "mov  v3.16b, v22.16b\n"      //out_o0hw1
                    "ldr d18, [%[f_0]]\n"            //f_o0c0
                    "mov  v4.16b, v22.16b\n"      //out_o0hw2
                    "ldr  x2, [%[f_0], #8]\n"
                    "mov  v5.16b, v22.16b\n"      //out_o0hw3
                    "ins v18.d[1], x2\n"
                    "mov v10.16b, v23.16b\n"      //out_o1hw0
                    "ldr d19, [%[f_0], #16]\n"            //f_o1c0
                    "mov v11.16b, v23.16b\n"      //out_o1hw1
                    "ldr  x3, [%[f_0], #24]\n"
                    "mov v12.16b, v23.16b\n"      //out_o1hw2
                    "ins v19.d[1], x3\n"
                    "mov v13.16b, v23.16b\n"      //out_o1hw3

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr  d1, [%[in_0], #8]\n"           //in_hw0
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "ldr d20, [%[f_0], #32]\n"            //f_o0c0
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "ldr  x2, [%[f_0], #40]\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "ins v20.d[1], x2\n"
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"
                    "ldr d21, [%[f_0], #48]\n"            //f_o1c0
                    "fmla v10.8h, v19.8h, v0.h[0]\n"
                    "ldr  x3, [%[f_0], #56]\n"
                    "fmla v11.8h, v19.8h, v0.h[1]\n"
                    "ins v21.d[1], x3\n"
                    "fmla v12.8h, v19.8h, v0.h[2]\n"
                    "sub x0, x0, #2\n"
                    "fmla v13.8h, v19.8h, v0.h[3]\n"

                    "ldr  d0, [%[in_0], #16]\n"           //in_hw0
                    "fmla  v2.8h, v20.8h, v1.h[0]\n"
                    "ldr d18, [%[f_0], #64]\n"            //f_o0c0
                    "fmla  v3.8h, v20.8h, v1.h[1]\n"
                    "ldr  x2, [%[f_0], #72]\n"
                    "fmla  v4.8h, v20.8h, v1.h[2]\n"
                    "ldr d19, [%[f_0], #80]\n"            //f_o1c0
                    "fmla  v5.8h, v20.8h, v1.h[3]\n"
                    "ins v18.d[1], x2\n"
                    "fmla v10.8h, v21.8h, v1.h[0]\n"
                    "ldr  x3, [%[f_0], #88]\n"
                    "fmla v11.8h, v21.8h, v1.h[1]\n"
                    "ins v19.d[1], x3\n"
                    "fmla v12.8h, v21.8h, v1.h[2]\n"
                    "add %[in_0], %[in_0], #16\n"
                    "fmla v13.8h, v21.8h, v1.h[3]\n"
                    "add %[f_0], %[f_0], #64\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"
                    "add %[f_0], %[f_0], #32\n"
                    "fmla v10.8h, v19.8h, v0.h[0]\n"
                    "fmla v11.8h, v19.8h, v0.h[1]\n"
                    "fmla v12.8h, v19.8h, v0.h[2]\n"
                    "fmla v13.8h, v19.8h, v0.h[3]\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "fmax  v3.8h,  v3.8h, v1.8h\n"
                    "fmax  v4.8h,  v4.8h, v1.8h\n"
                    "fmax  v5.8h,  v5.8h, v1.8h\n"
                    "fmax v10.8h, v10.8h, v1.8h\n"
                    "fmax v11.8h, v11.8h, v1.8h\n"
                    "fmax v12.8h, v12.8h, v1.8h\n"
                    "fmax v13.8h, v13.8h, v1.8h\n"

                    "3:\n"
                    "str   q2, [%[out_0]]\n"           //out_o0hw0
                    "str   q3, [%[out_0], #16]\n"      //out_o0hw1
                    "str   q4, [%[out_0], #32]\n"      //out_o0hw2
                    "str   q5, [%[out_0], #48]\n"      //out_o0hw3
                    "str  q10, [%[out_1]]\n"           //out_o1hw0
                    "str  q11, [%[out_1], #16]\n"      //out_o1hw1
                    "str  q12, [%[out_1], #32]\n"      //out_o1hw2
                    "str  q13, [%[out_1], #48]\n"      //out_o1hw3
                    :[out_0]"+r"(out_o0hw0),
                     [out_1]"+r"(out_o1hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_o0c0)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [b_1]"r"(b_o1),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v10", "v11", "v12", "v13", "v18", "v19", "v20", "v21", "v22", "v23", "x0", "x1", "x2", "x3"
                );
                b0 += 16;
                b1 += 16;
            }
            if (oc & 1) {
                // oc%2 != 0
                const F16 *f_r = filterArray + (oc-1)*8*fh*fw*ic;
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + (oc-1)*ohow*8 + hw*8;
                // bias
                const F16 *b_o0 = biasArray + (oc-1)*8;
                __asm__ __volatile__(
                    "ldr d22, [%[b_0]]\n"       //b_o0
                    "ldr  x1, [%[b_0], #8]\n"
                    "ins v22.d[1], x1\n"
                    "mov  x0, %[ic]\n"             //ic_blk
                    "mov  v2.16b, v22.16b\n"      //out_o0hw0
                    "ldr  d0, [%[in_0]]\n"           //in_hw0
                    "mov  v3.16b, v22.16b\n"      //out_o0hw1
                    "ldr d18, [%[f_0]]\n"            //f_o0c0
                    "mov  v4.16b, v22.16b\n"      //out_o0hw2
                    "ldr  x2, [%[f_0], #8]\n"
                    "mov  v5.16b, v22.16b\n"      //out_o0hw3
                    "ins v18.d[1], x2\n"

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr  d1, [%[in_0], #8]\n"           //in_hw0
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "ldr d20, [%[f_0], #16]\n"            //f_o0c0
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "ldr  x2, [%[f_0], #24]\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "ins v20.d[1], x2\n"
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"
                    "sub x0, x0, #2\n"

                    "ldr  d0, [%[in_0], #16]\n"           //in_hw0
                    "fmla  v2.8h, v20.8h, v1.h[0]\n"
                    "ldr d18, [%[f_0], #32]\n"            //f_o0c0
                    "fmla  v3.8h, v20.8h, v1.h[1]\n"
                    "ldr  x2, [%[f_0], #40]\n"
                    "fmla  v4.8h, v20.8h, v1.h[2]\n"
                    "ins v18.d[1], x2\n"
                    "fmla  v5.8h, v20.8h, v1.h[3]\n"
                    "add %[in_0], %[in_0], #16\n"
                    "add %[f_0], %[f_0], #32\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "fmla  v3.8h, v18.8h, v0.h[1]\n"
                    "add %[f_0], %[f_0], #16\n"
                    "fmla  v4.8h, v18.8h, v0.h[2]\n"
                    "fmla  v5.8h, v18.8h, v0.h[3]\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "fmax  v3.8h,  v3.8h, v1.8h\n"
                    "fmax  v4.8h,  v4.8h, v1.8h\n"
                    "fmax  v5.8h,  v5.8h, v1.8h\n"

                    "3:\n"
                    "str   q2, [%[out_0]]\n"           //out_o0hw0
                    "str   q3, [%[out_0], #16]\n"      //out_o0hw1
                    "str   q4, [%[out_0], #32]\n"      //out_o0hw2
                    "str   q5, [%[out_0], #48]\n"      //out_o0hw3
                    :[out_0]"+r"(out_o0hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_r)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v18", "v20", "v22", "x0", "x1", "x2"
                );
            }
        }
        // ohow_reminder % 4
        ohow_s = (ohow / 4) * 4;
        for (I32 hw = ohow_s; hw < ohow; hw++) {
            const F16 *b0 = biasArray;
            const F16 *b1 = biasArray + 8;
            const F16 *f_o0c0 = filterArray;
            F16 *in_pack = ((F16*)tmp) + ic*ih_pad*iw_pad;
            // pack input
            // NCHWc8 => NHWChw4 + im2col
            U32 in_h_0 = (hw/ow)*strideH;
            U32 in_w_0 = (hw%ow)*strideW;
            for (U32 c = 0; c < ic; c++) {
                for (U32 fh_idx = 0; fh_idx < fh; fh_idx++) {
                    for (U32 fw_idx = 0; fw_idx < fw; fw_idx++) {
                        F16 *in_hw = inArray_pad + c*ihiw + fh_idx*dilateH*iw_pad + dilateW*fw_idx;
                        F16 *in_0 = in_hw + in_h_0*iw_pad + in_w_0;
                        F16 *in_pack_hw1 = in_pack + fh_idx*fw*ic + fw_idx*ic + c;
                        /*
                         * for (U32 c8 = 0; c8 < 8; c8++) {
                         *         in_pack_c8hw1[c8] = in_0[c8];
                         * }
                         */
                        *in_pack_hw1 = *in_0;
                    }
                }
            }

            // compute
            for (I32 o = 0; o < I32(oc-1); o+=2) {
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + o*ohow*8 + hw*8;
                F16 *out_o1hw0 = out_o0hw0 + ohow*8;
                // bias
                const F16 *b_o0 = b0;
                const F16 *b_o1 = b1;
                __asm__ __volatile__(
                    "ldr d22, [%[b_0]]\n"       //b_o0
                    "ldr  x1, [%[b_0], #8]\n"
                    "ins v22.d[1], x1\n"
                    "ldr d23, [%[b_1]]\n"       //b_o1
                    "mov  x0, %[ic]\n"             //ic_blk
                    "ldr  x2, [%[b_1], #8]\n"
                    "ins v23.d[1], x2\n"
                    "ldr  h0, [%[in_0]]\n"           //in_hw0
                    "mov  v2.16b, v22.16b\n"      //out_o0hw0
                    "ldr d18, [%[f_0]]\n"            //f_o0c0
                    "mov v10.16b, v23.16b\n"      //out_o1hw0
                    "ldr  x2, [%[f_0], #8]\n"
                    "ins v18.d[1], x2\n"
                    "ldr d19, [%[f_0], #16]\n"            //f_o1c0
                    "ldr  x3, [%[f_0], #24]\n"
                    "ins v19.d[1], x3\n"

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr  h1, [%[in_0], #2]\n"           //in_hw0
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "ldr d20, [%[f_0], #32]\n"            //f_o0c0
                    "fmla v10.8h, v19.8h, v0.h[0]\n"
                    "ldr  x2, [%[f_0], #40]\n"
                    "ins v20.d[1], x2\n"
                    "ldr d21, [%[f_0], #48]\n"            //f_o1c0
                    "sub x0, x0, #2\n"
                    "ldr  x3, [%[f_0], #56]\n"
                    "ins v21.d[1], x3\n"

                    "ldr  h0, [%[in_0], #4]\n"           //in_hw0
                    "fmla  v2.8h, v20.8h, v1.h[0]\n"
                    "ldr d18, [%[f_0], #64]\n"            //f_o0c0
                    "fmla v10.8h, v21.8h, v1.h[0]\n"
                    "ldr  x2, [%[f_0], #72]\n"
                    "ins v18.d[1], x2\n"
                    "ldr d19, [%[f_0], #80]\n"            //f_o1c0
                    "add %[in_0], %[in_0], #4\n"
                    "ldr  x3, [%[f_0], #88]\n"
                    "ins v19.d[1], x3\n"
                    "add %[f_0], %[f_0], #64\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "add %[f_0], %[f_0], #32\n"
                    "fmla v10.8h, v19.8h, v0.h[0]\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "fmax v10.8h, v10.8h, v1.8h\n"

                    "3:\n"
                    "str   q2, [%[out_0]]\n"           //out_o0hw0
                    "str  q10, [%[out_1]]\n"           //out_o1hw0
                    :[out_0]"+r"(out_o0hw0),
                     [out_1]"+r"(out_o1hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_o0c0)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [b_1]"r"(b_o1),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v10", "v18", "v19", "v20", "v21", "v22", "v23", "x0", "x1", "x2", "x3"
                );
                b0 += 16;
                b1 += 16;
            }
            if (oc & 1) {
                // oc%2 != 0
                const F16 *f_r = filterArray + (oc-1)*8*fh*fw*ic;
                F16 *in_hw0 = in_pack;
                F16 *out_o0hw0 = outArray + n*oc*ohow*8 + (oc-1)*ohow*8 + hw*8;
                // bias
                const F16 *b_o0 = biasArray + (oc-1)*8;
                __asm__ __volatile__(
                    "ldr d22, [%[b_0]]\n"       //b_o0
                    "mov  x0, %[ic]\n"             //ic_blk
                    "ldr  x1, [%[b_0], #8]\n"
                    "ins v22.d[1], x1\n"
                    "ldr  h0, [%[in_0]]\n"           //in_hw0
                    "mov  v2.16b, v22.16b\n"      //out_o0hw0
                    "ldr d18, [%[f_0]]\n"            //f_o0c0
                    "ldr  x2, [%[f_0], #8]\n"
                    "ins v18.d[1], x2\n"

                    "0:\n"
                    "cmp x0, #1\n"
                    "ble 1f\n"
                    "ldr  h1, [%[in_0], #2]\n"           //in_hw0
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "ldr d20, [%[f_0], #16]\n"            //f_o0c0
                    "sub x0, x0, #2\n"
                    "ldr  x2, [%[f_0], #24]\n"
                    "ins v20.d[1], x2\n"

                    "ldr  h0, [%[in_0], #4]\n"           //in_hw0
                    "fmla  v2.8h, v20.8h, v1.h[0]\n"
                    "ldr d18, [%[f_0], #32]\n"            //f_o0c0
                    "ldr  x2, [%[f_0], #40]\n"
                    "ins v18.d[1], x2\n"
                    "add %[in_0], %[in_0], #4\n"
                    "add %[f_0], %[f_0], #32\n"
                    "b 0b\n"

                    "1:\n"
                    "blt 2f\n"
                    "fmla  v2.8h, v18.8h, v0.h[0]\n"
                    "add %[f_0], %[f_0], #16\n"

                    "2:\n"
                    "cbz %[activation], 3f\n"
                    "eor v1.16b, v1.16b, v1.16b\n"     //zero
                    "fmax  v2.8h,  v2.8h, v1.8h\n"     //max(v2, 0)
                    "3:\n"
                    "str   q2, [%[out_0]]\n"           //out_o0hw0
                    :[out_0]"+r"(out_o0hw0),
                     [in_0]"+r"(in_hw0),
                     [f_0]"+r"(f_r)
                    :[ic]"r"((I64)ic*fh*fw),
                     [b_0]"r"(b_o0),
                     [activation]"r"(activation)
                    :"memory", "cc", "v0", "v1", "v2", "v10", "v18", "v20", "v22", "x0", "x1", "x2"
                );
            }
        }
    }
    return ret;
}
