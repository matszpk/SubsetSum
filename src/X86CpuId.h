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

#ifndef __MYCOMPINT_X86CPUID_H__
#define __MYCOMPINT_X86CPUID_H__

#include <glibmm.h>

#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || \
        defined(_M_IX86) || defined(__i386) || defined(_X86_)
#  define HAVE_ARCH_INTEL 1
#  define HAVE_ARCH_X86 1
#  define HAVE_32BIT 1
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || \
        defined(_M_X64) || defined(_M_AMD64)
#  define HAVE_ARCH_INTEL 1
#  define HAVE_ARCH_X86_64 1
#  define HAVE_64BIT 1
#endif

#ifdef HAVE_ARCH_INTEL

extern bool X86CheckCPUIDAvailable();
extern void CPUID(guint32 level, guint32* eax, guint32* ebx, guint32* ecx, guint32* edx);

#endif

#endif
