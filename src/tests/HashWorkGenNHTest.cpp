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
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glibmm.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TextTestRunner.h>
#include "../Int128.h"
#include "../Controllers.h"

using namespace Glib;
using namespace CppUnit;

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

/*
 * hash controller base for testing
 */

class TestHashControllerBase
{
protected:
    guint numbersNum;
    guint nodeNumbersNum;
    Int128 numbers[128];
    
    Int128 hashSortedNumbers[112];
    MinMaxStep hashMinMaxTable[112];
    guint hashOrderTable[112];
    NodeSubset* subsets;
    guint subsetsCount;
public:
    TestHashControllerBase(guint nodeNumbersNum, const std::vector<Int128>& problem);
    ~TestHashControllerBase();
    
    guint getSubsetsCount() const
    { return subsetsCount; }
    
    const NodeSubset* getSubsets() const
    { return subsets; }
};

TestHashControllerBase::TestHashControllerBase(guint nodeNumbersNum,
           const std::vector<Int128>& problem)
{
    this->nodeNumbersNum = nodeNumbersNum;
    this->numbersNum = problem.size();
    std::copy(problem.begin(), problem.end(), numbers);
    
    subsets = new NodeSubset[1U<<(numbersNum-nodeNumbersNum)];
    subsetsCount = 0;
    
    NumberEntry* nentries = new NumberEntry[numbersNum];
    for (guint i = 0; i < numbersNum; i++)
    {
        nentries[i].number = problem[i];
        nentries[i].order = i;
    }
    std::sort(nentries, nentries + numbersNum);
    gint bestRegPos = smallestRangeRegion(numbersNum, nodeNumbersNum, nentries);
    orderNumberRanges(bestRegPos-1, bestRegPos+nodeNumbersNum, numbersNum, nentries,
                      hashSortedNumbers, hashOrderTable, hashMinMaxTable);
    
    delete[] nentries;
}

TestHashControllerBase::~TestHashControllerBase()
{
    delete[] subsets;
}

/*
 * hash controller
 */

class TestHashController: public TestHashControllerBase
{
public:
    TestHashController(guint nodeNumbersNum, const std::vector<Int128>& problem);
    void generateWork();
};

TestHashController::TestHashController(guint nodeNumbersNum,
           const std::vector<Int128>& problem)
    : TestHashControllerBase(nodeNumbersNum, problem)
{
}

void TestHashController::generateWork()
{
    Int128 maxSubset = 1;
    const guint mainNumbersNum = numbersNum-nodeNumbersNum;
    maxSubset <<= mainNumbersNum;
    Int128 sum = 0;
    
    gint rangeIndex = mainNumbersNum-1;
    
    bool skip = false;
    Int128 subset(0);
    
    while (true)
    {
        if (!skip)
        {   /* do not skip */
            while (rangeIndex >= 0 && sum >= hashMinMaxTable[rangeIndex].minValue &&
                sum <= hashMinMaxTable[rangeIndex].maxValue)
                rangeIndex--;
            
            if (rangeIndex == -1)
            {   // do push node */
                subsets[subsetsCount++] = NodeSubset(sum, subset);
                /*std::cout << "T: " << int128tostr(subset) << "," <<
                    int128tostr(sum) << std::endl;*/
                rangeIndex++;
            }
        }
        
        while ((subset & (Int128(1)<<rangeIndex)) && rangeIndex < gint(mainNumbersNum))
        {
            subset &= ~(Int128(1)<<rangeIndex);
            sum -= hashSortedNumbers[rangeIndex];
            rangeIndex++;
        }
        if (rangeIndex >= gint(mainNumbersNum))
            break; //end!!!!

        subset |= (Int128(1)<<rangeIndex);
        sum += hashSortedNumbers[rangeIndex];
        
        /* check sum value */
        {
            Int128 xbit = 1;
            Int128 checkSum = 0;
            for (guint x = 0; x < mainNumbersNum; x++, xbit<<=1)
                if (xbit&subset)
                    checkSum += hashSortedNumbers[x];
            if (sum != checkSum)
            {
                std::cerr << "checksum failed in " << int128tostr(subset) <<
                        ", sum:" << int128tostr(sum) <<
                        ", checksum: " <<int128tostr(checkSum) << std::endl;
            }
        }
    }
}

/*
 * hash controller reference
 */

class TestHashControllerRef: public TestHashControllerBase
{
public:
    TestHashControllerRef(guint nodeNumbersNum, const std::vector<Int128>& problem);
    
    void generateWork();
};

TestHashControllerRef::TestHashControllerRef(guint nodeNumbersNum,
           const std::vector<Int128>& problem)
        : TestHashControllerBase(nodeNumbersNum, problem)
{
}

void TestHashControllerRef::generateWork()
{
    Int128 maxSubset = 1;
    const guint mainNumbersNum = numbersNum-nodeNumbersNum;
    maxSubset <<= mainNumbersNum;
    Int128 sum = 0;
    Int128 prevSubset = 0;
    
    for (Int128 subset = 0; subset != maxSubset; ++subset)
    {
        Int128 bit = Int128(1);
        guint bitNum;
        const Int128 changes = prevSubset^subset;
        for (bitNum = 0; (bit & changes) && bitNum < mainNumbersNum; bit<<=1, bitNum++)
            if (subset&bit) // subset=1,prevSubset=0
                sum += hashSortedNumbers[bitNum];
            else // subset=0,prevSubset=1
                sum -= hashSortedNumbers[bitNum];
        
        if (sum >= hashMinMaxTable[0].minValue && sum <= hashMinMaxTable[0].maxValue)
        {
            /*std::cout << "R: " << int128tostr(subset) << "," <<
                    int128tostr(sum) << std::endl;*/
            subsets[subsetsCount++] = NodeSubset(sum, subset);
        }
        
        prevSubset = subset;
    }
}

static const char* workGenTest1[] =
{
    "-3523805087071",
    "-3041114903543",
    "-2518887187661",
    "-2182934400830",
    "-2076288539929",
    "-1904107257269",
    "-1093340343144",
    "-990750566727",
    "-353777389662",
    "-234393610880",
    "795253845080",
    "795584841371",
    "1060951811712",
    "2399454418710",
    "2536102847117",
    "2750232230939",
    "2752129257512",
    "2798685196216",
    "3431454064293",
    "3814474313166",
    "3891551143317",
    "4353457012691",
    NULL
};

static const char* workGenTest2[] =
{
    "-34637897567662",
    "-33382474545189",
    "-31948573965914",
    "-30872299469081",
    "-23987142734231",
    "-22253447831447",
    "-21586226328802",
    "-19296845849364",
    "-7503987483729",
    "-992458777410",
    "4041699271521",
    "6322842175825",
    "6757572299841",
    "9935842279250",
    "15222043178834",
    "17878855682493",
    "26273281124012",
    "30596800774779",
    "33041234677523",
    "33640060634241",
    "33667904367850",
    NULL
};

static const char* workGenTest3[] =
{
    "-34637897567662",
    "-33382474545189",
    "-31948573965914",
    "-872299469081",
    "-87142734231",
    "-3447831447",
    "-226328802",
    "-145849364",
    "-87483729",
    "-8777410",
    "9271521",
    "842175825",
    "3572299841",
    "5842279250",
    "22043178834",
    "178855682493",
    "273281124012",
    "30596800774779",
    "33041234677523",
    "33640060634241",
    "33667904367850",
    NULL
};

static const char* workGenTest4[] =
{
    "-4657",
    "-762",
    "-144",
    "-67",
    "-63",
    "-52",
    "-17",
    "-11",
    "-7",
    "1",
    "2",
    "5",
    "56"
    "134",
    "245",
    "677",
    "1567",
    "2527",
    "3455",
    NULL
};


static const char** workGenTestSuite[4] =
{
    workGenTest1,
    workGenTest2,
    workGenTest3,
    workGenTest4
};

class HashWorkGenNHTest: public TestFixture
{
private:
    TestHashController* testHashCtrl;
    TestHashControllerRef* testHashCtrlRef;
public:
    static TestSuite* suite()
    {
        TestSuite* suite = new TestSuite("HashWorkGenTest");
        suite->addTest(new TestCaller<HashWorkGenNHTest>(
            "testWorkGen", &HashWorkGenNHTest::testWorkGen));
        return suite;
    }
    
    void setUp()
    {
        testHashCtrl = NULL;
        testHashCtrlRef = NULL;
    }
    
    void tearDown()
    {
        if (testHashCtrl != NULL)
            delete testHashCtrl;
        if (testHashCtrlRef != NULL)
            delete testHashCtrlRef;
    }
    
    void checkWorkGen(const char* name, const char** numberStrings)
    {
        guint numbersNum = 0;
        std::vector<Int128> problem;
        for (numbersNum = 0; numberStrings[numbersNum]!=NULL; numbersNum++)
            problem.push_back(strtoint128(numberStrings[numbersNum]));
        
        char buf[128];
        testHashCtrl = new TestHashController(6, problem);
        testHashCtrlRef = new TestHashControllerRef(6, problem);
        
        testHashCtrl->generateWork();
        testHashCtrlRef->generateWork();
        
        const guint subsetsCount = testHashCtrl->getSubsetsCount();
        const guint subsetsCountRef = testHashCtrlRef->getSubsetsCount();
        snprintf(buf, 128, "subsetcount %s", name);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(buf, subsetsCountRef, subsetsCount);
        
        const NodeSubset* subsets = testHashCtrl->getSubsets();
        const NodeSubset* subsetsRef = testHashCtrlRef->getSubsets();
        
        std::cout << "Count: " << subsetsCountRef << "," << subsetsCount << std::endl;
        
        for (guint i = 0; i < subsetsCount; i++)
        {
            if (subsetsRef[i].sum != subsets[i].sum)
            {
                snprintf(buf, 128, "sum %s: [%u]", name, i);
                CPPUNIT_ASSERT_EQUAL_MESSAGE(buf, int128tostr(subsetsRef[i].sum),
                             int128tostr(subsets[i].sum));
            }
            
            if (subsetsRef[i].subset != subsets[i].subset)
            {
                snprintf(buf, 128, "subset %s: [%u]", name, i);
                CPPUNIT_ASSERT_EQUAL_MESSAGE(buf, int128tostr(subsetsRef[i].subset),
                             int128tostr(subsets[i].subset));
            }
        }
    }
    
    void testWorkGen()
    {
        char buf[64];
        for (guint i = 0; i < sizeof(workGenTestSuite)/sizeof(char**); i++)
        {
            snprintf(buf, 63, "%u", i);
            checkWorkGen(buf, workGenTestSuite[i]);
        }
    }
};

int main(int argc, const char** argv)
{
    Glib::init();
    TextTestRunner runner;
    runner.addTest(HashWorkGenNHTest::suite());
    if (!runner.run())
        return 1;
    return 0;
}
