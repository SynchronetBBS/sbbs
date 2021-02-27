/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
 *
 * Oct-2001 door32.sys/socket modifications by Rob Swindell (www.synchro.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *        File: ODGen.h
 *
 * Description: Contains general definitions used throughout OpenDoors,
 *              including: - version information manifest constants
 *                         - debugging macros
 *                         - compiler-dependent definitions
 *                         - internally used macros
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   Created.
 *              Oct 20, 1994  6.00  BP   Added DIM macro.
 *              Dec 31, 1994  6.00  BP   Remove USEINLINE option.
 *              Dec 12, 1995  6.00  BP   Moved ODPLAT_??? to OpenDoor.h.
 *              Dec 19, 1995  6.00  BP   Implement ASSERT() for Win32.
 *              Jan 23, 1996  6.00  BP   Added OD_TEXTMODE.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 24, 1996  6.00  BP   Turn off OD_DIAGNOSTICS.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 03, 1996  6.10  BP   Moved ODFAR to OpenDoor.h.
 *              Oct 19, 2001  6.20  RS   Incremented version for socket support.
 */

#ifndef _INC_ODGEN
#define _INC_ODGEN


/* PLATFORM-SPECIFIC DEFINITIONS. */

/* DLL specific defintions. */
#ifdef OD_DLL
#ifdef ODPLAT_WIN32
#define OD_DLL_NAME "ODOORS62"
#endif /* ODPLAT_WIN32 */
#endif /* OD_DLL */

/* Mutlithreading specific definitions. */
#ifdef ODPLAT_WIN32
#define OD_MULTITHREADED
#endif /* ODPLAT_WIN32 */

/* Text mode specific definitions. */
#if defined(ODPLAT_DOS) || defined(ODPLAT_NIX)
#define OD_TEXTMODE
#endif /* ODPLAT_DOS */

/* DOS specific definitions. */
#ifdef ODPLAT_DOS

/* Keyword to flag ISR functions. */
#define INTERRUPT interrupt

/* Inline assembly keyword varies from compiler to compiler. */
#ifdef _MSC_VER
#define ASM __asm
#else
#define ASM asm
#endif

/* Memory model information. */
#ifdef __TINY__
#define SMALLDATA
#define SMALLCODE
#endif
#ifdef __SMALL__
#define SMALLDATA
#define SMALLCODE
#endif
#ifdef __COMPACT__
#define LARGEDATA
#define SMALLCODE
#endif
#ifdef __MEDIUM__
#define SMALLDATA
#define LARGECODE
#endif
#ifdef __LARGE__
#define LARGEDATA
#define LARGECODE
#endif
#ifdef __HUGE__
#define LARGEDATA
#define LARGECODE
#endif
#endif /* ODPLAT_DOS */


/* VERSION INFORMATION CONSTANTS. */
#define OD_VER_SHORTNAME   "OpenDoors"
#define OD_VER_STATUSLINE  "  OpenDoors 6.24 - (C) Copyright 1991-2001" \
                           " by Brian Pirie                      "
#define OD_VER_UNREG_STAT  "  OpenDoors 6.24  *WARNING* Unregistered Version" \
                           " - Limit 1 month trial period! "

#ifdef ODPLAT_DOS
#define OD_VER_SIGNON      "[OpenDoors 6.24/DOS - " \
                           "(C) Copyright 1991-2001 by Brian Pirie]\n\r"
#define OD_VER_FULLNAME    "OpenDoors 6.24/DOS"
#endif /* ODPLAT_DOS */
#ifdef ODPLAT_WIN32
#define OD_VER_SIGNON      "[OpenDoors 6.24/Win32 - " \
                           "(C) Copyright 1991-2001 by Brian Pirie]\n\r"
#define OD_VER_FULLNAME    "OpenDoors 6.24/Win32"
#endif /* ODPLAT_WIN32 */
#ifdef ODPLAT_NIX
#define OD_VER_SIGNON      "[OpenDoors 6.24/*nix - " \
                           "(C) Copyright 1991-2001 by Brian Pirie]\n\r"
#define OD_VER_FULLNAME    "OpenDoors 6.24/*nix"
#endif /* ODPLAT_NIX */


/* COMPILER DEPENDENT DEFINITIONS. */

/* Some compilers don't like const keyword on parameters. */
#define CONST const


/* DEBUG MACROS. */

/* OD_DEBUG is defined for debug version of the library. */
/* #define OD_DEBUG */

/* OD_DIAGNOSTICS is defined to enable od_internal_debug. */
/* #define OD_DIAGNOSTICS */

/* ASSERTion macro - terminates if test condition fails. */
#ifdef OD_DEBUG
#define __STR(x) __VAL(x)
#define __VAL(x) #x
#ifdef ODPLAT_WIN32
#define ASSERT(x) if(!(x)) { MessageBox(NULL, __FILE__ ":" \
   __STR(__LINE__) "\n" #x,  OD_VER_FULLNAME " - Test condition failed", \
   MB_ICONSTOP | MB_OK); exit(1); }
#else /* !ODPLAT_WIN32 */
#define ASSERT(x) if(!(x)) { puts(OD_VER_FULLNAME \
   " - Test condition failed:\n" __FILE__ ":" __STR(__LINE__) "\n" #x); \
   exit(1); }
#endif /* !ODPLAT_WIN32 */
#else /* !OD_DEBUG */
#define ASSERT(x)
#endif /* !OD_DEBUG */

/* TRACE() macro - used to generate debug output. */
#ifdef OD_TRACE
#include <stdio.h>
#define TRACE_API 1
#define TRACE(x, y) printf("[%s]", y);
#else
#define TRACE(x, y)
#endif


/* SCREEN SIZE. */
#define OD_SCREEN_WIDTH    80
#define OD_SCREEN_HEIGHT   25


/* INTERNALLY USED MACROS. */

/* MIN() and MAX() macros. Note that expressions passed to macros may be */
/* evaluated more than once. For this reason, it is best to only pass    */
/* constants or variables to these macros.                               */
#ifndef MIN
#define MIN(x, y) ((x) > (y)) ? (y) : (x)
#endif /* !MIN */
#ifndef MAX
#define MAX(x, y) ((x) > (y)) ? (x) : (y)
#endif /* !MAX */

/* DIM() macro. Returns the number of elements in an array. */
#ifndef DIM
#define DIM(x) (sizeof(x) / sizeof(*x))
#endif /* !DIM */

/* UNUSED() macro. Used to flag that a function parameter is intentionally */
/* not used, thus preventing a compile-time warning.                       */
#define UNUSED(x) ((void)(x))

#endif /* !_INC_ODGEN */
