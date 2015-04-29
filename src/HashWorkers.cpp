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
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#include "PushPop.h"
#include "HashWorkers.h"

#ifdef HAVE_OPENCL
extern unsigned char HashKernel_cl[];
extern unsigned int HashKernel_cl_len;
extern unsigned char HashKernelGroups_cl[];
extern unsigned int HashKernelGroups_cl_len;
extern unsigned char HashKernelBigHash_cl[];
extern unsigned int HashKernelBigHash_cl_len;
extern unsigned char HashKernelBHGroups_cl[];
extern unsigned int HashKernelBHGroups_cl_len;
#endif

using namespace Glib;

guint subsetSumHashMethod(const gint64* sumChanges,
         guint hashBits, const NodeHashEntry* hashTable, const gint64* hashLists,
         gint64 inputSum, guint* foundIndices)
{
    guint foundIndicesNum = 0;
    const guint32 hmask = (1U<<hashBits)-1U;
    for (guint current = 0; current < 256; current++)
    {
        gint64 sum = inputSum + sumChanges[current];
        
        const guint32 temph1 = sum>>hashBits;
        const guint32 temph2 = temph1>>hashBits;
        const guint32 temph3 = temph2>>hashBits;
        const guint32 temph4 = temph3>>hashBits;
        guint32 hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const guint size = hashTable[hkey].size;
            const gint64* hlist = hashLists + hashTable[hkey].getPos();
            for (guint j = 0; j < size; j++)
                if (hlist[j] == sum)
                {
                    found = true;
                    break;
                }
            if (found)
                foundIndices[foundIndicesNum++] = current;
        }
    }
    
    return foundIndicesNum;
}

guint subsetSumHashSubsets8Method(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* foundIndices)
{
    guint foundIndicesNum = 0;
    const guint32 hmask = (1U<<hashBits)-1U;
    for (guint current = 0; current < 256; current++)
    {
        gint64 sum = inputSum + sumChanges[current];
        
        const guint32 temph1 = sum>>hashBits;
        const guint32 temph2 = temph1>>hashBits;
        const guint32 temph3 = temph2>>hashBits;
        const guint32 temph4 = temph3>>hashBits;
        guint32 hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const guint size = hashTable[hkey].size;
            const guint32* hsubsets = hashSubsets + hashTable[hkey].getPos();
            
            for (guint j = 0; j < size; j++)
            {
                gint64 hsum = subsumsTbl[hsubsets[j]];
                if (hsum == sum)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                foundIndices[foundIndicesNum++] = current;
        }
    }
    
    return foundIndicesNum;
}

guint subsetSumHashSubsets16Method(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* foundIndices)
{
    guint foundIndicesNum = 0;
    const guint32 hmask = (1U<<hashBits)-1U;
    for (guint current = 0; current < 256; current++)
    {
        gint64 sum = inputSum + sumChanges[current];
        
        const guint32 temph1 = sum>>hashBits;
        const guint32 temph2 = temph1>>hashBits;
        const guint32 temph3 = temph2>>hashBits;
        const guint32 temph4 = temph3>>hashBits;
        guint32 hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const guint size = hashTable[hkey].size;
            const guint32* hsubsets = hashSubsets + hashTable[hkey].getPos();
            
            for (guint j = 0; j < size; j++)
            {
                const guint32 hsubset = hsubsets[j];
                const gint64 hsum = subsumsTbl[hsubset&0xff] +
                    subsumsTbl[256 + (hsubset>>8)];
                if (hsum == sum)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                foundIndices[foundIndicesNum++] = current;
        }
    }
    
    return foundIndicesNum;
}

guint subsetSumHashSubsets24Method(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* foundIndices)
{
    guint foundIndicesNum = 0;
    const guint32 hmask = (1U<<hashBits)-1U;
    for (guint current = 0; current < 256; current++)
    {
        gint64 sum = inputSum + sumChanges[current];
        
        const guint32 temph1 = sum>>hashBits;
        const guint32 temph2 = temph1>>hashBits;
        const guint32 temph3 = temph2>>hashBits;
        const guint32 temph4 = temph3>>hashBits;
        guint32 hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const guint size = hashTable[hkey].size;
            const guint32* hsubsets = hashSubsets + hashTable[hkey].getPos();
            
            for (guint j = 0; j < size; j++)
            {
                const guint32 hsubset = hsubsets[j];
                const gint64 hsum = subsumsTbl[hsubset&0xff] +
                        subsumsTbl[256 + ((hsubset>>8)&0xff)] +
                        subsumsTbl[512 + (hsubset>>16)];
                if (hsum == sum)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                foundIndices[foundIndicesNum++] = current;
        }
    }
    
    return foundIndicesNum;
}

guint subsetSumHashSubsets32Method(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* foundIndices)
{
    guint foundIndicesNum = 0;
    const guint32 hmask = (1U<<hashBits)-1U;
    for (guint current = 0; current < 256; current++)
    {
        gint64 sum = inputSum + sumChanges[current];
        
        const guint32 temph1 = sum>>hashBits;
        const guint32 temph2 = temph1>>hashBits;
        const guint32 temph3 = temph2>>hashBits;
        const guint32 temph4 = temph3>>hashBits;
        guint32 hkey = (sum^temph1^temph2^temph3^temph4)&hmask;
        bool found = false;
        if (hashTable[hkey].size != 0)
        {
            const guint size = hashTable[hkey].size;
            const guint32* hsubsets = hashSubsets + hashTable[hkey].getPos();
            
            for (guint j = 0; j < size; j++)
            {
                const guint32 hsubset = hsubsets[j];
                const gint64 hsum = subsumsTbl[hsubset&0xff] +
                        subsumsTbl[256 + ((hsubset>>8)&0xff)] +
                        subsumsTbl[512 + ((hsubset>>16)&0xff)] +
                        subsumsTbl[768 + (hsubset>>24)];
                if (hsum == sum)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                foundIndices[foundIndicesNum++] = current;
        }
    }
    
    return foundIndicesNum;
}

void HashWorker::checkAndSendSolution(const Int128& initialSubset,
          const Int128& initialSum, guint foundIndex) const
{
    /*std::cout << "CheckAndSend: " << int128tostr(initialSubset) << "," <<
            foundIndex << std::endl;*/
    
    const guint numbersNum = controller->getNumbersNum();
    const guint nodeHashedNumbers = controller->getNodeHashedNumbers();
    const guint nodeBits = controller->getNodeBits();
    const Int128* nodeProblem = controller->getNodeProblem();
    const NodeHashEntry* nodeHash = controller->getNodeHash();
    const guint32* nodeHashSubsets = controller->getNodeHashSubsets();
    
    Int128 indexSum = initialSum;
    guint bit = 1;
    for (guint x = 0; x < 8; x++, bit<<=1)
        if ((bit&foundIndex)!=0)
            indexSum += nodeProblem[x];
    
    const guint32 nodeHKey = controller->getNodeHashKey(gint64(indexSum));
    if (nodeHash[nodeHKey].size != 0)
    {   // if found
        const guint32* nodeHList = nodeHashSubsets + nodeHash[nodeHKey].getPos();
        for (guint j = 0; j < nodeHash[nodeHKey].size; j++)
        {
            Int128 sum = indexSum;
            bit = 1;
            const guint32 hashedSubset = nodeHList[j];
            for (guint k = 0; k < nodeHashedNumbers; k++, bit<<=1)
                if (hashedSubset & bit)
                    sum += nodeProblem[8+k];
            if (sum == Int128(0)) // solution
            {
                Int128 finalSolution = initialSubset|
                        (Int128(foundIndex)<<gint(numbersNum-nodeBits))|
                        (Int128(hashedSubset)<<gint(numbersNum-nodeHashedNumbers));
                if (finalSolution)
                    controller->putSolution(controller->translateSubset(finalSolution));
            }
        }
    }
}

void HashCpuWorker::init(HashController* controller, guint threadId)
{
    this->controller = controller;
    std::cout << "HashWorker: " << threadId << std::endl;
}

void HashCpuWorker::run()
{
    const NodeHashEntry* nodeHashTable = controller->getNodeHash();
    const gint64* nodeHashLists = controller->getNodeHashLists();
    const guint32* nodeHashSubsets = controller->getNodeHashSubsets();
    guint nodesCount = 0;
    const gint64* sumChanges = controller->getSumChanges();
    const guint nodeHashBits = controller->getNodeHashBits();
    const guint nodeHashedNumbers = controller->getNodeHashedNumbers();
    
    DirectPop popper(controller->getMemQueue(), 16384);
    guint foundIndices[256];
    
    NodeSubset node;
    node.subset = 0;
    node.sum = 0;
    
    if (!controller->isUseHashSubsets())
    {
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumHashMethod(sumChanges, nodeHashBits,
                      nodeHashTable, nodeHashLists, gint64(node.sum), foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
            
            nodesCount++;
            if ((nodesCount & 0xfff) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    else if (nodeHashedNumbers <= 8)
    {
        const gint64* subsumsTbl = controller->getSubsumsTbl();
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumHashSubsets8Method(
                      sumChanges, subsumsTbl, nodeHashBits,
                      nodeHashTable, nodeHashSubsets, gint64(node.sum), foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
            
            nodesCount++;
            if ((nodesCount & 0xfff) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    else if (nodeHashedNumbers <= 16)
    {
        const gint64* subsumsTbl = controller->getSubsumsTbl();
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumHashSubsets16Method(
                      sumChanges, subsumsTbl, nodeHashBits,
                      nodeHashTable, nodeHashSubsets, gint64(node.sum), foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
            
            nodesCount++;
            if ((nodesCount & 0xfff) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    else if (nodeHashedNumbers <= 24)
    {
        const gint64* subsumsTbl = controller->getSubsumsTbl();
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumHashSubsets24Method(
                      sumChanges, subsumsTbl, nodeHashBits,
                      nodeHashTable, nodeHashSubsets, gint64(node.sum), foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
            
            nodesCount++;
            if ((nodesCount & 0xfff) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    else // 25 and more numbers
    {
        const gint64* subsumsTbl = controller->getSubsumsTbl();
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumHashSubsets32Method(
                      sumChanges, subsumsTbl, nodeHashBits,
                      nodeHashTable, nodeHashSubsets, gint64(node.sum), foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
            
            nodesCount++;
            if ((nodesCount & 0xfff) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    controller->updateProgress(nodesCount, node.subset);
}

gsize HashCpuWorker::getMemQueueSize() const
{
    return 16384;
}

#ifdef HAVE_OPENCL
/*
 * OpenCLHashWorker
 */
void HashOpenCLWorker::init(HashController* controller, cl::Platform& clPlatform,
                  cl::Device& clDevice, bool hashGroupping)
{
    hashOpenCLWorksNum = 128;
    
    this->controller = controller;
    bigHashGroupBits = 0; // no big hash
    this->hashGroupping = hashGroupping;
    std::string platformName;
    std::string deviceName;
    
    cl_context_properties clContextProps[5];
    clContextProps[0] = CL_CONTEXT_PLATFORM;
    clContextProps[1] = reinterpret_cast<cl_context_properties>(clPlatform());
    clContextProps[2] = 0;
    clContext = cl::Context(clDevice, clContextProps);
    
    clPlatform.getInfo(CL_PLATFORM_NAME, &platformName);
    clDevice.getInfo(CL_DEVICE_NAME, &deviceName);
    
    guint computeUnitsNum;
    guint64 maxAllocSize;
    guint64 globalMemorySize;
    
    clDevice.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &computeUnitsNum);
    clDevice.getInfo(CL_DEVICE_MAX_MEM_ALLOC_SIZE, &maxAllocSize);
    clDevice.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &globalMemorySize);
    
    clDevice.getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, &groupSize);
    cl_device_type devType;
    clDevice.getInfo(CL_DEVICE_TYPE, &devType);
    if ((devType & CL_DEVICE_TYPE_CPU) != 0)
        groupSize = 8;
    workSize = computeUnitsNum*groupSize*64;
    
    //maxAllocSize = 8*512;
    const guint nodeHashBits = controller->getNodeHashBits();
    const guint hashedNumbers = controller->getNodeHashedNumbers();
    // determine bigHashGroupBits
    const gsize neededNodeHashBytes = (1UL<<nodeHashBits)*sizeof(NodeHashEntry);
    gsize neededConstTotalBytes = workSize*(2UL*(8UL + sizeof(HashCLFoundStruct))) +
            256*1024 /* footprint */;
    gsize neededVarTotalBytes = neededNodeHashBytes;
    if (!controller->isUseHashSubsets()) // no subsets
        neededVarTotalBytes += 8UL*(1UL<<hashedNumbers);
    else // normal mode (use hashlists)
        neededVarTotalBytes += 4UL*(1UL<<hashedNumbers);
    
    if (maxAllocSize < neededNodeHashBytes ||
        (globalMemorySize < neededConstTotalBytes + neededVarTotalBytes))
    {   // use hashBits
        // quadruple worksize for performance
        workSize <<= 2;
        neededConstTotalBytes = workSize*(2UL*(8UL + sizeof(HashCLFoundStruct))) +
                256*1024; 
        hashOpenCLWorksNum >>= 2;
        // quadruple worksize for performance (end)
        gsize hashBytes = neededNodeHashBytes;
        gsize varTotalBytes = neededVarTotalBytes;
        for (bigHashGroupBits = 0; maxAllocSize < hashBytes ||
             globalMemorySize < neededConstTotalBytes + varTotalBytes;
             hashBytes>>=1, varTotalBytes>>=1, bigHashGroupBits++);
        
        neededConstTotalBytes = (workSize>>1)*(2UL*(8UL + sizeof(HashCLFoundStruct))) +
                256*1024;
        // check better way
        if (globalMemorySize >= neededConstTotalBytes + (varTotalBytes<<1) &&
            maxAllocSize < (hashBytes<<1))
        {   // better is available
            bigHashGroupBits--;
            workSize >>= 1;
            hashOpenCLWorksNum <<= 1;
        }
        else
        {
            neededConstTotalBytes = (workSize>>2)*(2UL*(8UL + sizeof(HashCLFoundStruct))) +
                    256*1024;
            if (globalMemorySize >= neededConstTotalBytes + (varTotalBytes<<1) &&
                maxAllocSize < (hashBytes<<1))
            {   // better is available
                bigHashGroupBits--;
                workSize >>= 2;
                hashOpenCLWorksNum <<= 2;
            }
        }
        
        if (bigHashGroupBits > 4)
            throw SubsetSumException("Cant use bigHash with more than 16 groups");
    }
    
    std::vector<cl::Device> clDevices;
    clDevices.push_back(clDevice);
    
    cl::Program::Sources kernelSources;
    if (bigHashGroupBits==0)
    {
        if (!hashGroupping)
            kernelSources.push_back(std::make_pair(
                    (char*)HashKernel_cl, HashKernel_cl_len));
        else
            kernelSources.push_back(std::make_pair(
                    (char*)HashKernelGroups_cl, HashKernelGroups_cl_len));
    }
    else    // choose bighash kernel
    {
        if (!hashGroupping)
            kernelSources.push_back(std::make_pair(
                    (char*)HashKernelBigHash_cl, HashKernelBigHash_cl_len));
        else
            kernelSources.push_back(std::make_pair(
                    (char*)HashKernelBHGroups_cl, HashKernelBHGroups_cl_len));
    }
    
    clProgram = cl::Program(clContext, kernelSources);
    try
    {
        if (!controller->isUseHashSubsets())
            clProgram.build(clDevices, "");
        else if (hashedNumbers <= 8)
            clProgram.build(clDevices, "-DSUBSUMSTBL=8");
        else if (hashedNumbers <= 16)
            clProgram.build(clDevices, "-DSUBSUMSTBL=16");
        else if (hashedNumbers <= 24)
            clProgram.build(clDevices, "-DSUBSUMSTBL=24");
        else
            clProgram.build(clDevices, "-DSUBSUMSTBL=32");
    }
    catch(cl::Error& err)
    {
        std::string buildLog;
        clProgram.getBuildInfo(clDevices[0], CL_PROGRAM_BUILD_LOG, &buildLog);
        std::cerr << buildLog << std::endl;
        throw;
    }
    
    kernel = cl::Kernel(clProgram, "subsetSumHashKernel");
    {
        gsize kernelGroupSize;
        kernel.getWorkGroupInfo(clDevice, CL_KERNEL_WORK_GROUP_SIZE, &kernelGroupSize);
        if ((devType & CL_DEVICE_TYPE_CPU) == 0)
        {
            if (kernelGroupSize != groupSize && (workSize % kernelGroupSize) == 0)
                groupSize = kernelGroupSize;
        }
    }
    
    std::cout << "HashOpenCLWorker: Device: " << platformName << ":" << deviceName <<
        "\n  GroupSize: " << groupSize << ", WorkSize: " << workSize  << 
        ((hashGroupping) ? " (hashGroupping)" : "") << std::endl;
    if (bigHashGroupBits != 0)
        std::cout << "  Using BigHash: GroupBits: " << bigHashGroupBits << std::endl;
    
    if (controller->getNumbersNum() <= 16)
        return;
    
    cmdQueue1 = cl::CommandQueue(clContext, clDevice);
    cmdQueue2 = cl::CommandQueue(clContext, clDevice);
    
    const gsize nodeHashSize = 1UL<<(controller->getNodeHashBits());
    const gsize nodeHashListsSize = 1UL<<(controller->getNodeHashedNumbers());
    
    workBuffer1 = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*workSize, NULL);
    workBuffer2 = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*workSize, NULL);
    sumChangesBuf = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*256UL, NULL);
    findIndicesBuf1 = cl::Buffer(clContext, CL_MEM_READ_WRITE,
                                 sizeof(HashCLFoundStruct)*workSize, NULL);
    if (bigHashGroupBits == 0)
    {
        hashBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                                    sizeof(NodeHashEntry)*nodeHashSize, NULL);
        if (!controller->isUseHashSubsets())
            hashListsBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                                    8UL*nodeHashListsSize, NULL);
        else
            hashListsBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                                    4UL*nodeHashListsSize, NULL);
    }
    else // big hash
    {
        guint maxGroupListElemsNum = 0;
        bigHashListIndices[0] = 0;
        guint hashGroupSize = (1U<<(nodeHashBits-bigHashGroupBits));
        const NodeHashEntry* nodeHash = controller->getNodeHash();
        
        for (guint grpId = 0; grpId < (1U<<bigHashGroupBits); grpId++)
        {
            bigHashListIndices[grpId+1] = bigHashListIndices[grpId];
            for (guint32 hkey = hashGroupSize*grpId + hashGroupSize;
                 hkey > hashGroupSize*grpId;)
            {
                hkey--;
                if (nodeHash[hkey].size != 0)
                {
                    bigHashListIndices[grpId+1] =
                            nodeHash[hkey].getPos() + nodeHash[hkey].size;
                    // update maxgrouplistelemsNum
                    maxGroupListElemsNum = std::max(maxGroupListElemsNum,
                            bigHashListIndices[grpId+1] - bigHashListIndices[grpId]);
                    break;
                }   
            }
        }
        hashBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                        sizeof(NodeHashEntry)*hashGroupSize, NULL);
        if (!controller->isUseHashSubsets())
            hashListsBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                                    8UL*maxGroupListElemsNum, NULL);
        else // useHashSubsets
            hashListsBuffer = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                                    4UL*maxGroupListElemsNum, NULL);
    }
    findIndicesBuf2 = cl::Buffer(clContext, CL_MEM_READ_WRITE,
                                 sizeof(HashCLFoundStruct)*workSize, NULL);
    findIndicesNumBuf1 = cl::Buffer(clContext, CL_MEM_READ_WRITE, gsize(4U), NULL);
    findIndicesNumBuf2 = cl::Buffer(clContext, CL_MEM_READ_WRITE, gsize(4U), NULL);
    
    cmdQueue1.enqueueWriteBuffer(sumChangesBuf, CL_TRUE, 0UL, 256UL*8UL,
                                 controller->getSumChanges());
    if (bigHashGroupBits == 0)
    {   // if no bighash
        cmdQueue1.enqueueWriteBuffer(hashBuffer, CL_TRUE, 0UL,
                         sizeof(NodeHashEntry)*nodeHashSize, controller->getNodeHash());
        if (!controller->isUseHashSubsets())
            cmdQueue1.enqueueWriteBuffer(hashListsBuffer, CL_TRUE, 0UL,
                         8UL*nodeHashListsSize, controller->getNodeHashLists());
        else // use hash subsets
            cmdQueue1.enqueueWriteBuffer(hashListsBuffer, CL_TRUE, 0UL,
                         4UL*nodeHashListsSize, controller->getNodeHashSubsets());
    }
    
    if (controller->isUseHashSubsets())
    {
        const gsize subsumsTblSize = ((hashedNumbers+7)>>3)*256;
        subsumsTblBuf = cl::Buffer(clContext, CL_MEM_READ_ONLY,
                        8UL*subsumsTblSize, NULL);
        cmdQueue1.enqueueWriteBuffer(subsumsTblBuf, CL_TRUE, 0UL, 8UL*subsumsTblSize,
                         controller->getSubsumsTbl());
    }
}

void HashOpenCLWorker::runOnNormalHash()
{
    gsize curWorkSize1 = 0;
    gsize curWorkSize2 = 0;
    
    guint zero = 0;
    
    NodeSubset* nodes1 = new NodeSubset[workSize];
    NodeSubset* nodes2 = new NodeSubset[workSize];
    
    kernel.setArg(1, sumChangesBuf());
    kernel.setArg(2, guint(controller->getNodeHashBits()));
    kernel.setArg(3, hashBuffer());
    kernel.setArg(4, hashListsBuffer());
    if (controller->isUseHashSubsets())
        kernel.setArg(8, subsumsTblBuf());
    
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1, CL_TRUE, 0UL, 4UL, &zero);
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf2, CL_TRUE, 0UL, 4UL, &zero);
    
    DirectPop popper(controller->getMemQueue(), workSize);
    
    do { // first pass
        cl::Event kernelEvent1;
        if (curWorkSize1 != 0)
        {   // execute kernel
            kernel.setArg(0, guint(curWorkSize1));
            kernel.setArg(5, workBuffer1());
            kernel.setArg(6, findIndicesNumBuf1());
            kernel.setArg(7, findIndicesBuf1());
            const gsize alignedWorkSize = ((curWorkSize1 + groupSize-1)/
                    groupSize)*groupSize;
            cmdQueue1.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                   cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                   NULL, &kernelEvent1);
        }
        
        if (curWorkSize2 != 0)
        {   // process previous work
            guint foundNum;
            cmdQueue2.enqueueReadBuffer(findIndicesNumBuf2, CL_TRUE,
                                        0UL, 4UL, &foundNum);
            if (foundNum != 0)
            {   // check
                HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                cmdQueue2.enqueueReadBuffer(findIndicesBuf2, CL_TRUE, 0UL,
                        sizeof(HashCLFoundStruct)*foundNum, founds);
                
                for (guint i = 0; i < foundNum; i++)
                {   /* check and send solution if good */
                    const NodeSubset& node = nodes2[founds[i].workIndex];
                    for (guint l = 0; l < 8; l++)
                        if (founds[i].foundBits[l]!=0)
                        {
                            guint bit = 1;
                            for (guint j = 0; j < 32; j++, bit <<= 1)
                                if ((founds[i].foundBits[l] & bit) != 0)
                                    checkAndSendSolution(node.subset, node.sum, j+(l<<5));
                        }
                }
                
                delete[] founds;
                // clear find buffers
                cmdQueue2.enqueueWriteBuffer(findIndicesNumBuf2,
                     CL_TRUE, 0UL, 4UL, &zero);
            }
            
            controller->updateProgress(curWorkSize2, nodes2[curWorkSize2-1].subset);
        }
        // get data to workBuffer2
        gint64* workData2 = (gint64*)cmdQueue2.enqueueMapBuffer(
                workBuffer2, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
        for (curWorkSize2 = 0; curWorkSize2 < workSize; curWorkSize2++)
        {
            NodeSubset node;
            if (!popper.pop<NodeSubset>(node))
                break;
            workData2[curWorkSize2] = node.sum;
            nodes2[curWorkSize2] = node;
        }
        cmdQueue2.enqueueUnmapMemObject(workBuffer2, workData2);
        cmdQueue2.finish();
        
        if (curWorkSize1 != 0)
            kernelEvent1.wait();
        
        /// second pass
        cl::Event kernelEvent2;
        if (curWorkSize2 != 0)
        {   // execute kernel
            kernel.setArg(0, guint(curWorkSize2));
            kernel.setArg(5, workBuffer2());
            kernel.setArg(6, findIndicesNumBuf2());
            kernel.setArg(7, findIndicesBuf2());
            const gsize alignedWorkSize = ((curWorkSize2 + groupSize-1)/
                    groupSize)*groupSize;
            cmdQueue2.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                   cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                   NULL, &kernelEvent2);
        }
        
        if (curWorkSize1 != 0)
        {   // process previous work
            guint foundNum;
            cmdQueue1.enqueueReadBuffer(findIndicesNumBuf1, CL_TRUE,
                                        0UL, 4UL, &foundNum);
            if (foundNum != 0)
            {   // check
                HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                cmdQueue1.enqueueReadBuffer(findIndicesBuf1, CL_TRUE, 0UL,
                        sizeof(HashCLFoundStruct)*foundNum, founds);
                
                for (guint i = 0; i < foundNum; i++)
                {   /* check and send solution if good */
                    const NodeSubset& node = nodes1[founds[i].workIndex];
                    for (guint l = 0; l < 8; l++)
                        if (founds[i].foundBits[l]!=0)
                        {
                            guint bit = 1;
                            for (guint j = 0; j < 32; j++, bit <<= 1)
                                if ((founds[i].foundBits[l] & bit) != 0)
                                    checkAndSendSolution(node.subset, node.sum, j+(l<<5));
                        }
                }
                
                delete[] founds;
                // clear find buffers
                cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1,
                     CL_TRUE, 0UL, 4UL, &zero);
            }
            
            controller->updateProgress(curWorkSize1, nodes1[curWorkSize1-1].subset);
        }
        // get data to workBuffer1
        gint64* workData1 = (gint64*)cmdQueue1.enqueueMapBuffer(
                workBuffer1, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
        for (curWorkSize1 = 0; curWorkSize1 < workSize; curWorkSize1++)
        {
            NodeSubset node;
            if (!popper.pop<NodeSubset>(node))
                break;
            workData1[curWorkSize1] = node.sum;
            nodes1[curWorkSize1] = node;
        }
        cmdQueue1.enqueueUnmapMemObject(workBuffer1, workData1);
        cmdQueue1.finish();
        
        if (curWorkSize2 != 0)
            kernelEvent2.wait();
        
    } while (curWorkSize1 != 0 || curWorkSize2 != 0);
    
    delete[] nodes1;
    delete[] nodes2;
}

//static const guint hashOpenCLWorksNum = 128;

void HashOpenCLWorker::runOnBigHash()
{
    gsize curWorkSize1 = 0;
    gsize curWorkSize2 = 0;
    
    const bool useHashSubsets = controller->isUseHashSubsets();
    guint zero = 0;
    
    gsize nodePosShift1 = 0;
    gsize nodePosShift2 = 0;
    NodeSubset* nodes = new NodeSubset[workSize*hashOpenCLWorksNum];
    
    const NodeHashEntry* nodeHash = controller->getNodeHash();
    const gint64* nodeHashLists = controller->getNodeHashLists();
    const guint32* nodeHashSubsets = controller->getNodeHashSubsets();
    
    const guint nodeHashBits = controller->getNodeHashBits();
    const gsize hashGroupSize = 1U<<(nodeHashBits-bigHashGroupBits);
        
    kernel.setArg(2, guint(bigHashGroupBits));
    kernel.setArg(4, sumChangesBuf());
    kernel.setArg(5, nodeHashBits);
    kernel.setArg(6, hashBuffer());
    kernel.setArg(7, hashListsBuffer());
    if (controller->isUseHashSubsets())
        kernel.setArg(11, subsumsTblBuf());
    
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1, CL_TRUE, 0UL, 4UL, &zero);
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf2, CL_TRUE, 0UL, 4UL, &zero);
    
    gsize nodesFetched = 0;
    do {
        kernel.setArg(1, guint(0U));
        kernel.setArg(3, guint(0U));
        // update buffers for first phase
        cmdQueue1.enqueueWriteBuffer(hashBuffer, CL_FALSE, 0UL,
                         sizeof(NodeHashEntry)*hashGroupSize, nodeHash);
        if (bigHashListIndices[1]!=0)
        {
            if (!useHashSubsets)
                cmdQueue2.enqueueWriteBuffer(hashListsBuffer, CL_TRUE, 0UL,
                         8UL*bigHashListIndices[1], nodeHashLists);
            else
                cmdQueue2.enqueueWriteBuffer(hashListsBuffer, CL_TRUE, 0UL,
                         4UL*bigHashListIndices[1], nodeHashSubsets);
        }
        cmdQueue1.finish();
        /* first phase */
        guint x = 0;
        
        curWorkSize1 = 0;
        curWorkSize2 = 0;
        
        nodesFetched = 0;
        {
            DirectPop popper(controller->getMemQueue(), workSize);
            do { // first pass
                cl::Event kernelEvent1;
                if (curWorkSize1 != 0)
                {   // execute kernel
                    kernel.setArg(0, guint(curWorkSize1));
                    kernel.setArg(8, workBuffer1());
                    kernel.setArg(9, findIndicesNumBuf1());
                    kernel.setArg(10, findIndicesBuf1());
                    const gsize alignedWorkSize =
                            ((curWorkSize1 + groupSize-1)/groupSize)*groupSize;
                    cmdQueue1.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                           cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                           NULL, &kernelEvent1);
                }
                
                if (curWorkSize2 != 0)
                {   // process previous work
                    guint foundNum;
                    cmdQueue2.enqueueReadBuffer(findIndicesNumBuf2, CL_TRUE,
                                                0UL, 4UL, &foundNum);
                    if (foundNum != 0)
                    {   // check
                        HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                        cmdQueue2.enqueueReadBuffer(findIndicesBuf2, CL_TRUE, 0UL,
                                sizeof(HashCLFoundStruct)*foundNum, founds);
                        
                        for (guint i = 0; i < foundNum; i++)
                        {   /* check and send solution if good */
                            const NodeSubset& node =
                                    nodes[nodePosShift2 + founds[i].workIndex];
                            for (guint l = 0; l < 8; l++)
                                if (founds[i].foundBits[l]!=0)
                                {
                                    guint bit = 1;
                                    for (guint j = 0; j < 32; j++, bit <<= 1)
                                        if ((founds[i].foundBits[l] & bit) != 0)
                                            checkAndSendSolution(node.subset, node.sum,
                                                                 j+(l<<5));
                                }
                        }
                        
                        delete[] founds;
                        // clear find buffers
                        cmdQueue2.enqueueWriteBuffer(findIndicesNumBuf2,
                             CL_TRUE, 0UL, 4UL, &zero);
                    }
                }
                // get data to workBuffer2
                if (x < hashOpenCLWorksNum)
                {
                    gint64* workData2 = (gint64*)cmdQueue2.enqueueMapBuffer(
                            workBuffer2, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
                    nodePosShift2 = x*workSize;
                    for (curWorkSize2 = 0; curWorkSize2 < workSize; curWorkSize2++)
                    {
                        NodeSubset node;
                        if (!popper.pop<NodeSubset>(node))
                            break;
                        workData2[curWorkSize2] = node.sum;
                        nodes[nodePosShift2 + curWorkSize2] = node;
                    }
                    cmdQueue2.enqueueUnmapMemObject(workBuffer2, workData2);
                    cmdQueue2.finish();
                    if (curWorkSize2 != 0)
                        nodesFetched = nodePosShift2 + curWorkSize2;
                    x++;
                }
                else // if no WorksNum
                    curWorkSize2 = 0;
                
                if (curWorkSize1 != 0)
                    kernelEvent1.wait();
                
                /// second pass
                cl::Event kernelEvent2;
                if (curWorkSize2 != 0)
                {   // execute kernel
                    kernel.setArg(0, guint(curWorkSize2));
                    kernel.setArg(8, workBuffer2());
                    kernel.setArg(9, findIndicesNumBuf2());
                    kernel.setArg(10, findIndicesBuf2());
                    const gsize alignedWorkSize =
                            ((curWorkSize2 + groupSize-1)/groupSize)*groupSize;
                    cmdQueue2.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                           cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                           NULL, &kernelEvent2);
                }
                
                if (curWorkSize1 != 0)
                {   // process previous work
                    guint foundNum;
                    cmdQueue1.enqueueReadBuffer(findIndicesNumBuf1, CL_TRUE,
                                                0UL, 4UL, &foundNum);
                    if (foundNum != 0)
                    {   // check
                        HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                        cmdQueue1.enqueueReadBuffer(findIndicesBuf1, CL_TRUE, 0UL,
                                sizeof(HashCLFoundStruct)*foundNum, founds);
                        
                        for (guint i = 0; i < foundNum; i++)
                        {   /* check and send solution if good */
                            const NodeSubset& node =
                                    nodes[nodePosShift1 + founds[i].workIndex];
                            for (guint l = 0; l < 8; l++)
                                if (founds[i].foundBits[l]!=0)
                                {
                                    guint bit = 1;
                                    for (guint j = 0; j < 32; j++, bit <<= 1)
                                        if ((founds[i].foundBits[l] & bit) != 0)
                                            checkAndSendSolution(node.subset, node.sum,
                                                                 j+(l<<5));
                                }
                        }
                        
                        delete[] founds;
                        // clear find buffers
                        cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1,
                             CL_TRUE, 0UL, 4UL, &zero);
                    }
                }
                // get data to workBuffer1
                if (x < hashOpenCLWorksNum)
                {
                    gint64* workData1 = (gint64*)cmdQueue1.enqueueMapBuffer(
                            workBuffer1, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
                    nodePosShift1 = x*workSize;
                    for (curWorkSize1 = 0; curWorkSize1 < workSize; curWorkSize1++)
                    {
                        NodeSubset node;
                        if (!popper.pop<NodeSubset>(node))
                            break;
                        workData1[curWorkSize1] = node.sum;
                        nodes[nodePosShift1+curWorkSize1] = node;
                    }
                    cmdQueue1.enqueueUnmapMemObject(workBuffer1, workData1);
                    cmdQueue1.finish();
                    if (curWorkSize1 != 0)
                        nodesFetched = nodePosShift1 + curWorkSize1;
                    x++;
                }
                else // if no works
                    curWorkSize1 = 0;
                
                if (curWorkSize2 != 0)
                    kernelEvent2.wait();
                
            } while (curWorkSize1 != 0 || curWorkSize2 != 0);
            
            if (nodesFetched == 0)
                break; // no data
        }
        
        /* next phases */
        for (guint grpId = 1; grpId < (1U<<bigHashGroupBits); grpId++)
        {
            const guint indexShift = bigHashListIndices[grpId];
            kernel.setArg(1, grpId<<(nodeHashBits-bigHashGroupBits));
            kernel.setArg(3, indexShift);
            // prepare buffers for next phase (group)
            if (bigHashListIndices[grpId+1]-bigHashListIndices[grpId] != 0)
            {
                if (!useHashSubsets)
                    cmdQueue2.enqueueWriteBuffer(hashListsBuffer, CL_FALSE, 0UL,
                         8UL*(bigHashListIndices[grpId+1]-bigHashListIndices[grpId]),
                         nodeHashLists + bigHashListIndices[grpId]);
                else
                    cmdQueue2.enqueueWriteBuffer(hashListsBuffer, CL_FALSE, 0UL,
                         4UL*(bigHashListIndices[grpId+1]-bigHashListIndices[grpId]),
                         nodeHashSubsets + bigHashListIndices[grpId]);
            }
            
            cmdQueue1.enqueueWriteBuffer(hashBuffer, CL_TRUE, 0UL,
                    sizeof(NodeHashEntry)*hashGroupSize, nodeHash + hashGroupSize*grpId);
            cmdQueue2.finish();
            
            // now doing this phase
            x = 0;
            curWorkSize1 = 0;
            curWorkSize2 = 0;
            
            do { // first pass
                cl::Event kernelEvent1;
                if (curWorkSize1 != 0)
                {   // execute kernel
                    kernel.setArg(0, guint(curWorkSize1));
                    kernel.setArg(8, workBuffer1());
                    kernel.setArg(9, findIndicesNumBuf1());
                    kernel.setArg(10, findIndicesBuf1());
                    const gsize alignedWorkSize =
                            ((curWorkSize1 + groupSize-1)/groupSize)*groupSize;
                    cmdQueue1.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                           cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                           NULL, &kernelEvent1);
                }
                
                if (curWorkSize2 != 0)
                {   // process previous work
                    guint foundNum;
                    cmdQueue2.enqueueReadBuffer(findIndicesNumBuf2, CL_TRUE,
                                                0UL, 4UL, &foundNum);
                    if (foundNum != 0)
                    {   // check
                        HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                        cmdQueue2.enqueueReadBuffer(findIndicesBuf2, CL_TRUE, 0UL,
                                sizeof(HashCLFoundStruct)*foundNum, founds);
                        
                        for (guint i = 0; i < foundNum; i++)
                        {   /* check and send solution if good */
                            const NodeSubset& node =
                                    nodes[nodePosShift2 + founds[i].workIndex];
                            for (guint l = 0; l < 8; l++)
                                if (founds[i].foundBits[l]!=0)
                                {
                                    guint bit = 1;
                                    for (guint j = 0; j < 32; j++, bit <<= 1)
                                        if ((founds[i].foundBits[l] & bit) != 0)
                                            checkAndSendSolution(node.subset, node.sum,
                                                                 j+(l<<5));
                                }
                        }
                        
                        delete[] founds;
                        // clear find buffers
                        cmdQueue2.enqueueWriteBuffer(findIndicesNumBuf2,
                             CL_TRUE, 0UL, 4UL, &zero);
                    }
                }
                // get data to workBuffer2
                if (workSize*x < nodesFetched)
                {
                    gint64* workData2 = (gint64*)cmdQueue2.enqueueMapBuffer(
                            workBuffer2, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
                    nodePosShift2 = x*workSize;
                    const gsize workSizeCur =
                        std::min(workSize, nodesFetched-nodePosShift2);
                        
                    for (curWorkSize2 = 0; curWorkSize2 < workSizeCur; curWorkSize2++)
                        workData2[curWorkSize2] = nodes[nodePosShift2 + curWorkSize2].sum;
                    cmdQueue2.enqueueUnmapMemObject(workBuffer2, workData2);
                    cmdQueue2.finish();
                    x++;
                }
                else // if no WorksNum
                    curWorkSize2 = 0;
                
                if (curWorkSize1 != 0)
                    kernelEvent1.wait();
                
                /// second pass
                cl::Event kernelEvent2;
                if (curWorkSize2 != 0)
                {   // execute kernel
                    kernel.setArg(0, guint(curWorkSize2));
                    kernel.setArg(8, workBuffer2());
                    kernel.setArg(9, findIndicesNumBuf2());
                    kernel.setArg(10, findIndicesBuf2());
                    const gsize alignedWorkSize =
                            ((curWorkSize2 + groupSize-1)/groupSize)*groupSize;
                    cmdQueue2.enqueueNDRangeKernel(kernel, cl::NDRange(0),
                           cl::NDRange(alignedWorkSize), cl::NDRange(groupSize),
                           NULL, &kernelEvent2);
                }
                
                if (curWorkSize1 != 0)
                {   // process previous work
                    guint foundNum;
                    cmdQueue1.enqueueReadBuffer(findIndicesNumBuf1, CL_TRUE,
                                                0UL, 4UL, &foundNum);
                    if (foundNum != 0)
                    {   // check
                        HashCLFoundStruct* founds = new HashCLFoundStruct[foundNum];
                        cmdQueue1.enqueueReadBuffer(findIndicesBuf1, CL_TRUE, 0UL,
                                sizeof(HashCLFoundStruct)*foundNum, founds);
                        
                        for (guint i = 0; i < foundNum; i++)
                        {   /* check and send solution if good */
                            const NodeSubset& node =
                                    nodes[nodePosShift1 + founds[i].workIndex];
                            for (guint l = 0; l < 8; l++)
                                if (founds[i].foundBits[l]!=0)
                                {
                                    guint bit = 1;
                                    for (guint j = 0; j < 32; j++, bit <<= 1)
                                        if ((founds[i].foundBits[l] & bit) != 0)
                                            checkAndSendSolution(node.subset, node.sum,
                                                                 j+(l<<5));
                                }
                        }
                        
                        delete[] founds;
                        // clear find buffers
                        cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1,
                             CL_TRUE, 0UL, 4UL, &zero);
                    }
                }
                // get data to workBuffer1
                if (workSize*x < nodesFetched)
                {
                    gint64* workData1 = (gint64*)cmdQueue1.enqueueMapBuffer(
                            workBuffer1, CL_TRUE, CL_MAP_WRITE, 0UL, 8UL*workSize);
                    nodePosShift1 = x*workSize;
                    const gsize workSizeCur =
                        std::min(workSize, nodesFetched-nodePosShift1);
                    for (curWorkSize1 = 0; curWorkSize1 < workSizeCur; curWorkSize1++)
                        workData1[curWorkSize1] = nodes[nodePosShift1+curWorkSize1].sum;
                    cmdQueue1.enqueueUnmapMemObject(workBuffer1, workData1);
                    cmdQueue1.finish();
                    x++;
                }
                else // if no works
                    curWorkSize1 = 0;
                
                if (curWorkSize2 != 0)
                    kernelEvent2.wait();
                
            } while (curWorkSize1 != 0 || curWorkSize2 != 0);
        }
        
        if (nodesFetched != 0)
            controller->updateProgress(nodesFetched, nodes[nodesFetched-1].subset);
    } while (nodesFetched >= workSize*hashOpenCLWorksNum);
    
    delete[] nodes;
}

void HashOpenCLWorker::run()
{
    if (controller->getNumbersNum() <= 16)
        return;
    if (bigHashGroupBits==0)
        runOnNormalHash();
    else
        runOnBigHash();
}

gsize HashOpenCLWorker::getMemQueueSize() const
{
    return (bigHashGroupBits==0)?workSize:workSize*(hashOpenCLWorksNum>>2);
}
#endif
