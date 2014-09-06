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

#include <string>
#include <algorithm>
#include <exception>
#include "Int128.h"

static const Int128 radix128Tbl[39] =
{
#ifndef HAVE_INT128
    Int128(1ULL,0ULL),
    Int128(10ULL,0ULL),
    Int128(100ULL,0ULL),
    Int128(1000ULL,0ULL),
    Int128(10000ULL,0ULL),
    Int128(100000ULL,0ULL),
    Int128(1000000ULL,0ULL),
    Int128(10000000ULL,0ULL),
    Int128(100000000ULL,0ULL),
    Int128(1000000000ULL,0ULL),
    Int128(10000000000ULL,0ULL),
    Int128(100000000000ULL,0ULL),
    Int128(1000000000000ULL,0ULL),
    Int128(10000000000000ULL,0ULL),
    Int128(100000000000000ULL,0ULL),
    Int128(1000000000000000ULL,0ULL),
    Int128(10000000000000000ULL,0ULL),
    Int128(100000000000000000ULL,0ULL),
    Int128(1000000000000000000ULL,0ULL),
    Int128(10000000000000000000ULL,0ULL),
    Int128(7766279631452241920ULL,5ULL),
    Int128(3875820019684212736ULL,54ULL),
    Int128(1864712049423024128ULL,542ULL),
    Int128(200376420520689664ULL,5421ULL),
    Int128(2003764205206896640ULL,54210ULL),
    Int128(1590897978359414784ULL,542101ULL),
    Int128(15908979783594147840ULL,5421010ULL),
    Int128(11515845246265065472ULL,54210108ULL),
    Int128(4477988020393345024ULL,542101086ULL),
    Int128(7886392056514347008ULL,5421010862ULL),
    Int128(5076944270305263616ULL,54210108624ULL),
    Int128(13875954555633532928ULL,542101086242ULL),
    Int128(9632337040368467968ULL,5421010862427ULL),
    Int128(4089650035136921600ULL,54210108624275ULL),
    Int128(4003012203950112768ULL,542101086242752ULL),
    Int128(3136633892082024448ULL,5421010862427522ULL),
    Int128(12919594847110692864ULL,54210108624275221ULL),
    Int128(68739955140067328ULL,542101086242752217ULL),
    Int128(687399551400673280ULL,5421010862427522170ULL)
#else
    Int128(1ULL)|Int128(0ULL)<<64,
    Int128(10ULL)|Int128(0ULL)<<64,
    Int128(100ULL)|Int128(0ULL)<<64,
    Int128(1000ULL)|Int128(0ULL)<<64,
    Int128(10000ULL)|Int128(0ULL)<<64,
    Int128(100000ULL)|Int128(0ULL)<<64,
    Int128(1000000ULL)|Int128(0ULL)<<64,
    Int128(10000000ULL)|Int128(0ULL)<<64,
    Int128(100000000ULL)|Int128(0ULL)<<64,
    Int128(1000000000ULL)|Int128(0ULL)<<64,
    Int128(10000000000ULL)|Int128(0ULL)<<64,
    Int128(100000000000ULL)|Int128(0ULL)<<64,
    Int128(1000000000000ULL)|Int128(0ULL)<<64,
    Int128(10000000000000ULL)|Int128(0ULL)<<64,
    Int128(100000000000000ULL)|Int128(0ULL)<<64,
    Int128(1000000000000000ULL)|Int128(0ULL)<<64,
    Int128(10000000000000000ULL)|Int128(0ULL)<<64,
    Int128(100000000000000000ULL)|Int128(0ULL)<<64,
    Int128(1000000000000000000ULL)|Int128(0ULL)<<64,
    Int128(10000000000000000000ULL)|Int128(0ULL)<<64,
    Int128(7766279631452241920ULL)|Int128(5ULL)<<64,
    Int128(3875820019684212736ULL)|Int128(54ULL)<<64,
    Int128(1864712049423024128ULL)|Int128(542ULL)<<64,
    Int128(200376420520689664ULL)|Int128(5421ULL)<<64,
    Int128(2003764205206896640ULL)|Int128(54210ULL)<<64,
    Int128(1590897978359414784ULL)|Int128(542101ULL)<<64,
    Int128(15908979783594147840ULL)|Int128(5421010ULL)<<64,
    Int128(11515845246265065472ULL)|Int128(54210108ULL)<<64,
    Int128(4477988020393345024ULL)|Int128(542101086ULL)<<64,
    Int128(7886392056514347008ULL)|Int128(5421010862ULL)<<64,
    Int128(5076944270305263616ULL)|Int128(54210108624ULL)<<64,
    Int128(13875954555633532928ULL)|Int128(542101086242ULL)<<64,
    Int128(9632337040368467968ULL)|Int128(5421010862427ULL)<<64,
    Int128(4089650035136921600ULL)|Int128(54210108624275ULL)<<64,
    Int128(4003012203950112768ULL)|Int128(542101086242752ULL)<<64,
    Int128(3136633892082024448ULL)|Int128(5421010862427522ULL)<<64,
    Int128(12919594847110692864ULL)|Int128(54210108624275221ULL)<<64,
    Int128(68739955140067328ULL)|Int128(542101086242752217ULL)<<64,
    Int128(687399551400673280ULL)|Int128(5421010862427522170ULL)<<64
#endif
};

Int128Exception::Int128Exception()
{
}

Int128Exception::Int128Exception(const Glib::ustring& msg)
{
    this->message = msg;
}

Int128Exception::~Int128Exception() throw()
{
}

const char* Int128Exception::what() const throw()
{
    return message.c_str();
}

Int128 strtoint128(const std::string& inputStr)
{
    bool isneg = false;
    gsize pos = 0;
    if (inputStr.empty())
        throw Int128Exception("String is empty");
    if (inputStr[0] == '+')
        pos++;
    else if (inputStr[0] == '-')
    {
        isneg = true;
        pos++;
    }
    
    Int128 curVal = 0;
    gsize inputSize = inputStr.size();
    guint radix = 0;
    if (inputSize == pos)
        throw Int128Exception("String is too short");
    
    guint firstDigit = inputStr[pos];
    for (;pos < inputSize; pos++, radix++)
    {
        const char c = inputStr[pos];
        if (c < '0' || c > '9')
            throw Int128Exception("Is not digit");
        if (radix==38 && firstDigit>0x31)
            throw Int128Exception("Number out of range");
        curVal = (curVal<<3) + (curVal<<1);
        curVal += guint64(c-'0');
        if (curVal < Int128(0) && (!isneg || curVal != (Int128(guint64(1ULL))<<127)))
            throw Int128Exception("Number out of range");
    }
    
    return (isneg) ? -curVal : curVal;
}

std::string int128tostr(const Int128& value)
{
    bool isneg = value < Int128(0);
    Int128 absVal = isneg ? -value : value;
    if (value == (Int128(guint64(1ULL<<63))<<64))
        return "-170141183460469231731687303715884105728";
    int decIdx = 0;
    for (; decIdx < 39 && radix128Tbl[decIdx] <= absVal; decIdx++);
    if (decIdx == 0)
        return "0";
    decIdx--;
    
    std::string out;
    if (isneg)
        out += '-';
    for (;decIdx >= 0; decIdx--)
    {
        guint digitVal = 0;
        Int128 radixVal = radix128Tbl[decIdx];
        guint xstart = 1;
        // maximal power of 2 for digit
        if (decIdx != 38)
        {
            radixVal <<= 3;
            xstart = 8;
        }
        // determine digit value
        for (guint x = xstart; x > 0; x>>=1)
        {
            if (radixVal <= absVal)
            {
                absVal -= radixVal;
                digitVal |= x;
            }
            radixVal>>=1;
        }
        out += ('0'+digitVal);
    }
    return out;
}
