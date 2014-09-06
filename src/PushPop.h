/*! \file PushPop.h
 * \brief MemQueue wrappers
 **/
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

#ifndef __MYCOMPSTREAMER_PUSHPOP_H__
#define __MYCOMPSTREAMER_PUSHPOP_H__

#include <glibmm.h>
#include "Queue.h"
#include "Util.h"

namespace MyComp
{

/// simple wrapper for pushing
/** A simple wrapper for direct (in queue data) pushing elements.
 */
class DirectPush
{
private:
    Glib::RefPtr<AbstractMemQueue> mQueue;
    gsize start;
    gsize pos;
    gsize end1;
    gsize end2; /* block size to queue end */
    guint pushId;
    void* queueData;
    gsize blockSize;
    bool secondPart;
    
    bool handlePush(guint64 timeoutMicro = 0ULL);
    bool handleTryPush();
    
    DirectPush(const DirectPush& p);
    DirectPush& operator=(const DirectPush& p);
public:
    DirectPush();
    /// constructor
    /**
     * \param mQueue queue reference
     * \param blockSize size of the block in elements
     */
    DirectPush(const Glib::RefPtr<AbstractMemQueue>& mQueue, gsize blockSize = 0);
    /// destructor
    ~DirectPush();
    
    /**
     * \param mQueue memqueue
     * \param blockSize block size
     */
    void setMemQueue(const Glib::RefPtr<AbstractMemQueue>& mQueue, gsize blockSize = 0);
    
    /// push element into queue
    /**
     * \param ob object to put
     * \return true if success, false when failed (queue is closed)
     */
    template<typename T>
    bool push(const T& ob)
    {
        if (pos < end1 || handlePush())
            ((T*)queueData)[pos++] = ob;
        else return false;
        return true;
    }
    
    /// push element into queue. If timeout happens returns false
    /**
     * \param ob object to put
     * \param timeoutMicro timeout in microseconds
     * \return true if success, false when failed (queue is closed or timeout)
     */
    template<typename T>
    bool push(const T& ob, guint64 timeoutMicro)
    {
        if (pos < end1 || handlePush(timeoutMicro))
            ((T*)queueData)[pos++] = ob;
        else return false;
        return true;
    }
    
    /// trying to push element into queue. If queue is not ready to push returns false
    /**
     * \param ob object to put
     * \return true if success, false when failed
     */
    template<typename T>
    bool tryPush(const T& ob)
    {
        if (pos < end1 || handleTryPush())
            ((T*)queueData)[pos++] = ob;
        else return false;
        return true;
    }
    
    /// finish pushing
    void finish();
    
    /// get block size
    gsize getBlockSize() const
    { return blockSize; }
    
    /// set block size
    void setBlockSize(gsize blockSize)
    { this->blockSize = blockSize; }
};

/// simple wrapper for popping (dequeueing)
/** A simple wrapper for direct (in queue data) pushing (dequeueing) elements.
 */
class DirectPop
{
private:
    Glib::RefPtr<AbstractMemQueue> mQueue;
    gsize start;
    gsize pos;
    gsize end1;
    gsize end2; /* block size to queue end */
    guint popId;
    void* queueData;
    gsize blockSize;
    bool secondPart;
    
    bool handlePop(guint64 timeoutMicro = 0ULL);
    bool handleTryPop();
    
    DirectPop(const DirectPop& p);
    DirectPop& operator=(const DirectPop& p);
public:
    DirectPop();
    /// constructor
    /**
     * \param mQueue queue reference
     * \param blockSize size of the block in elements
     */
    DirectPop(const Glib::RefPtr<AbstractMemQueue>& mQueue, gsize blockSize = 0);
    /// destructor
    ~DirectPop();

    /// set memqueue
    /**
     * \param mQueue memqueue
     * \param blockSize block size
     */
    void setMemQueue(const Glib::RefPtr<AbstractMemQueue>& mQueue, gsize blockSize = 0);
    
    /// pop element from queue
    /**
     * \param ob object which will be written after pop
     * \return true if success, false when failed (queue is closed)
     */
    template<typename T>
    bool pop(T& ob)
    {
        if (pos < end1 || handlePop())
            ob = ((const T*)queueData)[pos++];
        else return false;
        return true;
    }
    
    /// pop element from queue. If timeout happens returns false
    /**
     * \param ob object which will be written after pop
     * \param timeoutMicro timeout in microseconds
     * \return true if success, false when failed (queue is closed or timeout)
     */
    template<typename T>
    bool pop(T& ob, guint64 timeoutMicro)
    {
        if (pos < end1 || handlePop(timeoutMicro))
            ob = ((const T*)queueData)[pos++];
        else return false;
        return true;
    }
    
    /// trying to pop element from queue. If queue is not ready to pop returns false
    /**
     * \param ob object which will be written after pop
     * \return true if success, false when failed
     */
    template<typename T>
    bool tryPop(T& ob)
    {
        if (pos < end1 || handleTryPop())
            ob = ((const T*)queueData)[pos++];
        else return false;
        return true;
    }
    
    /// finish pushing
    void finish();
    
    /// get block size
    gsize getBlockSize() const
    { return blockSize; }
    
    /// set block size
    void setBlockSize(gsize blockSize)
    { this->blockSize = blockSize; }
};

};

#endif
