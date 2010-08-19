/*****************************************************************************
 * cpu.c: CPU detection code
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id: 693ae25694c13efa657386b34441572170048c49 $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.eduEujenio>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_cpu.h>

#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#else
#include <errno.h>
#endif

#include "libvlc.h"

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__))
#include <sys/sysctl.h>
#endif

#if defined(__OpenBSD__) && defined(__powerpc__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

#if defined( __i386__ ) || defined( __x86_64__ ) || defined( __powerpc__ ) \
 || defined( __ppc__ ) || defined( __ppc64__ ) || defined( __powerpc64__ )
# ifndef WIN32
static bool check_OS_capability( const char *psz_capability, pid_t pid )
{
    int status;

    if( pid == -1 )
        return false; /* fail safe :-/ */

    while( waitpid( pid, &status, 0 ) == -1 );

    if( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 )
        return true;

    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
    return false;
}

#  define check_capability(name, flag, code)   \
     do {                                      \
        pid_t pid = fork();                    \
        if( pid == 0 )                         \
        {                                      \
            signal(SIGILL, SIG_DFL);           \
            __asm__ __volatile__ ( code : : ); \
            _exit(0);                          \
        }                                      \
        if( check_OS_capability((name), pid )) \
            i_capabilities |= (flag);          \
     } while(0)

# else /* WIN32 */
#  define check_capability(name, flag, code)   \
        i_capabilities |= (flag);
# endif
#endif

/*****************************************************************************
 * CPUCapabilities: get the CPU capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
uint32_t CPUCapabilities( void )
{
    uint32_t i_capabilities = 0;

#if defined( __i386__ ) || defined( __x86_64__ )
     unsigned int i_eax, i_ebx, i_ecx, i_edx;
     bool b_amd;

    /* Needed for x86 CPU capabilities detection */
#   if defined( __x86_64__ )
#       define cpuid( reg )                    \
            asm volatile ( "cpuid\n\t"         \
                           "movl %%ebx,%1\n\t" \
                         : "=a" ( i_eax ),     \
                           "=b" ( i_ebx ),     \
                           "=c" ( i_ecx ),     \
                           "=d" ( i_edx )      \
                         : "a"  ( reg )        \
                         : "cc" );
#   else
#       define cpuid( reg )                    \
            asm volatile ( "push %%ebx\n\t"    \
                           "cpuid\n\t"         \
                           "movl %%ebx,%1\n\t" \
                           "pop %%ebx\n\t"     \
                         : "=a" ( i_eax ),     \
                           "=r" ( i_ebx ),     \
                           "=c" ( i_ecx ),     \
                           "=d" ( i_edx )      \
                         : "a"  ( reg )        \
                         : "cc" );
#   endif
     /* Check if the OS really supports the requested instructions */
# if defined (__i386__) && !defined (__i486__) && !defined (__i586__) \
  && !defined (__i686__) && !defined (__pentium4__) \
  && !defined (__k6__) && !defined (__athlon__) && !defined (__k8__)
    /* check if cpuid instruction is supported */
    asm volatile ( "push %%ebx\n\t"
                   "pushf\n\t"
                   "pop %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "push %%eax\n\t"
                   "popf\n\t"
                   "pushf\n\t"
                   "pop %%eax\n\t"
                   "movl %%ebx,%1\n\t"
                   "pop %%ebx\n\t"
                 : "=a" ( i_eax ),
                   "=r" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
        goto out;
# endif

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

# if defined (__i386__) && !defined (__i586__) \
  && !defined (__i686__) && !defined (__pentium4__) \
  && !defined (__k6__) && !defined (__athlon__) && !defined (__k8__)
    if( !i_eax )
        goto out;
#endif

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );
# if !defined (__MMX__)
    if( ! (i_edx & 0x00800000) )
        goto out;
# endif
    i_capabilities |= CPU_CAPABILITY_MMX;

# if defined (__SSE__)
    i_capabilities |= CPU_CAPABILITY_MMXEXT | CPU_CAPABILITY_SSE;
# else
    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        check_capability( "SSE", CPU_CAPABILITY_SSE,
                          "xorps %%xmm0,%%xmm0\n" );
#   endif
    }
# endif

# if defined (__SSE2__)
    i_capabilities |= CPU_CAPABILITY_SSE2;
# elif defined (CAN_COMPILE_SSE2)
    if( i_edx & 0x04000000 )
        check_capability( "SSE2", CPU_CAPABILITY_SSE2,
                          "movupd %%xmm0, %%xmm0\n" );
# endif

# if defined (__SSE3__)
    i_capabilities |= CPU_CAPABILITY_SSE3;
# elif defined (CAN_COMPILE_SSE3)
    if( i_ecx & 0x00000001 )
        check_capability( "SSE3", CPU_CAPABILITY_SSE3,
                          "movsldup %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSSE3__)
    i_capabilities |= CPU_CAPABILITY_SSSE3;
# elif defined (CAN_COMPILE_SSSE3)
    if( i_ecx & 0x00000200 )
        check_capability( "SSSE3", CPU_CAPABILITY_SSSE3,
                          "pabsw %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSE4_1__)
    i_capabilities |= CPU_CAPABILITY_SSE4_1;
# elif defined (CAN_COMPILE_SSE4_1)
    if( i_ecx & 0x00080000 )
        check_capability( "SSE4.1", CPU_CAPABILITY_SSE4_1,
                          "pmaxsb %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSE4_2__)
    i_capabilities |= CPU_CAPABILITY_SSE4_2;
# elif defined (CAN_COMPILE_SSE4_2)
    if( i_ecx & 0x00100000 )
        check_capability( "SSE4.2", CPU_CAPABILITY_SSE4_2,
                          "pcmpgtq %%xmm1, %%xmm0\n" );
# endif

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
        goto out;

    /* list these additional capabilities */
    cpuid( 0x80000001 );

# if defined (__3dNOW__)
    i_capabilities |= CPU_CAPABILITY_3DNOW;
# elif defined (CAN_COMPILE_3DNOW)
    if( i_edx & 0x80000000 )
        check_capability( "3D Now!", CPU_CAPABILITY_3DNOW,
                          "pfadd %%mm0,%%mm0\n" "femms\n" );
# endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }
out:

#elif defined( __arm__ )
#   if defined( __ARM_NEON__ )
    i_capabilities |= CPU_CAPABILITY_NEON;
#   endif

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __powerpc64__ ) \
    || defined( __ppc64__ )

#   if defined(__APPLE__) || defined(__OpenBSD__)
#   if defined(__OpenBSD__)
    int selectors[2] = { CTL_MACHDEP, CPU_ALTIVEC };
#   else
    int selectors[2] = { CTL_HW, HW_VECTORUNIT };
#   endif
    int i_has_altivec = 0;
    size_t i_length = sizeof( i_has_altivec );
    int i_error = sysctl( selectors, 2, &i_has_altivec, &i_length, NULL, 0);

    if( i_error == 0 && i_has_altivec != 0 )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   elif defined( CAN_COMPILE_ALTIVEC )
    pid_t pid = fork();
    if( pid == 0 )
    {
        signal(SIGILL, SIG_DFL);
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
        _exit(0);
    }

    if( check_OS_capability( "Altivec", pid ) )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   endif

#endif
    return i_capabilities;
}

uint32_t cpu_flags = 0;


/*****************************************************************************
 * vlc_CPU: get pre-computed CPU capability flags
 ****************************************************************************/
unsigned vlc_CPU (void)
{
    return cpu_flags;
}

const struct
{
    uint32_t value;
    char name[12];
} cap_dirs[] = {
#if defined ( __i386__ ) || defined ( __x86_64__ )
    { CPU_CAPABILITY_MMX,     "mmx" },
    { CPU_CAPABILITY_MMXEXT,  "mmxext" },
    { CPU_CAPABILITY_3DNOW,   "3dnow" },
    { CPU_CAPABILITY_SSE,     "sse" },
#endif
#if defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)
    { CPU_CAPABILITY_ALTIVEC, "altivec" },
#endif
#if defined (__arm__)
    { CPU_CAPABILITY_NEON,    "arm_neon" },
#endif
};

/**
 * Check if a directory name contains usable plugins w.r.t. the hardware
 * capabilities. Loading a plugin when the hardware has insufficient
 * capabilities may lead to illegal instructions (SIGILL) and must be avoided.
 *
 * @param name the name of the directory (<b>not</b> the path)
 *
 * @return true if the hardware has sufficient capabilities or the directory
 * does not require any special capability; false if the running hardware has
 * insufficient capabilities.
 */
bool vlc_CPU_CheckPluginDir (const char *name)
{
    const unsigned flags = vlc_CPU ();
    for (size_t i = 0; i < sizeof (cap_dirs) / sizeof (cap_dirs[0]); i++)
    {
        if (strcmp (name, cap_dirs[i].name))
            continue;
        return (flags & cap_dirs[i].value) != 0;
    }
    return true;
}

static vlc_memcpy_t pf_vlc_memcpy = memcpy;
static vlc_memset_t pf_vlc_memset = memset;

void vlc_fastmem_register (vlc_memcpy_t cpy, vlc_memset_t set)
{
    if (cpy)
        pf_vlc_memcpy = cpy;
    if (set)
        pf_vlc_memset = set;
}

/**
 * vlc_memcpy: fast CPU-dependent memcpy
 */
void *vlc_memcpy (void *tgt, const void *src, size_t n)
{
    return pf_vlc_memcpy (tgt, src, n);
}

/**
 * vlc_memset: fast CPU-dependent memset
 */
void *vlc_memset (void *tgt, int c, size_t n)
{
    return pf_vlc_memset (tgt, c, n);
}
