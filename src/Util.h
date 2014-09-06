/*! \file Util.h
 * \brief small utilities
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

#ifndef __MYCOMPSTREAMER_UTIL_H__
#define __MYCOMPSTREAMER_UTIL_H__

#include <string>
#include <vector>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif

/** main namespace
 */
namespace MyComp
{

/// An exception class for MyCompStreamer
/**
 * An exception class, used to throw exception from MyComp code.
 */
class Exception: public std::exception
{
private:
    Glib::ustring message;
public:
    /** empty constructor */
    Exception();
    /** basic constructor
     * \param msg message string in UTF-8 encoding
     */
    explicit Exception(const Glib::ustring& msg);
    /**
     * \param msg message string in UTF-8 encoding
     * \param glibError Glib error object
     */
    Exception(const Glib::ustring& msg, const Glib::Error& glibError);
#ifdef HAVE_OPENCL
    /** basic constructor for OpenCL error
     * \param msg message string in UTF-8 encoding
     * \param clError OpenCL error object
     */
    Exception(const Glib::ustring& msg, const cl::Error& clError);
    
    /** creates MyComp::Exception from OpenCL error
     * \param msg message string int UTF-8 encoding
     * \param clError OpenCL error code
     * \return MyComp exception
     */
    static MyComp::Exception createCLError(const Glib::ustring& msg, cl_int clError);
#endif
    /** destructor
     */
    virtual ~Exception() throw();
    
    /// get an exception message
    /** get an exception message as C-style string
     * \return message as C-style string
     */
    virtual const char* what() const throw();
};
   
/// allocates aligned memory
/** allocates aligned memory to specified aligmnemt
 * \param alignment specified alignment should be power of 2
 * \param size size in bytes for memory block and shouldn't be zero
 * \return pointer to allocated block or NULL if error encountered
 */
extern void* alignedMalloc(gsize alignment, gsize size);


/// frees aligned memory
/** frees aligned memory by using MyComp::alignedMalloc
 * \param ptr pointer to aligned memory block
 */
extern void alignedFree(void* ptr);

/// get time after microseconds
/** get time after microseconds
 * \param microsecs microseconds after current time
 * \return time after microseconds
 */
extern const Glib::TimeVal getAfterCurrentTime(guint64 microsecs);

/// simple thread-safe reference countable object class
class RefCountable
{
private:
    volatile gint refCount;
public:
    /// constructor
    RefCountable()
    { refCount = 1; }
    virtual ~RefCountable();
    
    /// reference object
    void reference()
    { 
        g_atomic_int_inc(&refCount);
    }
    
    /// unreference object (and delete object when no references)
    void unreference()
    { 
        if (g_atomic_int_dec_and_test(&refCount))
            delete this; /* delete this object */
    }
    
    /// get id (used by BufferPool)
    gsize getInternalId() const
    { return gsize(this); }
};

extern const Glib::TimeVal getAfterCurrentTime(guint64 microsecs);

/// simple queue which can holds more than one element
class SimpleQueueBase
{
private:
    // non copyable
    SimpleQueueBase(const SimpleQueueBase& c);
    SimpleQueueBase& operator=(const SimpleQueueBase& c);
protected:
    Glib::Mutex mutex;  ///< for synchronization
    Glib::Cond pushCond;  ///< for synchronization
    Glib::Cond popCond;  ///< for synchronization
    
    gsize elemsNum; ///< number of elements
    
    /// indicator of cancellation (1 - if cancelled)
    volatile gint cancelledQueue;
    
    gsize readPos;  ///< read position on queue
    gsize writePos; ///< write position on queue (can be greater than elemsNum)
    
    /// wait until queue will have free space for a pushing
    bool waitForPush(guint64 timeoutMicro);
    /// finalize a pushing
    void afterPush();
    /// wait until queue will have any elements for a popping
    bool waitForPop(guint64 timeoutMicro);
    /// finalize a popping
    void afterPop();
    
    /// get elements to end of queue of end size
    void getResizeSizes(gsize newElemsNum, gsize& toCopy, gsize& firstCopySize);
    /// finalize a resizing
    void finishResize(gsize newElemsNum);
    
    /// constructor of queue
    SimpleQueueBase(gsize elesmNum);
    
    /// clear queue positions
    void clearPositionsAndInform();
public:
    virtual ~SimpleQueueBase();
    
    /// returns used (pending) elements in queue
    gsize getUsedSize() const
    { return writePos - readPos; }
    
    bool isEmpty()
    {
        Glib::Mutex::Lock lock(mutex);
        return writePos == readPos;
    }
    
    /// cancel any operation on queue
    void cancel();
    
    /// returns true if queue cancelled
    bool isCancelled() const
    {
        return g_atomic_int_get(&cancelledQueue)!=0;
    }
};

/// templated simple queue object, which can handle structures and objects
template<typename T>
class SimpleQueue: public SimpleQueueBase
{
private:
    T* queueData;
public:
    /// constructor
    /**
     * \param elemsNum maximum element number
     */
    SimpleQueue(gsize elemsNum): SimpleQueueBase(elemsNum)
    {
        queueData = new T[elemsNum];
    }
    
    virtual ~SimpleQueue()
    {
        if (queueData != NULL)
            delete[] queueData;
    }
    
    /// resize queue size
    void resize(gsize newElemsNum)
    {
        if (newElemsNum == 0)
            throw MyComp::Exception("SimpleQueue: ElemsNum must be non-zero");
    
        Glib::Mutex::Lock lock(mutex);
        gsize toCopy;
        gsize firstCopySize;
        getResizeSizes(newElemsNum, toCopy, firstCopySize);
        
        T* newData = new T[newElemsNum];
        /* copy to new queue data */
        std::copy(queueData + readPos, queueData + readPos + firstCopySize, newData);
        if (toCopy > firstCopySize)
            std::copy(queueData, queueData + toCopy-firstCopySize,
                      newData + firstCopySize);
        delete[] queueData;
        queueData = newData;
        
        finishResize(newElemsNum);
    }
    
    /// Push element into queue
    /** Push element into queue. Waits if queue is full. If timeout happens
     * return false. If timeout is zero, waits infinitely
     * \param object object to push
     * \param timeoutMicro timeout in microseconds
     * \return true if success, false when timeout happens
     */
    bool push(const T& object, guint64 timeoutMicro = 0ULL)
    {
        Glib::Mutex::Lock lock(mutex);
        if (!waitForPush(timeoutMicro))
            return false;
        /* push object to queue */
        const gsize curWritePos = (writePos < elemsNum) ? writePos : writePos-elemsNum;
        queueData[curWritePos] = object;
        afterPush();
        return true;
    }
    
    /// try to push element into queue if it have free space
    bool tryPush(const T& object)
    {
        Glib::Mutex::Lock lock(mutex);
        if (readPos+elemsNum == writePos || cancelledQueue != 0)
            return false;
        /* push object to queue */
        gsize curWritePos = (writePos < elemsNum) ? writePos : writePos-elemsNum;
        queueData[curWritePos] = object;
        afterPush();
        return true;
    }
    
    /// Pop element from queue and remove it
    /** Pop element from queue and remove it. Wait until queue is empty. If timeout happens
     * return false. If timeout is zero, waits infinitely
     * \param object a popping object
     * \param timeoutMicro timeout in microseconds
     * \return if object is popped
     */
    bool pop(T& object, guint64 timeoutMicro = 0ULL)
    {
        Glib::Mutex::Lock lock(mutex);
        if (!waitForPop(timeoutMicro))
            return false;
        /* pop object from queue */
        object = queueData[readPos];
        queueData[readPos] = T();  // clear object
        afterPop();
        return true;
    }
    
    /// try to push element into queue if it have any elements
    bool tryPop(T& object)
    {
        Glib::Mutex::Lock lock(mutex);
        if (readPos == writePos || cancelledQueue != 0)
            return false;
        /* pop object from queue */
        object = queueData[readPos];
        queueData[readPos] = T(); // clear object
        afterPop();
        return true;
    }
    
    /// clear queue
    void clear()
    {
        Glib::Mutex::Lock lock(mutex);
        std::fill(queueData, queueData + elemsNum, T());
        clearPositionsAndInform();
    }
};

};

#endif
