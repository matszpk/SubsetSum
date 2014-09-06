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

#ifndef __SUBSETSUM_CONTROLLERS_H__
#define __SUBSETSUM_CONTROLLERS_H__

#include <string>
#include <vector>
#include <exception>
#include <glibmm.h>
#include "Int128.h"
#include "Queue.h"
#include "Util.h"

using namespace Glib;
using namespace MyComp;

struct NodeSubset
{
    Int128 sum;
    Int128 subset;
    NodeSubset() { }
    NodeSubset(const Int128& sum, const Int128& subset)
    {
        this->sum = sum;
        this->subset = subset;
    }
};

class SubsetSumException: public std::exception
{
private:
    Glib::ustring message;
public:
    SubsetSumException();
    explicit SubsetSumException(const Glib::ustring& msg);
    ~SubsetSumException() throw();
    
    const char* what() const throw();
};

class SubsetSumController
{
protected:
    guint totalWorkers;
    guint numbersNum;
    Int128* numbers;
    Int128* sumChanges;
    
    guint64 pushedSolutions;
    guint64 takenSolutions;
    volatile gint finished;
    
    Mutex progressMutex;
    guint64 mNodesCount;
    guint subMegas;
    std::string progressMessage;
    
    guint nodeBits;
    
    SimpleQueue<Int128> solutionQueue;
    
    RefPtr<MemQueueDeviceSupport> memQueue;
    
    void solveSmallProblem();
    
public:
    SubsetSumController(const std::vector<Int128>& problem, guint totalWorkers);
    virtual ~SubsetSumController();
    
    guint getNodeBits() const
    { return nodeBits; }
    
    void putSolution(const Int128& solution);
    
    bool getSolution(Int128& solution);
    
    std::string getProgressString();
    
    void updateProgress(guint64 nodesCount, const Int128& lastSubset);
    
    void finish();
    
    guint64 getMNodesCount() const
    { return mNodesCount; }
    guint getSubMegaCount() const
    { return subMegas; }
    
    guint64 getSolutionsNum() const
    { return pushedSolutions; }
    
    virtual void generateWork() = 0;
    
    const RefPtr<MemQueueDeviceSupport> getMemQueue() const
    { return memQueue; }
    
    guint getNumbersNum() const
    { return numbersNum; }
    
    const Int128* getNumbers() const
    { return numbers; }
    
    virtual void initMemQueue(gsize elemsNum) = 0;
};

class NaiveController: public SubsetSumController
{
private:
    gint64 sumChanges[41];
public:
    NaiveController(const std::vector<Int128>& problem, guint totalWorkers);
    ~NaiveController();
    
    void generateWork();
    
    const gint64* getSumChanges() const
    { return sumChanges; }
    
    void initMemQueue(gsize elemsNum);
};

struct NodeHashEntry
{
    guint16 poslo;
    guint16 poshi;
    guint16 size;
    
    void setPos(guint pos)
    {
        poslo = pos;
        poshi = pos>>16;
    }
    
    guint getPos() const
    {
        return (guint(poshi)<<16)|(poslo);
    }
};

struct MinMaxStep
{
    Int128 minValue;
    Int128 maxValue;
};

class HashController: public SubsetSumController
{
private:
    guint nodeHashBits;
    guint nodeHashedNumbers;
    bool useHashSubsets;
    
    NodeHashEntry* nodeHash;
    gint64* nodeHashLists;
    guint32* nodeHashSubsets;
    
    Int128 mainProblem[112];
    guint mainTransTable[112];
    
    Int128 nodeProblem[64];
    guint nodeTransTable[64];
    
    MinMaxStep hashMinMaxTable[112];
    
    gint64 sumChanges[256];
    gint64* subsumsTbl;
    
    void genSubsumsTbl();
public:
    HashController(const std::vector<Int128>& problem, guint totalWorkers,
                   guint nodeHashBits, guint nodeHashedNumbers, bool useHashSubsets);
    ~HashController();
    
    void generateNodeHash(guint threadsNum);
    
    void generateWork();
    
    bool isUseHashSubsets() const
    { return useHashSubsets; }
    
    guint getNodeHashBits() const
    { return nodeHashBits; }
    
    guint getNodeHashedNumbers() const
    { return nodeHashedNumbers; }
    
    const NodeHashEntry* getNodeHash() const
    { return nodeHash; }
    
    const gint64* getNodeHashLists() const
    { return nodeHashLists; }
    
    const guint32* getNodeHashSubsets() const
    { return nodeHashSubsets; }
    
    const Int128* getNodeProblem() const
    { return nodeProblem; }
    
    const guint* getNodeTransTable() const
    { return nodeTransTable; }
    
    const gint64* getSubsumsTbl() const
    { return subsumsTbl; }
    
    guint32 getNodeHashKey(gint64 sum) const
    {
        const guint32 hmask = (1U<<nodeHashBits)-1U;
        const guint32 temph1 = sum>>nodeHashBits;
        const guint32 temph2 = temph1>>nodeHashBits;
        const guint32 temph3 = temph2>>nodeHashBits;
        const guint32 temph4 = temph3>>nodeHashBits;
        return (sum^temph1^temph2^temph3^temph4)&hmask;
    }
    
    const gint64* getSumChanges() const
    { return sumChanges; }
    
    Int128 translateSubset(const Int128& subset) const;
    
    void initMemQueue(gsize elemsNum);
};

#endif
