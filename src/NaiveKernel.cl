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

__kernel void subsetSumNaiveKernel(uint n, const constant long* sumChanges,
               const global long* inputSums, global uint* foundIndicesNumBuf,
               global uint* foundIndices)
{
    uint gi = get_global_id(0);
    uint li = get_local_id(0);
    
    if (gi>=n)
        return;
    
    const long inputSum = inputSums[gi];
    
    const long v0 = sumChanges[32 + 0];
    const long v1 = sumChanges[32 + 1] - v0;
    const long s1 = v0 + sumChanges[32 + 1];
    const long v2 = sumChanges[32 + 2] - s1;
    const long s2 = s1 + sumChanges[32 + 2];
    const long v3 = sumChanges[32 + 3] - s2;
    const long s3 = s2 + sumChanges[32 + 3];
    const long v4 = sumChanges[32 + 4] - s3;
    const long s4 = s3 + sumChanges[32 + 4];
    const long v5 = sumChanges[32 + 5] - s4;
    const long s5 = s4 + sumChanges[32 + 5];
    const long v6 = sumChanges[32 + 6] - s5;
    const long s6 = s5 + sumChanges[32 + 6];
    const long v7 = sumChanges[32 + 7] - s6;
    const long s7 = s6 + sumChanges[32 + 7];
    const long v8 = sumChanges[32 + 8] - s7;
    
    const uint v0l = v0&0xffffffffU;
    const uint v0h = v0>>32;
    const uint v1l = v1&0xffffffffU;
    const uint v1h = v1>>32;
    const uint v2l = v2&0xffffffffU;
    const uint v2h = v2>>32;
    const uint v3l = v3&0xffffffffU;
    const uint v3h = v3>>32;
    const uint v4l = v4&0xffffffffU;
    const uint v4h = v4>>32;
    const uint v5l = v5&0xffffffffU;
    const uint v5h = v5>>32;
    const uint v6l = v6&0xffffffffU;
    const uint v6h = v6>>32;
    const uint v7l = v7&0xffffffffU;
    const uint v7h = v7>>32;
    const uint v8l = v8&0xffffffffU;
    const uint v8h = v8>>32;
    
    uint curFoundIdx = 0xffffffffU;
    
#define SUM(VL,VH) \
        suml += VL; \
        sumh += VH + (suml < VL);
    
    for (uint current = 0; current < 32; current++)
    {
        uint find = 0;
        uint suml = (inputSum + sumChanges[current])&0xffffffffU;
        uint sumh = (inputSum + sumChanges[current])>>32;
        //find |= (sum==0);
        find |= (suml==0) & (sumh==0);
#define NAIVE_CORE_1 \
        SUM(v0l,v0h); \
        find |= (suml==0) & (sumh==0); \
        SUM(v1l,v1h); \
        find |= (suml==0) & (sumh==0); \
        SUM(v0l,v0h); \
        find |= (suml==0) & (sumh==0);

#define NAIVE_CORE_2 \
        NAIVE_CORE_1 \
        SUM(v2l,v2h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_1

#define NAIVE_CORE_3 \
        NAIVE_CORE_2 \
        SUM(v3l,v3h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_2

#define NAIVE_CORE_4 \
        NAIVE_CORE_3 \
        SUM(v4l,v4h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_3

#define NAIVE_CORE_5 \
        NAIVE_CORE_4 \
        SUM(v5l,v5h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_4

#define NAIVE_CORE_6 \
        NAIVE_CORE_5 \
        SUM(v6l,v6h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_5

#define NAIVE_CORE_7 \
        NAIVE_CORE_6 \
        SUM(v7l,v7h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_6

#define NAIVE_CORE_8 \
        NAIVE_CORE_7 \
        SUM(v8l,v8h); \
        find |= (suml==0) & (sumh==0); \
        NAIVE_CORE_7

        NAIVE_CORE_8
        
        if (find)
        {
            if (curFoundIdx == 0xffffffffU)
            {
                curFoundIdx = atomic_inc(foundIndicesNumBuf);
                foundIndices[curFoundIdx*2+1] = 0;
                foundIndices[curFoundIdx*2] = gi;
            }
            foundIndices[curFoundIdx*2+1] |= 1U<<current;
        }
    }
}
