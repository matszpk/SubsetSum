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

__kernel void subsetSumNaiveKernel(uint n, const global long* gSumChanges,
               const global long* inputSums, global uint* foundIndicesNumBuf,
               global uint* foundIndices)
{
    local long sumChanges[41];
    uint gi = get_global_id(0);
    uint li = get_local_id(0);
    
    const uint grpSize = get_local_size(0);
    for (uint j = 0; j < 41; j += grpSize)
        if (j+li < 41)
            sumChanges[j+li] = gSumChanges[j+li];
    barrier(CLK_LOCAL_MEM_FENCE);
    
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
    
    uint curFoundIdx = 0xffffffffU;
    
    for (uint current = 0; current < 32; current++)
    {
        uint find = 0;
        long sum = inputSum + sumChanges[current];
        find |= (sum==0);
#define NAIVE_CORE_1 \
        sum += v0; \
        find |= (sum==0); \
        sum += v1; \
        find |= (sum==0); \
        sum += v0; \
        find |= (sum==0);

#define NAIVE_CORE_2 \
        NAIVE_CORE_1 \
        sum += v2; \
        find |= (sum==0); \
        NAIVE_CORE_1

#define NAIVE_CORE_3 \
        NAIVE_CORE_2 \
        sum += v3; \
        find |= (sum==0); \
        NAIVE_CORE_2

#define NAIVE_CORE_4 \
        NAIVE_CORE_3 \
        sum += v4; \
        find |= (sum==0); \
        NAIVE_CORE_3

#define NAIVE_CORE_5 \
        NAIVE_CORE_4 \
        sum += v5; \
        find |= (sum==0); \
        NAIVE_CORE_4

#define NAIVE_CORE_6 \
        NAIVE_CORE_5 \
        sum += v6; \
        find |= (sum==0); \
        NAIVE_CORE_5

#define NAIVE_CORE_7 \
        NAIVE_CORE_6 \
        sum += v7; \
        find |= (sum==0); \
        NAIVE_CORE_6

#define NAIVE_CORE_8 \
        NAIVE_CORE_7 \
        sum += v8; \
        find |= (sum==0); \
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
