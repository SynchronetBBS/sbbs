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
 *        File: ODPlat.c
 *
 * Description: Contains platform-specific utility functions. Non-platform
 *              specific utility functions are implemented in odutil.c
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 14, 1994  6.00  BP   Created, with od_yield().
 *              Nov 01, 1994  6.00  BP   Added new directory access functions.
 *              Dec 09, 1994  6.00  BP   Eliminate access to old dir functions.
 *              Dec 31, 1994  6.00  BP   Added timing, file delete functions.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Dec 31, 1994  6.00  BP   Added ODMultitasker and ODPlatInit()
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Added multithreading functions.
 *              Nov 21, 1995  6.00  BP   Ported to Win32.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 13, 1995  6.00  BP   Added ODThreadWaitForExit().
 *              Dec 13, 1995  6.00  BP   Added ODThreadGetCurrent().
 *              Dec 19, 1995  6.00  BP   Fixed ODThreadGetCurrent() (Win32).
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 23, 1996  6.00  BP   Added ODProcessExit().
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 30, 1996  6.00  BP   Add semaphore timeout.
 *              Jan 31, 1996  6.00  BP   Add ODTimerLeft(), rm ODTimerSleep().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Prevent TC calls N_LXMUL@ & N_LXDIV@.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "OpenDoor.h"
#ifdef ODPLAT_NIX
#include <sys/time.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "ODGen.h"
#include "ODCore.h"
#include "ODPlat.h"
#include "ODUtil.h"
#include "ODSwap.h"
#include "ODKrnl.h"

#ifdef ODPLAT_WIN32
#include "windows.h"
#endif /* ODPLAT_WIN32 */


/* Multitasker type, only availvable under DOS. */
#ifdef ODPLAT_DOS
tODMultitasker ODMultitasker = kMultitaskerNone;
static void ODPlatYield(void);
#endif /* ODPLAT_DOS */

/* ----------------------------------------------------------------------------
 * ODPlatInit()
 *
 * Performs any initialization required to use the utility functions supplied
 * by this module.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODPlatInit(void)
{
#ifdef ODPLAT_DOS
   /* If this is the DOS version of OpenDoors, then ODPlatInit() must */
   /* determine what multitasker we are running under.                */

   /* Check whether running under OS/2. */
   ASM       mov ah, 0x30
   ASM       int 0x21
   ASM       cmp al, 0x0a
   ASM       jl  NoOS2

   /* If we get to this point, then OS/2 has been detected. */
   ODMultitasker = kMultitaskerOS2;
   return;

NoOS2:
   /* Check whether we are running under DesqView. */
   ASM    mov cx, 0x4445
   ASM    mov dx, 0x5351
   ASM    mov ax, 0x2b01
   ASM    int 0x21
   ASM    cmp al, 0xff
   ASM    je NoDesqView

   /* If we get to this point, then DesqView has been detected. */
   ODMultitasker = kMultitaskerDV;

NoDesqView:
   /* Check whether we are running under Windows. */
   ASM    push di
   ASM    push si
   ASM    mov ax, 0x1600
   ASM    int 0x2f
   ASM    pop si
   ASM    pop di
   ASM    cmp al, 0x00
   ASM    je NoWindows
   ASM    cmp al, 0x80
   ASM    je NoWindows

    /* If we get to this point, then Windows has been detected. */
   ODMultitasker = kMultitaskerWin;

NoWindows:
   ODMultitasker = kMultitaskerNone;
#endif /* ODPLAT_DOS */
}


/* ----------------------------------------------------------------------------
 * ODPlatYield()                                       *** PRIVATE FUNCTION ***
 *
 * Yields control to other tasks when running as a DOS application under a
 * multitasking system.
 *
 * Parameters: none
 *
 *     Return: void
 */
#ifdef ODPLAT_DOS
static void ODPlatYield(void)
{
   switch(ODMultitasker)
   {
      case kMultitaskerDV:
         ASM  mov ax, 0x1000
         ASM  int 0x15
         break;

      case kMultitaskerWin:
         ASM  mov ax, 0x1680
         ASM  int 0x2f
         break;

      case kMultitaskerOS2:
      default:
         ASM  int 0x28
   }
}
#endif /* ODPLAT_DOS */


/* ========================================================================= */
/* Multithreading and synchronization support.                               */
/* ========================================================================= */

#ifdef OD_MULTITHREADED

/* ----------------------------------------------------------------------------
 * ODThreadCreate()
 *
 * Starts a new thread of concurrent execution.
 *
 * Parameters: phThread      - Pointer to the location where the handle to the
 *                             new thread should be stored.
 *
 *             pfThreadProc  - Function to call to begin execution of the
 *                             thread.
 *
 *             pThreadParam  - Parameter to pass to the thread function when
 *                             it is called.
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODThreadCreate(tODThreadHandle *phThread,
   ptODThreadProc *pfThreadProc, void *pThreadParam)
{
#ifdef ODPLAT_WIN32
   DWORD dwThreadID;
   HANDLE hNewThread;

   ASSERT(phThread != NULL);
   ASSERT(pfThreadProc != NULL);
   
   /* Attempt to create the new thread. */
   hNewThread = CreateThread(NULL, 0, pfThreadProc, pThreadParam,
      0, &dwThreadID);

   /* Check for thread creation failure. */
   if(hNewThread == NULL)
   {
      return(kODRCGeneralFailure);
   }

   /* Pass newly created thread's handle back to the caller. */
   *phThread = hNewThread;

   /* Return with success. */
   return(kODRCSuccess);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadExit()
 *
 * Causes the calling thread to be terminated.
 *
 * Parameters: none
 *
 *     Return: Never returns!
 */
void ODThreadExit()
{
#ifdef ODPLAT_WIN32
   ExitThread(0);
#endif /* ODPLAT_WIN32 */

   /* We should never get here. */
   ASSERT(FALSE);
}


/* ----------------------------------------------------------------------------
 * ODThreadTerminate()
 *
 * Terminates the specified thread. 
 *
 * Parameters: hThread - Handle to the thread to be terminated.
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODThreadTerminate(tODThreadHandle hThread)
{
   ASSERT(hThread != NULL);

#ifdef ODPLAT_WIN32
   return(TerminateThread(hThread, 0) ? kODRCSuccess : kODRCGeneralFailure);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadSuspend()
 *
 * Pauses execution of the specified thread, until the ODThreadResume()
 * function is called.
 *
 * Parameters: hThread - Handle to the thread to be suspended.
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODThreadSuspend(tODThreadHandle hThread)
{
   ASSERT(hThread != NULL);

#ifdef ODPLAT_WIN32
   return(SuspendThread(hThread) == 0xFFFFFFFF ? kODRCGeneralFailure
      : kODRCSuccess);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadResume()
 *
 * Continues execution of a thread previously paused by a call to
 * ODThreadSuspend().
 *
 * Parameters: hThread - Handle to the thread to be resumed.
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODThreadResume(tODThreadHandle hThread)
{
   ASSERT(hThread != NULL);

#ifdef ODPLAT_WIN32
   return(ResumeThread(hThread) == 0xFFFFFFFF ? kODRCGeneralFailure
      : kODRCSuccess);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadSetPriority()
 *
 * Changes the execution priority of a thread. Since the exact semantics of
 * thread priorities are different for each platform, this function should
 * be used carefully. The caller should assume that no thread will run if there
 * exists a non-blocked thread with a higher priority.
 *
 * Parameters: hThread        - Handle to the thread to change the priority of.
 *
 *             ThreadPriority - New priority to assign to the thread.
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODThreadSetPriority(tODThreadHandle hThread,
   tODThreadPriority ThreadPriority)
{
#ifdef ODPLAT_WIN32
   int nWindowsThreadPriority;

   ASSERT(hThread != NULL);

   /* Determine the Windows thread priority to assign to the thread. */
   switch(ThreadPriority)   
   {
      case OD_PRIORITY_LOWEST:
         nWindowsThreadPriority = THREAD_PRIORITY_LOWEST;
         break;
      case OD_PRIORITY_BELOW_NORMAL:
         nWindowsThreadPriority = THREAD_PRIORITY_BELOW_NORMAL;
         break;
      case OD_PRIORITY_NORMAL:
         nWindowsThreadPriority = THREAD_PRIORITY_NORMAL;
         break;
      case OD_PRIORITY_ABOVE_NORMAL:
         nWindowsThreadPriority = THREAD_PRIORITY_ABOVE_NORMAL;
         break;
      case OD_PRIORITY_HIGHEST:
         nWindowsThreadPriority = THREAD_PRIORITY_HIGHEST;
         break;
      default:
         ASSERT(FALSE);
   }

   /* Update the thread's priority. */
   return(SetThreadPriority(hThread, nWindowsThreadPriority)
      ? kODRCSuccess : kODRCGeneralFailure);

#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadWaitForExit()
 *
 * Blocks until the specified thread is terminated.
 *
 * Parameters: hThread - Handle to the thread to wait for.
 *
 *     Return: void
 */
void ODThreadWaitForExit(tODThreadHandle hThread)
{
#ifdef ODPLAT_WIN32
   WaitForSingleObject(hThread, INFINITE);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODThreadGetCurrent()
 *
 * Obtains a handle to the thread that called this function.
 *
 * Parameters: None.
 *
 *     Return: Handle to the current thread.
 */
tODThreadHandle ODThreadGetCurrent(void)
{
#ifdef ODPLAT_WIN32
   HANDLE hDuplicate;
   if(!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
      GetCurrentProcess(), &hDuplicate, 0, FALSE, DUPLICATE_SAME_ACCESS))
   {
      return(NULL);
   }
   return(hDuplicate);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODSemaphoreAlloc()
 *
 * Allocates a semaphore synchronization object.
 *
 * Parameters: phSemaphore   - Pointer to location where the handle to the
 *                             newly created semaphore should be stored.
 *
 *             nInitialCount - Initial value to assign to the semaphore.
 *
 *             nMaximumCount - Maximum value that the semaphore may have
 *                             (if supported by the current platform).
 *
 *     Return: kOCRCSuccess on success, or an error code on failure.
 */
tODResult ODSemaphoreAlloc(tODSemaphoreHandle *phSemaphore, INT nInitialCount,
   INT nMaximumCount)
{
   ASSERT(phSemaphore != NULL);
   ASSERT(nInitialCount >= 0);
   ASSERT(nMaximumCount >= nInitialCount);

#ifdef ODPLAT_WIN32
   *phSemaphore = CreateSemaphore(NULL, (LONG)nInitialCount,
      (LONG)nMaximumCount, NULL);

   return(*phSemaphore == NULL ? kODRCGeneralFailure : kODRCSuccess);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODSemaphoreFree()
 *
 * Deallocates a semaphore that we previously created by ODSemaphoreAlloc().
 *
 * Parameters: hSemaphore - Handle to semaphore to deallocate.
 *
 *     Return: void
 */
void ODSemaphoreFree(tODSemaphoreHandle hSemaphore)
{
   ASSERT(hSemaphore != NULL);

#ifdef ODPLAT_WIN32
   DeleteObject(hSemaphore);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODSemaphoreUp()
 *
 * Increments the count of the specified semaphore.
 *
 * Parameters: hSemaphore   - Semaphore to increment.
 *
 *             nIncrementBy - Amount to add to the semaphore's current value.
 *
 *     Return: void
 */
void ODSemaphoreUp(tODSemaphoreHandle hSemaphore, INT nIncrementBy)
{
   ASSERT(hSemaphore != NULL);
   ASSERT(nIncrementBy > 0);

#ifdef ODPLAT_WIN32
   ReleaseSemaphore(hSemaphore, nIncrementBy, NULL);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODSemaphoreDown()
 *
 * Decrements the count of the specified semaphore. A semaphore may never have
 * a value less than 0. Hence, an attempt to decrement the value of a
 * semaphore below zero will cause the calling thread to be blocked until some
 * other thread increments the semaphore.
 *
 * Parameters: hSemaphore   - Handle to the semaphore to decrement.
 *
 *             Timeout      - Maximum time to wait for the semaphore to be
 *                            incremented, or OD_NO_TIMEOUT to prevent this
 *                            function from returning before the semaphore is
 *                            incremented.
 *
 *     Return: kODRCSuccess, or kODRCTimeout if the semaphore was not
 *             decremented before Timeout milliseconds elapsed.
 */
tODResult ODSemaphoreDown(tODSemaphoreHandle hSemaphore, tODMilliSec Timeout)
{
   ASSERT(hSemaphore != NULL);

#ifdef ODPLAT_WIN32
   if(WaitForSingleObject(hSemaphore, Timeout) != WAIT_OBJECT_0)
   {
      return(kODRCTimeout);
   }
#endif /* ODPLAT_WIN32 */

   /* Return with success. */
   return(kODRCSuccess);
}

#endif /* OD_MULTITHREADED */


/* ----------------------------------------------------------------------------
 * ODProcessExit()
 *
 * Ends the current process.
 *
 * Parameters: nExitCode - Exit code to return to the calling process.
 *
 *     Return: Never returns.
 */
void ODProcessExit(INT nExitCode)
{
#ifdef ODPLAT_WIN32
   ExitProcess(nExitCode);
#else /* !ODPLAT_WIN32 */
   exit(nExitCode);
#endif /* !ODPLAT_WIN32 */
}


/* ========================================================================= */
/* Millisecond timer functions.                                              */
/* ========================================================================= */

#ifdef ODPLAT_DOS
/* For the DOS platform, we need to know the number of milliseconds per */
/* clock tick.                                                          */
#define MILLISEC_PER_TICK 55           /* (approx. == 1000 / CLOCKS_PER_SEC) */
#endif /* ODPLAT_DOS */

/* ----------------------------------------------------------------------------
 * ODTimerStart()
 *
 * Starts a timer for a specified number of milliseconds. Future calls to
 * ODTimerElapsed() can be used to determine whether or not specified time
 * has elapsed. Note that while this function accepts its parameter in
 * milliseconds, it does not gurantee millisecond resolution. In fact, under
 * DOS, this timer mechanism has just under a 55 millisecond resolution, with
 * an average error of about 27 milliseconds.
 *
 * Parameters: pTimer   - Pointer to a tODTimer structure that will be later
 *                        passed to ODTimerElapsed().
 *
 *             Duration - Number of milliseconds after which timer should
 *                        elapse.
 *
 *     Return: void
 */
void ODTimerStart(tODTimer *pTimer, tODMilliSec Duration)
{
#ifdef ODPLAT_NIX
   struct timeval tv;
#endif
   ASSERT(pTimer != NULL);
   ASSERT(Duration >= 0);

#ifdef ODPLAT_DOS
   /* Store timer start time right away. */
   pTimer->Start = clock();

   /* Calculate duration of timer. */
   ODDWordDivide((DWORD *)&pTimer->Duration, NULL, Duration,
      MILLISEC_PER_TICK);
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   /* Store timer start time now. */
   pTimer->Start = GetTickCount();
   pTimer->Duration = Duration;
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   gettimeofday(&tv,NULL);
   pTimer->Start=tv.tv_sec*1000+tv.tv_usec/1000;
   pTimer->Duration = Duration;
#endif
}


/* ----------------------------------------------------------------------------
 * ODTimerElapsed()
 *
 * Determines whether or not a timer set by ODTimerStart() has elapsed.
 *
 * Parameters: pTimer - Pointer to a tODTimer structure that was populated
 *                      by ODTimerStart().
 *
 *     Return: TRUE if timer has elapsed, FALSE if it has not.
 */
BOOL ODTimerElapsed(tODTimer *pTimer)
{
   ASSERT(pTimer != NULL);

#ifdef ODPLAT_DOS
   return(clock() > pTimer->Start + pTimer->Duration
      || clock() < pTimer->Start);
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   return(ODTimerLeft(pTimer)==0);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
	return(ODTimerLeft(pTimer)==0);
#endif
}


/* ----------------------------------------------------------------------------
 * ODTimerWaitForElapse()
 *
 * Sleeps until the specified timer elapses.
 *
 * Parameters: pTimer - Pointer to a tODTimer structure that was populated
 *                      by ODTimerStart().
 *
 *     Return: void
 */
void ODTimerWaitForElapse(tODTimer *pTimer)
{
   ASSERT(pTimer != NULL);

#ifdef ODPLAT_DOS

   /* Under DOS, our timer resolution is low enough (only 18.2 ticks per    */
   /* second), that we cannot accurately calculate the time to sleep for.   */
   /* For this reason, we simply loop until the timer has elapsed, yielding */
   /* control to other tasks if when the timer has not elapsed.             */

   /* While timer has not elapsed. */
   while(!ODTimerElapsed(pTimer))
   {
      /* Let other tasks run. */
      od_sleep(0);
   }

#else /* !ODPLAT_DOS */
   /* Under other platforms, timer resolution is high enough that we can */
   /* ask the OS to block this thread for the amount of time required    */
   /* for the timer to elapse.                                           */

   od_sleep(ODTimerLeft(pTimer));
#endif /* !ODPLAT_DOS */
}


/* ----------------------------------------------------------------------------
 * ODTimerLeft()
 *
 * Determines the number of milliseconds left before the timer elapses.
 *
 * Parameters: pTimer - Pointer to a tODTimer structure that was populated
 *                      by ODTimerStart().
 *
 *     Return: Number of milliseconds before timer elapses, or 0 if the timer
 *             has already elapsed.
 */
tODMilliSec ODTimerLeft(tODTimer *pTimer)
{
#ifdef ODPLAT_NIX
   struct timeval tv;
   time_t nowtick;
#endif
   ASSERT(pTimer != NULL);

#ifdef ODPLAT_DOS
   {
      clock_t Now = clock();
      clock_t Left;

      /* If timer has elapsed, return 0. */
      if(Now > pTimer->Start + pTimer->Duration
         || Now < pTimer->Start)
      {
         return(0);
      }

      Left = pTimer->Start + pTimer->Duration - Now;

      return(ODDWordMultiply(Left, MILLISEC_PER_TICK));
   }
#elif defined(ODPLAT_NIX)
   gettimeofday(&tv,NULL);
   nowtick=tv.tv_sec*1000+(tv.tv_usec/1000);
   if(pTimer->Start+pTimer->Duration <= nowtick)
      return(0);
   return((tODMilliSec)(pTimer->Start + pTimer->Duration - nowtick));
#else /* !ODPLAT_DOS */
   {
      tODMilliSec Now;

#ifdef ODPLAT_WIN32      
      Now = GetTickCount();
#endif /* ODPLAT_WIN32 */

      /* If timer has elapsed, return 0. */
      if(Now > pTimer->Start + pTimer->Duration
         || Now < pTimer->Start)
      {
         return(0);
      }

      return(pTimer->Start + pTimer->Duration - Now);
   }
#endif /* !ODPLAT_DOS */
}


/* ----------------------------------------------------------------------------
 * od_sleep()
 *
 * Sleeps for the specified number of milliseconds, being as friendly to other
 * running tasks as possible. Under DOS, this function uses the ODTimerStart()/
 * ODTimerElapsed() mechanism, and so its accuracy is limited by the accuracy
 * of that mechanism.
 *
 * Parameters: Milliseconds - Number of milliseconds to sleep. A value of 0
 *                            allows any other waiting processes to run for
 *                            the rest of the current timeslice.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_sleep(tODMilliSec Milliseconds)
{
#ifdef ODPLAT_NIX
   struct timeval tv;
   struct timeval start;
   time_t started;
   time_t left
#endif
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_sleep()");

   /* Ensure that OpenDoors is initialized before proceeding. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

#ifdef ODPLAT_DOS
   if(Milliseconds == 0)
   {
      ODPlatYield();
   }
   else
   {
      tODTimer SleepTimer;
      ODTimerStart(&SleepTimer, Milliseconds);
      while(!ODTimerElapsed(&SleepTimer))
      {
         /* Let other tasks run. */
         ODPlatYield();
      }
   }
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   Sleep(Milliseconds);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   if(Milliseconds==0)  {
      /* Prevent 100% CPU *only* no delay is actually required here */
      tv.tv_sec=0;
      tv.tv_usec=1000;
      select(0,NULL,NULL,NULL,&tv);
   }
   else  {
      gettimeofday(&start,NULL);
	  started=start.tv_sec*1000+(start.tv_usec/1000);

      while(1)  {
	     /* This is timing sensitive and *MUST* wait for at least Milliseconds regardless of 100% CPU or signals */
         gettimeofday(&tv,NULL);
		 left=tv.tv_sec*1000+(tv.tv_usec/1000);
		 left-=started;
		 left=Milliseconds-left;
         tv.tv_sec = left/1000;
         tv.tv_usec = (left*1000)%1000000;
         if(tv.tv_sec<0 || tv.tv_usec<0)
            break;
         if(!select(0,NULL,NULL,NULL,&tv))
            break;
      }
   }
#endif

   OD_API_EXIT();
}


/* ========================================================================= */
/* Directory access.                                                         */
/* ========================================================================= */

/* Structure for directories entries returned by DOS. */
#ifdef ODPLAT_DOS
typedef struct
{
   BYTE abtReserved[21];
   BYTE btAttrib;
   WORD wFileTime;
   WORD wFileDate;
   DWORD dwFileSize;
   char szFileName[13];
} tDOSDirEntry;
#endif /* ODPLAT_DOS */


/* Dir handle structure. */
typedef struct
{
   BOOL bEOF;
#ifdef ODPLAT_DOS
   tDOSDirEntry FindBlock;
#endif /* ODPLAT_DOS */
#ifdef ODPLAT_WIN32
   HANDLE hWindowsDir;
   WIN32_FIND_DATA WindowsDirEntry;
   int wAttributes;
#endif /* ODPLAT_WIN32 */
#ifdef ODPLAT_NIX
   glob_t	g;
   int		pos;
   int		wAttributes;
#endif
} tODDirInfo;


/* Directory access private function prototypes. */
#if defined(ODPLAT_DOS) || defined(ODPLAT_WIN32)
static time_t DOSToCTime(WORD wDate, WORD wTime);
#endif
#ifdef ODPLAT_DOS
static INT ODDirDOSFindFirst(CONST char *pszPath, tDOSDirEntry *pBlock,
   WORD wAttributes);
static INT ODDirDOSFindNext(tDOSDirEntry *pBlock);
#endif /* ODPLAT_DOS */
#ifdef ODPLAT_WIN32
static BOOL ODDirWinMatchesAttributes(tODDirInfo *pDirInfo);
#endif /* ODPLAT_WIN32 */


/* ----------------------------------------------------------------------------
 * ODDirOpen()
 *
 * Opens a directory for future access using ODDirRead(). On Success,
 * ODDirOpen() provides a directory handle that represents a list of directory
 * entries that match the specified path and attributes. When finished with
 * the directory handle, the caller should release it using ODDirClose().
 *
 * Parameters: pszPath     - Directory with filename (wildcards are supported),
 *                           for which matching files should be found. If there
 *                           are no matching files, ODDirOpen() returns with
 *                           kODRCNoMatch.
 *
 *             nAttributes - One or more of the DIR_ATTRIB_... constants,
 *                           connected by the bitmap-OR (|) operator.
 *
 *             phDir       - Pointer to a tODDirHandle, into which ODDirOpen()
 *                           will place a valid directory handle if and only
 *                           if it returns kODRCSuccess.
 *
 *     Return: A tODResult indicating success or reason for failure.
 */
tODResult ODDirOpen(CONST char *pszPath, WORD wAttributes, tODDirHandle *phDir)
{
   tODDirInfo *pDirInfo;

   ASSERT(pszPath != NULL);
   ASSERT(phDir != NULL);

   /* Attempt to allocate a directory information structure. */
   if((pDirInfo = malloc(sizeof(tODDirInfo))) == NULL)
   {
      /* If unable to allocate enough memory, return this state to the */
      /* caller. */
      return(kODRCNoMemory);
   }

   /* Initialize directory information structure. */
   pDirInfo->bEOF = FALSE;

#ifdef ODPLAT_DOS
   /* Read the first matching directory entry structure. */
   if(ODDirDOSFindFirst(pszPath, &pDirInfo->FindBlock, wAttributes))
   {
      /* If unable to read directory entry, release directory information   */
      /* structure, and return indicating that there are no matching files. */
      free(pDirInfo);
      return(kODRCNoMatch);
   }
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   /* Store a copy of the attributes passed to open function. */
   pDirInfo->wAttributes = wAttributes;

   /* Attempt to read first directory entry. */
   pDirInfo->hWindowsDir = FindFirstFile(pszPath, &pDirInfo->WindowsDirEntry);

   if(pDirInfo->hWindowsDir == INVALID_HANDLE_VALUE)
   {
      /* If unable to read directory entry, release directory information   */
      /* structure, and return indicating that there are no matching files. */
      free(pDirInfo);
      return(kODRCNoMatch);
   }

   /* If first file doesn't match specified attributes, then find one that */
   /* does.                                                                */
   /* Find next matching entry, if any. */
   while(!ODDirWinMatchesAttributes(pDirInfo))
   {
      if(!FindNextFile(pDirInfo->hWindowsDir, &pDirInfo->WindowsDirEntry))
      {
         /* If unable to find matching directory entry, then release       */
         /* structure, return indicating that there are no matching files. */
         free(pDirInfo);
         return(kODRCNoMatch);
      }
   }
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   if(glob(pszPath,GLOB_NOSORT,NULL,&(pDirInfo->g)))
      return(kODRCNoMatch);
   if(pDirInfo->g.gl_pathc==0)  {
      globfree(&(pDirInfo->g));
      return(kODRCNoMatch);
   }
   pDirInfo->pos=0;
   pDirInfo->wAttributes = wAttributes;
#endif

   /* Now that open operation is complete, give the caller a directory */
   /* handle. */
   *phDir = ODPTR2HANDLE(pDirInfo, tODDirInfo);

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODDirRead()
 *
 * Reads the next directory entry from an open directory, placing the directory
 * information into the tODDirEntry structure pointed to by pDirEntry.
 *
 * Parameters: hDir      - Handle to an open directory, as provided by
 *                         ODDirOpen().
 *
 *             pDirEntry - Pointer to structure into which directory entry
 *                         information should be placed.
 *
 *     Return: A tODResult indicating success or reason for failure. After the
 *             last directory entry has been read, all subsequent calls to
 *             ODDirRead() will return kODRCEndOfFile.
 */
tODResult ODDirRead(tODDirHandle hDir, tODDirEntry *pDirEntry)
{
   tODDirInfo *pDirInfo = ODHANDLE2PTR(hDir, tODDirInfo);
#ifdef ODPLAT_WIN32
   WORD wDOSDate;
   WORD wDOSTime;
#endif /* ODPLAT_WIN32 */
#ifdef ODPLAT_NIX
   struct stat st;
#endif
   
   ASSERT(pDirEntry != NULL);
   ASSERT(pDirInfo != NULL);

   /* Check whether the last directory entry has been returned yet. */
   if(pDirInfo->bEOF)
   {
      /* Return this state information to the caller. */
      return(kODRCEndOfFile);
   }

#ifdef ODPLAT_DOS
   /* Provide the caller with the information from the previously read */
   /* directory entry.                                                 */

   /* Copy the filename to the caller's structure. */
   ODStringCopy(pDirEntry->szFileName, pDirInfo->FindBlock.szFileName,
      DIR_FILENAME_SIZE);

   /* Copy the attributes to the caller's structure. */
   pDirEntry->wAttributes = pDirInfo->FindBlock.btAttrib;

   /* Copy the file size to the caller's structure. */
   pDirEntry->dwFileSize = pDirInfo->FindBlock.dwFileSize;

   /* Determine the last file write time, in C library time format. */
   pDirEntry->LastWriteTime = DOSToCTime(pDirInfo->FindBlock.wFileDate,
      pDirInfo->FindBlock.wFileTime);

   /* Read next directory entry, if any. */
   pDirInfo->bEOF = ODDirDOSFindNext(&pDirInfo->FindBlock);

#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   /* Provide the caller with the information from the previously read */
   /* directory entry.                                                 */

   /* Copy filename from Win32 8.3 filename. */
   if(strlen(pDirInfo->WindowsDirEntry.cAlternateFileName) == 0)
   {
      ODStringCopy(pDirEntry->szFileName,
         pDirInfo->WindowsDirEntry.cFileName, DIR_FILENAME_SIZE);
   }
   else
   {
      ODStringCopy(pDirEntry->szFileName,
         pDirInfo->WindowsDirEntry.cAlternateFileName, DIR_FILENAME_SIZE);
   }

   /* Copy attribute bits. */
   pDirEntry->wAttributes = DIR_ATTRIB_NORMAL;
   if(pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
   {
      pDirEntry->wAttributes |= DIR_ATTRIB_ARCH;
   }
   if(pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
   {
      pDirEntry->wAttributes |= DIR_ATTRIB_DIREC;
   }
   if(pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
   {
      pDirEntry->wAttributes |= DIR_ATTRIB_HIDDEN;
   }
   if(pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
   {
      pDirEntry->wAttributes |= DIR_ATTRIB_RDONLY;
   }
   if(pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
   {
      pDirEntry->wAttributes |= DIR_ATTRIB_SYSTEM;
   }

   /* Copy the file size to the caller's structure. */
   pDirEntry->dwFileSize = (long)pDirInfo->WindowsDirEntry.nFileSizeLow;

   /* Determine the last file write time, in C library time format. */
   FileTimeToDosDateTime(&pDirInfo->WindowsDirEntry.ftLastWriteTime, &wDOSDate,
      &wDOSTime);
   pDirEntry->LastWriteTime = DOSToCTime(wDOSDate, wDOSTime);

   /* Find next matching entry, if any. */
   do
   {
      if(!FindNextFile(pDirInfo->hWindowsDir, &pDirInfo->WindowsDirEntry))
      {
         pDirInfo->bEOF = TRUE;
      }
   } while(!ODDirWinMatchesAttributes(pDirInfo));
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   while(!pDirInfo->bEOF)  {
      if(strrchr(pDirInfo->g.gl_pathv[pDirInfo->pos],DIRSEP)==NULL)
	     strcpy(pDirEntry->szFileName,pDirInfo->g.gl_pathv[pDirInfo->pos]);
	  else
	     strcpy(pDirEntry->szFileName,strrchr(pDirInfo->g.gl_pathv[pDirInfo->pos],DIRSEP));
	  stat(pDirInfo->g.gl_pathv[pDirInfo->pos],&st);
	  pDirEntry->wAttributes=DIR_ATTRIB_NORMAL;
	  if(st.st_mode & S_IFDIR)
	  	 pDirEntry->wAttributes |= DIR_ATTRIB_DIREC;
	  if(!st.st_mode & S_IWUSR)
	  	 pDirEntry->wAttributes |= DIR_ATTRIB_RDONLY;
	  if(!st.st_mode & S_IRUSR)
	  	 pDirEntry->wAttributes |= DIR_ATTRIB_SYSTEM;
	  pDirEntry->LastWriteTime=st.st_mtime;
	  pDirEntry->dwFileSize=st.st_size;
	  pDirInfo->pos++;
	  if(pDirInfo->pos==pDirInfo->g.gl_pathc)
	     pDirInfo->bEOF=TRUE;
	  if(pDirEntry->wAttributes==pDirInfo->wAttributes)
	     return(kODRCSuccess);
	  if(pDirInfo->bEOF==TRUE)
	    return(kODRCEndOfFile);
   }
#endif

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODDirClose()
 *
 * Closes and open directory handle.
 *
 * Parameters: hDir - Handle to an open directory handle, as provided by the
 *                    ODDirOpen() function.
 *
 *     Return: void
 */
void ODDirClose(tODDirHandle hDir)
{
   tODDirInfo *pDirInfo = ODHANDLE2PTR(hDir, tODDirInfo);

   ASSERT(pDirInfo != NULL);

#ifdef ODPLAT_WIN32
   /* Under Win32, close directory handle. */
   FindClose(pDirInfo->hWindowsDir);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   globfree(&(pDirInfo->g));
#endif

   /* Free the directory information structure. */
   free(pDirInfo);
}


#if defined(ODPLAT_DOS) || defined(ODPLAT_WIN32)
/* ----------------------------------------------------------------------------
 * DOSToCTime()                                        *** PRIVATE FUNCTION ***
 *
 * Converts DOS directory entry time format to the C library time format.
 *
 * Parameters: uDate - Date portion of the time to be converted.
 *
 *             uTime - Time of day portion of the time to be converted.
 *
 *     Return: The specified time, represented as a time_t.
 */
static time_t DOSToCTime(WORD wDate, WORD wTime)
{
   struct tm TimeStruct;

   TimeStruct.tm_sec = (wTime & 0x001f) * 2;
   TimeStruct.tm_min = (wTime & 0x07e0) >> 5;
   TimeStruct.tm_hour = (wTime & 0xf800) >> 11;
   TimeStruct.tm_mday = wDate & 0x001f;
   TimeStruct.tm_mon = ((wDate & 0x01e0) >> 5) - 1;
   TimeStruct.tm_year = 80 + ((wDate & 0xfe00) >> 9);

   return(mktime(&TimeStruct));
}
#endif


/* MS-DOS specific functions for directory access. */
#ifdef ODPLAT_DOS

/* ----------------------------------------------------------------------------
 * ODDirDOSFindFirst()                                 *** PRIVATE FUNCTION ***
 *
 * MS-DOS specific "Find First" function for reading directory entries. This
 * is essentially just a C-language interface to the interrupt function call
 * that is provided by DOS.
 *
 * Parameters: pszPath     - Pointer to string containing directory and
 *                           filespec to search for.
 *
 *             pBlock      - Pointer to directory block.
 *
 *             nAttributes - Attributes to match, if any.
 *
 *     Return: 0 on success, -1 on failure.
 */
static int ODDirDOSFindFirst(CONST char *pszPath, tDOSDirEntry *pBlock,
   WORD wAttributes)
{
   int nToReturn;

   ASSERT(pszPath != NULL);
   ASSERT(pBlock != NULL);

   ASM     push ds
   ASM     mov ah, 0x2f            /* Int 0x21, ah=0x2f: Get current DOS DTA */
   ASM     int 0x21                                       /* Get current DTA */
   ASM     push bx                   /* Store offset of current DTA on stack */
   ASM     push es                  /* Store segment of current DTA on stack */
   ASM     mov ah, 0x1a                /* Int 0x21, ah=0x1a: Set new DOS DTA */
#ifdef LARGEDATA                                    /* If using far pointers */
   ASM     lds dx, pBlock           /* Load DS:DX with far address of pBlock */
#else                                              /* If using near pointers */
   ASM     mov dx, pBlock             /* Load DX with near address of pBlock */
#endif
   ASM     int 0x21                                           /* Set DOS DTA */
   ASM     mov ah, 0x4e         /* Int 0x21, ah=0x4e: DOS findfirst function */
   ASM     mov cx, wAttributes                    /* Load attributes into CX */
#ifdef LARGEDATA                                    /* If using far pointers */
   ASM     lds dx, pszPath         /* Load DS:DX with far address in pszPath */
#else                                              /* If using near pointers */
   ASM     mov dx, pszPath           /* Load DX with near address in pszPath */
#endif
   ASM     int 0x21                               /* Call findfirst function */
   ASM     jc error       /* If carry flag is set, then an error has ocurred */
   ASM     mov word ptr nToReturn, 0                /* If no error, return 0 */
   ASM     jmp after_result
error:
   ASM     mov word ptr nToReturn, -1                 /* If error, return -1 */
after_result:
   ASM     mov ah, 0x1a                /* Int 0x21, ah=0x1a: Set new DOS DTA */
   ASM     pop ds                   /* Pop original DTA segment off of stack */
   ASM     pop dx                      /* Pop original DTA offest from stack */
   ASM     int 0x21                             /* Reset DOS DTA to original */
   ASM     pop ds                   /* Restore DS stored at function startup */
   return(nToReturn);
}


/* ----------------------------------------------------------------------------
 * ODDirDOSFindNext()                                  *** PRIVATE FUNCTION ***
 *
 * MS-DOS specific "Find Next" function for reading directory entries. This
 * is essentially just a C-language interface to the interrupt function call
 * that is provided by DOS.
 *
 * Parameters: pBlock - Pointer to block in which to store next directory
 *                      entry.
 *
 *     Return: 0 on success, -1 on failure.
 */
static int ODDirDOSFindNext(tDOSDirEntry *pBlock)
{
   int nToReturn;

   ASSERT(pBlock != NULL);

   ASM     push ds                                                /* Save DS */
   ASM     mov ah, 0x2f            /* Int 0x21, ah=0x2f: Get current DOS DTA */
   ASM     int 0x21                                       /* Get current DTA */
   ASM     push bx                   /* Store offset of current DTA on stack */
   ASM     push es                  /* Store segment of current DTA on stack */
   ASM     mov ah, 0x1a                /* Int 0x21, ah=0x1a: Set new DOS DTA */
#ifdef LARGEDATA                                    /* If using far pointers */
   ASM     lds dx, pBlock           /* Load DS:DX with far address of pBlock */
#else                                              /* If using near pointers */
   ASM     mov dx, pBlock             /* Load DX with near address of pBlock */
#endif
   ASM     int 0x21                                           /* Set DOS DTA */
   ASM     mov ah, 0x4f          /* Int 0x21, ah=0x4f: DOS findnext function */
   ASM     int 0x21                               /* Call findfirst function */
   ASM     jc error       /* If carry flag is set, then an error has ocurred */
   ASM     mov word ptr nToReturn, 0                /* If no error, return 0 */
   ASM     jmp after_result
error:
   ASM     mov word ptr nToReturn, -1                 /* If error, return -1 */
after_result:
   ASM     mov ah, 0x1a                /* Int 0x21, ah=0x1a: Set new DOS DTA */
   ASM     pop ds                   /* Pop original DTA segment off of stack */
   ASM     pop dx                      /* Pop original DTA offest from stack */
   ASM     int 0x21                             /* Reset DOS DTA to original */
   ASM     pop ds                   /* Restore DS stored at function startup */
   return(nToReturn);
}

#endif /* ODPLAT_DOS */


/* Win32 specific private functions for directory access. */
#ifdef ODPLAT_WIN32
/* ----------------------------------------------------------------------------
 * ODDirWinMatchesAttributes()                         *** PRIVATE FUNCTION ***
 *
 * Determines whether or not the directory entry pDirInfo->WindowsDirEntry
 * meets the attribute requirements specified in pDirInfo->nAttributes
 *
 * Parameters: pDirInfo - Pointer to a directory information structure with
 *                        attribute and directory entry values.
 *
 *     Return: TRUE if the file matches the attributes, FALSE if it does not.
 */
static BOOL ODDirWinMatchesAttributes(tODDirInfo *pDirInfo)
{
   if((pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      && !(pDirInfo->wAttributes & DIR_ATTRIB_DIREC))
   {
      return(FALSE);
   }

   if((pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
      && !(pDirInfo->wAttributes & DIR_ATTRIB_ARCH))
   {
      return(FALSE);
   }

   if((pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
      && !(pDirInfo->wAttributes & DIR_ATTRIB_HIDDEN))
   {
      return(FALSE);
   }

   if((pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
      && !(pDirInfo->wAttributes & DIR_ATTRIB_RDONLY))
   {
      return(FALSE);
   }

   if((pDirInfo->WindowsDirEntry.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
      && !(pDirInfo->wAttributes & DIR_ATTRIB_SYSTEM))
   {
      return(FALSE);
   }

   return(TRUE);
}
#endif /* ODPLAT_WIN32 */


/* ----------------------------------------------------------------------------
 * ODDirChangeCurrent()
 *
 * Changes current directory to the one specified.
 *
 * Parameters: pszPath - String containing path to change to.
 *
 *     Return: void
 */
void ODDirChangeCurrent(char *pszPath)
{
#ifdef ODPLAT_DOS
   int nDrive = 0;

   if(pszPath[1] == ':')
   {
      nDrive = (toupper(pszPath[0]) - 'A');
   }

   _setdrvcd(nDrive, (char *)pszPath);
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   SetCurrentDirectory(pszPath);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   chdir(pszPath);
#endif
}


/* ----------------------------------------------------------------------------
 * ODDirGetCurrent()
 *
 * Obtains the name of the  current directory, including the current drive
 * designator.
 *
 * Parameters: pszPath       - String containing path to change to.
 *
 *             nMaxPathChars - Maximum characters in the buffer pointer to by
 *                             pszPath.
 *
 *     Return: void
 */
void ODDirGetCurrent(char *pszPath, INT nMaxPathChars)
{
   ASSERT(pszPath != NULL);
   ASSERT(nMaxPathChars > 0);

#ifdef ODPLAT_DOS
   UNUSED(nMaxPathChars);

   strcpy(pszPath, "X:\\");
   pszPath[0] = 'A' + _getdrv();
   _getcd(0, (char *)pszPath + 3);
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   GetCurrentDirectory(nMaxPathChars, pszPath);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   getcwd(pszPath,nMaxPathChars);
#endif

   ASSERT((INT)strlen(pszPath) + 1 <= nMaxPathChars);
}


/* ========================================================================= */
/* Misc. Functions                                                           */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODFileDelete()
 *
 * Deletes the file with the specified filename.
 *
 * Parameters: pszPath - Filename, possibly with path, to delete.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODFileDelete(CONST char *pszPath)
{
#ifdef ODPLAT_DOS
   {
      tODResult Result;

      ASM    push ds
#ifdef LARGEDATA
      ASM    lds dx, pszPath
#else /* !LARGEDATA */
      ASM    mov ax, ss
      ASM    mov ds, ax
      ASM    mov dx, pszPath
#endif /* !LARGEDATA */
      ASM    mov ah, 0x41
      ASM    int 0x21
      ASM    jc Failure
      ASM    mov word ptr Result, kODRCSuccess
      ASM    jmp Done
Failure:
      ASM    mov word ptr Result, kODRCGeneralFailure
Done:
      ASM    pop ds

      return(Result);
   }
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   return(DeleteFile(pszPath) ? kODRCSuccess : kODRCGeneralFailure);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   return(unlink(pszPath));
#endif
}


/* ----------------------------------------------------------------------------
 * ODFileAccessMode()
 *
 * Determines the access permissions of a file.
 *
 * Parameters: pszFilename - Name of file to test.
 *
 *             nAccessMode - Indicates which file access mode to test for.
 *                           A value of 0 indicates existance, 2 indicates
 *                           write permission, 4 indicates read permission,
 *                           and 6 indicates read/write permission.
 *
 *     Return: FALSE if file can be accessed or TRUE if file cannot be
 *             accessed.
 */
BOOL ODFileAccessMode(char *pszFilename, int nAccessMode)
{
   FILE *pfFileToTest;
   char *pszModeString;
   tODDirHandle hDir;

#ifdef ODPLAT_DOS
   BYTE nLength;
   /* If we are looking for the root directory. */
   nLength = strlen(pszFilename);
   if((nLength == 3 && pszFilename[1] == ':' && pszFilename[2] == DIRSEP) ||
      (nLength == 1 && pszFilename[0] == DIRSEP))
   {
      if(nAccessMode == 0)
      {
          int to_return = FALSE;

#ifdef LARGEDATA
         ASM push ds
         ASM lds dx, pszFilename
#else
         ASM mov dx, pszFilename
#endif
         ASM mov ax, 0x4300
         ASM int 0x21
         ASM jnc done
         ASM mov word ptr to_return, TRUE
done:
#ifdef LARGEDATA
         ASM pop ds
#endif
          return(to_return);
      }
      else
      {
          return(TRUE);
      }
   }
#endif /* ODPLAT_DOS */

   /* If the file doesn't exit, we fail in any mode. */
   if(ODDirOpen(pszFilename,
      DIR_ATTRIB_ARCH | DIR_ATTRIB_RDONLY | DIR_ATTRIB_DIREC,
      &hDir) != kODRCSuccess)
   {
      return(TRUE);
   }

   /* If directory open succeeded, then close it again. */
   ODDirClose(hDir);

   /* If the file does exist, then amode 0 is satisfied. */
   if(nAccessMode == 0) return(FALSE);

   /* If testing for an access permission, determine corresponding fopen() */
   /* mode.                                                                */
   switch(nAccessMode)
   {
      case 2:
        pszModeString = "a";
        break;
      case 4:
        pszModeString = "r";
        break;
      default:
         pszModeString = "r+";
   }

   /* Attempt to open the file, if unable to do so return failure. */
   if((pfFileToTest=fopen(pszFilename,pszModeString)) == NULL) return(TRUE);

   /* If file open was successful, close it again, and return success. */
   fclose(pfFileToTest);
   return(FALSE);
}
