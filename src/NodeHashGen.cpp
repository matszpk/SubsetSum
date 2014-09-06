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
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <glibmm.h>
#include "Controllers.h"
#include "Int128.h"
#include "NodeHashGen.h"
#include "Util.h"

using namespace Glib;

NodeHashGenController::NodeHashGenController(guint threadsNum,
             guint nodeHashedNumbers, guint nodeHashBits, const Int128* nodeProblem,
             bool useHashSubsets)
{
    this->useHashSubsets = useHashSubsets;
    tmpHashLists = NULL;
    subsumTbls = NULL;
    nodeHashListsBlocks = NULL;
    nodeHashSubsets = NULL;
    nodeHashLists = NULL;
    this->threadsNum = threadsNum;
    this->nodeHashBits = nodeHashBits;
    this->nodeHashSubsets = nodeHashSubsets;
    this->nodeHashedNumbers = nodeHashedNumbers;
    for (guint i = 0; i < nodeHashedNumbers; i++)
        this->nodeProblem[i] = nodeProblem[i];
    
    guint threadBits = 0;
    for (guint v = 1; v < threadsNum; v<<=1, threadBits++);
    
    putSubsetPartsNum = 2;
    
    subsetPartBits = std::min(nodeHashedNumbers, threadBits+4);
    hashPartBits = std::min(nodeHashBits, threadBits+4);
}

NodeHashGenController::~NodeHashGenController()
{
    if (tmpHashLists != NULL)
        delete[] tmpHashLists;
    if (subsumTbls != NULL)
        delete[] subsumTbls;
    if (nodeHashListsBlocks != NULL)
        delete[] nodeHashListsBlocks;
}

void NodeHashGenController::genSubsumTbls()
{
    const gsize subsumTblsNum = (nodeHashedNumbers+8)/9;
    
    subsumTbls = new gint64[subsumTblsNum*512];
    
    /* init subsumTbls */
    for (guint t = 0; t < subsumTblsNum; t++)
    {
        const guint numberPos = t*9;
        const guint numbersNum = std::min(9U, nodeHashedNumbers-t*9U);
        const guint subsumSize = 1U<<numbersNum;
        
        guint prevSubset = 0;
        gint64 sum = 0;
        for (guint subset = 0; subset < subsumSize; subset++)
        {
            guint bit = 1;
            const guint changes = prevSubset^subset;
            for (guint bitNum=0; (bit & changes)!=0 && bitNum < numbersNum;
                         bit<<=1, bitNum++)
                if ((subset&bit)!=0) // subset=1,prevSubset=0 (negated)
                    sum -= nodeProblem[numberPos + bitNum];
                else // subset=0,prevSubset=1 (negated)
                    sum += nodeProblem[numberPos + bitNum];
            
            prevSubset = subset;
            subsumTbls[512*t + subset] = sum;
        }
    }
}

void NodeHashGenController::singleThreadDoHashGen()
{
    guint listPos = 0;
    const guint hashedSubsets = 1U<<nodeHashedNumbers;
    const gsize nodeHashSize = 1UL<<nodeHashBits;
    
    nodeHash = (NodeHashEntry*)alignedMalloc(64, sizeof(NodeHashEntry)*nodeHashSize);
    
    memset(nodeHash, 0, sizeof(NodeHashEntry)*nodeHashSize);
    
    tmpHashLists = new TempNodeHashListEntry[hashedSubsets];
    
    if (nodeHashedNumbers <= 9)
    {
        for (guint subset = 0; subset < hashedSubsets; subset++)
        {
            const gint64 sum = subsumTbls[subset];
            guint32 hkey = getNodeHashKey(sum);
            if (nodeHash[hkey].size == 0)
            {
                nodeHash[hkey].setPos(listPos);
                nodeHash[hkey].size = 1;
                tmpHashLists[listPos].next = G_MAXUINT32;
                tmpHashLists[listPos++].subset = subset;
            }
            else
            {
                guint32 next = nodeHash[hkey].getPos();
                if (nodeHash[hkey].size == G_MAXUINT16)
                    throw SubsetSumException("Some nodehashentry have too long size");
                nodeHash[hkey].size++;
                nodeHash[hkey].setPos(listPos);
                tmpHashLists[listPos].next = next;
                tmpHashLists[listPos++].subset = subset;
            }
        }
    }
    else if (nodeHashedNumbers <= 18)
    {
        for (guint subset = 0; subset < hashedSubsets; subset++)
        {
            const gint64 sum = subsumTbls[subset&0x1ff] + subsumTbls[512 + (subset>>9)];
            guint32 hkey = getNodeHashKey(sum);
            if (nodeHash[hkey].size == 0)
            {
                nodeHash[hkey].setPos(listPos);
                nodeHash[hkey].size = 1;
                tmpHashLists[listPos].next = G_MAXUINT32;
                tmpHashLists[listPos++].subset = subset;
            }
            else
            {
                guint32 next = nodeHash[hkey].getPos();
                if (nodeHash[hkey].size == G_MAXUINT16)
                    throw SubsetSumException("Some nodehashentry have too long size");
                nodeHash[hkey].size++;
                nodeHash[hkey].setPos(listPos);
                tmpHashLists[listPos].next = next;
                tmpHashLists[listPos++].subset = subset;
            }
        }
    }
    else if (nodeHashedNumbers <= 27)
    {
        for (guint subset = 0; subset < hashedSubsets; subset++)
        {
            const gint64 sum = subsumTbls[subset&0x1ff] +
                subsumTbls[512 + ((subset>>9)&0x1ff)] +
                subsumTbls[1024 + (subset>>18)];
            guint32 hkey = getNodeHashKey(sum);
            if (nodeHash[hkey].size == 0)
            {
                nodeHash[hkey].setPos(listPos);
                nodeHash[hkey].size = 1;
                tmpHashLists[listPos].next = G_MAXUINT32;
                tmpHashLists[listPos++].subset = subset;
            }
            else
            {
                guint32 next = nodeHash[hkey].getPos();
                if (nodeHash[hkey].size == G_MAXUINT16)
                    throw SubsetSumException("Some nodehashentry have too long size");
                nodeHash[hkey].size++;
                nodeHash[hkey].setPos(listPos);
                tmpHashLists[listPos].next = next;
                tmpHashLists[listPos++].subset = subset;
            }
        }
    }
    else
    {
        for (guint subset = 0; subset < hashedSubsets; subset++)
        {
            gint64 sum = subsumTbls[subset&0x1ff] +
                subsumTbls[512 + ((subset>>9)&0x1ff)] +
                subsumTbls[1024 + ((subset>>18)&0x1ff)] + subsumTbls[1536 + (subset>>27)];
            guint32 hkey = getNodeHashKey(sum);
            if (nodeHash[hkey].size == 0)
            {
                nodeHash[hkey].setPos(listPos);
                nodeHash[hkey].size = 1;
                tmpHashLists[listPos].next = G_MAXUINT32;
                tmpHashLists[listPos++].subset = subset;
            }
            else
            {
                guint32 next = nodeHash[hkey].getPos();
                if (nodeHash[hkey].size == G_MAXUINT16)
                    throw SubsetSumException("Some nodehashentry have too long size");
                nodeHash[hkey].size++;
                nodeHash[hkey].setPos(listPos);
                tmpHashLists[listPos].next = next;
                tmpHashLists[listPos++].subset = subset;
            }
        }
    }
    
    // ordering in real hashlists
    nodeHashSubsets = (guint32*)alignedMalloc(64, 4UL*hashedSubsets);
    
    listPos = 0;
    for (guint i = 0; i < nodeHashSize; i++)
    {
        if (nodeHash[i].size == 0)
            continue;
        guint current = nodeHash[i].getPos();
        for (gint k = nodeHash[i].size-1; k >= 0; k--)
        {
            nodeHashSubsets[listPos+k] = tmpHashLists[current].subset;
            current = tmpHashLists[current].next;
        }
        nodeHash[i].setPos(listPos);
        listPos += nodeHash[i].size;
    }
    delete[] tmpHashLists;
    tmpHashLists = NULL;
    
    if (useHashSubsets)
        return; // only use nodeHashSubsets (is finish in this case)
    
    // filling up values
    nodeHashLists = (gint64*)alignedMalloc(64, 8UL*hashedSubsets);
    if (nodeHashedNumbers <= 9)
    {
        for (guint i = 0; i < hashedSubsets; i++)
        {
            gint64 value = 0;
            const guint subset = nodeHashSubsets[i];
            value = subsumTbls[subset];
            
            nodeHashLists[i] = value;
        }
    }
    else if (nodeHashedNumbers <= 18)
    {
        for (guint i = 0; i < hashedSubsets; i++)
        {
            gint64 value = 0;
            const guint subset = nodeHashSubsets[i];
            value = subsumTbls[subset&0x1ff] + subsumTbls[512 + (subset>>9)];
            
            nodeHashLists[i] = value;
        }
    }
    else if (nodeHashedNumbers <= 27)
    {
        for (guint i = 0; i < hashedSubsets; i++)
        {
            gint64 value = 0;
            const guint subset = nodeHashSubsets[i];
            value = subsumTbls[subset&0x1ff] + subsumTbls[512 + ((subset>>9)&0x1ff)] +
                subsumTbls[1024 + (subset>>18)];
            
            nodeHashLists[i] = value;
        }
    }
    else
    {
        for (guint i = 0; i < hashedSubsets; i++)
        {
            gint64 value = 0;
            const guint subset = nodeHashSubsets[i];
            value = subsumTbls[subset&0x1ff] + subsumTbls[512 + ((subset>>9)&0x1ff)] +
                subsumTbls[1024 + ((subset>>18)&0x1ff)] + subsumTbls[1536 + (subset>>27)];
            
            nodeHashLists[i] = value;
        }
    }
}

static const guint putSubsetBlockSize = 4096;

void NodeHashGenController::doHashGen()
{
    genSubsumTbls();
    if (threadsNum == 1 || nodeHashedNumbers < 20)
    {
        singleThreadDoHashGen();
        return;
    }
    
    std::cout << "Using parallel NodeHashGen: " << threadsNum << " threads" << std::endl;
    
    phase = NHG_PHASE_PUTSUBSET;
    
    const guint hashedSubsets = 1U<<nodeHashedNumbers;
    const gsize nodeHashSize = 1UL<<nodeHashBits;
    
    // multithreading
    tmpHashLists = new TempNodeHashListEntry[hashedSubsets + 2*putSubsetBlockSize];
    nodeHash = (NodeHashEntry*)alignedMalloc(64, sizeof(NodeHashEntry)*nodeHashSize);
    
    memset(nodeHash, 0, sizeof(NodeHashEntry)*nodeHashSize);
    
    putSubsetPartCount = 0;
    orderingPartCount = 0;
    fillValuePartCount = 0;
    putSubsetWorkers = threadsNum;
    orderingWorkers = threadsNum;
    fillValueWorkers = threadsNum;
    
    putSubsetBlockCount = 0;
    
    nodeHashListsBlocks = new NodeHashListsBlockInfo[1UL<<hashPartBits];
    
    Thread** workerThreads = new Thread*[threadsNum];
    for (guint i = 0; i < threadsNum; i++)
        workerThreads[i] = Thread::create(
            sigc::mem_fun(this, &NodeHashGenController::worker), true);
    
    {
        Mutex::Lock lock(mutex);
        while (putSubsetWorkers != 0)
            cond.wait(mutex);
        
        guint listPos = 0;
        for (guint i = 0; i < (1U<<hashPartBits); i++)
        {
            nodeHashListsBlocks[i].pos = listPos;
            listPos += nodeHashListsBlocks[i].size;
        }
        
        // ordering in real hashlists
        nodeHashSubsets = (guint32*)alignedMalloc(64, 4UL*hashedSubsets);
    
        phase = NHG_PHASE_ORDERING;
        cond.broadcast();
        
        // wait for finish ordering
        while (orderingWorkers != 0)
            cond.wait(mutex);
        
        delete[] tmpHashLists;
        tmpHashLists = NULL;
        
        if (!useHashSubsets)
        {   // if not useHashSubsets
            nodeHashLists = (gint64*)alignedMalloc(64, 8UL*hashedSubsets);

            phase = NHG_PHASE_FILLVALUE;
            cond.broadcast();
        }
    }
    
    // end
    for (guint i = 0; i < threadsNum; i++)
        workerThreads[i]->join();
    delete[] workerThreads;
}

void NodeHashGenController::worker()
{
    const guint subsetPartsNum = (1U<<subsetPartBits);
    const guint hashPartsNum = (1U<<hashPartBits);
    
    const guint hashedSubsets = 1U<<nodeHashedNumbers;
    
    // put subset phase
    guint putSubsetPart = g_atomic_int_add(&putSubsetPartCount, 1);
    if (putSubsetPart < putSubsetPartsNum)
    {
        guint* blockSizes = (guint*)alignedMalloc(64, sizeof(guint)*(hashPartsNum>>1));
        
        for (guint i = 0; i < guint(hashPartsNum>>1); i++)
            blockSizes[i] = 0;
        const guint hkeyBlockIdShift = nodeHashBits-hashPartBits;
        const guint32 hkeyBlockMask = (1U<<(hashPartBits-1))-1U;
        
        guint lp = g_atomic_int_add(&putSubsetBlockCount, putSubsetBlockSize);
        guint lpx = 0;
        if (nodeHashedNumbers <= 18)
            throw SubsetSumException("nodeHashedNumber must be >=18");
        if (nodeHashedNumbers <= 27)
        {
            for (guint subset = 0; subset < hashedSubsets; subset++)
            {
                const gint64 sum = subsumTbls[subset&0x1ff] +
                    subsumTbls[512 + ((subset>>9)&0x1ff)] +
                    subsumTbls[1024 + (subset>>18)];
                guint32 hkey = getNodeHashKey(sum);
                
                if ((hkey >> (nodeHashBits-1)) != putSubsetPart)
                    continue;
                
                if (lpx == putSubsetBlockSize)
                {
                    lp = g_atomic_int_add(&putSubsetBlockCount, putSubsetBlockSize);
                    lpx = 0;
                }
                
                blockSizes[(hkey>>hkeyBlockIdShift)&hkeyBlockMask]++;
                
                if (nodeHash[hkey].size == 0)
                {
                    nodeHash[hkey].setPos(lp+lpx);
                    nodeHash[hkey].size = 1;
                    tmpHashLists[lp+lpx].next = G_MAXUINT32;
                    tmpHashLists[lp+lpx].subset = subset;
                }
                else
                {
                    guint32 next = nodeHash[hkey].getPos();
                    if (nodeHash[hkey].size == G_MAXUINT16)
                        throw SubsetSumException("Some nodehashentry have too long size");
                    nodeHash[hkey].size++;
                    nodeHash[hkey].setPos(lp+lpx);
                    tmpHashLists[lp+lpx].next = next;
                    tmpHashLists[lp+lpx].subset = subset;
                }
                lpx++;
            }
        }
        else
        {
            for (guint subset = 0; subset < hashedSubsets; subset++)
            {
                const gint64 sum = subsumTbls[subset&0x1ff] +
                    subsumTbls[512 + ((subset>>9)&0x1ff)] +
                    subsumTbls[1024 + ((subset>>18)&0x1ff)] +
                    subsumTbls[1536 + (subset>>27)];
                guint32 hkey = getNodeHashKey(sum);
                
                if ((hkey >> (nodeHashBits-1)) != putSubsetPart)
                    continue;
                
                if (lpx == putSubsetBlockSize)
                {
                    lp = g_atomic_int_add(&putSubsetBlockCount, putSubsetBlockSize);
                    lpx = 0;
                }
                
                blockSizes[(hkey>>hkeyBlockIdShift)&hkeyBlockMask]++;
                
                if (nodeHash[hkey].size == 0)
                {
                    nodeHash[hkey].setPos(lp+lpx);
                    nodeHash[hkey].size = 1;
                    tmpHashLists[lp+lpx].next = G_MAXUINT32;
                    tmpHashLists[lp+lpx].subset = subset;
                }
                else
                {
                    guint32 next = nodeHash[hkey].getPos();
                    if (nodeHash[hkey].size == G_MAXUINT16)
                        throw SubsetSumException("Some nodehashentry have too long size");
                    nodeHash[hkey].size++;
                    nodeHash[hkey].setPos(lp+lpx);
                    tmpHashLists[lp+lpx].next = next;
                    tmpHashLists[lp+lpx].subset = subset;
                }
                lpx++;
            }
        }
        
        const guint shift = putSubsetPart*(1U<<(hashPartBits-1));
        for (guint i = 0; i < guint(hashPartsNum>>1); i++)
            nodeHashListsBlocks[shift + i].size = blockSizes[i];
        alignedFree(blockSizes);
    }
    
    {
        Mutex::Lock lock(mutex);
        if (g_atomic_int_dec_and_test(&putSubsetWorkers))
            // all workers out of PutSubset phase
            cond.broadcast();
        while (phase != NHG_PHASE_ORDERING)
            cond.wait(mutex);
    }
    
    // ordering phase
    guint orderingPart = g_atomic_int_add(&orderingPartCount, 1);
    while (orderingPart < hashPartsNum)
    {
        const guint hashStart = orderingPart<<(nodeHashBits-hashPartBits);
        const guint hashEnd = (orderingPart+1)<<(nodeHashBits-hashPartBits);
        guint listPos = nodeHashListsBlocks[orderingPart].pos;
        for (guint i = hashStart; i < hashEnd; i++)
        {
            if (nodeHash[i].size == 0)
                continue;
            guint current = nodeHash[i].getPos();
            for (gint k = nodeHash[i].size-1; k >= 0; k--)
            {
                nodeHashSubsets[listPos+k] = tmpHashLists[current].subset;
                current = tmpHashLists[current].next;
            }
            nodeHash[i].setPos(listPos);
            listPos += nodeHash[i].size;
        }
        
        orderingPart = g_atomic_int_add(&orderingPartCount, 1);
    }
    
    {
        Mutex::Lock lock(mutex);
        if (g_atomic_int_dec_and_test(&orderingWorkers))
            // all workers out of Ordering phase
            cond.broadcast();
        if (useHashSubsets)
            return; // end of worker if useHashSubsets
        
        while (phase != NHG_PHASE_FILLVALUE)
            cond.wait(mutex);
    }
    
    // fill value phase
    guint fillValuePart = g_atomic_int_add(&fillValuePartCount, 1);
    while (fillValuePart < subsetPartsNum)
    {
        const guint partHashSubsetStart = fillValuePart<<
                (nodeHashedNumbers-subsetPartBits);
        const guint partHashSubsetsEnd = (fillValuePart+1U)<<
                (nodeHashedNumbers-subsetPartBits);
        
        if (nodeHashedNumbers <= 18)
            throw SubsetSumException("nodeHashedNumber must be >=18");
        if (nodeHashedNumbers <= 27)
        {
            for (guint i = partHashSubsetStart; i < partHashSubsetsEnd; i++)
            {
                gint64 value = 0;
                const guint subset = nodeHashSubsets[i];
                value = subsumTbls[subset&0x1ff] + subsumTbls[512 + ((subset>>9)&0x1ff)] +
                    subsumTbls[1024 + (subset>>18)];
                
                nodeHashLists[i] = value;
            }
        }
        else
        {
            for (guint i = partHashSubsetStart; i < partHashSubsetsEnd; i++)
            {
                gint64 value = 0;
                const guint subset = nodeHashSubsets[i];
                value = subsumTbls[subset&0x1ff] + subsumTbls[512 + ((subset>>9)&0x1ff)] +
                    subsumTbls[1024 + ((subset>>18)&0x1ff)] +
                    subsumTbls[1536 + (subset>>27)];
                
                nodeHashLists[i] = value;
            }
        }
        
        fillValuePart = g_atomic_int_add(&fillValuePartCount, 1);
    }
}
