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
#include <iostream>
#include <string>
#include <vector>
#include <glibmm.h>
#include "PushPop.h"
#include "NaiveWorkers.h"

using namespace Glib;

#ifdef HAVE_OPENCL
extern unsigned char NaiveKernel_cl[];
extern unsigned int NaiveKernel_cl_len;
extern unsigned char Naive64BitKernel_cl[];
extern unsigned int Naive64BitKernel_cl_len;
#endif

guint subsetSumNaiveMethod(const gint64* sumChanges, gint64 inputSum,
           guint* foundIndices)
{
    const gint64 v0 = sumChanges[32 + 0];
    const gint64 v1 = sumChanges[32 + 1] - v0;
    const gint64 s1 = v0 + sumChanges[32 + 1];
    const gint64 v2 = sumChanges[32 + 2] - s1;
    const gint64 s2 = s1 + sumChanges[32 + 2];
    const gint64 v3 = sumChanges[32 + 3] - s2;
    const gint64 s3 = s2 + sumChanges[32 + 3];
    const gint64 v4 = sumChanges[32 + 4] - s3;
    const gint64 s4 = s3 + sumChanges[32 + 4];
    const gint64 v5 = sumChanges[32 + 5] - s4;
    const gint64 s5 = s4 + sumChanges[32 + 5];
    const gint64 v6 = sumChanges[32 + 6] - s5;
    const gint64 s6 = s5 + sumChanges[32 + 6];
    const gint64 v7 = sumChanges[32 + 7] - s6;
    const gint64 s7 = s6 + sumChanges[32 + 7];
    const gint64 v8 = sumChanges[32 + 8] - s7;
    
    guint foundIndicesNum = 0;
    for (guint current = 0; current < 32; current++)
    {
        guint find = 0;
        gint64 sum = inputSum + sumChanges[current];
        find |= (sum==0);
        /* main long work */
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
            foundIndices[foundIndicesNum++] = current;
    }
    return foundIndicesNum;
}

void NaiveWorker::checkAndSendSolution(const Int128& initialSubset,
               const Int128& initialSum, guint foundIndex) const
{
    const guint numbersNum = controller->getNumbersNum();
    const Int128* numbers = controller->getNumbers();
    Int128 findIndexSum = initialSum;
    guint bit = 1;
    for (guint x = 0; x < 5; x++, bit<<=1)
        if ((bit&foundIndex)!=0)
            findIndexSum += numbers[numbersNum-14+x];
    
    guint prevSubset = 0;
    Int128 sum = findIndexSum;
    for (guint subset = 0; subset < 512; subset++)
    {
        bit = 1;
        const guint changes = prevSubset^subset;
        for (guint bitNum=0; (bit & changes)!=0 && bitNum < 9; bit<<=1, bitNum++)
            if ((subset&bit)!=0) // subset=1,prevSubset=0
                sum += numbers[numbersNum-9+bitNum];
            else // subset=0,prevSubset=1
                sum -= numbers[numbersNum-9+bitNum];
        
        prevSubset = subset;
        Int128 finalSubset = initialSubset | (Int128(subset)<<int(numbersNum-9)) |
                (Int128(foundIndex)<<int(numbersNum-14));
        if (!sum && finalSubset) // if zero
            controller->putSolution(finalSubset);
    }
}

NaiveCpuWorker::NaiveCpuWorker()
{
#if defined(HAVE_ARCH_INTEL) && (defined(HAVE_SSE4) || defined(HAVE_SSE2))
    useSSE4 = false;
    useSSE2 = false;
    stdCode = false;
#endif
}

void NaiveCpuWorker::init(NaiveController* controller, guint threadId)
{
    this->controller = controller;
    std::cout << "NaiveCPUWorker: threadId " << threadId << std::endl;
#if defined(HAVE_ARCH_INTEL) && (defined(HAVE_SSE4) || defined(HAVE_SSE2))
    if (!useSSE2 && !useSSE4 && !stdCode && X86CheckCPUIDAvailable())
    {
        guint32 eax,ebx,ecx,edx;
        CPUID(0, &eax, &ebx, &ecx, &edx);
        guint32 maxCPUIDStandardLevel = eax;
#  ifdef HAVE_SSE4
        if (maxCPUIDStandardLevel >= 1)
        {
            CPUID(1UL, &eax, &ebx, &ecx, &edx);
            if ((ecx & (1U<<19)) != 0)
                useSSE4 = true;
            if (useSSE4)
                std::cout << "  Using SSE4.1" << std::endl;
            else
            {
#  endif
                if ((edx & (1U<<26)) != 0)
                    useSSE2 = true;
                if (useSSE2)
                    std::cout << "  Using SSE2" << std::endl;
            }
#  ifdef HAVE_SSE4
        }
#  endif
    }
    else
    {
        if (useSSE4)
            std::cout << "  Using SSE4.1" << std::endl;
        else if (useSSE2)
            std::cout << "  Using SSE2" << std::endl;
    }
#endif
}

#if defined(HAVE_ARCH_INTEL) && (defined(HAVE_SSE4) || defined(HAVE_SSE2))
void NaiveCpuWorker::setInsnExts(InsnExts insnExts)
{
    if (insnExts == INSN_EXT_NONE)
    {
        stdCode = true;
    }
    if (insnExts == INSN_EXT_SSE2)
    {
        useSSE2 = true;
        useSSE4 = false;
    }
#  ifdef HAVE_SSE4
    if (insnExts == INSN_EXT_SSE4_1)
    {
        useSSE2 = false;
        useSSE4 = true;
    }
#  endif
}
#endif

void NaiveCpuWorker::run()
{
    if (controller->getNumbersNum() <= 16)
        return;
    DirectPop popper(controller->getMemQueue(), 8192);
    const gint64* sumChanges = controller->getSumChanges();
    
    NodeSubset node;
    node.subset = 0;
    node.sum = 0;
    guint nodesCount = 0;
#if defined(HAVE_ARCH_INTEL) && (defined(HAVE_SSE4) || defined(HAVE_SSE2))
    if (useSSE4 || useSSE2)
    {
        guint foundIndices[64];
        NodeSubset node2;
        while (popper.pop(node))
        {
            bool second = popper.pop(node2);
            guint foundIndicesNum;
#  ifdef HAVE_SSE4
            if (useSSE4)
                foundIndicesNum = subsetSumNaiveMethodSSE4(
                            sumChanges, node.sum, node2.sum, foundIndices);
            else
#  endif
                foundIndicesNum = subsetSumNaiveMethodSSE2(
                            sumChanges, node.sum, node2.sum, foundIndices);
            // check and send if match
            for (guint i = 0; i < foundIndicesNum; i++)
                if (foundIndices[i] < 32)
                    checkAndSendSolution(node.subset, node.sum, foundIndices[i]);
                else if (second)
                    checkAndSendSolution(node2.subset, node2.sum, foundIndices[i]-32);
            nodesCount++;
            if (second)
                nodesCount++;
            if ((nodesCount & 0xffe) == 0)
            {
                controller->updateProgress(nodesCount, node.subset);
                nodesCount = 0;
            }
        }
    }
    else
#endif
    {
        guint foundIndices[32];
        while (popper.pop(node))
        {
            const guint foundIndicesNum = subsetSumNaiveMethod(
                    sumChanges, node.sum, foundIndices);
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

gsize NaiveCpuWorker::getMemQueueSize() const
{
    return 8192;
}

#ifdef HAVE_OPENCL
void NaiveOpenCLWorker::init(NaiveController* controller,
         cl::Platform& clPlatform, cl::Device& clDevice, bool use64BitCode)
{
    this->controller = controller;
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
    
    clDevice.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &computeUnitsNum);
    
    cl_device_type devType;
    clDevice.getInfo(CL_DEVICE_TYPE, &devType);
    
    std::vector<cl::Device> clDevices;
    clDevices.push_back(clDevice);
    
    cl::Program::Sources kernelSources;
    if (use64BitCode)
        kernelSources.push_back(
            std::make_pair((char*)Naive64BitKernel_cl, Naive64BitKernel_cl_len));
    else
        kernelSources.push_back(
            std::make_pair((char*)NaiveKernel_cl, NaiveKernel_cl_len));
    clProgram = cl::Program(clContext, kernelSources);
    try
    { clProgram.build(clDevices, "-O"); }
    catch(cl::Error& err)
    {
        std::string buildLog;
        clProgram.getBuildInfo(clDevices[0], CL_PROGRAM_BUILD_LOG, &buildLog);
        std::cerr << buildLog << std::endl;
        throw;
    }
    
    kernel = cl::Kernel(clProgram, "subsetSumNaiveKernel");
    
    kernel.getWorkGroupInfo(clDevice, CL_KERNEL_WORK_GROUP_SIZE, &groupSize);
    if ((devType & CL_DEVICE_TYPE_CPU) != 0)
        groupSize = 8;
    
    workSize = computeUnitsNum*groupSize*64;
    
    std::cout << "NaiveOpenCLWorker: Device: " << platformName << ":" << deviceName <<
        "\n  GroupSize: " << groupSize << ", WorkSize: " << workSize <<
        ((use64BitCode) ? ", use64BitKernel" : "") << std::endl;
    
    if (controller->getNumbersNum() <= 16)
        return;
        
    cmdQueue1 = cl::CommandQueue(clContext, clDevice);
    cmdQueue2 = cl::CommandQueue(clContext, clDevice);
    
    workBuffer1 = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*workSize, NULL);
    workBuffer2 = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*workSize, NULL);
    sumChangesBuf = cl::Buffer(clContext, CL_MEM_READ_ONLY, 8UL*41UL, NULL);
    findIndicesBuf1 = cl::Buffer(clContext, CL_MEM_READ_WRITE,
                                 sizeof(NaiveCLFoundStruct)*workSize, NULL);
    findIndicesBuf2 = cl::Buffer(clContext, CL_MEM_READ_WRITE,
                                 sizeof(NaiveCLFoundStruct)*workSize, NULL);
    findIndicesNumBuf1 = cl::Buffer(clContext, CL_MEM_READ_WRITE, gsize(4), NULL);
    findIndicesNumBuf2 = cl::Buffer(clContext, CL_MEM_READ_WRITE, gsize(4), NULL);
    
    cmdQueue1.enqueueWriteBuffer(sumChangesBuf, CL_TRUE, 0UL, 41UL*8UL,
                                 controller->getSumChanges());
}

void NaiveOpenCLWorker::run()
{
    if (controller->getNumbersNum() <= 16)
        return;
    gsize curWorkSize1 = 0;
    gsize curWorkSize2 = 0;
    
    guint zero = 0;
    
    NodeSubset* nodes1 = new NodeSubset[workSize];
    NodeSubset* nodes2 = new NodeSubset[workSize];
    
    kernel.setArg(1, sumChangesBuf);
    
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf1, CL_TRUE, 0UL, 4UL, &zero);
    cmdQueue1.enqueueWriteBuffer(findIndicesNumBuf2, CL_TRUE, 0UL, 4UL, &zero);
    
    DirectPop popper(controller->getMemQueue(), workSize);
    
    do { // first pass
        cl::Event kernelEvent1;
        if (curWorkSize1 != 0)
        {   // execute kernel
            kernel.setArg(0, guint(curWorkSize1));
            kernel.setArg(2, workBuffer1);
            kernel.setArg(3, findIndicesNumBuf1);
            kernel.setArg(4, findIndicesBuf1);
            gsize alignedWorkSize = ((curWorkSize1 + groupSize-1)/groupSize)*groupSize;
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
                NaiveCLFoundStruct* founds = new NaiveCLFoundStruct[foundNum];
                cmdQueue2.enqueueReadBuffer(findIndicesBuf2, CL_TRUE, 0UL,
                        sizeof(NaiveCLFoundStruct)*foundNum, founds);
                
                for (guint i = 0; i < foundNum; i++)
                {   /* check and send solution if good */
                    const NodeSubset& node = nodes2[founds[i].workIndex];
                    if (founds[i].foundBits!=0)
                    {
                        guint bit = 1;
                        for (guint j = 0; j < 32; j++, bit <<= 1)
                            if ((founds[i].foundBits & bit) != 0)
                                checkAndSendSolution(node.subset, node.sum, j);
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
            kernel.setArg(2, workBuffer2);
            kernel.setArg(3, findIndicesNumBuf2);
            kernel.setArg(4, findIndicesBuf2);
            gsize alignedWorkSize = ((curWorkSize2 + groupSize-1)/groupSize)*groupSize;
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
                NaiveCLFoundStruct* founds = new NaiveCLFoundStruct[foundNum];
                cmdQueue1.enqueueReadBuffer(findIndicesBuf1, CL_TRUE, 0UL,
                        sizeof(NaiveCLFoundStruct)*foundNum, founds);
                
                for (guint i = 0; i < foundNum; i++)
                {   /* check and send solution if good */
                    const NodeSubset& node = nodes1[founds[i].workIndex];
                    if (founds[i].foundBits!=0)
                    {
                        guint bit = 1;
                        for (guint j = 0; j < 32; j++, bit <<= 1)
                            if ((founds[i].foundBits & bit) != 0)
                                checkAndSendSolution(node.subset, node.sum, j);
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

gsize NaiveOpenCLWorker::getMemQueueSize() const
{
    return workSize;
}
#endif
