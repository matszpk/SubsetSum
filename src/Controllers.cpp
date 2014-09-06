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
#include <cstdio>
#include <string>
#include <inttypes.h>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#include "Controllers.h"
#include "NodeHashGen.h"
#include "PushPop.h"
#include "Queue.h"

using namespace Glib;

SubsetSumException::SubsetSumException()
{
}

SubsetSumException::SubsetSumException(const Glib::ustring& msg)
{
    this->message = msg;
}

SubsetSumException::~SubsetSumException() throw()
{
}

const char* SubsetSumException::what() const throw()
{
    return message.c_str();
}


SubsetSumController::SubsetSumController(const std::vector<Int128>& problem,
         guint totalWorkers)
        : solutionQueue(20)
{
    this->totalWorkers = totalWorkers;
    if (problem.size() == 0)
        throw SubsetSumException("Problem size must be positive");
    if (problem.size() > 128)
        throw SubsetSumException("Problem size must be lower than 129");
    pushedSolutions = 0;
    takenSolutions = 0;
    mNodesCount = 0;
    nodeBits = 0;
    subMegas = 0;
    finished = 0;
    sumChanges = NULL;
    numbersNum = problem.size();
    numbers = new Int128[numbersNum];
    std::copy(problem.begin(), problem.end(), numbers);
}

SubsetSumController::~SubsetSumController()
{
    delete[] numbers;
    if (sumChanges != NULL)
        delete[] sumChanges;
}

void SubsetSumController::putSolution(const Int128& solution)
{
    Mutex::Lock lock(progressMutex);
    pushedSolutions++;
    solutionQueue.push(solution);
}
    
bool SubsetSumController::getSolution(Int128& solution)
{
    if (g_atomic_int_get(&finished)!=0 && takenSolutions == pushedSolutions)
        return false;
    if (!solutionQueue.pop(solution))
        return false;
    takenSolutions++;
    return true;
}

std::string SubsetSumController::getProgressString()
{
    Mutex::Lock lock(progressMutex);
    char buf[128];
    if (mNodesCount != 0)
        snprintf(buf, 128, "Sols: %"PRIu64". %"PRIu64"%06u Nodes. ",
                 pushedSolutions, mNodesCount, subMegas);
    else
        snprintf(buf, 128, "Sols: %"PRIu64". %u Nodes. ",
                 pushedSolutions, subMegas);
    std::string out = buf;
    out += progressMessage;
    return out;
}
    
void SubsetSumController::updateProgress(guint64 nodesCount, const Int128& lastSubset)
{
    Mutex::Lock lock(progressMutex);
    
    guint64 diffM = nodesCount / 1000000;
    guint64 subMegaDiff = nodesCount - diffM*1000000;
    mNodesCount += diffM;
    subMegas += subMegaDiff;
    if (subMegas >= 1000000)
    {
        mNodesCount++;
        subMegas -= 1000000;
    }
    
    progressMessage.clear();
    if (nodeBits != 0)
    {   // only if problem divides into nodes
        guint ctrlBits = numbersNum-nodeBits;
        
        Int128 x(guint64(1ULL));
        for (guint i = 0; i < ctrlBits; i++, x<<=1)
            progressMessage += ((lastSubset&x)!=Int128(guint64(0)))?'1':'0';
    }
}

void SubsetSumController::finish()
{
    g_atomic_int_set(&finished, 1);
    if (solutionQueue.isEmpty())
        solutionQueue.cancel();
}

void SubsetSumController::solveSmallProblem()
{
    Int128 maxSubset = 1;
    maxSubset <<= numbersNum;
    Int128 sum = 0;
    Int128 prevSubset = 0;
    for (Int128 subset = 1; subset != maxSubset; ++subset)
    {
        Int128 bit = 1;
        const Int128 changes = prevSubset^subset;
        for (guint bitNum=0; (bit & changes) && bitNum < 128; bit<<=1, bitNum++)
            if (subset&bit) // subset=1,prevSubset=0
                sum += numbers[bitNum];
            else // subset=0,prevSubset=1
                sum -= numbers[bitNum];
        
        prevSubset = subset;
        if (!sum)
            putSolution(subset);
    }
}

/*
 * NaiveController
 */

NaiveController::NaiveController(const std::vector<Int128>& problem, guint totalWorkers)
        : SubsetSumController(problem, totalWorkers)
{
    if (problem.size() <= 16)
        return;
    nodeBits = 14;
    for (guint i = 0; i < 32; i++)
    {
        gint64 sum = 0;
        guint bit = 1;
        for (guint x = 0; x < 5; x++, bit<<=1)
        {
            if (bit&i)
                sum += gint64(numbers[numbersNum-14+x]);
        }
        sumChanges[i] = sum;
    }
    for (guint i = 0; i < 9; i++)
        sumChanges[32+i] = gint64(numbers[numbersNum-9+i]);
}

NaiveController::~NaiveController()
{
}

void NaiveController::generateWork()
{
    if (numbersNum<=16)
    {   // if problem too small to divide to small pieces
        solveSmallProblem();
        memQueue->close();
        return;
    }
    Int128 maxSubset = 1;
    maxSubset <<= numbersNum-14;
    Int128 sum = 0;
    DirectPush pusher(memQueue);
    Int128 prevSubset = 0;
    for (Int128 subset = 0; subset != maxSubset; ++subset)
    {
        Int128 bit = 1;
        const Int128 changes = prevSubset^subset;
        for (guint bitNum=0; (bit & changes) && bitNum < 128; bit<<=1, bitNum++)
            if (subset&bit) // subset=1,prevSubset=0
                sum += numbers[bitNum];
            else // subset=0,prevSubset=1
                sum -= numbers[bitNum];
        
        prevSubset = subset;
        pusher.push<NodeSubset>(NodeSubset(sum, subset));
    }
    pusher.finish();
    memQueue->close();
}

void NaiveController::initMemQueue(gsize elemsNum)
{
    memQueue = MemQueueDeviceSupport::create(sizeof(NodeSubset), elemsNum*3,
                 totalWorkers*3, totalWorkers*3);
    std::cout << "MemQueue initialized. ElemsNum: " << memQueue->getQueueSize() <<
            ", concurOps: " << totalWorkers*3 << std::endl;
}

/*
 * HashController
 */

struct NumberEntry
{
    Int128 number;
    guint order;
    
    NumberEntry() { }
    
    NumberEntry(const Int128& v)
    {
        number = v;
    }
    
    bool operator==(const NumberEntry& ne) const
    {
        return number == ne.number;
    }
    bool operator<(const NumberEntry& ne) const
    {
        return number < ne.number;
    }
};

static void rangeForRegion(guint numbersNum, const NumberEntry* numbers,
               Int128& minVal, Int128& maxVal)
{
    minVal = 0;
    maxVal = 0;
    for (guint i = 0; i < numbersNum; i++)
    {
        if (numbers[i].number < Int128(0))
            minVal += numbers[i].number;
        else if (numbers[i].number > Int128(0))
            maxVal += numbers[i].number;
    }
    
    if (minVal == Int128(0))
        minVal = numbers[0].number;
    if (maxVal == Int128(0))
        maxVal = numbers[numbersNum-1].number;
}

static guint smallestRangeRegion(guint numbersNum, guint regionSize,
        const NumberEntry* numbers)
{
    Int128 minVal = 0;
    Int128 maxVal = 0;
    for (guint i = 0; i < regionSize; i++)
    {
        if (numbers[i].number < Int128(0))
            minVal += numbers[i].number;
        else if (numbers[i].number > Int128(0))
            maxVal += numbers[i].number;
    }
    
    if (minVal == Int128(0))
        minVal = numbers[0].number;
    if (maxVal == Int128(0))
        maxVal = numbers[regionSize-1].number;
    // moving forward
    Int128 range = maxVal-minVal;
    Int128 bestRange = maxVal-minVal;
    guint bestPos = 0;
    for (guint pos = 1; pos < numbersNum-regionSize; pos++)
    {
        if (numbers[pos-1].number < Int128(0))
            minVal -= numbers[pos-1].number;
        if (minVal >= Int128(0))
            minVal = numbers[pos].number;
        
        if (numbers[pos+regionSize-1].number > Int128(0))
        {
            if (maxVal > Int128(0))
                maxVal += numbers[pos+regionSize-1].number;
        }
        else if (minVal < Int128(0))
            minVal += numbers[pos+regionSize-1].number;
        
        if (maxVal < Int128(0))
            maxVal = numbers[pos+regionSize-1].number;
        else if (numbers[pos-1].number > Int128(0))
            maxVal -= numbers[pos-1].number;
        
        range = maxVal-minVal;
        if (range < bestRange)
        {
            bestPos = pos;
            bestRange = range;
        }
    }
    return bestPos;
}

static void orderNumberRanges(gint startLeftPos, gint startRightPos, guint numbersNum,
          const NumberEntry* numbers, Int128* destNumbers, guint* destOrders,
          MinMaxStep* destMinMax)
{
    gint left = startLeftPos;
    gint right = startRightPos;
    
    Int128 minVal = 0;
    Int128 maxVal = 0;
    rangeForRegion(startRightPos-startLeftPos-1, numbers + startLeftPos+1, minVal, maxVal);
    
    // first range
    destMinMax[0].maxValue = -minVal;
    destMinMax[0].minValue = -maxVal;
    
    for (guint destPos = 0; left >= 0 || right < gint(numbersNum); destPos++)
    {   // save range to dest minmax (that range which must be match for next level)
        destMinMax[destPos].maxValue = -minVal;
        destMinMax[destPos].minValue = -maxVal;
        
        if (left >= 0 && right < gint(numbersNum))
        {
            Int128 newMinVal1 = minVal;
            Int128 newMaxVal1 = maxVal;
            Int128 newMinVal2 = minVal;
            Int128 newMaxVal2 = maxVal;
            if (newMinVal1 > Int128(0))
                newMinVal1 = numbers[left].number;
            else // cumulate negative
                newMinVal1 += numbers[left].number;
            if (newMaxVal1 > Int128(0) && numbers[left].number > Int128(0))
                newMaxVal1 += numbers[left].number;
            
            if (newMaxVal2 < Int128(0))
                newMaxVal2 = numbers[right].number;
            else // cumulate positives
                newMaxVal2 += numbers[right].number;
            if (newMinVal2 < Int128(0) && numbers[right].number < Int128(0))
                newMinVal2 += numbers[right].number;
            
            Int128 range1 = newMaxVal1 - newMinVal1;
            Int128 range2 = newMaxVal2 - newMinVal2;
            
            if (range1 < range2)
            {   // choose left
                destNumbers[destPos] = numbers[left].number;
                destOrders[destPos] = numbers[left].order;
                left--;
                minVal = newMinVal1;
                maxVal = newMaxVal1;
            }
            else // otherwise
            {
                destNumbers[destPos] = numbers[right].number;
                destOrders[destPos] = numbers[right].order;
                right++;
                minVal = newMinVal2;
                maxVal = newMaxVal2;
            }
        }
        else if (left >= 0)
        {
            if (minVal > Int128(0))
                minVal = numbers[left].number;
            else // cumulate negative
                minVal += numbers[left].number;
            if (numbers[left].number > Int128(0) && maxVal > Int128(0))
                maxVal += numbers[left].number;
            destNumbers[destPos] = numbers[left].number;
            destOrders[destPos] = numbers[left].order;
            left--;
        }
        else if (right < gint(numbersNum))
        {
            if (maxVal < Int128(0))
                maxVal = numbers[right].number;
            else // cumulate positives
                maxVal += numbers[right].number;
            if (numbers[right].number < Int128(0) && minVal < Int128(0))
                minVal += numbers[right].number;
            destNumbers[destPos] = numbers[right].number;
            destOrders[destPos] = numbers[right].order;
            right++;
        }
    }
}

HashController::HashController(const std::vector<Int128>& problem, guint totalWorkers,
       guint initNodeHashBits, guint initNodeHashedNumbers, bool useHashSubsets)
        : SubsetSumController(problem, totalWorkers)
{
    nodeHashBits = initNodeHashBits;
    nodeHashedNumbers = initNodeHashedNumbers;
    this->useHashSubsets = useHashSubsets;
    subsumsTbl = NULL;
    nodeHash = NULL;
    nodeHashLists = NULL;
    nodeHashSubsets = NULL;
    
    if (numbersNum <= 16)
        return;
    
    if (nodeHashedNumbers == 0 && nodeHashBits != 0)
    {
        if (nodeHashBits >= 3)
            nodeHashedNumbers = nodeHashBits-2;
        else
            nodeHashedNumbers = 1;
    }
    else if (nodeHashedNumbers != 0 && nodeHashBits == 0)
    {
        if ((nodeHashedNumbers+8) > numbersNum-1)
            nodeHashedNumbers = numbersNum - 10;
        nodeHashBits = nodeHashedNumbers+2;
    }
    else if (nodeHashedNumbers == 0 && nodeHashBits == 0)
    {   // default presets
        nodeHashBits = 20;
        nodeHashedNumbers = 18;
    }
    // again fix nodeHashedNumbers
    if ((nodeHashedNumbers+8) > numbersNum-1)
        nodeHashedNumbers = numbersNum - 10;
    
    nodeBits = 8 + nodeHashedNumbers;
    
    std::cout << "NodeHashBits: " << nodeHashBits << "\n"
            "NodeHashedNumbers: " << nodeHashedNumbers << std::endl;
    
    /*
     * choose best numbers setup
     */
    NumberEntry tempNumbers[128];
    for (guint i = 0; i < numbersNum; i++)
    {
        tempNumbers[i].number = numbers[i];
        tempNumbers[i].order = i;
    }
    std::sort(tempNumbers, tempNumbers + numbersNum);
    // choose smallest region
    const guint nodeBestRegPos = smallestRangeRegion(numbersNum, nodeBits, tempNumbers);
    const guint hashedBestRegPos = smallestRangeRegion(nodeBits,
                       nodeHashedNumbers, tempNumbers+nodeBestRegPos) + nodeBestRegPos;
    // copy to nodeproblem and nodetranstable
    guint j = 0;
    for (guint i = nodeBestRegPos; i < hashedBestRegPos; i++, j++)
    {
        nodeProblem[j] = tempNumbers[i].number;
        nodeTransTable[j] = tempNumbers[i].order;
    }
    for (guint i = hashedBestRegPos + nodeHashedNumbers;
         i < nodeBestRegPos+nodeBits; i++, j++)
    {
        nodeProblem[j] = tempNumbers[i].number;
        nodeTransTable[j] = tempNumbers[i].order;
    }
    // now we copying hashedNumbers
    for (guint i = hashedBestRegPos; i < hashedBestRegPos + nodeHashedNumbers; i++, j++)
    {
        nodeProblem[j] = tempNumbers[i].number;
        nodeTransTable[j] = tempNumbers[i].order;
    }
    // generate sumchanges
    {
        for (guint i = 0; i < 256; i++)
        {
            gint64 sum = 0;
            guint bit = 1;
            for (guint x = 0; x < 8; x++, bit<<=1)
            {
                if (bit&i)
                    sum += gint64(nodeProblem[x]);
            }
            sumChanges[i] = sum;
        }
    }
    // main number problem
    orderNumberRanges(gint(nodeBestRegPos)-1, nodeBestRegPos+nodeBits, numbersNum,
              tempNumbers, mainProblem, mainTransTable, hashMinMaxTable);
    
    if (useHashSubsets)
        genSubsumsTbl();
    /*
     * print info
     */
    /*for (guint i = 0; i < nodeBits; i++)
        std::cout << "N: " << i << " : " << int128tostr(nodeProblem[i]) << std::endl;
    for (guint i = 0; i < numbersNum-nodeBits; i++)
        std::cout << "M: " << i << " : " << int128tostr(mainProblem[i]) << std::endl;*/
}

void HashController::generateNodeHash(guint threadsNum)
{
    if (numbersNum <= 16)
        return; // do nothing
    NodeHashGenController* nhgController = new NodeHashGenController(threadsNum,
             nodeHashedNumbers, nodeHashBits, nodeProblem + 8, useHashSubsets);
    nhgController->doHashGen();
    
    nodeHash = nhgController->getNodeHash();
    nodeHashLists = nhgController->getNodeHashLists();
    nodeHashSubsets = nhgController->getNodeHashSubsets();
    
    delete nhgController;
}

HashController::~HashController()
{
    if (nodeHash != NULL)
        alignedFree(nodeHash);
    if (nodeHashLists != NULL)
        alignedFree(nodeHashLists);
    if (nodeHashSubsets != NULL)
        alignedFree(nodeHashSubsets);
    if (subsumsTbl != NULL)
        alignedFree(subsumsTbl);
}

void HashController::genSubsumsTbl()
{
    if (numbersNum <= 16)
        return; // do nothing
    const guint subsumTblsNum = (nodeHashedNumbers+7)>>3;
    
    subsumsTbl = (gint64*)alignedMalloc(64, 8UL*256*subsumTblsNum);
    /* init subsumTbls */
    for (guint t = 0; t < subsumTblsNum; t++)
    {
        const guint numberPos = t*8;
        const guint numbersNum = std::min(8U, nodeHashedNumbers-t*8U);
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
                    sum -= gint64(nodeProblem[8 + numberPos + bitNum]);
                else // subset=0,prevSubset=1 (negated)
                    sum += gint64(nodeProblem[8 + numberPos + bitNum]);
            
            prevSubset = subset;
            subsumsTbl[256*t + subset] = sum;
        }
    }
}

Int128 HashController::translateSubset(const Int128& subset) const
{
    Int128 out = 0;
    guint i;
    Int128 bit(1);
    for (i = 0; i < numbersNum-nodeBits; i++, bit<<=1)
        if (bit&subset)
            out |= Int128(1)<<gint(mainTransTable[i]);
    for (; i < numbersNum; i++, bit<<=1)
        if (bit&subset)
            out |= Int128(1)<<gint(nodeTransTable[i-numbersNum+nodeBits]);
    return out;
}

void HashController::generateWork()
{
    if (numbersNum<=16)
    {   // if problem too small to divide to small pieces
        solveSmallProblem();
        memQueue->close();
        return;
    }
    Int128 maxSubset = 1;
    const guint mainNumbersNum = numbersNum-nodeBits;
    maxSubset <<= mainNumbersNum;
    Int128 sum(0);
    DirectPush pusher(memQueue);
    
    gint rangeIndex = mainNumbersNum-1;
    
    Int128 subset(0);
    while (true)
    {
        while (rangeIndex >= 0 && sum >= hashMinMaxTable[rangeIndex].minValue &&
            sum <= hashMinMaxTable[rangeIndex].maxValue)
            rangeIndex--;
        
        if (rangeIndex == -1)
        {   // do push node */
            pusher.push<NodeSubset>(NodeSubset(sum, subset));
            rangeIndex++;
        }
        
        while ((subset & (Int128(1)<<rangeIndex)) && rangeIndex < gint(mainNumbersNum))
        {
            subset &= ~(Int128(1)<<rangeIndex);
            sum -= mainProblem[rangeIndex];
            rangeIndex++;
        }
        if (rangeIndex >= gint(mainNumbersNum))
            break; //end!!!!

        subset |= (Int128(1)<<rangeIndex);
        sum += mainProblem[rangeIndex];
    }
    
    pusher.finish();
    memQueue->close();
}

void HashController::initMemQueue(gsize elemsNum)
{
    memQueue = MemQueueDeviceSupport::create(sizeof(NodeSubset), elemsNum*3,
                 totalWorkers*10, totalWorkers*10);
    std::cout << "MemQueue initialized. ElemsNum: " << memQueue->getQueueSize() <<
            ", concurOps: " << totalWorkers*10 << std::endl;
}
