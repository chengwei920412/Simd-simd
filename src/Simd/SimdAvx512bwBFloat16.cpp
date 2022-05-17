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
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdBFloat16.h"

namespace Simd
{
#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        template <bool align, bool mask> SIMD_INLINE void Float32ToBFloat16(const float* src, uint16_t* dst, __mmask16 srcMask[2],  __mmask32 dstMask[1])
        {
            __m512 s0 = Avx512f::Load<align, mask>(src + 0 * F, srcMask[0]);
            __m512 s1 = Avx512f::Load<align, mask>(src + 1 * F, srcMask[1]);
            __m512i d0 = Float32ToBFloat16(s0);
            __m512i d1 = Float32ToBFloat16(s1);
            Store<align, mask>(dst, _mm512_permutexvar_epi64(K64_PERMUTE_FOR_PACK, _mm512_packus_epi32(d0, d1)), dstMask[0]);
        }

        void Float32ToBFloat16(const float* src, size_t size, uint16_t* dst)
        {
            size_t size32 = AlignLo(size, 32);
            __mmask16 srcMask[2];
            __mmask32 dstMask[1];
            size_t i = 0;
            for (; i < size32; i += 32)
                Float32ToBFloat16<false, false>(src + i, dst + i, srcMask, dstMask);
            if (size32 < size)
            {
                srcMask[0] = TailMask16(size - size32 - F * 0);
                srcMask[1] = TailMask16(size - size32 - F * 1);
                dstMask[0] = TailMask32(size - size32);
                Float32ToBFloat16<false, true>(src + i, dst + i, srcMask, dstMask);
            }
        }

        //---------------------------------------------------------------------------------------------

        template<bool align, bool mask> SIMD_INLINE void BFloat16ToFloat32(const uint16_t* src, float* dst, __mmask32 srcMask[1], __mmask16 dstMask[2])
        {
            __m512i _src = Load<align, mask>(src, srcMask[0]);
            __m512i s0 = _mm512_cvtepu16_epi32(_mm512_extracti64x4_epi64(_src, 0));
            __m512i s1 = _mm512_cvtepu16_epi32(_mm512_extracti64x4_epi64(_src, 1));
            Avx512f::Store<align, mask>(dst + 0, BFloat16ToFloat32(s0), dstMask[0]);
            Avx512f::Store<align, mask>(dst + F, BFloat16ToFloat32(s1), dstMask[1]);
        }

        void BFloat16ToFloat32(const uint16_t* src, size_t size, float* dst)
        {
            size_t size32 = AlignLo(size, 32);
            __mmask32 srcMask[1];
            __mmask16 dstMask[2];
            size_t i = 0;
            for (; i < size32; i += 32)
                BFloat16ToFloat32<false, false>(src + i, dst + i, srcMask, dstMask);
            if (size32 < size)
            {
                srcMask[0] = TailMask32(size - size32);
                dstMask[0] = TailMask16(size - size32 - F * 0);
                dstMask[1] = TailMask16(size - size32 - F * 1);
                BFloat16ToFloat32<false, true>(src + i, dst + i, srcMask, dstMask);
            }
        }
    }
#endif
}