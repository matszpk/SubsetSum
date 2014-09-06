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

//#undef HAVE_POSIX_MEMALIGN
//#undef HAVE_MEMALIGN

#include <algorithm>
#include <cstdlib>
// MSVC need standard header for new which throw exception
#include <new>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <glibmm.h>
#include <glib/gstdio.h>
#ifdef HAVE_OPENCL
#include <sstream>
#include <CL/cl.hpp>
#endif
#include "Util.h"

using namespace MyComp;
using namespace Glib;

MyComp::Exception::Exception()
{
}

MyComp::Exception::Exception(const ustring& msg)
{
    this->message = msg;
}

MyComp::Exception::Exception(const Glib::ustring& msg, const Glib::Error& glibError)
{
    this->message = msg + ": " + glibError.what();
}

#ifdef HAVE_OPENCL
MyComp::Exception::Exception(const ustring& msg, const cl::Error& clError)
{
    std::ostringstream oss;
    oss << msg << ". CLError: ";
    oss << clError.what() << ". Error number: " << clError.err();
    oss.flush();
    this->message = oss.str();
}

MyComp::Exception MyComp::Exception::createCLError(
                const Glib::ustring& msg, cl_int clError)
{
    std::ostringstream oss;
    oss << msg << ". CLError: ";
    oss << "OpenCL Error. " << ". Error number: " << clError;
    oss.flush();
    return MyComp::Exception(oss.str());
}
#endif

MyComp::Exception::~Exception() throw()
{
}

const char* MyComp::Exception::what() const throw()
{
    return message.c_str();
}


/*
 * alignedMalloc - allocate aligned memory area
 */
void* MyComp::alignedMalloc(gsize alignment, gsize size)
{
    void* ret;
    if (posix_memalign(&ret, alignment, size) == 0)
        return ret;
    return NULL;
}

/*
 * alignedFree- allocate aligned memory area
 */
void MyComp::alignedFree(void* ptr)
{
    ::free(ptr);
}

/* virtual destructor for refcountable */
RefCountable::~RefCountable()
{
}

const TimeVal MyComp::getAfterCurrentTime(guint64 timeoutMicro)
{
    TimeVal deadline;
    const guint64 tmoutSeconds = timeoutMicro/1000000ULL;
    const TimeVal period(tmoutSeconds, timeoutMicro-tmoutSeconds*1000000ULL);
    deadline.assign_current_time();
    deadline += period;
    return deadline;
}

/*
 * SimpleQueue
 */

SimpleQueueBase::SimpleQueueBase(gsize elemsNum)
{
    if (elemsNum == 0)
        throw MyComp::Exception("SimpleQueueBase: ElemsNum must be non-zero");
    
    this->elemsNum = elemsNum;
    readPos = 0;
    writePos = 0;
    cancelledQueue = 0;
}

SimpleQueueBase::~SimpleQueueBase()
{
}

void SimpleQueueBase::getResizeSizes(gsize newElemsNum, gsize& toCopy, gsize& firstCopySize)
{
    toCopy = std::min(writePos-readPos, newElemsNum);
    firstCopySize = std::min(toCopy, elemsNum-readPos);
}

void SimpleQueueBase::finishResize(gsize newElemsNum)
{
    const gsize used = writePos-readPos;
    const gsize toCopy = std::min(used, newElemsNum);
    
    writePos = toCopy;
    readPos = 0;
    if (newElemsNum > elemsNum && used == elemsNum)
        pushCond.broadcast(); // if can push new data after resizing
    
    elemsNum = newElemsNum;
}


bool SimpleQueueBase::waitForPush(guint64 timeoutMicro)
{
    TimeVal deadline;
    if (timeoutMicro != 0ULL)
        deadline = getAfterCurrentTime(timeoutMicro);
    
    if (cancelledQueue != 0)
        return false; // cancelled
    while (readPos+elemsNum == writePos)
    {
        if (timeoutMicro != 0ULL)
        {
            if (!pushCond.timed_wait(mutex, deadline))
                return false;
        }
        else // waits infinitely
            pushCond.wait(mutex);
        
        if (cancelledQueue != 0)
            return false; // cancelled
    }
    return true;
}

void SimpleQueueBase::afterPush()
{
    const gsize oldWritePos = writePos;
    writePos++;
    
    if (oldWritePos == readPos)
        popCond.broadcast(); // inform that queue have data
}

bool SimpleQueueBase::waitForPop(guint64 timeoutMicro)
{
    TimeVal deadline;
    if (timeoutMicro != 0ULL)
        deadline = getAfterCurrentTime(timeoutMicro);
    
    if (cancelledQueue != 0)
        return false; // cancelled
    while (readPos == writePos)
    {
        if (timeoutMicro != 0ULL)
        {
            if (!popCond.timed_wait(mutex, deadline))
                return false;
        }
        else // waits infinitely
            popCond.wait(mutex);
        
        if (cancelledQueue != 0)
            return false; // cancelled
    }
    return true;
}

void SimpleQueueBase::afterPop()
{
    const gsize oldReadPos = readPos;
    readPos++;
    
    if (oldReadPos + elemsNum == writePos)
        pushCond.broadcast(); // inform when queue have free space
    
    if (readPos == elemsNum)
    {   // fix positions
        readPos -= elemsNum;
        writePos -= elemsNum;
    }
}

void SimpleQueueBase::clearPositionsAndInform()
{
    readPos = 0;
    writePos = 0;
    
    // inform pusher that queue have empty space
    pushCond.broadcast();
}

void SimpleQueueBase::cancel()
{
    Mutex::Lock lock(mutex);
    g_atomic_int_set(&cancelledQueue, 1);
    // inform pushers and poppers
    pushCond.broadcast();
    popCond.broadcast();
}

