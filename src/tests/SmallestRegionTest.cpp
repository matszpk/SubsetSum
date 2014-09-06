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
    //std::cout << "T:" << 0 << ": " << int128tostr(minVal) << "," << int128tostr(maxVal) << std::endl;
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
        /* for minmax test */
        Int128 minVal2 = 0;
        Int128 maxVal2 = 0;
        for (guint i = 0; i < regionSize; i++)
        {
            if (numbers[pos+i].number < Int128(0))
                minVal2 += numbers[pos+i].number;
            else if (numbers[pos+i].number > Int128(0))
                maxVal2 += numbers[pos+i].number;
        }
        if (minVal2 == Int128(0))
            minVal2 = numbers[pos].number;
        if (maxVal2 == Int128(0))
            maxVal2 = numbers[pos+regionSize-1].number;
        
        if (minVal != minVal2 || maxVal != maxVal2)
        {   /* if failed! */
            std::cout << pos << ": (" <<
                int128tostr(minVal) << "," << int128tostr(minVal2) <<
                "), " <<int128tostr(maxVal) << "," << int128tostr(maxVal2) << std::endl;
            abort();
        }
        /* */
        
        //std::cout << "T:" << i << ": " << int128tostr(minVal) << "," << int128tostr(maxVal) << std::endl;
        if (range < bestRange)
        {
            bestPos = pos;
            bestRange = range;
        }
    }
    return bestPos;
}

static guint smallestRangeRegionReference(guint numbersNum, guint regionSize,
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
    
    //std::cout << "R:" << 0 << ": " << int128tostr(minVal) << "," << int128tostr(maxVal) << std::endl;
    Int128 range = maxVal-minVal;
    Int128 bestRange = maxVal-minVal;
    guint bestPos = 0;
    for (guint pos = 1; pos < numbersNum-regionSize; pos++)
    {
        Int128 minVal = 0;
        Int128 maxVal = 0;
        for (guint i = 0; i < regionSize; i++)
        {
            if (numbers[pos+i].number < Int128(0))
                minVal += numbers[pos+i].number;
            else if (numbers[pos+i].number > Int128(0))
                maxVal += numbers[pos+i].number;
        }
        if (minVal == Int128(0))
            minVal = numbers[pos].number;
        if (maxVal == Int128(0))
            maxVal = numbers[pos+regionSize-1].number;
        
        range = maxVal-minVal;
        //std::cout << "R:" << pos << ": " << int128tostr(minVal) << "," << int128tostr(maxVal) << std::endl;
        if (range < bestRange)
        {
            bestPos = pos;
            bestRange = range;
        }
    }
    return bestPos;
}

/*
 * smallest region test suite
 */

static const char* smallestRegionTest1[] =
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

static const char* smallestRegionTest2[] =
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

static const char* smallestRegionTest3[] =
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

static const char* smallestRegionTest4[] =
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

static const char* smallestRegionTest5[] =
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

static const char* smallestRegionTest6[] =
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

static const char* smallestRegionTest7[] =
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

static const char* smallestRegionTest8[] =
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


static const char** smallestRegionTestSuite[8] =
{
    smallestRegionTest1,
    smallestRegionTest2,
    smallestRegionTest3,
    smallestRegionTest4,
    smallestRegionTest5,
    smallestRegionTest6,
    smallestRegionTest7,
    smallestRegionTest8
};

class SmallestRegionTest: public TestFixture
{
private:
    NumberEntry* nentries;
public:
    static TestSuite* suite()
    {
        TestSuite* suite = new TestSuite("SmallestRegionTest");
        suite->addTest(new TestCaller<SmallestRegionTest>(
            "testSmallestRegion", &SmallestRegionTest::testSmallestRegion));
        return suite;
    }
    
    void setUp()
    {
        nentries = new NumberEntry[1024];
    }
    
    void tearDown()
    {
        delete[] nentries;
    }
    
    void checkBestRegionPos(const char* name, const char** numberStrings)
    {
        guint numbersNum = 0;
        for (numbersNum = 0; numberStrings[numbersNum]!=NULL; numbersNum++)
            nentries[numbersNum].number = strtoint128(numberStrings[numbersNum]);
        
        char buf[128];
        for (guint regSize = 2; regSize < numbersNum-1; regSize++)
        {
            const guint expectedPos = smallestRangeRegionReference(
                numbersNum, regSize, nentries);
            const guint resultPos = smallestRangeRegion(
                numbersNum, regSize, nentries);
            snprintf(buf, 128, "subset %s, regsize=%u\n", name, regSize);
            CPPUNIT_ASSERT_EQUAL_MESSAGE(buf, expectedPos, resultPos);
        }
    }
    
    void testSmallestRegion()
    {
        char buf[64];
        for (guint i = 0; i < sizeof(smallestRegionTestSuite)/sizeof(char**); i++)
        {
            snprintf(buf, 64, "%u", i);
            checkBestRegionPos(buf, smallestRegionTestSuite[i]);
        }
    }
};

int main(int argc, const char** argv)
{
    Glib::init();
    TextTestRunner runner;
    runner.addTest(SmallestRegionTest::suite());
    if (!runner.run())
        return 1;
    return 0;
}
