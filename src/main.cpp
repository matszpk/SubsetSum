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

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <string>
#include <inttypes.h>
#include <unistd.h>
#include <popt.h>
#include <glibmm.h>
#if GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<32
#include <glib/gatomic.h>
#endif
#ifdef HAVE_OPENCL
#include <CL/cl.hpp>
#endif
#include "Int128.h"
#include "Queue.h"
#include "PushPop.h"
#include "Controllers.h"
#include "NaiveWorkers.h"
#include "HashWorkers.h"

using namespace Glib;

static guint threadsNum = 1;
static int threadsNumInt = 1;
static guint nodeHashBits = 0;
static int nodeHashBitsInt = 0;
static int nodeHashedNumbers = 0;
static int nodeHashedNumbersInt = 0;
#ifdef HAVE_OPENCL
static int useGPU = 0;
static int useOnlyGPU = 0;
static int useOnlyCPUCL = 0;
static int useHashGroupping = 0;
static int use64BitNaiveCL = 0;
#endif
#if defined(HAVE_SSE2) || defined(HAVE_SSE4)
static int useInsnExtensions = 0;
#endif
static int useMethod = 0;
static int useHashSubset = 0;

static const poptOption optionsTable[] =
{
    { "threadsNum", 'T', POPT_ARG_INT, &threadsNumInt, 'T',
      "number of threads", "THREADS" },
#ifdef HAVE_OPENCL
    { "useGPU", 'G', POPT_ARG_VAL, &useGPU, 'G',
      "use GPU", NULL },
    { "useOnlyGPU", 'H', POPT_ARG_VAL, &useOnlyGPU, 'H',
      "use only GPU", NULL },
    { "useOnlyCPUCL", 'P', POPT_ARG_VAL, &useOnlyCPUCL, 'P',
      "use only CPU OpenCL Device", NULL },
    { "hashGroupping", 'X', POPT_ARG_VAL, &useHashGroupping, 'X',
      "use hash groupping to accelerate on GPU", NULL },
    { "use64BitCL", 0, POPT_ARG_VAL, &use64BitNaiveCL, 0x100,
      "use 64 bit code in OpenCL in naive method", NULL },
#endif
    { "naive", 'N', POPT_ARG_VAL, &useMethod, 'N',
      "use naive method", NULL },
    { "hashBits", 'S', POPT_ARG_INT, &nodeHashBitsInt, 'S',
      "set node hash bits", "BITS" },
    { "hashedNums", 'h', POPT_ARG_INT, &nodeHashedNumbersInt, 'h',
      "set node hashed numbers", "NUMBERS" },
    { "hashSubset", 'Y', POPT_ARG_VAL, &useHashSubset, 'Y',
      "use only hash subsets", NULL },
#if defined(HAVE_SSE2) || defined(HAVE_SSE4)
    { "useStdCode", 0, POPT_ARG_VAL, &useInsnExtensions, '1',
      "do not use CPU extensions", NULL },
#endif
#ifdef HAVE_SSE2
    { "useSSE2", 0, POPT_ARG_VAL, &useInsnExtensions, '2',
      "use SSE2 extensions", NULL },
#ifdef HAVE_SSE4
    { "useSSE4.1", 0, POPT_ARG_VAL, &useInsnExtensions, '4',
      "use SSE4.1 extensions", NULL },
#endif
#endif
    POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0 }
};

#ifdef HAVE_OPENCL
struct CLDeviceStruct
{
    cl::Platform platform;
    cl::Device device;
    
    CLDeviceStruct() { }
    CLDeviceStruct(const cl::Platform& platform, const cl::Device& device)
    {
        this->platform = platform;
        this->device = device;
    }
};

static std::vector<CLDeviceStruct> allCLDevices;

static void initOpenCL()
{
    std::vector<cl::Platform> clPlatforms;
    cl::Platform::get(&clPlatforms);
    
    for (guint i = 0; i < clPlatforms.size(); i++)
    {
        std::vector<cl::Device> clDevices;
        clPlatforms[i].getDevices(CL_DEVICE_TYPE_ALL, &clDevices);
        for (guint j = 0; j < clDevices.size(); j++)
            allCLDevices.push_back(CLDeviceStruct(clPlatforms[i], clDevices[j]));
    }
}

void getAllOpenCLDevicesByType(cl_device_type choosenDeviceType,
                   std::vector<CLDeviceStruct>& outCLDevices)
{
    outCLDevices.clear();
    bool usedCpu = false;
    for (guint j = 0; j < allCLDevices.size(); j++)
    {
        CLDeviceStruct& deviceStruct = allCLDevices[j];
        cl_device_type devType;
        deviceStruct.device.getInfo(CL_DEVICE_TYPE, &devType);
        if ((devType & choosenDeviceType) == choosenDeviceType)
        {
            if (choosenDeviceType == CL_DEVICE_TYPE_CPU && usedCpu)
                continue; // ignore next CPUs
            outCLDevices.push_back(deviceStruct);
            if (choosenDeviceType == CL_DEVICE_TYPE_CPU)
                usedCpu = true;
        }
    }
}
#endif

static void loadProblem(const std::string& problemFilename, std::vector<Int128>& problem,
            std::string& solFilename)
{
    problem.clear();
    RefPtr<IOChannel> channel = IOChannel::create_from_file(problemFilename, "r");
    channel->set_encoding("");
    
    IOStatus status = IO_STATUS_NORMAL;
    guint lineno = 1;
    while (status == Glib::IO_STATUS_NORMAL)
    {
        ustring line;
        status = channel->read_line(line);
        std::string sline = line;
        gsize linesize = sline.size();
        for (gsize i = 0; i < linesize && i != std::string::npos;)
            if (!Ascii::isspace(sline[i]))
            {
                gsize newPos = sline.find_first_of(" \t\n\r\v\f", i);
                std::string sub;
                if (newPos != std::string::npos)
                    sub = sline.substr(i, newPos-i);
                else
                    sub = sline.substr(i);
                // try to parse
                try
                {
                    problem.push_back(strtoint128(sub));
                }
                catch(Int128Exception& ex)
                {
                    std::ostringstream oss;
                    oss << "Parse error: " << problemFilename << ":line " << lineno <<
                            ": " << ex.what();
                    oss.flush();
                    throw SubsetSumException(oss.str());
                }
                i = newPos;
            }
            else i++;
        lineno++;
    }
    
    if (problem.empty())
        throw SubsetSumException("Problem must not be empty!");
    if (std::find(problem.begin(), problem.end(), Int128(0)) != problem.end())
        throw SubsetSumException("Set must not have zero elements!");
    
    Int128 msum = 0;
    Int128 psum = 0;
    for (guint i = 0; i < problem.size(); i++)
        if (problem[i] > Int128(0))
        {
            psum += problem[i];
            if (psum <= Int128(0))
                throw SubsetSumException("Sum of all positive values is out of range");
        }
        else if (problem[i] < Int128(0))
        {
            msum -= problem[i];
            if (msum <= Int128(0))
                throw SubsetSumException("Sum of all negative values is out of range");
        }
    
    solFilename.clear();
    size_t extPos = problemFilename.find_last_of('.');
    if (extPos != ustring::npos)
        solFilename = problemFilename.substr(0, extPos);
    else
        solFilename = problemFilename;
    solFilename += ".sol";
}

static volatile gint progressCancel = 0;

static void progressThreadFunc(SubsetSumController* controller)
{
    while (g_atomic_int_get(&progressCancel)==0)
    {
        std::cout << controller->getProgressString() << "\r";
        std::cout.flush();
        Glib::usleep(100000ULL);
    }
}

class SolutionSaver
{
private:
    SubsetSumController* controller;
    std::string solFilename;
    std::vector<Int128> problem;
public:
    SolutionSaver(SubsetSumController* controller, const std::string& solFilename,
            const std::vector<Int128> problem)
    {
        this->controller = controller;
        this->solFilename = solFilename;
        this->problem = problem;
    }
    
    void run()
    {
        Int128 solution;
        const gsize problemSize = problem.size();
        for (guint64 i = 0; controller->getSolution(solution); i++)
        {
            Int128 bit = 1;
            Int128 sum = 0;
            for (guint x = 0; x < problemSize; x++, bit<<=1)
                if (bit&solution)
                    sum += problem[x];
            if (sum || !solution)
            {   // no-zero
                std::cerr << "Solver sent incorrect solution! ("
                        << i << ")\n";
                bit = 1;
                for (guint x = 0; x < problemSize; x++, bit<<=1)
                    if (bit&solution)
                        std::cerr << int128tostr(problem[x]) << "\n";
                
                std::cerr << "Combination bits:\n";
                bit = 1;
                for (guint x = 0; x < problemSize; x++, bit<<=1)
                    if (bit&solution)
                        std::cerr << "1";
                    else
                        std::cerr << "0";
                
                std::cerr.flush();
                continue;
            }
            
            RefPtr<IOChannel> outChannel = IOChannel::create_from_file(solFilename, "a");
            
            char buf[64];
            snprintf(buf, 64, "%"PRIu64, i);
            
            outChannel->write("Solution: ");
            outChannel->write(buf);
            outChannel->write('\n');
            outChannel->flush();
            bit = 1;
            for (guint x = 0; x < problemSize; x++, bit<<=1)
                if (bit&solution)
                {
                    snprintf(buf, 64, "%u: ", x);
                    outChannel->write(buf);
                    outChannel->write(int128tostr(problem[x]));
                    outChannel->write('\n');
                }
        }
    }
};

static void solveProblem(const std::string& solFilename,
             const std::vector<Int128>& problem)
{
    progressCancel = 0;
    Timer totalTimer;
    Timer timer;
    totalTimer.start();
    
    ::remove(solFilename.c_str());
    
    guint cpuWorkersNum = 0;
    guint totalWorkersNum = 0;
    gsize memQueueSize = 0;
    
    /// cpu workers
    NaiveCpuWorker* cpuWorkers = NULL;
    HashCpuWorker* hashCpuWorkers = NULL;
#ifdef HAVE_OPENCL
    if (!useOnlyCPUCL && !useOnlyGPU)
#endif
    {
        if (useMethod == 'N')
            cpuWorkers = new NaiveCpuWorker[threadsNum];
        else
            hashCpuWorkers = new HashCpuWorker[threadsNum];
        totalWorkersNum = threadsNum;
        cpuWorkersNum = threadsNum;
    }
#ifdef HAVE_OPENCL
    std::vector<CLDeviceStruct> choosenCLDevices;
    // OpenCL Workers
    if (useOnlyCPUCL)
        getAllOpenCLDevicesByType(CL_DEVICE_TYPE_CPU, choosenCLDevices);
    else if (useGPU || useOnlyGPU)
        getAllOpenCLDevicesByType(CL_DEVICE_TYPE_GPU, choosenCLDevices);
    if ((useGPU || useOnlyGPU || useOnlyCPUCL) && choosenCLDevices.empty())
        std::cout << "WARNING: No OpenCL devices found." << std::endl;
    totalWorkersNum += choosenCLDevices.size();
    
    NaiveOpenCLWorker* openCLWorkers = NULL;
    HashOpenCLWorker* hashOpenCLWorkers = NULL;
    if (!choosenCLDevices.empty())
    {
        if (useMethod == 'N')
            openCLWorkers = new NaiveOpenCLWorker[choosenCLDevices.size()];
        else
            hashOpenCLWorkers = new HashOpenCLWorker[choosenCLDevices.size()];
    }
#endif
    
    if (totalWorkersNum==0)
        throw SubsetSumException("No workers to solve problem!");
    
    if (useMethod == 'N')
        std::cout << "Using naive method" << std::endl;
    else
        std::cout << "Using hash method" << std::endl;
    
    SubsetSumController* controller = NULL;
    if (useMethod == 'N')
        controller = new NaiveController(problem, totalWorkersNum);
    else // hash method
    {
        HashController* hcontroller;
        controller = hcontroller = new HashController(problem, totalWorkersNum,
                    nodeHashBits, nodeHashedNumbers, useHashSubset!=0);
        std::cout << "Generating node hash" << std::endl;
        Timer nhTimer;
        nhTimer.start();
        hcontroller->generateNodeHash(threadsNum);
        nhTimer.stop();
        std::cout << "NodeHashGen Time: " << nhTimer.elapsed() << std::endl;
    }
    // solution saver
    SolutionSaver solSaver(controller, solFilename, problem);
    Thread* solSaverThread = Thread::create(
            sigc::mem_fun(&solSaver, &SolutionSaver::run), true);
    
    Thread** workerThreads = new Thread*[totalWorkersNum];
    if (useMethod == 'N')
    {
        for (guint i = 0; i < cpuWorkersNum; i++)
        {
#ifdef HAVE_SSE2
            if (useInsnExtensions == '1')
                cpuWorkers[i].setInsnExts(INSN_EXT_NONE);
            else if (useInsnExtensions == '2')
                cpuWorkers[i].setInsnExts(INSN_EXT_SSE2);
#    ifdef HAVE_SSE4
            else if (useInsnExtensions == '4')
                cpuWorkers[i].setInsnExts(INSN_EXT_SSE4_1);
#    endif
#  endif
            cpuWorkers[i].init(static_cast<NaiveController*>(controller), i);
            memQueueSize += cpuWorkers[i].getMemQueueSize();
        }
        
#ifdef HAVE_OPENCL
        for (guint i = 0; i < choosenCLDevices.size(); i++)
        {
            openCLWorkers[i].init(static_cast<NaiveController*>(controller),
                      choosenCLDevices[i].platform, choosenCLDevices[i].device,
                      use64BitNaiveCL);
            memQueueSize += openCLWorkers[i].getMemQueueSize();
        }
#endif
        // initialize memqueue
        controller->initMemQueue(memQueueSize);

        for (guint i = 0; i < cpuWorkersNum; i++)
            workerThreads[i] = Thread::create(
                sigc::mem_fun(cpuWorkers+i, &NaiveCpuWorker::run), true);
#ifdef HAVE_OPENCL
        for (guint i = 0; i < choosenCLDevices.size(); i++)
            workerThreads[i+cpuWorkersNum] = Thread::create(
                sigc::mem_fun(openCLWorkers+i, &NaiveOpenCLWorker::run), true);
#endif
    }
    else
    {
        for (guint i = 0; i < cpuWorkersNum; i++)
        {
            hashCpuWorkers[i].init(static_cast<HashController*>(controller), i);
            memQueueSize += hashCpuWorkers[i].getMemQueueSize();
        }
#ifdef HAVE_OPENCL
        for (guint i = 0; i < choosenCLDevices.size(); i++)
        {
            hashOpenCLWorkers[i].init(static_cast<HashController*>(controller),
                      choosenCLDevices[i].platform, choosenCLDevices[i].device,
                      useHashGroupping!=0);
            memQueueSize += hashOpenCLWorkers[i].getMemQueueSize();
        }
#endif
        // initialize memqueue
        controller->initMemQueue(memQueueSize);
        
        for (guint i = 0; i < cpuWorkersNum; i++)
            workerThreads[i] = Thread::create(
                sigc::mem_fun(hashCpuWorkers+i, &HashCpuWorker::run), true);
#ifdef HAVE_OPENCL
        for (guint i = 0; i < choosenCLDevices.size(); i++)
            workerThreads[i+cpuWorkersNum] = Thread::create(
                sigc::mem_fun(hashOpenCLWorkers+i, &HashOpenCLWorker::run), true);
#endif
    }
    // progress
    Thread* progressThread = Thread::create(sigc::bind(
            sigc::ptr_fun(&progressThreadFunc), controller), true);
    
    timer.start();
    controller->generateWork();
    // end of computations
    for (guint i = 0; i < totalWorkersNum; i++)
        workerThreads[i]->join();
    
    controller->finish();
    timer.stop();
    
    g_atomic_int_set(&progressCancel, 1);
    progressThread->join();
    solSaverThread->join();
    
    totalTimer.stop();
    
    std::cout << "\nTime: " << timer.elapsed() << "s" <<
        "\nTotal Time: " << totalTimer.elapsed() << "s" << std::endl;
    std::cout << "\nFound " << controller->getSolutionsNum() << " solutions after ";
    if (controller->getMNodesCount() != 0)
        std::cout << controller->getMNodesCount() << std::setfill('0') <<
            std::setw(6) << controller->getSubMegaCount() << " Nodes!" << std::endl;
    else // 
        std::cout << controller->getSubMegaCount() << " Nodes!" << std::endl;
    
    if (controller->getSolutionsNum() != 0)
        std::cout << "Solutions available in " << solFilename << std::endl;
    
    if (cpuWorkers != NULL)
        delete[] cpuWorkers;
    if (hashCpuWorkers != NULL)
        delete[] hashCpuWorkers;
    
    delete[] workerThreads;
#ifdef HAVE_OPENCL
    if (openCLWorkers != NULL)
        delete[] openCLWorkers;
    if (hashOpenCLWorkers != NULL)
        delete[] hashOpenCLWorkers;
#endif
    if (controller != NULL)
        delete controller;
}

int main(int argc, const char** argv)
{
    int cmd;
    poptContext optsContext;
    
    Glib::init();
    Glib::thread_init();
    
    threadsNum = threadsNumInt = sysconf(_SC_NPROCESSORS_ONLN);
    
    optsContext = poptGetContext("SubsetSum", argc, argv, optionsTable, 0);
    
    /* parse options */
    while((cmd = poptGetNextOpt(optsContext)) >= 0);
    
    if (cmd < -1)
    {
        std::cerr << poptBadOption(optsContext, POPT_BADOPTION_NOALIAS) << ": " <<
            poptStrerror(cmd) << std::endl;
        return 1;
    }
    
    const char** leftOverArgs = poptGetArgs(optsContext);
    if (leftOverArgs==NULL)
    {
        poptPrintUsage(optsContext, stdout, 0);
        return 1;
    }
    
    if (threadsNumInt <= 0)
    {
        std::cerr << "Threads number must be positive" << std::endl;
        return 1;
    }
    if (nodeHashBitsInt < 0 || nodeHashBitsInt >= 64)
    {
        std::cerr << "NodeHashBits must be in range: 0-64" << std::endl;
        return 1;
    }
    if (nodeHashedNumbersInt < 0 || nodeHashedNumbersInt >= 64)
    {
        std::cerr << "NodeHashedNumbers must be in range: 0-64" << std::endl;
        return 1;
    }
    
    
    nodeHashBits = nodeHashBitsInt;
    nodeHashedNumbers = nodeHashedNumbersInt;
    
    int retValue = 0;

#ifdef HAVE_OPENCL
    try
    {
        initOpenCL();
    }
    catch(cl::Error& ex)
    {
        std::cerr << "Error encountered (OpenCL): " << ex.what() << std::endl;
        return 1;
    }
#endif
    
    while(*leftOverArgs != NULL)
    {
        try
        {
            std::vector<Int128> problem;
            std::string solFilename;
            loadProblem(*leftOverArgs, problem, solFilename);
            
            solveProblem(solFilename, problem);
        }
        catch(Glib::Exception& ex)
        {
            std::cerr << "Error encountered (Glib): " << ex.what() << std::endl;
            retValue = 1;
        }
#ifdef HAVE_OPENCL
        catch(cl::Error& ex)
        {
            std::cerr << "Error encountered (OpenCL): " << ex.what() << std::endl;
            retValue = 1;
        }
#endif
        catch(std::exception& ex)
        {
            std::cerr << "Error encountered: " << ex.what() << std::endl;
            retValue = 1;
        }
        leftOverArgs++;
    }
    
    return retValue;
}
