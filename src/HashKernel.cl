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

typedef struct _FoundStruct
{
    uint workIndex;
    uint foundBits[8];
} FoundStruct;

typedef struct _NodeHashEntry
{
    ushort poslo;
    ushort poshi;
    ushort size;
} NodeHashEntry;

__kernel void subsetSumHashKernel(uint n, const constant long* sumChanges, uint hashBits,
           const global NodeHashEntry* hashTable,
#ifdef SUBSUMSTBL
           const global uint* hashSubsets,
#else
           const global long* hashLists,
#endif
           const global long* inputSums, global uint* foundIndicesNumBuf,
           global FoundStruct* foundIndices
#ifdef SUBSUMSTBL
           , global const long* gSubsumsTbl
#endif
                                 )
{
#ifdef SUBSUMSTBL
    local long subsumsTbl[SUBSUMSTBL*32];
#endif
    uint gi = get_global_id(0);
    uint li = get_local_id(0);
    
#ifdef SUBSUMSTBL
    for (uint j = 0; j < SUBSUMSTBL*32; j += grpSize)
        if (j+li < SUBSUMSTBL*32)
            subsumsTbl[j+li] = gSubsumsTbl[j+li];
    
    barrier(CLK_LOCAL_MEM_FENCE);
#endif
    
    if (gi>=n)
        return;
    
    const long inputSum = inputSums[gi];
    
    uint curFoundIdx = 0xffffffffU;
    const uint hmask = (1U<<hashBits)-1U;
    
    for (uint current = 0; current < 256; current++)
    {
        long sum = inputSum + sumChanges[current];
        
        const uint temph1 = sum>>hashBits;
        const uint temph2 = temph1>>hashBits;
        const uint temph3 = temph2>>hashBits;
        const uint temph4 = temph3>>hashBits;
        const uint hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const uint size = hashTable[hkey].size;
            
#ifdef SUBSUMSTBL
            const global uint* hsubsets = hashSubsets +
                    upsample(hashTable[hkey].poshi,hashTable[hkey].poslo);
            for (uint j = 0; j < size; j++)
            {
                const uint hsubset = hsubsets[j];
#  if SUBSUMSTBL == 8
                const long hsum = subsumsTbl[hsubset];
#  endif
#  if SUBSUMSTBL == 16
                const long hsum = subsumsTbl[hsubset&0xff] +
                        subsumsTbl[256 + (hsubset>>8)];
#  endif
#  if SUBSUMSTBL == 24
                const long hsum = subsumsTbl[hsubset&0xff] +
                        subsumsTbl[256 + ((hsubset>>8)&0xff)] +
                        subsumsTbl[512 + (hsubset>>16)];
#  endif
#  if SUBSUMSTBL == 32
                const long hsum = subsumsTbl[hsubset&0xff] +
                        subsumsTbl[256 + ((hsubset>>8)&0xff)] +
                        subsumsTbl[512 + ((hsubset>>16)&0xff)] +
                        subsumsTbl[768 + (hsubset>>24)];
#  endif
                if (hsum == sum)
                {
                    found = true;
                    break;
                }
            }
#else
            const global long* hlist = hashLists +
                    upsample(hashTable[hkey].poshi,hashTable[hkey].poslo);
            for (uint j = 0; j < size; j++)
                if (hlist[j] == sum)
                {
                    found = true;
                    break;
                }
#endif
        }
        if (found)
        {
            if (curFoundIdx == 0xffffffffU)
            {
                curFoundIdx = atomic_inc(foundIndicesNumBuf);
                foundIndices[curFoundIdx].workIndex = gi;
                for (uint x = 0; x < 8; x++)
                    foundIndices[curFoundIdx].foundBits[x] = 0;
            }
            foundIndices[curFoundIdx].foundBits[current>>5] |= 1U<<(current&31);
        }
    }
}
