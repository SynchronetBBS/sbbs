/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
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
 *        File: ODPlat.h
 *
 * Description: Contains platform-related definitions and prototypes for
 *              those function's whose implementation is platform-specific
 *              (functions implemented in odplat.c). Non-platform specific
 *              utility functions are defined in odutil.h and implemented in
 *              odutil.c.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 14, 1994  6.00  BP   Created.
 *              Nov 01, 1994  6.00  BP   Added ODDir...() functions.
 *              Dec 31, 1994  6.00  BP   Added timing, file delete functions.
 *              Dec 31, 1994, 6.00  BP   Added ODMultitasker and ODPlatInit()
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 17, 1995  6.00  BP   Added multithreading functions.
 *              Dec 13, 1995  6.00  BP   Added ODThreadWaitForExit().
 *              Dec 13, 1995  6.00  BP   Added ODThreadGetCurrent().
 *              Jan 23, 1996  6.00  BP   Added ODProcessExit().
 *              Jan 30, 1996  6.00  BP   Add semaphore timeout.
 *              Jan 31, 1996  6.00  BP   Add ODTimerLeft(), rm ODTimerSleep().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODPLAT
#define _INC_ODPLAT

#include <time.h>

#include "ODTypes.h"
#include "ODGen.h"

#ifdef ODPLAT_NIX
#include <sys/time.h>
#endif

#ifdef ODPLAT_WIN32
#include "windows.h"
#endif /* ODPLAT_WIN32 */

/* odplat.c initialization function prototype */
void ODPlatInit(void);


/* ========================================================================= */
/* Millisecond timer functions.                                              */
/* ========================================================================= */

/* Timer data type. */
typedef struct
{
#ifdef ODPLAT_DOS
   clock_t Start;
   clock_t Duration;
#elif defined(ODPLAT_NIX)
   time_t Start;
   tODMilliSec Duration;
#else /* !ODPLAT_DOS */
   tODMilliSec Start;
   tODMilliSec Duration;
#endif /* !ODPLAT_DOS */
} tODTimer;

/* Timer function prototypes. */
void ODTimerStart(tODTimer *pTimer, tODMilliSec Duration);
BOOL ODTimerElapsed(tODTimer *pTimer);
void ODTimerWaitForElapse(tODTimer *pTimer);
tODMilliSec ODTimerLeft(tODTimer *pTimer);


/* ========================================================================= */
/* Multithreading and synchronization support.                               */
/* ========================================================================= */

#ifdef OD_MULTITHREADED

/* Thread handle data type. */
#ifdef ODPLAT_WIN32
typedef HANDLE tODThreadHandle;
#endif /* ODPLAT_WIN32 */

/* Thread priority enumeration. */
typedef enum
{
   OD_PRIORITY_LOWEST,
   OD_PRIORITY_BELOW_NORMAL,
   OD_PRIORITY_NORMAL,
   OD_PRIORITY_ABOVE_NORMAL,
   OD_PRIORITY_HIGHEST
} tODThreadPriority;

/* Thread start proceedure type. */
#define OD_THREAD_FUNC WINAPI
#ifdef ODPLAT_WIN32
typedef DWORD (OD_THREAD_FUNC ptODThreadProc)(void *);
#endif /* ODPLAT_WIN32 */

/* Thread creation, temination and suspension. */
tODResult ODThreadCreate(tODThreadHandle *phThread,
   ptODThreadProc *pfThreadProc, void *pThreadParam);
void ODThreadExit();
tODResult ODThreadTerminate(tODThreadHandle hThread);
tODResult ODThreadSuspend(tODThreadHandle hThread);
tODResult ODThreadResume(tODThreadHandle hThread);
tODResult ODThreadSetPriority(tODThreadHandle hThread,
   tODThreadPriority ThreadPriority);
void ODThreadWaitForExit(tODThreadHandle hThread);
tODThreadHandle ODThreadGetCurrent(void);


/* Semaphore handle data type. */
#ifdef ODPLAT_WIN32
typedef HANDLE tODSemaphoreHandle;
#endif /* ODPLAT_WIN32 */

/* Semaphore manipulation functions. */
tODResult ODSemaphoreAlloc(tODSemaphoreHandle *phSemaphore, INT nInitialCount,
   INT nMaximumCount);
void ODSemaphoreFree(tODSemaphoreHandle hSemaphore);
void ODSemaphoreUp(tODSemaphoreHandle hSemaphore, INT nIncrementBy);
tODResult ODSemaphoreDown(tODSemaphoreHandle hSemaphore, tODMilliSec Timeout);

#endif /* OD_MULTITHREADED */

void ODProcessExit(INT nExitCode);


/* ========================================================================= */
/* DOS multitasker information.                                              */
/* ========================================================================= */
#ifdef ODPLAT_DOS
typedef enum
{
   kMultitaskerNone,
   kMultitaskerDV,
   kMultitaskerWin,
   kMultitaskerOS2
} tODMultitasker;

extern tODMultitasker ODMultitasker;
#endif /* ODPLAT_DOS */


/* ========================================================================= */
/* Directory Access.                                                         */
/* ========================================================================= */

/* Open directory handle type. */
typedef tODHandle tODDirHandle;

/* Directory entry structure. */
#define DIR_FILENAME_SIZE 1024

#define DIR_ATTRIB_NORMAL   0x00
#define DIR_ATTRIB_RDONLY   0x01
#define DIR_ATTRIB_HIDDEN   0x02
#define DIR_ATTRIB_SYSTEM   0x04
#define DIR_ATTRIB_LABEL    0x08
#define DIR_ATTRIB_DIREC    0x10
#define DIR_ATTRIB_ARCH     0x20

typedef struct
{
   char szFileName[DIR_FILENAME_SIZE];
   WORD wAttributes;
   time_t LastWriteTime;
   DWORD dwFileSize;
} tODDirEntry;

/* Directory function prototypes. */
tODResult ODDirOpen(CONST char *pszPath, WORD wAttributes, tODDirHandle *phDir);
tODResult ODDirRead(tODDirHandle hDir, tODDirEntry *pDirEntry);
void ODDirClose(tODDirHandle hDir);
void ODDirChangeCurrent(char *pszPath);
void ODDirGetCurrent(char *pszPath, INT nMaxPathChars);


/* ========================================================================= */
/* Miscellaneous Functions.                                                  */
/* ========================================================================= */
tODResult ODFileDelete(CONST char *pszPath);
BOOL ODFileAccessMode(char *pszFilename, int nAccessMode);

#endif /* !_INC_ODPLAT */
