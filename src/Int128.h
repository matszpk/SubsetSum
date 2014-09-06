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

#ifndef __SS_INT128_H__
#define __SS_INT128_H__

#include <string>
#include <glibmm.h>

//#undef HAVE_INT128

class Int128Exception: public std::exception
{
private:
    Glib::ustring message;
public:
    Int128Exception();
    explicit Int128Exception(const Glib::ustring& msg);
    ~Int128Exception() throw();
    
    const char* what() const throw();
};

#ifndef HAVE_INT128
class Int128
{
private:
    guint64 lo;
    gint64 hi;
public:
    Int128() { }
    Int128(guint16 x)
    {
        lo = x;
        hi = 0;
    }
    Int128(guint32 x)
    {
        lo = x;
        hi = 0;
    }
    Int128(guint64 x)
    {
        lo = x;
        hi = 0;
    }
    Int128(guint64 lo, gint64 hi)
    {
        this->lo = lo;
        this->hi = hi;
    }
    Int128(gint16 x)
    {
        lo = x;
        hi = (x<0)?-1LL:0;;
    }
    Int128(gint32 x)
    {
        lo = x;
        hi = (x<0)?-1LL:0;
    }
    Int128(gint64 x)
    {
        lo = x;
        hi = (x<0)?-1LL:0;
    }
    Int128& operator=(guint16 v)
    {
        lo = v;
        hi = 0;
        return *this;
    }
    Int128& operator=(gint16 v)
    {
        lo = v;
        hi = (v<0)?-1LL:0;
        return *this;
    }
    Int128& operator=(guint32 v)
    {
        lo = v;
        hi = 0;
        return *this;
    }
    Int128& operator=(gint32 v)
    {
        lo = v;
        hi = (v<0)?-1LL:0;
        return *this;
    }
    Int128& operator=(guint64 v)
    {
        lo = v;
        hi = 0;
        return *this;
    }
    Int128& operator=(gint64 v)
    {
        lo = v;
        hi = (v<0)?-1LL:0;
        return *this;
    }
    operator guint64() const
    { return lo; }
    operator gint64() const
    { return lo; }
    
    Int128& operator+=(const Int128& x)
    {
        guint64 tmp = x.lo;
        lo += x.lo;
        hi += x.hi + (lo < tmp);
        return *this;
    }
    Int128& operator+=(guint64 x)
    {
        lo += x;
        hi += (lo < x);
        return *this;
    }
    Int128& operator+=(gint64 x)
    {
        lo += x;
        if (x>=0)
            hi += (lo < guint64(x));
        else
            hi -= (lo >= guint64(x));
        return *this;
    }
    Int128& operator-=(const Int128& x)
    {
        guint64 tmp = lo;
        lo -= x.lo;
        hi -= x.hi + (lo > tmp);
        return *this;
    }
    Int128& operator-=(guint64 x)
    {
        guint64 tmp = lo;
        lo -= x;
        hi -= (lo > tmp);
        return *this;
    }
    Int128& operator-=(gint64 x)
    {
        guint64 tmp = lo;
        lo -= x;
        if (x>=0)
            hi -= (lo > tmp);
        else
            hi += (lo <= tmp);
        return *this;
    }
    
    Int128 operator+(const Int128& x) const
    {
        Int128 tmp = *this;
        return tmp+=x;
    }
    Int128 operator+(guint64 x) const
    {
        Int128 tmp = *this;
        return tmp+=x;
    }
    Int128 operator-(const Int128& x) const
    {
        Int128 tmp = *this;
        return tmp-=x;
    }
    Int128 operator-(guint64 x) const
    {
        Int128 tmp = *this;
        return tmp-=x;
    }
    
    Int128& operator&=(const Int128& b)
    {
        lo &= b.lo;
        hi &= b.hi;
        return *this;
    }
    Int128& operator|=(const Int128& b)
    {
        lo |= b.lo;
        hi |= b.hi;
        return *this;
    }
    Int128& operator^=(const Int128& b)
    {
        lo ^= b.lo;
        hi ^= b.hi;
        return *this;
    }
    
    Int128 operator&(const Int128& b) const
    {
        Int128 tmp = *this;
        return tmp&=b;
    }
    Int128 operator|(const Int128& b) const
    {
        Int128 tmp = *this;
        return tmp|=b;
    }
    Int128 operator^(const Int128& b) const
    {
        Int128 tmp = *this;
        return tmp^=b;
    }
    
    bool operator==(const Int128& x) const
    {
        return (lo == x.lo && hi == x.hi);
    }
    bool operator!=(const Int128& x) const
    {
        return (lo != x.lo || hi != x.hi);
    }
    bool operator<(const Int128& x) const
    {
      return (hi < x.hi || (hi == x.hi && lo < x.lo));
    }
    bool operator<=(const Int128& x) const
    {
        return (hi < x.hi || (hi == x.hi && lo <= x.lo));
    }
    bool operator>(const Int128& x) const
    {
      return (hi > x.hi || (hi == x.hi && lo > x.lo));
    }
    bool operator>=(const Int128& x) const
    {
        return (hi > x.hi || (hi == x.hi && lo >= x.lo));
    }
    
    bool operator!() const
    { return (!lo && !hi); }
    
    Int128 operator~() const
    {
        return Int128(~lo, ~hi);
    }
    
    Int128 operator-() const
    {
        return Int128(-lo, -hi - (-lo!=0));
    }
    
    Int128& operator<<=(int x)
    {
        if (x == 0) // in some architectures shift 64 doesnt work properly (64-x)
            return *this; // do nothing
        if (x < 64)
        {
            guint64 tmp = lo >> (64-x);
            lo = lo << x;
            hi = (hi << x) | tmp;
        }
        else if (x < 128)
        {
            hi = lo << (x-64);
            lo = 0;
        }
        else // handle 128-bit shifts
            lo = hi = 0;
        return *this;
    }
    Int128& operator>>=(int x)
    {
        if (x == 0) // in some architectures shift 64 doesnt work properly (64-x)
            return *this; // do nothing
        if (x < 64)
        {
            guint64 tmp = (hi << (64-x));
            hi = hi >> x;
            lo = (lo >> x) | tmp;
        }
        else if (x < 128)
        {
            lo = hi >> (x-64);
            hi = hi>>63;
        }
        else // handle 128-bit shifts
            lo = hi = hi>>63;
        return *this;
    }
    
    Int128 operator<<(int x) const
    {
        Int128 tmp = *this;
        return tmp<<=x;
    }
    Int128 operator>>(int x) const
    {
        Int128 tmp = *this;
        return tmp>>=x;
    }
    
    Int128& operator++()
    {
        lo++;
        if (lo==0)
            hi++;
        return *this;
    }
    Int128 operator++(int)
    {
        Int128 before = *this;
        lo++;
        if (lo==0)
            hi++;
        return before;
    }
    
    Int128& operator--()
    {
        lo--;
        if (lo==G_MAXUINT64)
            hi--;
        return *this;
    }
    
    Int128 operator--(int)
    {
        Int128 before = *this;
        lo--;
        if (lo==G_MAXUINT64)
            hi--;
        return before;
    }
    
    operator bool() const
    {
        return lo!=0 || hi!=0;
    }
};
#else
typedef __int128 Int128;
#endif

extern Int128 strtoint128(const std::string& inputStr);

extern std::string int128tostr(const Int128& value);

#endif
