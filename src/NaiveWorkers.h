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

#ifndef __SUBSETSUM_NAIVEWORKERS_H__
#define __SUBSETSUM_NAIVEWORKERS_H__

#include <string>
#include <exception>
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif
#include <glibmm.h>
#include "Int128.h"
#include "Queue.h"
#include "Controllers.h"
#include "X86CpuId.h"

using namespace Glib;

extern guint subsetSumNaiveMethod(const gint64* sumChanges, gint64 inputSum,
                                  guint* foundIndices);

#ifdef HAVE_ARCH_INTEL
extern guint subsetSumNaiveMethodSSE2(const gint64* sumChanges, gint64 inputSum1,
           guint64 inputSum2, guint* foundIndices);

extern guint subsetSumNaiveMethodSSE4(const gint64* sumChanges, gint64 inputSum1,
           guint64 inputSum2, guint* foundIndices);
#endif

class NaiveWorker
{
protected:
    NaiveController* controller;
public:
    void checkAndSendSolution(const Int128& initialSubset, const Int128& initialSum,
                  guint foundIndex) const;
                  
   virtual gsize getMemQueueSize() const = 0;
};

enum InsnExts {
    INSN_EXT_NONE = 0,
    INSN_EXT_SSE2 = 1,
    INSN_EXT_SSE4_1 = 2,
};

class NaiveCpuWorker: public NaiveWorker
{
private:
#if defined(HAVE_SSE2) || defined(HAVE_SSE4)
    bool stdCode;
    bool useSSE2;
    bool useSSE4;
#endif
public:
    NaiveCpuWorker();
    void init(NaiveController* controller, guint threadId);
    void run();
#if defined(HAVE_ARCH_INTEL) && (defined(HAVE_SSE2) || defined(HAVE_SSE4))
    void setInsnExts(InsnExts insnExts);
#endif
    
    gsize getMemQueueSize() const;
};

#ifdef HAVE_OPENCL
struct NaiveCLFoundStruct
{
    guint32 workIndex;
    guint32 foundBits;
};

class NaiveOpenCLWorker: public NaiveWorker
{
private:
    cl::Context clContext;
    cl::CommandQueue cmdQueue1;
    cl::CommandQueue cmdQueue2;
    
    gsize workSize;
    gsize groupSize;
    
    cl::Program clProgram;
    cl::Kernel kernel;
    
    cl::Buffer workBuffer1;
    cl::Buffer workBuffer2;
    cl::Buffer sumChangesBuf;
    cl::Buffer findIndicesBuf1;
    cl::Buffer findIndicesBuf2;
    cl::Buffer findIndicesNumBuf1;
    cl::Buffer findIndicesNumBuf2;
public:
    void init(NaiveController* controller, cl::Platform& clPlatform,
                      cl::Device& clDevice, bool use64BitCode);
    
    void run();
    
    gsize getMemQueueSize() const;
};
#endif

#endif
