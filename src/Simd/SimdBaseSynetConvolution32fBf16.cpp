/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2022 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdSynetConvolution32f.h"
#include "Simd/SimdSynetConvolution32fCommon.h"
#include "Simd/SimdSynet.h"
#include "Simd/SimdBFloat16.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdCpu.h"

namespace Simd
{
#if defined(SIMD_SYNET_ENABLE)
    namespace Base
    {
        SynetConvolution32fBf16Gemm::SynetConvolution32fBf16Gemm(const ConvParam32f & p)
            : SynetConvolution32f(p)
        {
            Init(true);
            _biasAndActivation = Base::ConvolutionBiasAndActivation;
        }

        void SynetConvolution32fBf16Gemm::Init(bool ref)
        {
            _ref = ref;
            const ConvParam32f& p = _param;
            if (_ref)
                InitRef();
            else
            {
                assert(0);
            }
        }

        size_t SynetConvolution32fBf16Gemm::ExternalBufferSize() const
        {
            return _sizeB*_merge;
        };

        void SynetConvolution32fBf16Gemm::SetParams(const float * weight, SimdBool * internal, const float * bias, const float * params)
        {
            Simd::SynetConvolution32f::SetParams(weight, internal, bias, params);
            if (_ref)
                Float32ToBFloat16(_weight, _bf16Weight.size, _bf16Weight.data);
            else
            {
                assert(0);
            }
            if (internal)
                *internal = SimdTrue;
        }

        void SynetConvolution32fBf16Gemm::Forward(const float * src, float * buf_, float * dst)
        {
            const ConvParam32f & p = _param;
            uint16_t * buf = (uint16_t*)Buffer(buf_);
            if (_ref)
                ForwardRef(src, buf, dst);
            else
            {
                assert(0);
            }
        }

        void SynetConvolution32fBf16Gemm::InitRef()
        {
            const ConvParam32f& p = _param;
            if (p.trans)
            {
                _M = p.dstH * p.dstW;
                _N = p.dstC / p.group;
                _K = p.srcC * p.kernelY * p.kernelX / p.group;
                _ldS = _K;
                _ldW = p.dstC;
                _ldD = p.dstC;
                _grW = _N;
                _grS = _K * _M;
                _grD = _N;
                _bf16Weight.Resize(_K * _N);
            }
            else
            {
                _M = p.dstC / p.group;
                _N = p.dstH * p.dstW;
                _K = p.srcC * p.kernelY * p.kernelX / p.group;
                _ldW = _K;
                _ldS = _N;
                _ldD = _N;
                _grW = _M * _K;
                _grS = _K * _N;
                _grD = _M * _N;
                _bf16Weight.Resize(_K * _M);
            }
            _batch = p.batch;
            _sizeS = p.srcC * p.srcH * p.srcW;
            _sizeB = p.srcC * p.kernelY * p.kernelX * p.dstH * p.dstW;
            _sizeD = p.dstC * p.dstH * p.dstW;
            _merge = 1;
        }

        void SynetConvolution32fBf16Gemm::ForwardRef(const float* src, uint16_t* buf, float* dst)
        {
            const ConvParam32f& p = _param;
            const uint16_t* wgt = _bf16Weight.data;
            for (size_t b = 0; b < _batch; ++b)
            {
                if (_param.trans)
                {
                    ImgToRowRef(src, buf);
                    for (size_t g = 0; g < p.group; ++g)
                        GemmRef(_M, _N, _K, buf + _grS * g, _ldS, wgt + _grW * g, _ldW, dst + _grD * g, _ldD);
                }
                else
                {
                    ImgToColRef(src, buf);
                    for (size_t g = 0; g < p.group; ++g)
                        GemmRef(_M, _N, _K, wgt + _grW * g, _ldW, buf + _grS * g, _ldS, dst + _grD * g, _ldD);
                }
                _biasAndActivation(_bias, p.dstC, p.dstH * p.dstW, p.activation, _params, p.trans, dst);
                src += _sizeS;
                dst += _sizeD;
            }
        }

        void SynetConvolution32fBf16Gemm::ImgToColRef(const float* src, uint16_t* dst)
        {
            const ConvParam32f& p = _param;
            assert(!p.trans);
            size_t srcSize = p.srcW * p.srcH;
            for (size_t c = 0; c < p.srcC; ++c)
            {
                for (size_t ky = 0; ky < p.kernelY; ky++)
                {
                    for (size_t kx = 0; kx < p.kernelX; kx++)
                    {
                        size_t sy = ky * p.dilationY - p.padY;
                        for (size_t dy = 0; dy < p.dstH; ++dy)
                        {
                            if (sy < p.srcH)
                            {
                                size_t sx = kx * p.dilationX - p.padX;
                                for (size_t dx = 0; dx < p.dstW; ++dx)
                                {
                                    if (sx < p.srcW)
                                        *(dst++) = Float32ToBFloat16(src[sy * p.srcW + sx]);
                                    else
                                        *(dst++) = 0;
                                    sx += p.strideX;
                                }
                            }
                            else
                            {
                                for (size_t dx = 0; dx < p.dstW; ++dx)
                                    *(dst++) = 0;
                            }
                            sy += p.strideY;
                        }
                    }
                }
                src += srcSize;
            }
        }

        void SynetConvolution32fBf16Gemm::ImgToRowRef(const float* src, uint16_t* dst)
        {
            const ConvParam32f& p = _param;
            assert(p.trans);
            size_t size = p.srcC / p.group;
            for (size_t g = 0; g < p.group; ++g)
            {
                for (size_t dy = 0; dy < p.dstH; ++dy)
                {
                    for (size_t dx = 0; dx < p.dstW; ++dx)
                    {
                        for (size_t ky = 0; ky < p.kernelY; ky++)
                        {
                            size_t sy = dy * p.strideY + ky * p.dilationY - p.padY;
                            if (sy < p.srcH)
                            {
                                for (size_t kx = 0; kx < p.kernelX; kx++)
                                {
                                    size_t sx = dx * p.strideX + kx * p.dilationX - p.padX;
                                    if (sx < p.srcW)
                                    {
                                        Float32ToBFloat16(src + (sy * p.srcW + sx) * p.srcC, size, dst);
                                        dst += size;
                                    }
                                    else
                                    {
                                        memset(dst, 0, size * sizeof(uint16_t));
                                        dst += size;
                                    }
                                }
                            }
                            else
                            {
                                memset(dst, 0, p.kernelX * size * sizeof(uint16_t));
                                dst += p.kernelX * size;
                            }
                        }
                    }
                }
                src += size;
            }
        }

        void SynetConvolution32fBf16Gemm::GemmRef(size_t M, size_t N, size_t K, const uint16_t* A, size_t lda, const uint16_t* B, size_t ldb, float* C, size_t ldc)
        {
            for (size_t i = 0; i < M; ++i)
            {
                float* pC = C + i * ldc;
                for (size_t j = 0; j < N; ++j)
                    pC[j] = 0.0f;
                for (size_t k = 0; k < K; ++k)
                {
                    const uint16_t* pB = B + k * ldb;
                    float a = BFloat16ToFloat32(A[i * lda + k]);
                    for (size_t j = 0; j < N; ++j)
                        pC[j] += a * BFloat16ToFloat32(pB[j]);
                }
            }
        }

        void SynetConvolution32fBf16Gemm::ImgToCol(const float * src, uint16_t* dst)
        {
            const ConvParam32f & p = _param;
            assert(!p.trans);
            assert(0);
        }

        void SynetConvolution32fBf16Gemm::ImgToRow(const float * src, uint16_t* dst)
        {
            const ConvParam32f & p = _param;
            assert(p.trans);
            assert(0);
        }
    }
#endif
}