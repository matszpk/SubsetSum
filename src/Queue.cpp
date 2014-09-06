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
// MSVC needs standard header for new which throw exception
#include <new>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif
#include "Queue.h"
#include "Util.h"

using namespace Glib;
using namespace MyComp;

AbstractMemQueue::AbstractMemQueue()
{
    closedQueue = 0;
    cancelledQueue = 0;
    pausedQueue = 0;
    noDataQueue = 0;
}

AbstractMemQueue::~AbstractMemQueue()
{
}

static const gsize defaultAlignment = 256;

RefPtr<MemQueue> MemQueue::create(gsize elemSize, gsize elemsNum)
{
   RefPtr<MemQueue> memQueue = RefPtr<MemQueue>(
           new MemQueue(elemSize, elemsNum));
   if (!memQueue)
       throw MyComp::Exception("Cant create MemQueue");
   return memQueue;
}

RefPtr<MemQueue> MemQueue::create(gsize elemSize, gsize elemsNum,
         guint pushConcurTasks, guint popConcurTasks)
{
    RefPtr<MemQueue> memQueue = RefPtr<MemQueue>(
           new MemQueue(elemSize, elemsNum, pushConcurTasks, popConcurTasks));
   if (!memQueue)
       throw MyComp::Exception("Cant create MemQueue");
   return memQueue;
}

MemQueue::MemQueue(gsize elemSize, gsize elemsNum)
        : pushFoldMutex(mutex), popFoldMutex(mutex)
{
    if (elemSize == 0)
        throw MyComp::Exception("MemQueue: ElemsNum must be non-zero");
    this->elemSize = elemSize;
    this->queueSize = elemsNum;
    readPos = writePos = 0;
    const guint numCpus = sysconf(_SC_NPROCESSORS_ONLN);
    pushConcurAllocated = numCpus*3;
    popConcurAllocated = numCpus*3;
    pushConcurStart = pushConcurUsed = 0;
    popConcurStart = popConcurUsed = 0;
    newReadPos = newWritePos = 0;
    concurPushes = 0;
    closingQueue = false;
    pushConcurEntries = new ConcurEntry[pushConcurAllocated];
    try
    { popConcurEntries = new ConcurEntry[popConcurAllocated]; }
    catch(...)
    { 
        delete[] pushConcurEntries;
        throw;
    }
    
    if (elemsNum > 0)
    {
        queueData = (char*)alignedMalloc(defaultAlignment, elemSize*elemsNum);
        if (queueData == NULL)
        {
            delete[] pushConcurEntries;
            delete[] popConcurEntries;
            throw MyComp::Exception("Out of memory at creating MemQueue");
        }
    }
    else
        queueData = NULL;
}

MemQueue::MemQueue(gsize elemSize, gsize elemsNum, guint pushConcurTasks,
                   guint popConcurTasks)
            : pushFoldMutex(mutex), popFoldMutex(mutex)
{
    if (elemSize == 0)
        throw MyComp::Exception("MemQueue: ElemSize must be non-zero");
    this->elemSize = elemSize;
    this->queueSize = elemsNum;
    readPos = writePos = 0;
    const guint numCpus = sysconf(_SC_NPROCESSORS_ONLN);
    pushConcurAllocated = (pushConcurTasks!=0)?pushConcurTasks:numCpus*3;
    popConcurAllocated = (popConcurTasks!=0)?popConcurTasks:numCpus*3;
    pushConcurStart = pushConcurUsed = 0;
    popConcurStart = popConcurUsed = 0;
    newReadPos = newWritePos = 0;
    concurPushes = 0;
    closingQueue = false;
    pushConcurEntries = new ConcurEntry[pushConcurAllocated];
    try
    { popConcurEntries = new ConcurEntry[popConcurAllocated]; }
    catch(...)
    { 
        delete[] pushConcurEntries;
        throw;
    }
    
    if (elemsNum > 0)
    {
        queueData = (char*)alignedMalloc(defaultAlignment, elemSize*elemsNum);
        if (queueData == NULL)
        {
            delete[] pushConcurEntries;
            delete[] popConcurEntries;
            throw MyComp::Exception("Out of memory at creating MemQueue");
        }
    }
    else
        queueData = NULL;
}

MemQueue::~MemQueue()
{
    if (queueData != NULL)
        alignedFree(queueData);
    delete[] pushConcurEntries;
    delete[] popConcurEntries;
}

gsize MemQueue::getUsedSize() const
{
    return writePos-readPos;
}

gsize MemQueue::getRecommendedPushingSize() const
{
    const gsize baseSize = queueSize/(pushConcurAllocated+popConcurAllocated);
    if (baseSize >= 256)
        return (baseSize&~gsize(15));
    else if (baseSize >= 64)
        return (baseSize&~gsize(3));
    else
        return baseSize;
}

gsize MemQueue::getRecommendedPoppingSize() const
{
    return getRecommendedPushingSize();
}

void MemQueue::copyToQueue(gsize pos, gsize elemsNum, const void* elems)
{
    const gsize wPos = pos<queueSize?pos:pos-queueSize;
    if (wPos+elemsNum > queueSize)
    {
        memcpy(queueData+wPos*elemSize, elems, elemSize*(queueSize-wPos));
        memcpy(queueData, (const char*)elems + elemSize*(queueSize-wPos),
            (elemsNum-(queueSize-wPos))*elemSize);
    }
    else
        memcpy(queueData+wPos*elemSize, elems, elemSize*elemsNum);
}

void MemQueue::copyFromQueue(gsize pos, gsize elemsNum, void* elems)
{
    if (pos+elemsNum > queueSize)
    {
        memcpy(elems, queueData+pos*elemSize, elemSize*(queueSize-pos));
        memcpy((char*)elems + elemSize*(queueSize-pos), queueData,
                elemSize*(elemsNum-(queueSize-pos)));
    }
    else
        memcpy(elems, queueData+pos*elemSize, elemSize*elemsNum);
}

/***
 * direct pushing and poping
 ***/

bool MemQueue::finishPushingInt(gsize oldElemsNum, guint pushId, Mutex::Lock& lock)
{
    bool success = true;
    if (pushId != CONCURID_NONE)
    {
        gsize oldWritePos = writePos;
        gsize oldNewWritePos = newWritePos;
        bool fixed = false;
        success = finishPushConcur(pushId, oldElemsNum, lock, fixed);
        
        if (fixed)
        {
            oldWritePos -= queueSize;
            oldNewWritePos -= queueSize;
        }
        
        if (writePos!=oldWritePos && oldWritePos==newReadPos)
            popCond.broadcast();
        
        if (pausedQueue!=0 && readPos == writePos &&
            pushConcurUsed == 0 && popConcurUsed == 0)
            /* set is flushed and stopped (when no pop operations) and
             * when push sends zero elements */
            pausedQueue = 2;
        concurPushes--;
        if (closingQueue!=0 && concurPushes == 0)
            closeCond.broadcast();
    }
    return success;
}

guint MemQueue::allocPushConcur(gsize elemsNum, gsize& pushPos)
{
    guint pushId = pushConcurStart+pushConcurUsed;
    pushConcurUsed++;
    if (pushId >= pushConcurAllocated)
        pushId -= pushConcurAllocated;
    
    ConcurEntry& entry = pushConcurEntries[pushId];
    entry.startPos = newWritePos;
    entry.endPos = newWritePos+elemsNum;
    entry.processed = true;
    pushPos = newWritePos;
    if (pushPos >= queueSize)
        pushPos -= queueSize;
    newWritePos += elemsNum;
    return pushId;
}

bool MemQueue::finishPushConcur(guint pushId, gsize elemsNum, Mutex::Lock& lock, bool& fixed)
{
    bool success = true;
    fixed = false;
    
    if (pushConcurUsed == 0)
        return true; // do nothing
    
    if (pushId >= pushConcurAllocated)
        pushId -= pushConcurAllocated;
    ConcurEntry& entry = pushConcurEntries[pushId];
    if (entry.endPos-entry.startPos < elemsNum)
    {
        success = false; // exception happened
        entry.endPos = entry.startPos; // no data push
    }
    else // is good
        entry.endPos = entry.startPos+elemsNum;
    
    entry.processed = false;
    
    const gsize oldPushConcurUsed = pushConcurUsed;
    
    guint tmpPushConcurUsed = pushConcurUsed;
    guint tmpPushConcurStart = pushConcurStart;
    gsize tmpWritePos = writePos;
    gsize oldWritePos = writePos;
    
    // try fold pushes
    while (tmpPushConcurUsed != 0 && !pushConcurEntries[tmpPushConcurStart].processed)
    {
        ConcurEntry& toFold = pushConcurEntries[tmpPushConcurStart];
        gsize wPos = tmpWritePos<queueSize?tmpWritePos:tmpWritePos-queueSize;
        gsize sPos = toFold.startPos<queueSize?toFold.startPos:toFold.startPos-queueSize;
        const gsize blockSize = toFold.endPos-toFold.startPos;
        if (wPos != sPos && blockSize != 0)
        {   // move concur block to proper place
            if (lock.locked()) // release if needed
                lock.release();
            
            gsize b1 = blockSize;
            gsize b2 = blockSize;
            if (sPos+blockSize > queueSize)
                b1 = queueSize-sPos;
            if (wPos+blockSize > queueSize)
            {
                if (b1 < blockSize)
                    b2 = queueSize-wPos;
                else
                    b1 = queueSize-wPos;
            }
            
            memmove(queueData + wPos*elemSize, queueData + sPos*elemSize, elemSize*b1);
            
            if (b1 < blockSize)
            {
                wPos += b1;
                if (wPos >= queueSize)
                    wPos -= queueSize;
                sPos += b1;
                if (sPos >= queueSize)
                    sPos -= queueSize;
                
                memmove(queueData + wPos*elemSize, queueData + sPos*elemSize,
                        elemSize*(b2-b1));
                if (b2 < blockSize)
                {
                    wPos += b2-b1;
                    if (wPos >= queueSize)
                        wPos -= queueSize;
                    sPos += b2-b1;
                    if (sPos >= queueSize)
                        sPos -= queueSize;
                    memmove(queueData + wPos*elemSize, queueData + sPos*elemSize,
                            elemSize*(blockSize-b2));
                }
            }
        }
        
        tmpWritePos += blockSize;
        
        tmpPushConcurUsed--;
        tmpPushConcurStart++;
        if (tmpPushConcurStart >= pushConcurAllocated) // fix pushid
            tmpPushConcurStart -= pushConcurAllocated;
    }
    
    if (!lock.locked())
        lock.acquire(); // again lock
    // update variables
    if (oldWritePos == writePos + queueSize) // if fixed by popper
    {
        writePos = tmpWritePos-queueSize;  // we fix it again
        fixed = true;
    }
    else
        writePos = tmpWritePos;
    
    pushConcurStart = tmpPushConcurStart;
    pushConcurUsed = tmpPushConcurUsed;
    
    if ((pushConcurUsed < oldPushConcurUsed && oldPushConcurUsed == pushConcurAllocated) ||
        (pushConcurUsed == 0 && writePos < newWritePos))
        // also unlock when newWritePos went to back
        pushCond.broadcast(); // unlock pushConcurEntries for other threads
    
    if (pushConcurUsed == 0)  // reset concur start
    {
        pushConcurStart = 0;
        newWritePos = writePos;
    }
    return success;
}

gsize MemQueue::doPushing(gsize elemsNum, gsize oldElemsNum,
        guint& pushId, gsize& queuePos, guint64 timeoutMicro)
{
    TimeVal deadline;
    if (timeoutMicro != 0ULL)
        deadline = getAfterCurrentTime(timeoutMicro);
    
    Mutex::Lock lock(mutex);
    CondMutexLock pushFoldLock(pushFoldMutex);
    
    if (!finishPushingInt(oldElemsNum, pushId, lock))
    {
        pushId = CONCURID_NONE; // reset pushId for obsolete finish
        throw MyComp::Exception("Given elemsNum is greater than "
                "returned by doPushing method!");
    }
    pushId = CONCURID_NONE; // reset pushId for obsolete finish
    /* begin next */
    if (closingQueue || closedQueue!=0 || cancelledQueue != 0)
        return 0;
    
    concurPushes++;
    /* wait to unlock queue and free entries */
    while (pausedQueue!=0 || queueSize - (newWritePos-readPos) == 0 ||
            pushConcurUsed == pushConcurAllocated)
    {
        pushFoldLock.release();
        
        if (timeoutMicro != 0ULL)
        {
            if (!pushCond.timed_wait(mutex, deadline))
            {
                concurPushes--;
                if (closingQueue && concurPushes == 0)
                    closeCond.broadcast();
                return 0;
            }
        }
        else
            pushCond.wait(mutex);
        
        pushFoldLock.acquire();
        
        if (closingQueue || closedQueue!=0 || cancelledQueue != 0)
        {
            concurPushes--;
            if (closingQueue && concurPushes == 0)
                closeCond.broadcast();
            return 0;
        }
    }
    
    const gsize taken = std::min(elemsNum, queueSize - (newWritePos-readPos));
    pushId = allocPushConcur(taken, queuePos);
    return taken;
}

gsize MemQueue::tryDoPushing(gsize elemsNum, gsize oldElemsNum,
        guint& pushId, gsize& queuePos)
{
    Mutex::Lock lock(mutex);
    CondMutexLock pushFoldLock(pushFoldMutex);
    
    if (!finishPushingInt(oldElemsNum, pushId, lock))
    {
        pushId = CONCURID_NONE; // reset pushId for obsolete finish
        throw MyComp::Exception("Given elemsNum is greater than "
                "returned by doPushing method!");
    }
    pushId = CONCURID_NONE; // reset pushId for obsolete finish
    /* begin next */
    /* if queue and free entries do nothing */
    if (pausedQueue!=0 || closedQueue!=0 || closingQueue ||
        cancelledQueue != 0 || queueSize - (newWritePos-readPos) == 0 ||
          pushConcurUsed == pushConcurAllocated)
        return 0;
    
    concurPushes++;
    
    const gsize taken = std::min(elemsNum, queueSize - (newWritePos-readPos));
    pushId = allocPushConcur(taken, queuePos);
    return taken;
}

void MemQueue::finishPushing(gsize oldElemsNum, guint pushId)
{
    Mutex::Lock lock(mutex);
    CondMutexLock pushFoldLock(pushFoldMutex);
    if (!finishPushingInt(oldElemsNum, pushId, lock))
        throw MyComp::Exception("Given elemsNum is greater than "
                "returned by doPushing method!");
}


bool MemQueue::finishPoppingInt(gsize oldElemsNum, guint popId, Mutex::Lock& lock)
{
    bool success = true;
    if (popId != CONCURID_NONE)
    {
        const gsize oldReadPos = readPos;
        success = finishPopConcur(popId, oldElemsNum, lock);
        if (readPos != oldReadPos && oldReadPos+queueSize == newWritePos)
            pushCond.broadcast();
        if (cancelledQueue != 0 || (closedQueue != 0 && readPos == writePos &&
            popConcurUsed == 0))
        {
            popCond.broadcast(); // if no data, inform poppers
            g_atomic_int_set(&noDataQueue, 1);
        }
        if (pausedQueue!=0 && readPos == writePos && popConcurUsed == 0)
            pausedQueue = 2; // set is flushed and stopped
        if (readPos >= queueSize)
        {
            readPos -= queueSize;
            writePos -= queueSize;
            newReadPos -= queueSize;
            newWritePos -= queueSize;
        }
    }
    return success;
}

guint MemQueue::allocPopConcur(gsize elemsNum, gsize& popPos)
{
    guint popId = popConcurStart+popConcurUsed;
    popConcurUsed++;
    if (popId >= popConcurAllocated)
        popId -= popConcurAllocated;
    
    ConcurEntry& entry = popConcurEntries[popId];
    entry.startPos = newReadPos;
    entry.endPos = newReadPos+elemsNum;
    entry.processed = true;
    popPos = newReadPos;
    if (popPos >= queueSize)
        popPos -= queueSize;
    newReadPos += elemsNum;
    return popId;
}

bool MemQueue::finishPopConcur(guint popId, gsize elemsNum, Mutex::Lock& lock)
{
    bool success = true;
    
    if (popConcurUsed == 0)
        return true; // do nothing
        
    if (popId >= popConcurAllocated)
        popId -= popConcurAllocated;
    ConcurEntry& entry = popConcurEntries[popId];
    if (entry.endPos-entry.startPos < elemsNum)
        success = false;
    else // is good
        entry.startPos += elemsNum;
    
    // set startPos to star position for unread data
    entry.processed = false;
    
    const gsize oldPopConcurUsed = popConcurUsed;
    
    guint tmpPopConcurStart = popConcurStart;
    guint tmpPopConcurUsed = popConcurUsed;
    gsize tmpReadPos = readPos;
    gsize tmpNewReadPos = newReadPos;
    
    /* removes unused */
    while (tmpPopConcurUsed != 0 &&
        !popConcurEntries[tmpPopConcurStart].processed &&
        popConcurEntries[tmpPopConcurStart].startPos ==
        popConcurEntries[tmpPopConcurStart].endPos)
    {
        const ConcurEntry& toDel = popConcurEntries[tmpPopConcurStart];
        if (toDel.endPos-tmpReadPos>queueSize)
            tmpReadPos = toDel.endPos-queueSize;
        else
            tmpReadPos = toDel.endPos;
        tmpPopConcurStart++;
        if (tmpPopConcurStart >= popConcurAllocated)
            tmpPopConcurStart -= popConcurAllocated;
        tmpPopConcurUsed--;
    }
    
    /* move unread data to front */
    guint itPopId = tmpPopConcurStart + tmpPopConcurUsed-1;
    if (itPopId >= popConcurAllocated)
        itPopId -= popConcurAllocated;
    bool addedToUnreaded = false;
    while (tmpPopConcurUsed!=0 && !popConcurEntries[itPopId].processed)
    {
        ConcurEntry& toFold = popConcurEntries[itPopId];
        
        gsize sPos = toFold.startPos<queueSize ?
                toFold.startPos : toFold.startPos-queueSize;
        gsize dPos = tmpNewReadPos<queueSize?tmpNewReadPos:tmpNewReadPos-queueSize;
        const gsize ePos = toFold.endPos<queueSize ? 
                toFold.endPos : toFold.endPos-queueSize;
        
        const gsize blockSize = toFold.endPos-toFold.startPos;
        if (ePos != dPos && blockSize != 0)
        { // move to proper place
            if (lock.locked()) // release if needed
                lock.release();
            
            if (dPos >= blockSize)
                dPos -= blockSize;
            else
                dPos += queueSize - blockSize;
            
            gsize b1 = blockSize;
            gsize b2 = blockSize;
            if (dPos+blockSize > queueSize)
                b1 = queueSize-dPos;
            if (sPos+blockSize > queueSize)
            {
                if (b1 < blockSize)
                    b2 = queueSize-sPos;
                else
                    b1 = queueSize-sPos;
            }
            
            const gsize prevDPos2 = dPos;
            const gsize prevSPos2 = sPos;
            
            if (b1 < blockSize)
            {
                dPos += b1;
                if (dPos >= queueSize)
                    dPos -= queueSize;
                sPos += b1;
                if (sPos >= queueSize)
                    sPos -= queueSize;
                
                const gsize prevDPos = dPos;
                const gsize prevSPos = sPos;
                
                if (b2 < blockSize)
                {
                    dPos += b2-b1;
                    if (dPos >= queueSize)
                        dPos -= queueSize;
                    sPos += b2-b1;
                    if (sPos >= queueSize)
                        sPos -= queueSize;
                    memmove(queueData + dPos*elemSize, queueData + sPos*elemSize,
                            elemSize*(blockSize-b2));
                }
                memmove(queueData + prevDPos*elemSize, queueData + prevSPos*elemSize,
                        elemSize*(b2-b1));
            }
            memmove(queueData + prevDPos2*elemSize, queueData + prevSPos2*elemSize,
                    elemSize*b1);
        }
        
        tmpNewReadPos -= blockSize;
        
        if (blockSize != 0) // do inform other poppers about new data
            addedToUnreaded = true;
        
        if (itPopId == 0)
            itPopId = popConcurAllocated-1;
        else itPopId--;
        tmpPopConcurUsed--;
    }
    
    if (!lock.locked())
        lock.acquire();
    
    readPos = tmpReadPos;
    newReadPos = tmpNewReadPos;
    popConcurStart = tmpPopConcurStart;
    popConcurUsed = tmpPopConcurUsed;
    
    if ((popConcurUsed < oldPopConcurUsed && oldPopConcurUsed == popConcurAllocated)
            || addedToUnreaded)
        // unlock pushConcurEntries for other threads or inform about new data
        popCond.broadcast(); 
    
    if (popConcurUsed == 0)  // reset concur start
    {
        readPos = newReadPos;
        popConcurStart = 0;
    }
    return success;
}

gsize MemQueue::doPopping(gsize elemsNum, gsize oldElemsNum,
        guint& popId, gsize& queuePos, guint64 timeoutMicro)
{
    TimeVal deadline;
    if (timeoutMicro != 0ULL)
        deadline = getAfterCurrentTime(timeoutMicro);
    
    Mutex::Lock lock(mutex);
    CondMutexLock popFoldLock(popFoldMutex);
    
    if (!finishPoppingInt(oldElemsNum, popId, lock))
    {
        popId = CONCURID_NONE; // reset popId for obsolete finish
        throw MyComp::Exception("Given elemsNum is greater "
                "than returned by doPopping method!");
    }
    popId = CONCURID_NONE; // reset popId for obsolete finish
    
    if (cancelledQueue!=0 || (closedQueue!=0 && popConcurUsed == 0 &&
        readPos==writePos))
        return 0;
    
    /* wait to unlock queue and free entries */
    while ((writePos-newReadPos) == 0 || popConcurUsed == popConcurAllocated)
    {
        popFoldLock.release();
        
        if (timeoutMicro != 0ULL)
        {
            if (!popCond.timed_wait(mutex, deadline))
                return 0;
        }
        else
            popCond.wait(mutex);
        
        popFoldLock.acquire();
        
        if (pausedQueue!=0 && readPos == writePos && popConcurUsed == 0)
            pausedQueue = 2; // set is flushed and stopped
        
        if (cancelledQueue!=0 || (closedQueue!=0 && popConcurUsed == 0 &&
            pushConcurUsed == 0 && readPos==writePos))
        {
            popCond.broadcast();
            return 0;
        }
    }
    
    const gsize taken = std::min(elemsNum, writePos-newReadPos);
    popId = allocPopConcur(taken, queuePos);
    return taken;
}

gsize MemQueue::tryDoPopping(gsize elemsNum, gsize oldElemsNum,
        guint& popId, gsize& queuePos)
{
    Mutex::Lock lock(mutex);
    CondMutexLock popFoldLock(popFoldMutex);
    
    if (!finishPoppingInt(oldElemsNum, popId, lock))
    {
        popId = CONCURID_NONE; // reset popId for obsolete finish
        throw MyComp::Exception("Given elemsNum is greater than "
                "returned by doPoping method!");
    }
    popId = CONCURID_NONE; // reset popId for obsolete finish
    
    if (pausedQueue!=0 && readPos == writePos && popConcurUsed == 0)
        pausedQueue = 2; // set is flushed and stopped
    
    if (cancelledQueue!=0 || (closedQueue!=0 && popConcurUsed == 0 &&
        readPos==writePos))
        return 0;
    
    /*  if queue and free entries  do nothing */
    if ((writePos-newReadPos) == 0  || popConcurUsed == popConcurAllocated)
        return 0;
    
    const gsize taken = std::min(elemsNum, writePos-newReadPos);
    popId = allocPopConcur(taken, queuePos);
    return taken;
}

void MemQueue::finishPopping(gsize oldElemsNum, guint popId)
{
    Mutex::Lock lock(mutex);
    CondMutexLock popFoldLock(popFoldMutex);
    if (!finishPoppingInt(oldElemsNum, popId, lock))
        throw MyComp::Exception("Given elemsNum is greater than "
                "returned by doPoping method!");
}

/*****
 * normal access
 ****/

gsize MemQueue::pushAll(gsize elemsNum, const void* elems, guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    const char* remainElems = (const char*)elems;
    gsize oldElemsNum = 0;
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while (closedQueue==0 && remainElemsNum > 0)
    {
        oldElemsNum = doPushing(remainElemsNum, oldElemsNum, pushId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyToQueue(pos, oldElemsNum, remainElems);
            remainElemsNum -= oldElemsNum;
            remainElems += elemSize*oldElemsNum;
        }
        else break;
    }
    finishPushing(oldElemsNum, pushId);
    return elemsNum-remainElemsNum;
}

gsize MemQueue::push(gsize elemsNum, const void* elems, guint64 timeoutMicro)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPushing(elemsNum, 0, pushId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyToQueue(pos, oldElemsNum, elems);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

gsize MemQueue::tryPush(gsize elemsNum, const void* elems)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPushing(elemsNum, 0, pushId, pos);
    if (oldElemsNum!=0)
        copyToQueue(pos, oldElemsNum, elems);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

/* trying poll all required elems before timeout */

gsize MemQueue::popAll(gsize elemsNum, void* elems, guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    char* remainElems = (char*)elems;
    gsize oldElemsNum = 0;
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while ((closedQueue==0 || writePos!=readPos) && remainElemsNum > 0)
    {
        oldElemsNum = doPopping(remainElemsNum, oldElemsNum, popId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyFromQueue(pos, oldElemsNum, remainElems);
            remainElemsNum -= oldElemsNum;
            remainElems += elemSize*oldElemsNum;
        }
        else break;
    }
    finishPopping(oldElemsNum, popId);
    return elemsNum-remainElemsNum;
}

/* trying poll any elems before timeout */
gsize MemQueue::pop(gsize elemsNum, void* elems, guint64 timeoutMicro)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPopping(elemsNum, 0, popId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyFromQueue(pos, oldElemsNum, elems);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}

gsize MemQueue::tryPop(gsize elemsNum, void* elems)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPopping(elemsNum, 0, popId, pos);
    if (oldElemsNum!=0)
        copyFromQueue(pos, oldElemsNum, elems);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}


void MemQueue::close()
{
    if (g_atomic_int_get(&closedQueue)!=0)
        return; // do nothing
    Mutex::Lock lock(mutex);
    closingQueue = true;
    pushCond.broadcast();
    while (concurPushes != 0 && cancelledQueue == 0)
        closeCond.wait(mutex);
    
    if (closedQueue != 0 || cancelledQueue != 0)
        return; // has been closed
    g_atomic_int_set(&closedQueue, 1);
    popCond.broadcast();
    pushCond.broadcast();
    if (popConcurUsed == 0 && readPos == writePos)
        g_atomic_int_set(&noDataQueue, 1);
}

void MemQueue::cancel()
{
    Mutex::Lock lock(mutex);
    g_atomic_int_set(&cancelledQueue, 1);
    if (!isClosed())
    {
        closingQueue = true;
        // immediatelly closes queue
        g_atomic_int_set(&closedQueue, 1);
    }
    // inform about this popper (popping threads)
    pushCond.broadcast();
    popCond.broadcast();
    closeCond.broadcast();
    
    g_atomic_int_set(&noDataQueue, 1);
}

/*
 * Device Support Classes
 * 
 */

RefPtr<MemQueueDeviceSupport> MemQueueDeviceSupport::create(
            gsize elemSize, gsize elemsNum)
{
   RefPtr<MemQueueDeviceSupport> memQueue = RefPtr<MemQueueDeviceSupport>(
           new MemQueueDeviceSupport(elemSize, elemsNum));
   if (!memQueue)
       throw MyComp::Exception("Cant create MemQueueDeviceSupport");
   return memQueue;
}

RefPtr<MemQueueDeviceSupport> MemQueueDeviceSupport::create(
        gsize elemSize, gsize elemsNum, guint pushConcurTasks, guint popConcurTasks)
{
    RefPtr<MemQueueDeviceSupport> memQueue = RefPtr<MemQueueDeviceSupport>(
           new MemQueueDeviceSupport(elemSize, elemsNum, pushConcurTasks, popConcurTasks));
   if (!memQueue)
       throw MyComp::Exception("Cant create MemQueueDeviceSupport");
   return memQueue;
}

MemQueueDeviceSupport::MemQueueDeviceSupport(gsize elemSize, gsize elemsNum) :
        MemQueue(elemSize, elemsNum)
{
}

MemQueueDeviceSupport::MemQueueDeviceSupport(gsize elemSize, gsize elemsNum,
           guint pushConcurTasks, guint popConcurTasks) :
        MemQueue(elemSize, elemsNum, pushConcurTasks, popConcurTasks)
{
}

MemQueueDeviceSupport::~MemQueueDeviceSupport()
{
}

#ifdef HAVE_OPENCL
void MemQueueDeviceSupport::copyCLToQueue(gsize pos, gsize bufPos, gsize elemsNum,
        cl::CommandQueue& cmdQueue, cl::Buffer& buf, guint pushId)
{
    try
    {
        const gsize wPos = pos<queueSize?pos:pos-queueSize;
        if (wPos+elemsNum > queueSize)
        {
            cmdQueue.enqueueReadBuffer(buf, CL_TRUE, bufPos*elemSize,
                   elemSize*(queueSize-wPos), queueData+wPos*elemSize);
            cmdQueue.enqueueReadBuffer(buf, CL_TRUE, elemSize*(bufPos + queueSize-wPos),
                    (elemsNum-(queueSize-wPos))*elemSize, queueData);
        }
        else
            cmdQueue.enqueueReadBuffer(buf, CL_TRUE, bufPos*elemSize, elemSize*elemsNum,
                    queueData+wPos*elemSize);
    }
    catch(cl::Error& error)
    {
        finishPushing(0, pushId);
        throw MyComp::Exception("Cant enqueue read buffer", error);
    }
}

void MemQueueDeviceSupport::copyCLFromQueue(gsize pos, gsize bufPos, gsize elemsNum,
        cl::CommandQueue& cmdQueue, cl::Buffer& buf, guint popId)
{
    try
    {
        if (pos+elemsNum > queueSize)
        {
            cmdQueue.enqueueWriteBuffer(buf, CL_TRUE, bufPos*elemSize,
                    elemSize*(queueSize-pos), queueData + pos*elemSize);
            cmdQueue.enqueueWriteBuffer(buf, CL_TRUE, (bufPos + queueSize-pos)*elemSize,
                    elemSize*(elemsNum-(queueSize-pos)), queueData);
        }
        else
            cmdQueue.enqueueWriteBuffer(buf, CL_TRUE, bufPos*elemSize, elemSize*elemsNum,
                    queueData + pos*elemSize);
    }
    catch(cl::Error& error)
    {
        finishPopping(0, popId);
        throw MyComp::Exception("Cant enqueue write buffer", error);
    }
}


gsize MemQueueDeviceSupport::pushAllCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
       cl::Buffer& buf, guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    gsize remainPos = 0;
    gsize oldElemsNum = 0;
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while (closedQueue==0 && remainElemsNum > 0)
    {
        oldElemsNum = doPushing(remainElemsNum, oldElemsNum, pushId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyCLToQueue(pos, remainPos, oldElemsNum, cmdQueue, buf, pushId);
            remainElemsNum -= oldElemsNum;
            remainPos += oldElemsNum;
        }
        else break;
    }
    finishPushing(oldElemsNum, pushId);
    return elemsNum-remainElemsNum;
}


gsize MemQueueDeviceSupport::pushCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
        cl::Buffer& buf, guint64 timeoutMicro)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPushing(elemsNum, 0, pushId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyCLToQueue(pos, 0, oldElemsNum, cmdQueue, buf, pushId);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::tryPushCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
       cl::Buffer& buf)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPushing(elemsNum, 0, pushId, pos);
    if (oldElemsNum!=0)
        copyCLToQueue(pos, 0, oldElemsNum, cmdQueue, buf, pushId);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::popAllCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
        cl::Buffer& buf, guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    gsize remainPos = 0;
    gsize oldElemsNum = 0;
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while ((closedQueue==0||writePos!=readPos) && remainElemsNum > 0)
    {
        oldElemsNum = doPopping(remainElemsNum, oldElemsNum, popId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyCLFromQueue(pos, remainPos, oldElemsNum, cmdQueue, buf, popId);
            remainElemsNum -= oldElemsNum;
            remainPos += oldElemsNum;
        }
        else break;
    }
    finishPopping(oldElemsNum, popId);
    return elemsNum-remainElemsNum;
}

gsize MemQueueDeviceSupport::popCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
       cl::Buffer& buf, guint64 timeoutMicro)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPopping(elemsNum, 0, popId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyCLFromQueue(pos, 0, oldElemsNum, cmdQueue, buf, popId);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::tryPopCL(gsize elemsNum, cl::CommandQueue& cmdQueue,
          cl::Buffer& buf)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPopping(elemsNum, 0, popId, pos);
    if (oldElemsNum!=0)
        copyCLFromQueue(pos, 0, oldElemsNum, cmdQueue, buf, popId);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}

/*
 * OpenCL methods for C version
 */

void MemQueueDeviceSupport::copyCLCToQueue(gsize pos, gsize bufPos, gsize elemsNum,
        cl_command_queue cmdQueue, cl_mem buf, guint pushId)
{
    cl_int clret;
    const gsize wPos = pos<queueSize?pos:pos-queueSize;
    if (wPos+elemsNum > queueSize)
    {
        clret = clEnqueueReadBuffer(cmdQueue, buf, CL_TRUE, bufPos*elemSize,
                elemSize*(queueSize-wPos), queueData+wPos*elemSize, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPushing(0, pushId);
            throw MyComp::Exception::createCLError("Cant enqueue read buffer", clret);
        }
        clret = clEnqueueReadBuffer(cmdQueue, buf, CL_TRUE,
                elemSize*(bufPos + queueSize-wPos), (elemsNum-(queueSize-wPos))*elemSize,
                queueData, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPushing(0, pushId);
            throw MyComp::Exception::createCLError("Cant enqueue read buffer", clret);
        }
    }
    else
    {
        clret = clEnqueueReadBuffer(cmdQueue, buf, CL_TRUE, bufPos*elemSize,
                elemSize*elemsNum, queueData+wPos*elemSize, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPushing(0, pushId);
            throw MyComp::Exception::createCLError("Cant enqueue read buffer", clret);
        }
    }
}

void MemQueueDeviceSupport::copyCLCFromQueue(gsize pos, gsize bufPos, gsize elemsNum,
        cl_command_queue cmdQueue, cl_mem buf, guint popId)
{
    cl_int clret;
    if (pos+elemsNum > queueSize)
    {
        clret = clEnqueueWriteBuffer(cmdQueue, buf, CL_TRUE, bufPos*elemSize,
                elemSize*(queueSize-pos), queueData + pos*elemSize, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPopping(0, popId);
            throw MyComp::Exception::createCLError("Cant enqueue write buffer", clret);
        }
        clret = clEnqueueWriteBuffer(cmdQueue, buf, CL_TRUE,
                (bufPos + queueSize-pos)*elemSize, elemSize*(elemsNum-(queueSize-pos)),
                queueData, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPopping(0, popId);
            throw MyComp::Exception::createCLError("Cant enqueue write buffer", clret);
        }
    }
    else
    {
        clret = clEnqueueWriteBuffer(cmdQueue, buf, CL_TRUE, bufPos*elemSize,
                 elemSize*elemsNum, queueData + pos*elemSize, 0, NULL, NULL);
        if (clret != CL_SUCCESS)
        {
            finishPopping(0, popId);
            throw MyComp::Exception("Cant enqueue write buffer");
        }
    }
}

gsize MemQueueDeviceSupport::pushAllCLC(gsize elemsNum, cl_command_queue cmdQueue,
        cl_mem buf, guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    gsize remainPos = 0;
    gsize oldElemsNum = 0;
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while (closedQueue==0 && remainElemsNum > 0)
    {
        oldElemsNum = doPushing(remainElemsNum, oldElemsNum, pushId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyCLCToQueue(pos, remainPos, oldElemsNum, cmdQueue, buf, pushId);
            remainElemsNum -= oldElemsNum;
            remainPos += oldElemsNum;
        }
        else break;
    }
    finishPushing(oldElemsNum, pushId);
    return elemsNum-remainElemsNum;
}

gsize MemQueueDeviceSupport::pushCLC(gsize elemsNum, cl_command_queue cmdQueue,
        cl_mem buf, guint64 timeoutMicro)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPushing(elemsNum, 0, pushId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyCLCToQueue(pos, 0, oldElemsNum, cmdQueue, buf, pushId);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::tryPushCLC(gsize elemsNum, cl_command_queue cmdQueue,
        cl_mem buf)
{
    gsize pos;
    guint pushId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPushing(elemsNum, 0, pushId, pos);
    if (oldElemsNum!=0)
        copyCLCToQueue(pos, 0, oldElemsNum, cmdQueue, buf, pushId);
    finishPushing(oldElemsNum, pushId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::popAllCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
        guint64 timeoutMicro)
{
    gsize remainElemsNum = elemsNum;
    gsize remainPos = 0;
    gsize oldElemsNum = 0;
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    
    while ((closedQueue==0||writePos!=readPos) && remainElemsNum > 0)
    {
        oldElemsNum = doPopping(remainElemsNum, oldElemsNum, popId, pos, timeoutMicro);
        if (oldElemsNum!=0)
        {
            copyCLCFromQueue(pos, remainPos, oldElemsNum, cmdQueue, buf, popId);
            remainElemsNum -= oldElemsNum;
            remainPos += oldElemsNum;
        }
        else break;
    }
    finishPopping(oldElemsNum, popId);
    return elemsNum-remainElemsNum;
}

gsize MemQueueDeviceSupport::popCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
        guint64 timeoutMicro)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = doPopping(elemsNum, 0, popId, pos, timeoutMicro);
    if (oldElemsNum!=0)
        copyCLCFromQueue(pos, 0, oldElemsNum, cmdQueue, buf, popId);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}

gsize MemQueueDeviceSupport::tryPopCLC(gsize elemsNum, cl_command_queue cmdQueue,
       cl_mem buf)
{
    gsize pos;
    guint popId = CONCURID_NONE;
    if (elemsNum == 0)
        return 0;
    const gsize oldElemsNum = tryDoPopping(elemsNum, 0, popId, pos);
    if (oldElemsNum!=0)
        copyCLCFromQueue(pos, 0, oldElemsNum, cmdQueue, buf, popId);
    finishPopping(oldElemsNum, popId);
    return oldElemsNum;
}

#endif

