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

#include <glibmm.h>
#include "X86CpuId.h"

#ifdef HAVE_ARCH_INTEL
#ifdef _MSC_VER

void CPUID(guint32 level, guint32* eax, guint32* ebx, guint32* ecx, guint32* edx)
{    
    guint32 _eax, _ebx, _ecx, _edx;
    __asm {
        mov eax, level
        cpuid
        mov _eax, eax
        mov _ebx, ebx
        mov _ecx, ecx
        mov _edx, edx
    }

    if (eax) *eax = _eax;
    if (ebx) *ebx = _ebx;
    if (ecx) *ecx = _ecx;
    if (edx) *edx = _edx;
}

#else

void CPUID(guint32 level, guint32* eax, guint32* ebx, guint32* ecx,
                      guint32* edx) {
    guint32 _eax, _ebx, _ecx, _edx;
#if defined(__PIC__) && defined(HAVE_ARCH_X86)
    asm("mov %%ebx,%%edi;"
	"cpuid;"
	"xchgl %%ebx,%%edi;"
        : "=a" (_eax), "=D" (_ebx), "=c" (_ecx), "=d" (_edx)
        : "a" (level));
#else
    asm("cpuid"
        : "=a" (_eax), "=b" (_ebx), "=c" (_ecx), "=d" (_edx)
        : "a" (level));
#endif

    if (eax) *eax = _eax;
    if (ebx) *ebx = _ebx;
    if (ecx) *ecx = _ecx;
    if (edx) *edx = _edx;
}

#endif

bool X86CheckCPUIDAvailable()
{
    /* code from SDL */
    int has_CPUID = 0;
#ifdef HAVE_ARCH_X86_64
    has_CPUID = 1;
#else 
#  if defined(__GNUC__)
    __asm__ (
"        pushfl                      # Get original EFLAGS             \n"
"        popl    %%eax                                                 \n"
"        movl    %%eax,%%ecx                                           \n"
"        xorl    $0x200000,%%eax     # Flip ID bit in EFLAGS           \n"
"        pushl   %%eax               # Save new EFLAGS value on stack  \n"
"        popfl                       # Replace current EFLAGS value    \n"
"        pushfl                      # Get new EFLAGS                  \n"
"        popl    %%eax               # Store new EFLAGS in EAX         \n"
"        xorl    %%ecx,%%eax         # Can not toggle ID bit,          \n"
"        jz      1f                  # Processor=80486                 \n"
"        movl    $1,%0               # We have CPUID support           \n"
"1:                                                                    \n"
    : "=m" (has_CPUID)
    :
    : "%eax", "%ecx"
    );
#  else
#    if defined(_MSC_VER)
    __asm {
        pushfd                      ; Get original EFLAGS
        pop     eax
        mov     ecx, eax
        xor     eax, 200000h        ; Flip ID bit in EFLAGS
        push    eax                 ; Save new EFLAGS value on stack
        popfd                       ; Replace current EFLAGS value
        pushfd                      ; Get new EFLAGS
        pop     eax                 ; Store new EFLAGS in EAX
        xor     eax, ecx            ; Can not toggle ID bit,
        jz      done                ; Processor=80486
        mov     has_CPUID,1         ; We have CPUID support
done:
    }
#    else
    has_CPUID = 0;  // by default no cpuid
#    endif
#  endif
#endif
    return has_CPUID!=0;
}

#endif
