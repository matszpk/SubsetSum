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

using namespace Glib;
using namespace CppUnit;

struct MinMaxStep
{
    Int128 minValue;
    Int128 maxValue;
};

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
    {
        // save range to dest minmax (that range which must be match for next level)
        destMinMax[destPos].maxValue = -minVal;
        destMinMax[destPos].minValue = -maxVal;
        /*std::cout << "T: " << int128tostr(destMinMax[destPos].minValue) << ":" <<
                int128tostr(destMinMax[destPos].maxValue) << std::endl;*/
        
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

static void orderNumberRangesRef(gint startLeftPos, gint startRightPos, guint numbersNum,
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
    {
        // save range to dest minmax (that range which must be match for next level)
        destMinMax[destPos].maxValue = -minVal;
        destMinMax[destPos].minValue = -maxVal;
        /*std::cout << "R: " << int128tostr(destMinMax[destPos].minValue) << ":" <<
                int128tostr(destMinMax[destPos].maxValue) << std::endl;*/
        
        if (left >= 0 && right < gint(numbersNum))
        {
            Int128 minVal1, maxVal1;
            Int128 minVal2, maxVal2;
            rangeForRegion(right-left, numbers + left, minVal1, maxVal1);
            rangeForRegion(right-left, numbers + left+1, minVal2, maxVal2);
            
            Int128 range1 = maxVal1-minVal1;
            Int128 range2 = maxVal2-minVal2;
            if (range1 < range2)
            {
                destNumbers[destPos] = numbers[left].number;
                destOrders[destPos] = numbers[left].order;
                minVal = minVal1;
                maxVal = maxVal1;
                left--;
            }
            else
            {
                destNumbers[destPos] = numbers[right].number;
                destOrders[destPos] = numbers[right].order;
                minVal = minVal2;
                maxVal = maxVal2;
                right++;
            }
        }
        else if (left >= 0)
        {
            destNumbers[destPos] = numbers[left].number;
            destOrders[destPos] = numbers[left].order;
            left--;
            rangeForRegion(right-left-1, numbers + left+1, minVal, maxVal);
        }
        else if (right < gint(numbersNum))
        {
            destNumbers[destPos] = numbers[right].number;
            destOrders[destPos] = numbers[right].order;
            right++;
            rangeForRegion(right-left-1, numbers + left+1, minVal, maxVal);
        }
    }
}

static const char* orderRangesTest1[] =
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

static const char* orderRangesTest2[] =
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

static const char* orderRangesTest3[] =
{
    "-4657",
    "-762",
    "-52",
    "-17",
    "-7",
    "1",
    "2",
    "5",
    "677",
    "1567",
    NULL
};

static const char* orderRangesTest4[] =
{
    "-957",
    "-762",
    "-152",
    "-117",
    "-77",
    "8",
    "9",
    "12",
    "15",
    "18",
    NULL
};

static const char* orderRangesTest5[] =
{
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

static const char* orderRangesTest6[] =
{
    "-34637897567662",
    "-33382474545189",
    "-31948573965914",
    "-30872299469081",
    "-23987142734231",
    "-22253447831447",
    "-21586226328802",
    "-19296845849364",
    NULL
};

static const char* orderRangesTest7[] =
{
    "-226328802",
    "-145849364",
    "-87483729",
    "-8777410",
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

static const char* orderRangesTest8[] =
{
    "-34637897567662",
    "-33382474545189",
    "-31948573965914",
    "-30872299469081",
    "-23987142734231",
    "-22253447831447",
    "-21586226328802",
    "-19296845849364",
    "842175825",
    "3572299841",
    "5842279250",
    "22043178834",
    NULL
};

static const char** orderRangesTestSuite[8] =
{
    orderRangesTest1,
    orderRangesTest2,
    orderRangesTest3,
    orderRangesTest4,
    orderRangesTest5,
    orderRangesTest6,
    orderRangesTest7,
    orderRangesTest8
};

class OrderRangesTest: public TestFixture
{
private:
    NumberEntry* nentries;
    Int128* expectedNumbers;
    MinMaxStep* expectedMinMaxes;
    guint* expectedOrders;
    Int128* resultNumbers;
    guint* resultOrders;
    MinMaxStep* resultMinMaxes;
public:
    static TestSuite* suite()
    {
        TestSuite* suite = new TestSuite("SmallestRegionTest");
        suite->addTest(new TestCaller<OrderRangesTest>(
            "testOrderRanges", &OrderRangesTest::testOrderRanges));
        return suite;
    }
    
    void setUp()
    {
        nentries = new NumberEntry[1024];
        expectedNumbers = new Int128[1024];
        resultNumbers = new Int128[1024];
        expectedOrders = new guint[1024];
        resultOrders = new guint[1024];
        expectedMinMaxes = new MinMaxStep[1024];
        resultMinMaxes = new MinMaxStep[1024];
    }
    
    void tearDown()
    {
        delete[] nentries;
        delete[] expectedNumbers;
        delete[] expectedOrders;
        delete[] expectedMinMaxes;
        delete[] resultNumbers;
        delete[] resultOrders;
        delete[] resultMinMaxes;
    }
    
    void checkOrderRanges(const char* name, const char** numberStrings)
    {
        guint numbersNum = 0;
        for (numbersNum = 0; numberStrings[numbersNum]!=NULL; numbersNum++)
        {
            nentries[numbersNum].number = strtoint128(numberStrings[numbersNum]);
            nentries[numbersNum].order = numbersNum;
        }
        char buf[128];
        for (gint left = -1; left < gint(numbersNum)-2; left++)
            for (gint right = left+2; right <= gint(numbersNum); right++)
            {
                orderNumberRangesRef(left, right, numbersNum, nentries,
                         expectedNumbers, expectedOrders, expectedMinMaxes);
                orderNumberRanges(left, right, numbersNum, nentries,
                         resultNumbers, resultOrders, resultMinMaxes);
                
                for (guint i = 0; i < numbersNum; i++)
                {
                    if (expectedOrders[i] != resultOrders[i])
                    {
                        snprintf(buf, 128, "order %s: left=%d,right=%d [%u]",
                                 name, left, right, i);
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(buf,
                                 expectedOrders[i], resultOrders[i]);
                    }
                    if (expectedNumbers[i] != resultNumbers[i])
                    {
                        snprintf(buf, 128, "number %s: left=%d,right=%d [%u]",
                                 name, left, right, i);
                        CPPUNIT_FAIL(buf);
                    }
                    if (expectedMinMaxes[i].minValue != resultMinMaxes[i].minValue ||
                        expectedMinMaxes[i].maxValue != resultMinMaxes[i].maxValue)
                    {
                        snprintf(buf, 128, "minmax %s: left=%d,right=%d [%u]",
                                 name, left, right, i);
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(buf,
                            int128tostr(expectedMinMaxes[i].minValue)+" "+
                                int128tostr(expectedMinMaxes[i].maxValue),
                            int128tostr(resultMinMaxes[i].minValue)+" "+
                                int128tostr(resultMinMaxes[i].maxValue));
                    }
                }
            }
    }
    
    void testOrderRanges()
    {
        char buf[64];
        for (guint i = 0; i < sizeof(orderRangesTestSuite)/sizeof(char**); i++)
        {
            snprintf(buf, 64, "%u", i);
            checkOrderRanges(buf, orderRangesTestSuite[i]);
        }
    }
};

int main(int argc, const char** argv)
{
    Glib::init();
    TextTestRunner runner;
    runner.addTest(OrderRangesTest::suite());
    if (!runner.run())
        return 1;
    return 0;
}
