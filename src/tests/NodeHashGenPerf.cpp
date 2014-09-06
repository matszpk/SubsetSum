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

#include <iostream>
#include <glibmm.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unistd.h>
#include <popt.h>
#include <glibmm.h>
#include "../Controllers.h"
#include "../Int128.h"
#include "../NodeHashGen.h"

using namespace Glib;

static guint threadsNum = 1;
static int threadsNumInt = 1;
static guint nodeHashBits = 0;
static int nodeHashBitsInt = 0;
static int nodeHashedNumbers = 0;
static int nodeHashedNumbersInt = 0;

static const poptOption optionsTable[] =
{
    { "threadsNum", 'T', POPT_ARG_INT, &threadsNumInt, 'T',
      "number of threads", "THREADS" },
    { "hashBits", 'S', POPT_ARG_INT, &nodeHashBitsInt, 'S',
      "set node hash bits", "BITS" },
    { "hashedNums", 'h', POPT_ARG_INT, &nodeHashedNumbersInt, 'h',
      "set node hashed numbers", "NUMBERS" },
    POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0 }
};

static const char* problemNumbersTbl[] =
{
    "-131283481077268755",
    "136889843822247074",
    "-568433792372202447",
    "-349752338390282713",
    "515822946853842783",
    "297176960941695799",
    "33607046693814508",
    "106997031701884345",
    "160360770983473270",
    "-176059912948443769",
    "-469759609594344343",
    "-289277648529526666",
    "-477614559409137104",
    "-227036053130939320",
    "-483906215911524114",
    "-400908759249197546",
    "486505402317317344",
    "252175864249706471",
    "-456228533744088140",
    "220389498705375901",
    "385944540662698768",
    "432397639728450704",
    "350714996894085632",
    "-23446014261217207",
    "1442424033775222",
    "-104771366495038092",
    "422039783838300846",
    "64061017451952423",
    "344990748985722037",
    "560449214230881723",
    "288520527255517057",
    "-19607668127427639",
    "390898295078146230",
    "50954269216147225",
    "377919923515602664",
    "168920879391016771",
    "-346687056479987386",
    "-138593043281327717",
    "-147965286165608487",
    "-72917117914910308",
    "312389928190330134",
    "-378616944268934717",
    "122296005698949422",
    "-174787026779563194",
    "-136291783333434321",
    "-556615314443493086",
    "427545399480016315",
    "-332420531608707683",
    "-500881163904376298",
    "558295817042135947",
    "-125869358270674445",
    "150259918777672260",
    "-223651267318079367",
    "125515581821673408",
    "-271321955469855690",
    "90153668296948715",
    "-232345900763345000",
    "-66877144273170611",
    "239287976206147416",
    "343600694210674325",
    "284715971799764850",
    "-24146416229155357",
    "-140695760204100679",
    "122337311786959358",
    NULL
};

static void compareNodeHashGen()
{
    std::vector<Int128> problem;
    for (const char** numberString = problemNumbersTbl; *numberString != NULL;
         numberString++)
         problem.push_back(strtoint128(*numberString));
    
    HashController* hcontroller = new HashController(problem, 1,
                 nodeHashBits, nodeHashedNumbers, false);
    
    Timer timer;
    timer.start();
    hcontroller->generateNodeHash(1);
    timer.stop();
    std::cout << "OrigNodeHashGen Time: " << timer.elapsed() << std::endl;
    
    const guint nodeHashedNumbers = hcontroller->getNodeHashedNumbers();
    const guint nodeHashBits = hcontroller->getNodeHashBits();
    NodeHashGenController* nhgcontroller = new NodeHashGenController(threadsNum,
            nodeHashedNumbers, nodeHashBits, hcontroller->getNodeProblem() + 8, false);
    
    timer.start();
    nhgcontroller->doHashGen();
    timer.stop();
    std::cout << "NHGNodeHashGen Time: " << timer.elapsed() << std::endl;
    
    // check
    const guint hashListsSize = 1UL<<nodeHashedNumbers;
    const guint hashSize = 1UL<<nodeHashBits;
    
    const NodeHashEntry* oHash = hcontroller->getNodeHash();
    const gint64* oHashLists = hcontroller->getNodeHashLists();
    const guint32* oHashSubsets = hcontroller->getNodeHashSubsets();
    
    const NodeHashEntry* nHash = nhgcontroller->getNodeHash();
    const gint64* nHashLists = nhgcontroller->getNodeHashLists();
    const guint32* nHashSubsets = nhgcontroller->getNodeHashSubsets();
    
    for (guint i = 0; i < hashSize; i++)
        if (oHash[i].size != nHash[i].size || oHash[i].poslo != nHash[i].poslo ||
            oHash[i].poshi != nHash[i].poshi)
        {
            std::cout << "hash doesnt match in " << i << "entry: " << 
                "(" << oHash[i].size << "?" << nHash[i].size << ") " <<
                "(" << oHash[i].poslo << "?" << nHash[i].poslo << ") " <<
                "(" << oHash[i].poshi << "?" << nHash[i].poshi << ") " << std::endl;
            break;
        }
    for (guint i = 0; i < hashListsSize; i++)
        if (oHashLists[i] != nHashLists[i])
        {
            std::cout << "hashlists doesnt match in " << i << "entry: " <<
                oHashLists[i] << "?" << nHashLists[i] << std::endl;
            break;
        }
    for (guint i = 0; i < hashListsSize; i++)
        if (oHashSubsets[i] != nHashSubsets[i])
        {
            std::cout << "hashsubsets doesnt match in " << i << "entry: " <<
                oHashSubsets[i] << "?" << nHashSubsets[i] << std::endl;
            break;
        }
    
    delete hcontroller;
    delete nhgcontroller;
}

int main(int argc, const char** argv)
{
    int cmd;
    poptContext optsContext;
    
    Glib::init();
    Glib::thread_init();
    
    threadsNum = threadsNumInt = sysconf(_SC_NPROCESSORS_ONLN);
    
    optsContext = poptGetContext("NodeHashGenPerf", argc, argv, optionsTable, 0);
    
    /* parse options */
    while((cmd = poptGetNextOpt(optsContext)) >= 0);
    
    if (cmd < -1)
    {
        std::cerr << poptBadOption(optsContext, POPT_BADOPTION_NOALIAS) << ": " <<
            poptStrerror(cmd) << std::endl;
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
    
    threadsNum = threadsNumInt;
    nodeHashBits = nodeHashBitsInt;
    nodeHashedNumbers = nodeHashedNumbersInt;
    
    compareNodeHashGen();
    
    return 0;
}
