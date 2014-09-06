/*! \file Queue.h
 * \brief queue system
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

#ifndef __MYCOMPSTREAMER_QUEUE_H__
#define __MYCOMPSTREAMER_QUEUE_H__

#include <vector>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif
#include "Util.h"

namespace MyComp
{

/// mutex based on condition variable
/** Condition variable's mutex. Should be locked after locking real mutex and
 * unlocked before real mutex. This mutex broadcasts signal only if needed (counts locks)
 */
class CondMutex
{
private:
    Glib::Mutex& mutex;
    Glib::Cond cond;
    guint32 count;
    bool locked;
    
    // non-copyable
    CondMutex(const CondMutex& cm);
    CondMutex& operator=(const CondMutex& cm);
public:
    /// constructor
    CondMutex(Glib::Mutex& mutex_) : mutex(mutex_)
    {
        count = 0;
        locked = false;
    }
    
    /// lock conditional mutex
    void lock()
    {
        count++;
        while (locked)
            cond.wait(mutex);
        locked = true;
    }
    
    /// unlock conditional mutex
    void unlock()
    {
        count--;
        locked = false;
        if (count != 0)
            cond.broadcast();
    }
    
    /// return true if locked
    bool isLocked() const
    { return locked; }
};

/// conditional mutex lock
class CondMutexLock
{
private:
    CondMutex& condMutex;
    bool locked;
    
    // non-copyable
    CondMutexLock(const CondMutexLock& cmLock);
    CondMutexLock& operator=(const CondMutexLock& cmLock);
public:
    /// constructor
    CondMutexLock(CondMutex& condMutex_): condMutex(condMutex_)
    {
        condMutex.lock();
        locked = true;
    }
    
    ~CondMutexLock()
    {
        if (locked)
            condMutex.unlock();
    }
    
    /// acquire condition mutex
    void acquire()
    {
        condMutex.lock();
        locked = true;
    }
    
    /// release condition mutex
    void release()
    {
        condMutex.unlock();
        locked = false;
    }
    
    /// return true if locked
    bool isLocked() const
    { return locked; }
};


enum {
    CONCURID_NONE = G_MAXUINT ///< no concurrent operation id
};

enum {
    CHANNEL_NONE = G_MAXUINT
};

/// an abstract memory queue class.
class AbstractMemQueue: public RefCountable
{
protected:
    gsize elemSize; ///< sizeof of element in bytes
    gsize queueSize; ///< sizeof of queue (in element's number)
    char* queueData; ///< data (holds elements)
    
    volatile gint closedQueue; ///< indicates, that queue has been closed
    volatile gint cancelledQueue; ///< indicates, that queue operations should be cancelled
    volatile gint pausedQueue;  ///< nonzero if queue is paused (2 when all operations done)
    volatile gint noDataQueue; ///< nonzero if no data in queue

private:
    AbstractMemQueue(const AbstractMemQueue& ob);
    AbstractMemQueue& operator=(const AbstractMemQueue& ob);
public:
    AbstractMemQueue();
    virtual ~AbstractMemQueue();
    
    /// get queue size in elements
    /** IMPORTANT NOTICE: queue size MUST BE
     * always updated after every doPushing/doPopping call and also every
     * push/pop/resize operation, because will be changed during awaiting 
     * \return queue size (number of elements)
     */
    gsize getQueueSize() const
    { return queueSize; }
    
    /// get element size in bytes
    gsize getElemSize() const
    { return elemSize; }
    
    /// get reccommended size for pushing
    virtual gsize getRecommendedPushingSize() const = 0;
    /// get reccommended size for popping
    virtual gsize getRecommendedPoppingSize() const = 0;
    
    /// get used (pushed) elements
    virtual gsize getUsedSize() const = 0;
    
    /// push all elements into queue
    /** Push all elements into queue. When number of a free elements (in the queue)
     * is less than number of required elements, waits until freeing elements.
     * \param elemsNum number of the elements to push
     * \param elems elements data
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    virtual gsize pushAll(gsize elemsNum, const void* elems, guint64 timeoutMicro = 0ULL) = 0;
    /// push elements into queue
    /** Push elements into queue. When queue is full waits until freeing space.
     * When number of a free elements (in the queue) is less than number of specified elements,
     * terminates waiting and pushes only number of free elements.
     * \param elemsNum number of the elements to push
     * \param elems elements data
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    virtual gsize push(gsize elemsNum, const void* elems, guint64 timeoutMicro = 0ULL) = 0;
    /// trying to push elements into queue
    /** Trying to push elements into queue. When queue is full immediately returns.
     * When number of a free elements (in the queue) is less than number of specified elements,
     * terminates waiting and pushes only number of free elements.
     * \param elemsNum number of the elements to push
     * \param elems elements data
     * \return number of pushed elements
     */
    virtual gsize tryPush(gsize elemsNum, const void* elems) = 0;
    /// pop all elements from queue
    /** Pop all elements from queue. When number of a pushed elements (in queue)
     * is less than number of required elements, waits until writing elements.
     * \param elemsNum number of the elements to pop
     * \param elems elements data
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    virtual gsize popAll(gsize elemsNum, void* elems, guint64 timeoutMicro = 0ULL) = 0;
    /// pop elements from queue
    /** Pop elements from queue. When queue is full waits until writing elements.
     * When number of a pushed elements (in the queue) is less than number of specified elements,
     * terminates waiting and pops only number of pushed elements.
     * \param elemsNum number of the elements to pop
     * \param elems elements data
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    virtual gsize pop(gsize elemsNum, void* elems, guint64 timeoutMicro = 0ULL) = 0;
    /// trying to pop elements from queue
    /** Trying to pop elements from queue. When queue is empty immediately returns.
     * When number of a pushed elements (in the queue) is less than number of specified elements,
     * terminates waiting and pops only number of pushed elements.
     * \param elemsNum number of the elements to pop
     * \param elems elements data
     * \return number of popped elements
     */
    virtual gsize tryPop(gsize elemsNum, void* elems) = 0;
    
    /* direct pushing and poping */
    /// get queue data for direct access
    /** get queue data pointer for direct access. IMPORTANT NOTICE: queue data pointer MUST BE
     * always updated after every doPushing/doPopping call and also every
     * push/pop/resize operation, because will be changed during awaiting
     * \return queue data pointer
     */
    const char* getQueueData() const
    { return queueData; }
    /// get queue data for direct access
    /** get queue data pointer for direct access. IMPORTANT NOTICE: queue data pointer MUST BE
     * always updated after every doPushing/doPopping call and also every
     * push/pop/resize operation, because will be changed during awaiting
     * \return queue data pointer
     */
    char* getQueueData()
    { return queueData; }
    
    /// begins or continue a pushing operation
    /** Finish previous push operation (if pushId is not CONCURID_NONE) and begins new
     * push operation. At first call pushID must be CONCURID_NONE.
     * After call application MUST retrieve current queue size and
     * queue data pointer before pushing data directly. If timeout happens method returns
     * 0 and set pushId to CONCURID_NONE. Queue position is in range [0,queueSize).
     * However region can overlaps queue buffer (second part of region can be in
     * start of buffer) and this case must be handled by application.
     * 
     * \param elemsNum number of requested elements for new region (must be non-zero)
     * \param oldElemsNum number of pushed elements in previous region (or zero)
     * \param pushId concurrent push operation id. Pass previous pushId and
     *               returns new pushId
     * \param queuePos position in queue buffer for region. Pass previous position and
     *              after success returns new queue position
     * \param timeoutMicro timeout in microseconds
     * \return allocated size (in elements) for pushing (or zero)
     */
    virtual gsize doPushing(gsize elemsNum, gsize oldElemsNum, guint& pushId,
            gsize& queuePos, guint64 timeoutMicro = 0ULL) = 0;
    
    /// trying to begin or continue a pushing operation
    /** Finish previous push operation (if pushId is not CONCURID_NONE) and and trying to
     * begin new push operation. At first call pushID must be CONCURID_NONE.
     * After call application MUST retrieve current queue size
     * and queue data pointer before pushing data directly. If timeout happens method
     * returns 0 and set pushId to CONCURID_NONE. Queue position is in range [0,queueSize).
     * However region can overlaps queue buffer (second part of region can be in
     * start of buffer) and this case must be handled by application.
     * 
     * \param elemsNum number of requested elements for new region (must be non-zero)
     * \param oldElemsNum number of pushed elements in previous region (or zero)
     * \param pushId concurrent push operation id. Pass previous pushId and
     *               returns new pushId
     * \param queuePos position in queue buffer for region. Pass previous position and
     *              after success returns new queue position
     * \return allocated size (in elements) for a pushing (or zero)
     */
    virtual gsize tryDoPushing(gsize elemsNum, gsize oldElemsNum, guint& pushId,
            gsize& queuePos) = 0;
    
    /// finish a pushing operation
    /**
     * \param oldElemsNum number of pushed elements in previous region (or zero)
     * \param pushId previous concurrent push operation id.
     */
    virtual void finishPushing(gsize oldElemsNum, guint pushId) = 0;
    
    /// begins or continue a popping operation
    /** Finish previous pop operation (if popId is not CONCURID_NONE) and begins new
     * pop operation. At first call popId must be CONCURID_NONE.
     * After call application MUST retrieve current queue size and
     * queue data pointer before pushing data directly. If timeout happens method returns
     * 0 and set popId to CONCURID_NONE. Queue position is in range [0,queueSize).
     * However region can overlaps queue buffer (second part of region can be in
     * start of buffer) and this case must be handled by application.
     * 
     * \param elemsNum number of requested elements for new region (must be non-zero)
     * \param oldElemsNum number of popped elements in previous region (or zero)
     * \param popId concurrent pop operation id. Pass previous popId and
     *               returns new popId
     * \param queuePos position in queue buffer for region. Pass previous position and
     *              after success returns new queue position
     * \param timeoutMicro timeout in microseconds
     * \return allocated size (in elements) for a poping (or zero)
     */
    virtual gsize doPopping(gsize elemsNum, gsize oldElemsNum, guint& popId,
            gsize& queuePos, guint64 timeoutMicro = 0ULL) = 0;
    
    /// trying to begin or continue a popping operation
    /** Finish previous pop operation (if popId is not CONCURID_NONE) and trying to begins
     * new a pop operation.  At first call popId must be CONCURID_NONE.
     * After call application MUST retrieve current queue size and
     * queue data pointer before pushing data directly. If timeout happens method returns
     * 0 and set popId to CONCURID_NONE. Queue position is in range [0,queueSize).
     * However region can overlaps queue buffer (second part of region can be in
     * start of buffer) and this case must be handled by application.
     * 
     * \param elemsNum number of requested elements for new region (must be non-zero)
     * \param oldElemsNum number of popped elements in previous region (or zero)
     * \param popId concurrent pop operation id. Pass previous popId and
     *               returns new popId
     * \param queuePos position in queue buffer for region. Pass previous position and
     *              after success returns new queue position
     * \return allocated size (in elements) for a poping (or zero)
     */
    virtual gsize tryDoPopping(gsize elemsNum, gsize oldElemsNum, guint& popId,
                               gsize& queuePos) = 0;
    
    /// finish a popping operation
    /**
     * \param oldElemsNum number of popped elements in previous region (or zero)
     * \param popId previous concurrent pop operation id.
     */
    virtual void finishPopping(gsize oldElemsNum, guint popId) = 0;
    
    // close and waiting for flushing queue
    /// close queue
    virtual void close() = 0;
    /// check whether is closed
    /**
     * \return true if closed
     */
    bool isClosed() const
    {
        return g_atomic_int_get(&closedQueue)!=0;
    }
    
    /// close and cancel queue
    virtual void cancel() = 0;
    
    /// check whether is cancelled
    /**
     * \return true if cancel
     */
    bool isCancelled() const
    {
        return g_atomic_int_get(&cancelledQueue)!=0;
    }
    
    /// check whether is stopped (pushing stopped)
    bool isPaused() const
    {
        return g_atomic_int_get(&pausedQueue)!=0;
    }
    
    /// check whether is stopped (pushing stopped)
    bool isPausedAndFlushed() const
    {
        return g_atomic_int_get(&pausedQueue)==2;
    }
    
    /// check whether is no data
    bool isNoData() const
    {
        return g_atomic_int_get(&noDataQueue)==1;
    }    
};

/* simple blocking queue
 */

/// main memory queue.
/** main memory queue designed for high throughput and parallellity. Can handles
 * many concurrently executed push and pop tasks.
 */
class MemQueue: public AbstractMemQueue
{
private:
    struct ConcurEntry
    {
        gsize startPos, endPos;
        bool processed; // if operation is not finished (currently processed)
    };
    
    guint pushConcurAllocated;
    guint pushConcurStart, pushConcurUsed;
    ConcurEntry* pushConcurEntries;
    guint popConcurAllocated;
    guint popConcurStart, popConcurUsed;
    ConcurEntry* popConcurEntries;
    guint concurPushes;
    bool closingQueue; // indicates, that queue is closing
    
    Glib::Mutex mutex;
    /* condition, which is listened by pushing threads:
     * when pushing threads can push data to queue
     * this condition can be broadcasted by pushing threads for other pushing threads,
     * when newWritePos went to back and some place in queue has been freed */
    Glib::Cond pushCond;
    /* condition, which is listened by popping threads:
     * when popping threads can pop data from queue,
     * this condition can be broadcasted by popping threads for other popping threads, 
     * when no more data available, or when some data has been collected after
     * not fully finished popping (reads) */
    Glib::Cond popCond;
    /* condvar for closing waiting, broadcasted when no more pushing operations */
    Glib::Cond closeCond;
    
    CondMutex pushFoldMutex;
    CondMutex popFoldMutex;
    
    // internal method
    void copyToQueue(gsize pos, gsize elemsNum, const void* elems);
    void copyFromQueue(gsize pos, gsize elemsNum, void* elems);
    bool finishPushingInt(gsize oldElemsNum, guint pushId, Glib::Mutex::Lock& lock);
    bool finishPoppingInt(gsize oldElemsNum, guint popId, Glib::Mutex::Lock& lock);
        
    guint allocPushConcur(gsize elemsNum, gsize& pushPos);
    bool finishPushConcur(guint pushId, gsize elemsNum, Glib::Mutex::Lock& lock,
                          bool& fixed);
    
    guint allocPopConcur(gsize elemsNum, gsize& popPos);
    bool finishPopConcur(guint popId, gsize elemsNum, Glib::Mutex::Lock& lock);
    
    void handlePollBeforePush(gsize oldNewWritePos);
    void handlePollAfterPush(gsize oldWritePos, gsize oldNewWritePos);
    void handlePollBeforePop(gsize oldNewReadPos);
    void handlePollAfterPop(gsize oldReadPos, gsize oldNewReadPos);
    
    void pauseNoLock(); // pause without locking mutex
protected:
    gsize readPos; ///< position where all pending a reading operations begins
    gsize writePos; ///< position where all pending a writing operations begins
    /* newXXXPos - used to control concurrent pushing and poping data to queue */
    /* for concurrent pushing/popping */
    gsize newReadPos; ///< new read position (for new operations)
    gsize newWritePos;  ///< new write position (for new operations)
    
public:
    /// create new memqueue and returns reference
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     * \return reference of memqueue
     */
    static Glib::RefPtr<MemQueue> create(gsize elemSize, gsize elemsNum);
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     * \param pushConcurTasks max number of concurrent push operations (0 is default)
     * \param popConcurTasks max number of concurrent pop operations (0 is default)
     * \return reference of memqueue
     */
    static Glib::RefPtr<MemQueue> create(gsize elemSize, gsize elemsNum,
             guint pushConcurTasks, guint popConcurTasks);
    
    // constructor
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     */
    MemQueue(gsize elemSize, gsize elemsNum);
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     * \param pushConcurTasks max number of concurrent push operations (0 is default)
     * \param popConcurTasks max number of concurrent pop operations (0 is default)
     */
    MemQueue(gsize elemSize, gsize elemsNum, guint pushConcurTasks, guint popConcurTasks);
    virtual ~MemQueue();
    
    gsize getRecommendedPushingSize() const;
    gsize getRecommendedPoppingSize() const;
    
    gsize getUsedSize() const;
    
    gsize pushAll(gsize elemsNum, const void* elems, guint64 timeoutMicro = 0ULL);
    gsize push(gsize elemsNum, const void* elems, guint64 timeoutMicro = 0ULL);
    gsize tryPush(gsize elemsNum, const void* elems);
    /* trying poll all required elems before timeoutMicro */
    gsize popAll(gsize elemsNum, void* elems, guint64 timeoutMicro = 0ULL);
    /* trying poll any elems before timeoutMicro */
    gsize pop(gsize elemsNum, void* elems, guint64 timeoutMicro = 0ULL);
    gsize tryPop(gsize elemsNum, void* elems);
    
    /* direct pushing and poping */
    gsize doPushing(gsize elemsNum, gsize oldElemsNum, guint& pushId, gsize& queuePos,
            guint64 timeoutMicro = 0ULL);
    gsize tryDoPushing(gsize elemsNum, gsize oldElemsNum, guint& pushId, gsize& queuePos);
    void finishPushing(gsize oldElemsNum, guint pushId);
    
    gsize doPopping(gsize elemsNum, gsize oldElemsNum, guint& popId, gsize& queuePos,
            guint64 timeoutMicro = 0ULL);
    gsize tryDoPopping(gsize elemsNum, gsize oldElemsNum, guint& popId, gsize& queuePos);
    void finishPopping(gsize oldElemsNum, guint popId);
    

    void close();
    void cancel();
};

/// internal class for device support (OpenCL and others)
class MemQueueDeviceSupport: public MemQueue
{
private:
#ifdef HAVE_OPENCL
    void copyCLToQueue(gsize pos, gsize bufPos, gsize elemsNum,
            cl::CommandQueue& cmdQueue, cl::Buffer& buf, guint pushId);
    void copyCLFromQueue(gsize pos, gsize bufPos, gsize elemsNum,
            cl::CommandQueue& cmdQueue, cl::Buffer& buf, guint popId);
    
    void copyCLCToQueue(gsize pos, gsize bufPos, gsize elemsNum,
            cl_command_queue cmdQueue, cl_mem buf, guint pushId);
    void copyCLCFromQueue(gsize pos, gsize bufPos, gsize elemsNum,
            cl_command_queue cmdQueue, cl_mem buf, guint popId);
#endif
public:
    /// create memqueue device support
    static Glib::RefPtr<MemQueueDeviceSupport> create(gsize elemSize, gsize elemsNum);
    /// create memqueue device support
    static Glib::RefPtr<MemQueueDeviceSupport> create(gsize elemSize, gsize elemsNum,
             guint pushConcurTasks, guint popConcurTasks);
    
    // constructor
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     */
    MemQueueDeviceSupport(gsize elemSize, gsize elemsNum);
    /**
     * \param elemSize size of the element (in bytes)
     * \param elemsNum size of the queue (in elements)
     * \param pushConcurTasks max number of concurrent push operations (0 is default)
     * \param popConcurTasks max number of concurrent pop operations (0 is default)
     */
    MemQueueDeviceSupport(gsize elemSize, gsize elemsNum, guint pushConcurTasks,
                  guint popConcurTasks);
    virtual ~MemQueueDeviceSupport();
    
#ifdef HAVE_OPENCL
    /// push all elements into queue from OpenCL Buffer
    /** \see pushAll
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    gsize pushAllCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf,
            guint64 timeoutMicro = 0ULL);
    /// push elements into queue from OpenCL Buffer
    /** \see push
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    gsize pushCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf,
            guint64 timeoutMicro = 0ULL);
    /// try to push elements into queue from OpenCL Buffer
    /** \see tryPush
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \return number of pushed elements
     */
    gsize tryPushCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf);
    
    /// pop all elements from queue into OpenCL Buffer
    /** \see popAll
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    gsize popAllCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf,
            guint64 timeoutMicro = 0ULL);
    /// pop elements from queue into OpenCL Buffer
    /** \see pop
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    gsize popCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf,
            guint64 timeoutMicro = 0ULL);
    /// try to pop elements from queue into OpenCL Buffer
    /** \see tryPop
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \return number of popped elements
     */
    gsize tryPopCL(gsize elemsNum, cl::CommandQueue& cmdQueue, cl::Buffer& buf);
    
    /// push all elements into queue from OpenCL Buffer
    /** \see pushAll
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    gsize pushAllCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
            guint64 timeoutMicro = 0ULL);
    /// push elements into queue from OpenCL Buffer
    /** \see push
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of pushed elements
     */
    gsize pushCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
            guint64 timeoutMicro = 0ULL);
    /// try to push elements into queue from OpenCL Buffer
    /** \see tryPush
     * \param elemsNum number of the elements to push
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \return number of pushed elements
     */
    gsize tryPushCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf);
    
    /// pop all elements from queue into OpenCL Buffer
    /** \see popAll
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    gsize popAllCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
            guint64 timeoutMicro = 0ULL);
    /// pop elements from queue into OpenCL Buffer
    /** \see pop
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \param timeoutMicro timeout in microseconds
     * \return number of popped elements
     */
    gsize popCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf,
            guint64 timeoutMicro = 0ULL);
    /// try to pop elements from queue into OpenCL Buffer
    /** \see tryPop
     * \param elemsNum number of the elements to pop
     * \param cmdQueue an OpenCL command queue object
     * \param buf an OpenCL buffer
     * \return number of popped elements
     */
    gsize tryPopCLC(gsize elemsNum, cl_command_queue cmdQueue, cl_mem buf);
#endif
};

};

#endif
