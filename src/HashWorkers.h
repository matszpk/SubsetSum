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

#ifndef __SUBSETSUM_HASHWORKERS_H__
#define __SUBSETSUM_HASHWORKERS_H__

#include <string>
#include <exception>
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#include "Int128.h"
#include "Queue.h"
#include "Controllers.h"

using namespace Glib;

extern guint subsetSumHashMethod(const gint64* sumChanges,
         guint hashBits, const NodeHashEntry* hashTable, const gint64* hashLists,
         gint64 inputSum, guint* findIndices);

extern guint subsetSumHashSubsetsMethod8(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* findIndices);

extern guint subsetSumHashSubsetsMethod16(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* findIndices);

extern guint subsetSumHashSubsetsMethod24(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* findIndices);

extern guint subsetSumHashSubsetsMethod32(const gint64* sumChanges,
         const gint64* subsumsTbl, guint hashBits, const NodeHashEntry* hashTable,
         const guint32* hashSubsets, gint64 inputSum, guint* findIndices);

class HashWorker
{
protected:
    HashController* controller;
public:
    void checkAndSendSolution(const Int128& initialSubset, const Int128& initialSum,
                  guint foundIndex) const;
    virtual gsize getMemQueueSize() const = 0;
};

class HashCpuWorker: public HashWorker
{
public:
    void init(HashController* controller, guint threadId);
    void run();
    
    gsize getMemQueueSize() const;
};

#ifdef HAVE_OPENCL

struct HashCLFoundStruct
{
    guint32 workIndex;
    guint32 foundBits[8];
};

class HashOpenCLWorker: public HashWorker
{
private:
    cl::Context clContext;
    cl::CommandQueue cmdQueue1;
    cl::CommandQueue cmdQueue2;
    
    gsize workSize;
    gsize groupSize;
    
    cl::Program clProgram;
    cl::Kernel kernel;
    
    cl::Buffer hashBuffer;
    cl::Buffer hashListsBuffer;
    cl::Buffer workBuffer1;
    cl::Buffer workBuffer2;
    cl::Buffer sumChangesBuf;
    cl::Buffer findIndicesBuf1;
    cl::Buffer findIndicesBuf2;
    cl::Buffer findIndicesNumBuf1;
    cl::Buffer findIndicesNumBuf2;
    cl::Buffer subsumsTblBuf;
    
    guint bigHashGroupBits;
    bool hashGroupping;
    guint hashOpenCLWorksNum;
    
    guint bigHashListIndices[17];
public:
    void init(HashController* controller, cl::Platform& clPlatform,
                      cl::Device& clDevice, bool hashGroupping);
    void run();
    
    void runOnNormalHash();
    void runOnBigHash();
    
    gsize getMemQueueSize() const;
};
#endif

#endif
