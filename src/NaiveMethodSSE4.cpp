/*
 *  SubsetSum - OpenCL SubsetSum solver
 *  Copyright (C) 2014 Mateusz Szpakowski
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_SSE4
#include <smmintrin.h>
#endif
#include <glibmm.h>
#include "NaiveWorkers.h"

#if defined(HAVE_ARCH_INTEL) && defined(HAVE_SSE4)
guint subsetSumNaiveMethodSSE4(const gint64* sumChanges, gint64 inputSum1,
           guint64 inputSum2, guint* foundIndices)
{
    __m128i v0 = _mm_set1_epi64((__m64)sumChanges[32 + 0]);
    __m128i v1 = _mm_set1_epi64((__m64)sumChanges[32 + 1]);
    __m128i v2 = _mm_set1_epi64((__m64)sumChanges[32 + 2]);
    __m128i v3 = _mm_set1_epi64((__m64)sumChanges[32 + 3]);
    __m128i v4 = _mm_set1_epi64((__m64)sumChanges[32 + 4]);
    __m128i v5 = _mm_set1_epi64((__m64)sumChanges[32 + 5]);
    __m128i v6 = _mm_set1_epi64((__m64)sumChanges[32 + 6]);
    __m128i v7 = _mm_set1_epi64((__m64)sumChanges[32 + 7]);
    __m128i v8 = _mm_set1_epi64((__m64)sumChanges[32 + 8]);
    const __m128i s1 = _mm_add_epi64(v0, v1);
    const __m128i s2 = _mm_add_epi64(s1, v2);
    const __m128i s3 = _mm_add_epi64(s2, v3);
    const __m128i s4 = _mm_add_epi64(s3, v4);
    const __m128i s5 = _mm_add_epi64(s4, v5);
    const __m128i s6 = _mm_add_epi64(s5, v6);
    const __m128i s7 = _mm_add_epi64(s6, v7);
    v1 = _mm_sub_epi64(v1, v0);
    v2 = _mm_sub_epi64(v2, s1);
    v3 = _mm_sub_epi64(v3, s2);
    v4 = _mm_sub_epi64(v4, s3);
    v5 = _mm_sub_epi64(v5, s4);
    v6 = _mm_sub_epi64(v6, s5);
    v7 = _mm_sub_epi64(v7, s6);
    v8 = _mm_sub_epi64(v8, s7);
    
    const __m128i inputSums = _mm_set_epi64((__m64)inputSum2, (__m64)inputSum1);
    
    guint foundIndicesNum = 0;
    for (guint current = 0; current < 32; current++)
    {
        const __m128i zero = _mm_setzero_si128();
        __m128i find = _mm_setzero_si128();
        __m128i sum = _mm_set1_epi64((__m64)sumChanges[current]);
        sum = _mm_add_epi64(sum, inputSums);
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero));
        /* main long work */
#define NAIVE_CORE_SSE4_1 \
        sum = _mm_add_epi64(sum, v0); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        sum = _mm_add_epi64(sum, v1); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        sum = _mm_add_epi64(sum, v0); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); 

#define NAIVE_CORE_SSE4_2 \
        NAIVE_CORE_SSE4_1 \
        sum = _mm_add_epi64(sum, v2); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_1

#define NAIVE_CORE_SSE4_3 \
        NAIVE_CORE_SSE4_2 \
        sum = _mm_add_epi64(sum, v3); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_2

#define NAIVE_CORE_SSE4_4 \
        NAIVE_CORE_SSE4_3 \
        sum = _mm_add_epi64(sum, v4); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_3

#define NAIVE_CORE_SSE4_5 \
        NAIVE_CORE_SSE4_4 \
        sum = _mm_add_epi64(sum, v5); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_4

#define NAIVE_CORE_SSE4_6 \
        NAIVE_CORE_SSE4_5 \
        sum = _mm_add_epi64(sum, v6); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_5

#define NAIVE_CORE_SSE4_7 \
        NAIVE_CORE_SSE4_6 \
        sum = _mm_add_epi64(sum, v7); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_6

#define NAIVE_CORE_SSE4_8 \
        NAIVE_CORE_SSE4_7 \
        sum = _mm_add_epi64(sum, v8); \
        find = _mm_or_si128(find, _mm_cmpeq_epi64(sum, zero)); \
        NAIVE_CORE_SSE4_7

        NAIVE_CORE_SSE4_8
        
        if (_mm_extract_epi32(find, 0))
            foundIndices[foundIndicesNum++] = current;
        if (_mm_extract_epi32(find, 2))
            foundIndices[foundIndicesNum++] = current+32;
    }
    
    return foundIndicesNum;
}
#endif
