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
#include <cstdlib>
#include <cstring>
#include <glibmm.h>
#include "Queue.h"
#include "PushPop.h"

using namespace Glib;
using namespace MyComp;

/*
 * DirectPush - pusher object for MemQueue
 */

DirectPush::DirectPush()
{
    this->start = 0;
    this->pos = 0;
    this->end1 = 0;
    this->end2 = 0;
    this->pushId = CONCURID_NONE;
    this->blockSize = 0;
    this->secondPart = false;
}

DirectPush::DirectPush(const RefPtr<AbstractMemQueue>& mQueue, gsize blockSize)
{
    this->mQueue = mQueue;
    this->start = 0;
    this->pos = 0;
    this->end1 = 0;
    this->end2 = 0;
    this->pushId = CONCURID_NONE;
    
    if (blockSize != 0UL)
        this->blockSize = blockSize;
    else if (mQueue)
        this->blockSize = mQueue->getRecommendedPushingSize();
    
    this->secondPart = false;
}

DirectPush::~DirectPush()
{
    finish();
}

void DirectPush::setMemQueue(const Glib::RefPtr<AbstractMemQueue>& mQueue,
         gsize blockSize)
{
    this->mQueue = mQueue;
    if (blockSize != 0UL)
        this->blockSize = blockSize;
    else if (mQueue)
        this->blockSize = mQueue->getRecommendedPushingSize();
}

bool DirectPush::handlePush(guint64 timeoutMicro)
{
    if (end1 != end2)
    {   // pos breaks queueSize and going to start of ringbuffer
        pos = 0;
        end1 = end2;
        secondPart = true;
    }
    else
    {
        const gsize qSize = mQueue->getQueueSize();
        gsize size = (secondPart) ? (pos+qSize-start) : pos-start;
        size = mQueue->doPushing(blockSize, size, pushId, start, timeoutMicro);
        
        secondPart = false;
        
        if (size == 0)
            return false;
        // update queue data for every doPushing/doPopping
        this->queueData = mQueue->getQueueData();
        
        pos = start;
        end1 = pos+size;
        if (end1 <= qSize)
            end2 = end1;
        else
        {   // divide block into two segment (at end of queue and at start of queue)
            end1 = qSize;
            end2 = pos+size-qSize;
        }
    }
    
    return true;
}

bool DirectPush::handleTryPush()
{
    if (end1 != end2)
    {   // pos breaks queueSize and going to start of ringbuffer
        pos = 0;
        end1 = end2;
        secondPart = true;
    }
    else
    {
        const gsize qSize = mQueue->getQueueSize();
        gsize size = (secondPart) ? (pos+qSize-start) : pos-start;
        size = mQueue->tryDoPushing(blockSize, size, pushId, start);
        
        secondPart = false;
        
        if (size == 0)
            return false;
        // update queue data for every doPushing/doPopping
        this->queueData = mQueue->getQueueData();
        
        pos = start;
        end1 = pos+size;
        if (end1 <= qSize)
            end2 = end1;
        else
        {   // divide block into two segment (at end of queue and at start of queue)
            end1 = qSize;
            end2 = pos+size-qSize;
        }
    }
    
    return true;
}

void DirectPush::finish()
{
    if (mQueue)
    {
        const gsize size = (secondPart) ? (pos+mQueue->getQueueSize()-start) : pos-start;
        mQueue->finishPushing(size, pushId);
        pushId = CONCURID_NONE; // preventing many finishes
        secondPart = false;
    }
}

/*
 * DirectPop - popper object for MemQueue
 */

DirectPop::DirectPop()
{
    this->start = 0;
    this->pos = 0;
    this->end1 = 0;
    this->end2 = 0;
    this->popId = CONCURID_NONE;
    this->blockSize = 0;
    this->secondPart = false;
}

DirectPop::DirectPop(const RefPtr<AbstractMemQueue>& mQueue, gsize blockSize)
{
    this->mQueue = mQueue;
    this->start = 0;
    this->pos = 0;
    this->end1 = 0;
    this->end2 = 0;
    this->popId = CONCURID_NONE;
    
    if (blockSize != 0UL)
        this->blockSize = blockSize;
    else if (mQueue)
        this->blockSize = mQueue->getRecommendedPoppingSize();
    
    this->secondPart = false;
}

DirectPop::~DirectPop()
{
    finish();
}

void DirectPop::setMemQueue(const Glib::RefPtr<AbstractMemQueue>& mQueue,
         gsize blockSize)
{
    this->mQueue = mQueue;
    if (blockSize != 0UL)
        this->blockSize = blockSize;
    else if (mQueue)
        this->blockSize = mQueue->getRecommendedPushingSize();
}

bool DirectPop::handlePop(guint64 timeoutMicro)
{
    if (end1 != end2)
    {   // pos breaks queueSize and going to start of ringbuffer
        pos = 0;
        end1 = end2;
        secondPart = true;
    }
    else
    {
        const gsize qSize = mQueue->getQueueSize();
        gsize size = (secondPart) ? (pos+qSize-start) : pos-start;
        size = mQueue->doPopping(blockSize, size, popId, start, timeoutMicro);
        
        secondPart = false;
        
        if (size == 0)
            return false;
        // update queue data for every doPushing/doPopping
        this->queueData = mQueue->getQueueData();
        
        pos = start;
        end1 = pos+size;
        if (end1 <= qSize)
            end2 = end1;
        else
        {   // divide block into two segment (at end of queue and at start of queue)
            end1 = qSize;
            end2 = pos+size-qSize;
        }
    }
    
    return true;
}

bool DirectPop::handleTryPop()
{
    if (end1 != end2)
    {   // pos breaks queueSize and going to start of ringbuffer
        pos = 0;
        end1 = end2;
        secondPart = true;
    }
    else
    {
        const gsize qSize = mQueue->getQueueSize();
        gsize size = (secondPart) ? (pos+qSize-start) : pos-start;
        size = mQueue->tryDoPopping(blockSize, size, popId, start);
        
        secondPart = false;
        
        if (size == 0)
            return false;
        // update queue data for every doPushing/doPopping
        this->queueData = mQueue->getQueueData();
        
        pos = start;
        end1 = pos+size;
        if (end1 <= qSize)
            end2 = end1;
        else
        {   // divide block into two segment (at end of queue and at start of queue)
            end1 = qSize;
            end2 = pos+size-qSize;
        }
    }
    
    return true;
}

void DirectPop::finish()
{
    if (mQueue)
    {
        const gsize size = (secondPart) ? (pos+mQueue->getQueueSize()-start) : pos-start;
        mQueue->finishPopping(size, popId);
        popId = CONCURID_NONE; // preventing many finishes
        secondPart = false;
    }
}
