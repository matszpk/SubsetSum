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

#ifndef __SUBSETSUM_NODEHASHGEN_H__
#define __SUBSETSUM_NODEHASHGEN_H__

#include <string>
#include <vector>
#include <glibmm.h>
#include "Controllers.h"
#include "Int128.h"

using namespace Glib;

struct TempNodeHashListEntry
{
    guint subset;
    guint next;
};

enum NodeHashGenPhase
{
    NHG_PHASE_PUTSUBSET = 0,
    NHG_PHASE_ORDERING = 1,
    NHG_PHASE_FILLVALUE = 2
};

struct NodeHashListsBlockInfo
{
    guint pos;
    guint size;
};

class NodeHashGenController
{
private:
    bool useHashSubsets;
    guint threadsNum;
    guint nodeHashedNumbers;
    guint nodeHashBits;
    gint64 nodeProblem[64];
    
    NodeHashEntry* nodeHash;
    gint64* nodeHashLists;
    guint32* nodeHashSubsets;
    
    TempNodeHashListEntry* tmpHashLists;
    gint64* subsumTbls;
    
    guint putSubsetPartsNum;
    guint subsetPartBits;
    guint hashPartBits;
    
    Mutex mutex;
    Cond cond;
    
    volatile gint putSubsetWorkers;
    volatile gint orderingWorkers;
    volatile gint fillValueWorkers;
    
    volatile gint putSubsetPartCount;
    volatile gint orderingPartCount;
    volatile gint fillValuePartCount;
    
    volatile guint putSubsetBlockCount;
    
    NodeHashGenPhase phase;
    
    NodeHashListsBlockInfo* nodeHashListsBlocks;
public:
    NodeHashGenController(guint threadsNum, guint nodeHashedNumbers, guint nodeHashBits,
            const Int128* nodeProblem, bool useHashSubsets);
    ~NodeHashGenController();
    
    void genSubsumTbls();
    
    void singleThreadDoHashGen();
    
    void doHashGen();
    
    NodeHashEntry* getNodeHash()
    { return nodeHash; }
    
    gint64* getNodeHashLists()
    { return nodeHashLists; }
    
    guint32* getNodeHashSubsets()
    { return nodeHashSubsets; }
    
    guint32 getNodeHashKey(gint64 sum) const
    {
        const guint32 hmask = (1U<<nodeHashBits)-1U;
        const guint32 temph1 = sum>>nodeHashBits;
        const guint32 temph2 = temph1>>nodeHashBits;
        const guint32 temph3 = temph2>>nodeHashBits;
        const guint32 temph4 = temph3>>nodeHashBits;
        return (sum^temph1^temph2^temph3^temph4)&hmask;
    }
    
    void worker();
};

#endif
