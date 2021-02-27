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
 *        File: ODKrnl.c
 *
 * Description: Contains the OpenDoors kernel, which is responsible for many
 *              of the core functions which continue regardless of what the
 *              client program is doing. The implementation of this file is
 *              central to the OpenDoors architecture. The functionality
 *              implemented by the OpenDoors kernel includes (but is not
 *              limited to):
 *
 *                     - Obtaining and  input from the user, through the modem
 *                       and possibly the local keyboard.
 *                     - Monitoring maximum time and inactivity time limits.
 *                     - Responding to loss of carrier.
 *                     - Forcing the status line to be updated regularily,
 *                       on platforms that it exists.
 *                     - Implementing the system operator <-> remote user chat
 *                       mode.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Jan 01, 1995  6.00  BP   Split off from odcore.c
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 15, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Nov 21, 1995  6.00  BP   Ported to Win32.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 13, 1995  6.00  BP   Moved chat mode code to ODKrnl.h.
 *              Dec 24, 1995  6.00  BP   od_chat_active = TRUE on chat start.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 04, 1996  6.00  BP   tODInQueueEvent -> tODInputEvent.
 *              Jan 12, 1996  6.00  BP   Added bOnlyShiftArrow.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 30, 1996  6.00  BP   Add semaphore timeout.
 *              Feb 06, 1996  6.00  BP   Added od_silent_mode.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 23, 1996  6.00  BP   Only create active semapore once.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Prevent TC generated N_SCOPY@ call.
 *              Mar 13, 1996  6.10  BP   bOnlyShiftArrow -> nArrowUseCount.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Oct 22, 2001  6.21  RS   Lowered thread priorities to normal.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#include "OpenDoor.h"
#ifdef ODPLAT_NIX
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include "ODCore.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODCom.h"
#include "ODKrnl.h"
#include "ODScrn.h"
#include "ODInQue.h"
#include "ODInEx.h"
#ifdef ODPLAT_WIN32
#include "ODFrame.h"
#endif /* ODPLAT_WIN32 */


/* Multithreading performance tuning. */
#define REMOTE_INPUT_THREAD_PRIORITY      OD_PRIORITY_NORMAL /* was ABOVE_NORMAL */
#define NO_CARRIER_THREAD_PRIORITY        OD_PRIORITY_NORMAL /* was ABOVE_NORMAL */
#define NO_CARRIER_THREAD_SLEEP_TIME      6000
#define TIME_UPDATE_THREAD_PRIORITY       OD_PRIORITY_NORMAL
#define TIME_UPDATE_THREAD_SLEEP_TIME     3000

/* Misc performance tuning. */
#define STATUS_UPDATE_PERIOD        3L
#define CHAT_YIELD_PERIOD           25L

/* Pending command identifiers. */
#define KERNEL_FUNC_CHATTOGGLE      0x0001

/* Private function prototypes. */
static void ODKrnlHandleReceivedChar(char chReceived, BOOL bFromRemote);
static void ODKrnlTimeUpdate(void);
static void ODKrnlChatCleanup(void);
static void ODKrnlChatMode(void);
#ifdef ODPLAT_NIX
#ifdef USE_KERNEL_SIGNAL
static void sig_run_kernel(int sig);
static void sig_get_char(int sig);
static void sig_no_carrier(int sig);
#endif
#endif

/* Functions specific to the multithreaded implementation of the kernel. */
#ifdef OD_MULTITHREADED
/* Thread proceedures. */
DWORD OD_THREAD_FUNC ODKrnlRemoteInputThread(void *pParam);
DWORD OD_THREAD_FUNC ODKrnlNoCarrierThread(void *pParam);
DWORD OD_THREAD_FUNC ODKrnlTimeUpdateThread(void *pParam);
DWORD OD_THREAD_FUNC ODKrnlChatThread(void *pParam);

/* Helper functions. */
static void ODKrnlWaitForExclusiveControl(void);
static void ODKrnlGiveUpExclusiveControl(void);
#endif /* OD_MULTITHREADED */

/* Local working variables. */
#ifdef OD_MULTITHREADED
static tODThreadHandle hRemoteInputThread = NULL;
static tODThreadHandle hNoCarrierThread = NULL;
static tODThreadHandle hTimeUpdateThread = NULL;
static tODThreadHandle hClientThread = NULL;
static tODThreadHandle hChatThread = NULL;
static BOOL bHaveExclusiveControl;
static BOOL bChatActivatedInternally;
#endif /* OD_MULTITHREADED */
static BOOL bKernelActive = FALSE;
static BOOL bWarnedAboutInactivity = FALSE;
static INT16 nLastInactivitySetting = 0;
static time_t nNextStatusUpdateTime;
static INT nKrnlFuncPending;
static BOOL bLastStatusSetting;
static INT16 nChatOriginalAttrib;

/* Global kernel-related variables. */
tODTimer RunKernelTimer;
time_t nNextTimeDeductTime;
char chLastControlKey = '\0';
INT nArrowUseCount = 0;
BOOL bForceStatusUpdate = FALSE;
BOOL bIsShell;
#ifdef OD_MULTITHREADED
tODSemaphoreHandle hODActiveSemaphore = NULL;
#endif /* OD_MULTITHREADED */



/* ========================================================================= */
/* Core of the OpenDoors Kernel.                                             */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODKrnlInitialize()
 *
 * Initializes kernel activities. In multithreaded versions of OpenDoors, this
 * is the function that starts the various kernel threads.
 *
 * Parameters: kODRCSuccess on success, or an error code on failure.
 *
 *     Return: void
 */
tODResult ODKrnlInitialize(void)
{
#ifdef ODPLAT_NIX
   sigset_t		block;
#ifdef USE_KERNEL_SIGNAL
   struct sigaction act;
   struct itimerval itv;
#endif
#endif

   tODResult Result = kODRCSuccess;
   
#ifdef ODPLAT_NIX
#ifdef USE_KERNEL_SIGNAL
   /* HUP Detection */
   act.sa_handler=sig_no_carrier;
   /* If two HUP signals are recieved, die on the second */
   act.sa_flags=SA_RESETHAND|SA_RESTART;
   sigemptyset(&(act.sa_mask));
   sigaction(SIGHUP,&act,NULL);

   /* Run kernel on SIGALRM (Every .01 seconds) */
   act.sa_handler=sig_run_kernel;
   act.sa_flags=SA_RESTART;
   sigemptyset(&(act.sa_mask));
   sigaction(SIGALRM,&act,NULL);
   itv.it_interval.tv_sec=0;
   itv.it_interval.tv_usec=10000;
   itv.it_value.tv_sec=0;
   itv.it_value.tv_usec=10000;
   setitimer(ITIMER_REAL,&itv,NULL);

   /* Make stdin signal driven. */
//   act.sa_handler=sig_get_char;
//   act.sa_flags=0;
//   sigemptyset(&(act.sa_mask));
//   sigaction(SIGIO,&act,NULL);
//
//   /* Have SIGIO signals delivered to this process */
//   fcntl(0,F_SETOWN,getpid());
//   
//   /* Enable SIGIO when read possible on stdin */
//   fcntl(0,F_SETFL,fcntl(0,F_GETFL)|O_ASYNC); 

   /* Make sure SIGHUP, SIGALRM, and SIGIO are unblocked */
   sigemptyset(&block);
   sigaddset(&block,SIGHUP);
   sigaddset(&block,SIGALRM);
#if 0
   sigaddset(&block,SIGIO);
#endif
   sigprocmask(SIG_UNBLOCK,&block,NULL);
#else	/* Using ODComCarrier... don't catch HUP signal */
   sigemptyset(&block);
   sigaddset(&block,SIGHUP);
   sigprocmask(SIG_BLOCK,&block,NULL);
#endif
#endif

   /* Initialize time of next status update and next time deduction. */
   nNextStatusUpdateTime = time(NULL) + STATUS_UPDATE_PERIOD;
   nNextTimeDeductTime = time(NULL) + 60L;
   bLastStatusSetting = od_control.od_status_on = TRUE;

   /* Initially, no kernel functions are pending. */
   nKrnlFuncPending = 0;

   /* Initially, the kernel is not active. */
   bKernelActive = FALSE;

#ifdef OD_MULTITHREADED
   /* Initially, we do not have exclusive control of the application. */
   bHaveExclusiveControl = FALSE;

   /* Obtain a handle to the client thread. */
   hClientThread = ODThreadGetCurrent();

   /* Create OpenDoors activation semaphore. */
   if(hODActiveSemaphore == NULL)
   {
      Result = ODSemaphoreAlloc(&hODActiveSemaphore, 0, INT_MAX);
      if(Result != kODRCSuccess) return(Result);
   }

   /* Start the remote input thread if we are not operating in local mode. */
   if(od_control.baud != 0)
   {
      Result = ODThreadCreate(&hRemoteInputThread, ODKrnlRemoteInputThread,
         NULL);
      if(Result != kODRCSuccess) return(Result);
      ODThreadSetPriority(hRemoteInputThread, REMOTE_INPUT_THREAD_PRIORITY);
   }

   /* Start the carrier detection thread if we are not operating in local */
   /* mode.                                                               */
   if(od_control.baud != 0)
   {
      Result = ODThreadCreate(&hNoCarrierThread, ODKrnlNoCarrierThread, NULL);
      if(Result != kODRCSuccess) return(Result);
      ODThreadSetPriority(hNoCarrierThread, NO_CARRIER_THREAD_PRIORITY);
   }

   /* Start the time update thread. */
   Result = ODThreadCreate(&hTimeUpdateThread, ODKrnlTimeUpdateThread, 0);
   if(Result != kODRCSuccess) return(Result);
   ODThreadSetPriority(hTimeUpdateThread, TIME_UPDATE_THREAD_PRIORITY);
#endif /* OD_MULTITHREADED */

   /* Return with success. */
   return(Result);
}


/* ----------------------------------------------------------------------------
 * ODKrnlShutdown()
 *
 * Shuts down kernel activities.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODKrnlShutdown(void)
{
   if(bKernelActive) return;

#ifdef OD_MULTITHREADED
#if defined(OD_DIAGNOSTICS) && defined(ODPLAT_WIN32)
   if(od_control.od_internal_debug)
      MessageBox(NULL, "Terminating remote input thread", "OpenDoors Diagnostics", MB_OK);
#endif
   /* Shutdown the remote input thread, if it exists. */
   if(hRemoteInputThread != NULL) ODThreadTerminate(hRemoteInputThread);

#if defined(OD_DIAGNOSTICS) && defined(ODPLAT_WIN32)
   if(od_control.od_internal_debug)
      MessageBox(NULL, "Terminating carrier detection", "OpenDoors Diagnostics", MB_OK);
#endif
   /* Shutdown the carrier detection thread, if it exists. */
   if(hNoCarrierThread != NULL) ODThreadTerminate(hNoCarrierThread);

#if defined(OD_DIAGNOSTICS) && defined(ODPLAT_WIN32)
   if(od_control.od_internal_debug)
      MessageBox(NULL, "Terminating time update thread", "OpenDoors Diagnostics", MB_OK);
#endif
   /* Shutdown the time update thread, if it exists. */
   if(hTimeUpdateThread != NULL) ODThreadTerminate(hTimeUpdateThread);

#if defined(OD_DIAGNOSTICS) && defined(ODPLAT_WIN32)
   if(od_control.od_internal_debug)
      MessageBox(NULL, "Releasing activation semaphore", "OpenDoors Diagnostics", MB_OK);
#endif
#endif /* OD_MULTITHREADED */
}


/* ----------------------------------------------------------------------------
 * od_kernel()
 *
 * Carries out any kernel tasks that must be performed through regular,
 * explicit calls to this function,
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_kernel(void)
{
#ifndef OD_MULTITHREADED
   char ch;
#ifdef ODPLAT_DOS
   WORD wKey;
   BYTE btShiftStatus;
   char *pszShellName;
#endif
   BOOL bCarrier;
#endif /* OD_MULTITHREADED */

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_kernel()");

   /* Initialize OpenDoors if not already done. */
   if(!bODInitialized) od_init();

   /* If this is an attempt at a re-entrant call to od_kernel() from another */
   /* function called by a currently active od_kernel(), then return without */
   /* doing anything.                                                        */
   if(bKernelActive) return;

   OD_API_ENTRY();

   /* Note that kernel is active to prevent recursive calls to the kernel. */
   bKernelActive = TRUE;

   /* Call od_ker_exec function if required. */
   if(od_control.od_ker_exec != NULL)
   {
      (*od_control.od_ker_exec)();
   }

   /* The remainder of od_kernel() only applies to non-multithreaded */
   /* versions of OpenDoors.                                         */
#ifndef OD_MULTITHREADED
   /* If not operating in local mode, then perform remote-mode specific */
   /* activies.                                                         */
   if(od_control.baud != 0)
   {
#ifndef USE_KERNEL_SIGNAL
      /* If carrier detection is enabled, then shutdown OpenDoors if */
      /* the carrier detect signal is no longer high.                */
      if(!(od_control.od_disable&DIS_CARRIERDETECT))
      {
         ODComCarrier(hSerialPort, &bCarrier);
         if(!bCarrier)
         {
            ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_NOCARRIER);
         }
      }
#endif

      /* Loop, obtaining any new characters from the serial port and */
      /* adding them to the common local/remote input queue.         */
      while(ODComGetByte(hSerialPort, &ch, FALSE) == kODRCSuccess)
      {
         ODKrnlHandleReceivedChar(ch, TRUE);
      }
   }

#ifdef ODPLAT_DOS
check_keyboard_again:
    if(nKrnlFuncPending && !bShellChatActive)
    {
       if(nKrnlFuncPending & KERNEL_FUNC_CHATTOGGLE)
       {
          nKrnlFuncPending &=~ KERNEL_FUNC_CHATTOGGLE;
          goto chat_pressed;
       }
    }

   /* Don't check local keyboard if sysop DIS_SYSOP_KEYS is set, or if we */
   /* are operatingin silent mode.                                        */
   if(od_control.od_disable & DIS_SYSOP_KEYS
      || od_control.od_silent_mode)
   {
      goto after_key_check;
   }

   ASM    mov ah, 1
   ASM    push si
   ASM    push di
   ASM    int 0x16
   ASM    jnz key_waiting
   ASM    pop di
   ASM    pop si
   ASM    jmp after_key_check
key_waiting:
   ASM    mov ah, 0
   ASM    int 0x16
   ASM    mov wKey, ax
   ASM    mov ah, 2
   ASM    int 0x16
   ASM    mov btShiftStatus, al
   ASM    pop di
   ASM    pop si

      if(nArrowUseCount > 0 && (wKey == 0x4800 || wKey == 0x5000)
         && !(btShiftStatus & 2))
      {
         /* Pass key on to od_local_input, if it is defined. */
         if(od_control.od_local_input != NULL)
         {
            (*od_control.od_local_input)(wKey);
         }

         /* Add this key to the local/remote input queue. */
         ODKrnlHandleLocalKey(wKey);
      }

      /* If hangup key is pressed. */
      else if(wKey == od_control.key_hangup)
      {
         ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_HANGUP);
      }

      /* If drop to BBS key is pressed. */
      else if(wKey == od_control.key_drop2bbs)
      {
         ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_DROPTOBBS);
      }

      else if(wKey == od_control.key_dosshell)
      {
         if(!bShellChatActive)
         {
            if(pfLogWrite != NULL)
               (*pfLogWrite)(6);

            /* If function hook is defined. */
            if(od_control.od_cbefore_shell != NULL)
            {
               /* Then call it. */
               bShellChatActive = TRUE;
               (*od_control.od_cbefore_shell)();
               bShellChatActive = FALSE;
            }

            if(od_control.od_before_shell != NULL)
               od_disp_str(od_control.od_before_shell);

            if((pszShellName = (char *)getenv("COMSPEC")) == NULL)
            {
               pszShellName = (char *)"COMMAND.COM";
            }
            bIsShell = TRUE;
            od_spawnvpe(P_WAIT, pszShellName, NULL, NULL);
            bIsShell = FALSE;

            if(od_control.od_after_shell != NULL)
               od_disp_str(od_control.od_after_shell);

            /* If a function hook is defined. */
            if(od_control.od_cafter_shell != NULL)
            {
               /* Then call it. */
               bShellChatActive = TRUE;
               (*od_control.od_cafter_shell)();
               bShellChatActive = FALSE;
            }

            if(pfLogWrite != NULL)
               (*pfLogWrite)(7);
         }
      }

      /* If toggle chat mode key is pressed. */
      else if(wKey == od_control.key_chat)
      {
chat_pressed:
         if(!bShellChatActive || od_control.od_chat_active)
         {
            /* If chat mode is active. */
            if(od_control.od_chat_active)
            {
               /* Signal exit of chat mode. */
               ODKrnlEndChatMode();
            }

            /* If chat mode is off. */
            else
            {
               /* Enable second call to kernel. */
               bKernelActive = FALSE;

               /* Enter chat mode. */
               ODKrnlChatMode();

               /* Disable second call to kernel. */
               bKernelActive = TRUE;
            }
         }
         else
         {
            if(nKrnlFuncPending & KERNEL_FUNC_CHATTOGGLE)
            {
               nKrnlFuncPending &= ~KERNEL_FUNC_CHATTOGGLE;
            }
            else
            {
               nKrnlFuncPending |= KERNEL_FUNC_CHATTOGGLE;
            }
         }
      }

      /* If sysop next key is pressed. */
      else if(wKey == od_control.key_sysopnext)
      {
         /* Toggle sysop next setting. */
         od_control.sysop_next = !od_control.sysop_next;

         /* Update status line. */
         goto statup;
      }

      /* If ESCape key is pressed and we are in chat mode. */
      else if((wKey&0xff) == 27 && od_control.od_chat_active)
      {
         /* Signal exit from chat mode. */
         od_control.od_chat_active = FALSE;
      }

      /* If lockout user key is pressed. */
      else if(wKey == od_control.key_lockout)
      {
         /* Set the user's access security level to 0. */
         od_control.user_security = 0;

         /* Shutdown OpenDoors. */
         ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_HANGUP);
      }


      /* If toggle keyboard off key is pressed. */
      else if(wKey == od_control.key_keyboardoff)
      {
         /* Toggle user keyboard settings. */
         od_control.od_user_keyboard_on =! od_control.od_user_keyboard_on;

         /* Update status line. */
         goto statup;
      }

      /* If increase time key is pressed. */
      else if(wKey == od_control.key_moretime)
      {
         /* If time limit is less than maximum possible time limit. */
         if(od_control.user_timelimit < 1440)
         {
             /* Increase time left online. */
            ++od_control.user_timelimit;
         }

         /* Update status line. */
         goto statup;
      }

      /* If decrease time key is pressed. */
      else if(wKey == od_control.key_lesstime)
      {
         /* Never let user's time limit be set to a negative value. */
         if(od_control.user_timelimit > 0)
         {
            /* Decrease user's timelimit. */
            --od_control.user_timelimit;
         }

         /* Update the status line. */
         goto statup;
      }

      else
      {
         for(ch = 0; ch < 9; ++ch)
         {
            if(wKey == od_control.key_status[ch])
            {
               if(btCurrentStatusLine != ch && od_control.od_status_on)
               {
                  od_set_statusline(ch);
               }
               goto check_keyboard_again;
            }
         }

         /* Look for user-defined hotkeys. */
         for(ch=0; ch<od_control.od_num_keys; ++ch)
         {
            /* If it matches. */
            if(wKey == (WORD)od_control.od_hot_key[ch])
            {
               /* Record keypress. */
               od_control.od_last_hot = wKey;

               /* Notify the current personality. */
               (*pfCurrentPersonality)(21);

               /* Check for a hotkey function. */
               if(od_control.od_hot_function[ch] != NULL)
               {
                  /* Call it if it exists. */
                  (*od_control.od_hot_function[ch])();
               }

               /* Stop searching. */
               break;
            }
         }

         /* If no hotkeys found. */
         if(ch >= od_control.od_num_keys)
         {
            /* Pass key on to od_local_input, if it is defined. */
            if(od_control.od_local_input != NULL)
            {
               (*od_control.od_local_input)(wKey);
            }

            /* Add this key to the local/remote input queue. */
            ODKrnlHandleLocalKey(wKey);
         }
      }
   goto check_keyboard_again;

after_key_check:

   /* If status line has been turned on since last call to kernel. */
   if(bLastStatusSetting != od_control.od_status_on)
   {
      /* Generate the status line. */
      od_set_statusline(0);
   }

   bLastStatusSetting = od_control.od_status_on;

   if(od_control.od_update_status_now)
   {
      od_set_statusline(btCurrentStatusLine);
      od_control.od_update_status_now = FALSE;
   }

   /* Update status line when needed. */
   if(nNextStatusUpdateTime < time(NULL) || bForceStatusUpdate)
   {
statup:
      nNextStatusUpdateTime = time(NULL) + STATUS_UPDATE_PERIOD;

      /* Turn off status line update force flag */
      bForceStatusUpdate = FALSE;

      if(od_control.od_status_on && btCurrentStatusLine != 8)
      {
         /* Store console settings. */
         ODStoreTextInfo();

         /* Enable writes to whole screen. */
         ODScrnSetBoundary(1, 1, 80, 25);
         ODScrnEnableCaret(FALSE);
         (*pfCurrentPersonality)((BYTE)(10 + btCurrentStatusLine));
         ODRestoreTextInfo();
         ODScrnEnableCaret(TRUE);
      }
   }
#endif

   ODKrnlTimeUpdate();

   ODTimerStart(&RunKernelTimer, 250);

   OD_API_EXIT();

   bKernelActive = FALSE;
#endif /* !OD_MULTITHREADED */
}


/* ----------------------------------------------------------------------------
 * ODKrnlHandleLocalKey()
 *
 * Called when a key is pressed on the local keyboard that should be placed
 * in the common local/remote input queue. This function is not called for
 * sysop function keys.
 *
 * Parameters: wKeyCode
 *
 *     Return: void
 */
void ODKrnlHandleLocalKey(WORD wKeyCode)
{
   /* If local keyboard input by sysop has not been disabled. */
   if(!(od_control.od_disable & DIS_LOCAL_INPUT))
   {
      if((wKeyCode & 0xff) == 0)
      {
         ODKrnlHandleReceivedChar('\0', FALSE);
         ODKrnlHandleReceivedChar((char)(wKeyCode >> 8), FALSE);
      }
      else
      {
         ODKrnlHandleReceivedChar((char)wKeyCode, FALSE);
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODKrnlHandleReceivedChar()                          *** PRIVATE FUNCTION ***
 *
 * Called when a character is received from the local or remote system.
 *
 * Parameters: chReceived  - Character that should be handled.
 *
 *             bFromRemote - TRUE if this character was received from the
 *                           remote system, FALSE if it originated from the
 *                           local console.
 *
 *     Return: void
 */
static void ODKrnlHandleReceivedChar(char chReceived, BOOL bFromRemote)
{
   tODInputEvent InputEvent;

   /* If we are operating in remote mode, and remote user keyboard has been */
   /* disabled by the sysop, then return, ignoring this character.          */
   if(bFromRemote && !od_control.od_user_keyboard_on)
   {
      return;
   }

   /* Add this input event to the local/remote common input queue. */
   InputEvent.EventType = EVENT_CHARACTER;
   InputEvent.bFromRemote = bFromRemote;
   InputEvent.chKeyPress = chReceived;
   ODInQueueAddEvent(hODInputQueue, &InputEvent);

   /* Update last control key information. */
   switch(chReceived)
   {
      case 's':
      case 'S':
      case 3:
      case 11:
      case 0x18:
         chLastControlKey = 's';
         break;
      case 'p':
      case 'P':
         chLastControlKey = 'p';
   }
}


/* ----------------------------------------------------------------------------
 * ODKrnlTimeUpdate()                                  *** PRIVATE FUNCTION ***
 *
 * Performs regular updating of time remaining online, inactivity time, and
 * forces OpenDoors to exit if a time limit has been exceeded.
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODKrnlTimeUpdate(void)
{
   time_t CurrentTime;
   static char szTemp[80];

   /* Obtain the current time. */
   CurrentTime = time(NULL);

   /* If inactivity setting has changed. */
   if(nLastInactivitySetting != od_control.od_inactivity)
   {
      /* If it was previously disabled. */
      if(nLastInactivitySetting == 0)
      {
         /* Prevent immediate timeout. */
         ODInQueueResetLastActivity(hODInputQueue);
      }

      /* Store current value. */
      nLastInactivitySetting = od_control.od_inactivity;
   }

   /* Check user keyboard inactivity. */
   if((ODInQueueGetLastActivity(hODInputQueue) + od_control.od_inactivity)
      < CurrentTime)
   {
      /* If timeout, display message. */
      if(od_control.od_inactivity != 0 && !od_control.od_disable_inactivity)
      {
         if(od_control.od_time_msg_func == NULL)
         {
            od_disp_str(od_control.od_inactivity_timeout);
         }
         else
         {
            (*od_control.od_time_msg_func)(od_control.od_inactivity_timeout);
         }

         /* End connection. */
         ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_INACTIVITY);
      }
   }

   /* If less than 5s left of inactivity. */
   else if(ODInQueueGetLastActivity(hODInputQueue) + od_control.od_inactivity
      < CurrentTime + od_control.od_inactive_warning)
   {
      if(!bWarnedAboutInactivity && od_control.od_inactivity != 0
         && !od_control.od_disable_inactivity)
      {
         /* Warn the user. */
         if(od_control.od_time_msg_func == NULL)
         {
            od_disp_str(od_control.od_inactivity_warning);
         }
         else
         {
            (*od_control.od_time_msg_func)(od_control.od_inactivity_warning);
         }
         /* Don't warn the user a second time. */
         bWarnedAboutInactivity = TRUE;
      }
   }
   else
   {
      /* Re-enable inactivity warning. */
      bWarnedAboutInactivity = FALSE;
   }

   /* If chat mode is active. */
   if(od_control.od_chat_active)
   {
      /* Prevent the user's time from being drained. */
      nNextTimeDeductTime = time(NULL) + 60;
   }

   /* If 1 minute has passed since last time update. */
   if(CurrentTime >= nNextTimeDeductTime)
   {
      /* Next time update should occur 60 seconds after this one was */
      /* scheduled.                                                  */
      nNextTimeDeductTime += 60;

      /* Force status line to be updated immediately. */
      bForceStatusUpdate = TRUE;

      /* Decrement time left. */
      --od_control.user_timelimit;

      /* If the user's time limit is close to expiring, then notify */
      /* the user.                                                  */
      if(od_control.user_timelimit <= 3 &&
         od_control.user_timelimit > 0 &&
         !(od_control.od_disable & DIS_TIMEOUT))
      {
         /* If less than 3 mins left, tell user. */
         sprintf(szTemp, od_control.od_time_warning,
            od_control.user_timelimit);
         if(od_control.od_time_msg_func == NULL)
         {
            od_disp_str(szTemp);
         }
         else
         {
            (*od_control.od_time_msg_func)(szTemp);
         }
      }

#ifdef ODPLAT_WIN32
      ODFrameUpdateTimeDisplay();
#endif /* ODPLAT_WIN32 */
   }

   /* If user has no time left. */
   if(od_control.user_timelimit <= 0
      && !(od_control.od_disable & DIS_TIMEOUT))
   {
      /* Notify the user. */
      if(od_control.od_time_msg_func == NULL)
      {
         od_disp_str(od_control.od_no_time);
      }
      else
      {
         (*od_control.od_time_msg_func)(od_control.od_no_time);
      }

      /* Force OpenDoors to shutdown. */
      ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_TIMEOUT);
   }
}


/* ----------------------------------------------------------------------------
 * ODKrnlForceOpenDoorsShutdown()
 *
 * Called to force the application to exit due to some event in OpenDoors,
 * such as loss of carrier, user inactivity timeout, the hangup command
 * being chosen by the system operator, etc. The only time when OpenDoors
 * is shutdown without going through this function should be as a result of
 * an explicit call to od_exit() by the client application.
 *
 * Parameters: btReasonForShutdown - An OpenDoors exit reason code.
 *
 *     Return: Never returns.
 */
void ODKrnlForceOpenDoorsShutdown(BYTE btReasonForShutdown)
{
   BOOL bHangup;

#ifdef OD_MULTITHREADED
   /* First, wait until an OpenDoors API is active. This way, we won't  */
   /* interrupt any client application operations that may leave the    */
   /* system in an unstable state (for instance, interrupting some file */
   /* I/O operations).                                                  */
   ODKrnlWaitForExclusiveControl();
#endif /* OD_MULTITHREADED */

   bKernelActive = TRUE;

   /* Determine whether we should hangup on the user before exiting. */
   if(btReasonForShutdown == ERRORLEVEL_HANGUP
      || btReasonForShutdown == ERRORLEVEL_INACTIVITY)
   {
      bHangup = TRUE;
   }
   else
   {
      bHangup = FALSE;
   }

   /* Record exit reason in global variable. */
   btExitReason = btReasonForShutdown - 1;

   /* Use the client-defined errorlevel, if any. */
   if(od_control.od_errorlevel[0])
   {
      od_exit(od_control.od_errorlevel[btReasonForShutdown], bHangup);
   }

   /* Otherwise, use the default OpenDoors errorlevel. */
   else
   {
      od_exit(btReasonForShutdown - 1, bHangup);
   }
}


/* ========================================================================= */
/* OpenDoors Kernel multithreaded implementation.                            */
/* ========================================================================= */

#ifdef OD_MULTITHREADED

/* ----------------------------------------------------------------------------
 * ODKrnlRemoteInputThread()                           *** PRIVATE FUNCTION ***
 *
 * Code for the remote input thread. This thread executes an infinite loop,
 * blocking until a character is received from the remote system, and then
 * adding this character to the common local/remote input queue. This thread
 * should be given higher than normal priority.
 *
 * In non-multithreaded versions of OpenDoors, the task of checking for new
 * characters from the remote system and adding them to the common input
 * queue is performed on each call to od_kernel().
 *
 * Parameters: As dictated for any thread function.
 *
 *     Return: As dictated for any thread function.
 */
DWORD OD_THREAD_FUNC ODKrnlRemoteInputThread(void *pParam)
{
   char chReceived;

   /* We keep looping until someone else terminates this thread. */
   for(;;)
   {
      /* Get next character from the modem, blocking if no character */
      /* is waiting.                                                 */
      ODComGetByte(hSerialPort, &chReceived, TRUE);

      /* Handle this received character, adding it to the local/remote */
      /* common input queue, if appropriate.                           */
      ODKrnlHandleReceivedChar(chReceived, TRUE);
   }

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODKrnlNoCarrierThread()                             *** PRIVATE FUNCTION ***
 *
 * Thread which performs carrier detection. Normally, this thread doesn't
 * execute at all, but instead blocks waiting for a no carrier serial port
 * event. Only when the carrier detect signal goes low does this thread
 * execute, performing its one purpose in live - to trigger an OpenDoors
 * shutdown. This thread should be given higher than normal priority.
 *
 * This thread should only be created when OpenDoors is operating in remote
 * mode.
 *
 * In non-multithreaded versions of OpenDoors, this task is performed by
 * od_kernel().
 *
 * Parameters: As dictated for any thread function.
 *
 *     Return: As dictated for any thread function.
 */
DWORD OD_THREAD_FUNC ODKrnlNoCarrierThread(void *pParam)
{
   /* Block until the carrier detect signal goes low with carrier */
   /* detection enabled.                                          */
   for(;;)
   {
      /* Wait for carrier detect signal to go low. */
      ODComWaitEvent(hSerialPort, kNoCarrier);

      /* If carrier detection has not been disabled, then we have found */
      /* a condition where OpenDoors should exit.                       */
      if(!(od_control.od_disable&DIS_CARRIERDETECT)) break;

      /* If we have no carrier but carrier detection is currently   */
      /* disabled, then we sleep for a while before checking again. */
      /* This isn't a very elegant implementation, and perhaps a    */
      /* better approach will be used for future versions.          */
      od_sleep(NO_CARRIER_THREAD_SLEEP_TIME);
   }

   /* Force OpenDoors to exit. */
   ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_NOCARRIER);

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODKrnlTimeUpdateThread()                            *** PRIVATE FUNCTION ***
 *
 * Thread which performs time limit updating and checking. This thread executes
 * an infinite loop, sleeping for several seconds, waking up to perform time
 * limit updating, and then going back to sleep. This thead should typically
 * operate at normal priority.
 *
 * In non-multithreaded versions of OpenDoors, this task is performed by
 * od_kernel().
 *
 * Parameters: As dictated for any thread function.
 *
 *     Return: As dictated for any thread function.
 */
DWORD OD_THREAD_FUNC ODKrnlTimeUpdateThread(void *pParam)
{
   /* We keep looping until someone else terminates this thread. */
   for(;;)
   {
      /* Sleep until it is time to do the next update. */
      od_sleep(TIME_UPDATE_THREAD_SLEEP_TIME);

      /* Now, perform time update. */
      ODKrnlTimeUpdate();
   }

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODKrnlWaitForExclusiveControl()                     *** PRIVATE FUNCTION ***
 *
 * Claims exclusive control of the application by the OpenDoors kernel. This is
 * required to ensure that the client application is not busy when the
 * OpenDoors kernel interrupts other operations for one reason or another
 * (for example, to start chat mode or to force the program to exit).
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODKrnlWaitForExclusiveControl(void)
{
   /* If we already have exclusive control, then don't do anything. */
   if(bHaveExclusiveControl) return;

   /* Wait until an OpenDoors API is active. */
   ODSemaphoreDown(hODActiveSemaphore, OD_NO_TIMEOUT);

   /* Now, suspend the client thread. */
   ASSERT(hClientThread != NULL);
   ODThreadSuspend(hClientThread);

   /* Record that we now have exclusive control. */
   bHaveExclusiveControl = TRUE;
}


/* ----------------------------------------------------------------------------
 * ODKrnlGiveUpExclusiveControl()                      *** PRIVATE FUNCTION ***
 *
 * Relinguishes exclusive control of the application by the OpenDoors kernel.
 * A call to this function should only take place after a previous call to
 * ODKrnlWaitForExclusiveControl().
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODKrnlGiveUpExclusiveControl(void)
{
   /* If we don't have exclusive control, then this call doesn't do */
   /* anything.                                                     */
   if(!bHaveExclusiveControl) return;

   /* First, restart the client thread. */
   ASSERT(hClientThread != NULL);
   ODThreadResume(hClientThread);

   /* Now, allow currently active OpenDoors API to return control */
   /* to the client application.                                  */
   ODSemaphoreUp(hODActiveSemaphore, 1);

   /* Note that we no longer have exclusive control. */
   bHaveExclusiveControl = FALSE;
}

#endif /* OD_MULTITHREADED */



/* ========================================================================= */
/* OpenDoors chat mode.                                                      */
/* ========================================================================= */

BOOL bChatted;
BOOL bSysopColor;

#ifdef OD_MULTITHREADED

/* ----------------------------------------------------------------------------
 * ODKrnlChatThread()                                  *** PRIVATE FUNCTION ***
 *
 * Thread which implements sysop <-> remote user chat mode.
 *
 * Parameters: As dictated for any thread function.
 *
 *     Return: As dictated for any thread function.
 */
DWORD OD_THREAD_FUNC ODKrnlChatThread(void *pParam)
{
   BOOL bTriggeredInsideOpenDoors = bChatActivatedInternally;

   /* The chat thread doesn't start up chat mode until the kernel has */
   /* exclusive control of the client application.                    */
   if(bTriggeredInsideOpenDoors)
   {
      ODKrnlWaitForExclusiveControl();
   }

   /* Now, execute the chat mode loop. */
   ODKrnlChatMode();

   /* If we get here, then we are responsible for relinguishing exclusive */
   /* control of the application.                                         */
   if(bTriggeredInsideOpenDoors)
   {
      ODKrnlGiveUpExclusiveControl();
   }

   /* Exit the chat thread. */
   return(0);
}


/* ----------------------------------------------------------------------------
 * ODKrnlStartChatThread()
 *
 * Starts the chat mode thread.
 *
 * Parameters: bTriggeredInternally - TRUE if chat mode has been triggered
 *                                    inside OpenDoors, or FALSE if it has
 *                                    been triggered by a call to od_chat().
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODKrnlStartChatThread(BOOL bTriggeredInternally)
{
   tODResult Result;

   bChatActivatedInternally = bTriggeredInternally;

   Result = ODThreadCreate(&hChatThread, ODKrnlChatThread, NULL);
   if(Result != kODRCSuccess)
   {
      return(Result);
   }

   /* If chat mode command has been chosen, then toggle chat */
   /* mode on or off.                                        */
   od_control.od_chat_active = TRUE;

#ifdef ODPLAT_WIN32
   /* Update the enabled and checked state of commands. */
   ODFrameUpdateCmdUI();
#endif /* ODPLAT_WIN32 */

   return(kODRCSuccess);
}


#endif /* OD_MULTITHREADED */


/* ----------------------------------------------------------------------------
 * ODKrnlEndChatMode()
 *
 * Forces chat mode to exit.
 *
 * Parameters: None
 *
 *     Return: void
 */
void ODKrnlEndChatMode(void)
{
#ifdef OD_MULTITHREADED

   /* Shutdown the chat thread. */
   ODThreadTerminate(hChatThread);

   /* Perform post-chat cleanup operations. */
   ODKrnlChatCleanup();

#else /* !OD_MULTITHREADED */

   /* Turn off chat mode. */
   od_control.od_chat_active = FALSE;

#endif /* !OD_MULTITHREADED */
}


/* ----------------------------------------------------------------------------
 * od_chat()
 *
 * Allows the client application to activate the line-by-line default chat
 * mode provided by OpenDoors, allowing the local sysop and remote user to
 * communicate with one another in real time.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_chat(void)
{
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_chat()");

   /* Set the main chat active flag in od_control. */
   od_control.od_chat_active = TRUE;

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

#ifdef OD_MULTITHREADED

   /* In multithreaded versions of OpenDoors, od_chat() causes the chat */
   /* mode thread to be started, which in turn implements chat mode.    */
   /* od_chat() only returns when this thread exits.                    */
   if(ODKrnlStartChatThread(FALSE) != kODRCSuccess)
   {
      od_control.od_error = ERR_GENERALFAILURE;
      OD_API_EXIT();
   }

   /* Now, wait for the chat thread to exit. */
   ODThreadWaitForExit(hChatThread);

   /* Now, note that the chat thread no longer exists. */
   hChatThread = NULL;

#else /* !OD_MULTITHREADED */

   /* In non-multithreaded versions, a call to od_chat() maps directly to a */
   /* call to ODKrnlChatMode(), which implements chat mode.                 */
   ODKrnlChatMode();

#endif /* !OD_MULTITHREADED */

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * ODKrnlChatMode()                                    *** PRIVATE FUNCTION ***
 *
 * Implements the OpenDoors chat mode.
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODKrnlChatMode(void)
{
   BYTE chKeyPressed;
   char szCurrentWord[79];
   BYTE btWordLength = 0;
   BYTE btCurrentColumn = 0;
   char *pchCurrent;
   BYTE btCount;
#ifndef OD_MULTITHREADED
   tODTimer Timer;
#endif /* !OD_MULTITHREADED */

   /* Empty current word string. */
   szCurrentWord[0] = '\0';

   /* Save current display color attribute. */
   nChatOriginalAttrib = od_control.od_cur_attrib;

   /* Record that sysop has entered chat mode. */
   bChatted = TRUE;

   /* Turn off "user wants to chat" indicator, and force the status line. */
   /* to be updated.                                                      */
   od_control.user_wantchat = FALSE;
#ifdef ODPLAT_WIN32
   ODFrameUpdateWantChat();
#endif /* ODPLAT_WIN32 */

   bForceStatusUpdate = TRUE;
   CALL_KERNEL_IF_NEEDED();

   /* Note that chat mode is now active. */
   od_control.od_chat_active = TRUE;

   /* If a pre-chat function hook has been defined, then call it. */
   if(od_control.od_cbefore_chat!=NULL)
   {
      bShellChatActive = TRUE;
      (*od_control.od_cbefore_chat)();
      bShellChatActive = FALSE;

      /* If chat has been deactivated, then return right away */
      if(!od_control.od_chat_active) goto cleanup;
   }

   /* Display a message indicating that the sysop has entered chat mode. */
   od_set_attrib(od_control.od_chat_color1);
   if(od_control.od_before_chat != NULL)
      od_disp_str(od_control.od_before_chat);

   /* Currently set to sysop color. */
   bSysopColor = TRUE;

   /* If the logfile system is hooked up, then write a log entry */
   /* indicating that the sysop has entered chat mode.           */
   if(pfLogWrite != NULL)
   {
      (*pfLogWrite)(9);
   }

#ifndef OD_MULTITHREADED
   /* Start a timer that will elapse after 25 milliseconds. */
   ODTimerStart(&Timer, CHAT_YIELD_PERIOD);
#endif /* !OD_MULTITHREADED */

   /* Loop while sysop chat mode is stilil on. */
   while(od_control.od_chat_active)
   {
      /* Obtain the next key from the user. */
#ifdef OD_MULTITHREADED
      chKeyPressed = od_get_key(TRUE);
#else /* !OD_MULTITHREADED */
      chKeyPressed = od_get_key(FALSE);
#endif /* !OD_MULTITHREADED */

      /* If color not set correctly. */
      if((od_control.od_last_input && !bSysopColor)
         || (!od_control.od_last_input && bSysopColor))
      {
         /* If sysop was last person to type. */
         if(od_control.od_last_input)
         {
            /* Switch to sysop text color. */
            od_set_attrib(od_control.od_chat_color1);
         }
         else
         {
            /* Otherwise, switch to the user text color. */
            od_set_attrib(od_control.od_chat_color2);
         }

         /* Record current color setting. */
         bSysopColor = od_control.od_last_input;
      }

      /* If this is a displayable character. */
      if(chKeyPressed >= 32)
      {
         /* Display the character that was typed. */
         od_putch(chKeyPressed);

         /* If the user pressed spacebar, then this is the end of the */
         /* previous word. */
         if(chKeyPressed == 32)
         {
            btWordLength = 0;
            szCurrentWord[0] = 0;
         }

         /* Add this character to the current word, if we haven't exceeded */
         /* the maximum word length.                                       */
         else if(btWordLength < 70)
         {
            szCurrentWord[btWordLength++] = chKeyPressed;
            szCurrentWord[btWordLength] = '\0';
         }

         /* If we are not yet at the end of the line, then increment the */
         /* current column number.                                       */
         if(btCurrentColumn < 75)
         {
            ++btCurrentColumn;
         }

         /* If we are at the end of the line. */
         else
         {
            /* If the current word should be wrapped to the next line. */
            if(btWordLength < 70 && btWordLength > 0)
            {
               /* Generate a string to erase the word from the current line. */
               pchCurrent = (char *)szODWorkString;
               for(btCount = 0; btCount < btWordLength; ++btCount)
               {
                  *(pchCurrent++) = 8;
               }

               for(btCount = 0; btCount < btWordLength; ++btCount)
               {
                  *(pchCurrent++) = ' ';
               }

               *pchCurrent = '\0';

               /* Display the string to erase the old word. */
               od_disp_str(szODWorkString);

               /* Move to the next line. */               
               od_disp_str("\n\r");

               /* Redisplay the word on the next line. */
               od_disp_str(szCurrentWord);

               /* Update current column number. */               
               btCurrentColumn = btWordLength;
            }

            /* If we have reached the end of the line, but word wrap should */
            /* not be performed.                                            */
            else
            {
               /* Move to the next line. */
               od_disp_str("\n\r");

               /* Update the current column number. */
               btCurrentColumn = 0;
            }

            /* Reset the current word information. */
            btWordLength = 0;
            szCurrentWord[0] = 0;
         }
      }

      /* If the backspace key was pressed. */
      else if(chKeyPressed == 8)
      {
         /* Send backspace sequence. */
         od_disp_str(szBackspaceWithDelete);

         /* If we are in the middle of a word, then we must remove the */
         /* last character of the word.                                */         
         if(btWordLength > 0)
         {
            szCurrentWord[--btWordLength] = '\0';
         }

         /* Update the current column number. */
         if(btCurrentColumn > 0) --btCurrentColumn;
      }

      /* If the enter key was pressed. */
      else if(chKeyPressed == 13)
      {
         /* Send carriage return / line feed sequence. */
         od_disp_str("\n\r");

         /* Reset the current word contents. */
         btWordLength = 0;
         szCurrentWord[0] = 0;

         /* Update the current column number. */
         btCurrentColumn = 0;
      }

      /* If the sysop pressed the escape key. */
      else if(chKeyPressed == 27 && od_control.od_last_input)
      {
         /* Exit chat mode. */
         goto cleanup;
      }

#ifndef OD_MULTITHREADED
      /* Give up processor after 25 milliseconds elapsed. */
      else if(ODTimerElapsed(&Timer))
      {
         od_sleep(0);

         /* Restart the timer, so that it will elapse after another */
         /* 25 milliseconds.                                        */
         ODTimerStart(&Timer, CHAT_YIELD_PERIOD);
      }
#endif /* !OD_MULTITHREADED */
   }

cleanup:
   ODKrnlChatCleanup();
}


/* ----------------------------------------------------------------------------
 * ODKrnlChatCleanup()                                 *** PRIVATE FUNCTION ***
 *
 * Performs post-chat operations, such as resetting the original display
 * color, etc.
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODKrnlChatCleanup(void)
{
   od_set_attrib(od_control.od_chat_color1);

   /* Indicate that chat mode is exiting. */
   if(od_control.od_after_chat != NULL)
   {
      od_disp_str(od_control.od_after_chat);
   }

   /* If an after chat function has been provided, then call it. */
   if(od_control.od_cafter_chat != NULL)
   {
      bShellChatActive = TRUE;
      (*od_control.od_cafter_chat)();
      bShellChatActive = FALSE;
   }

   /* If the logfile system is hooked up, then write a line to the log */
   /* indicating that chat mode has been exited.                       */
   if(pfLogWrite != NULL)
   {
      (*pfLogWrite)(10);
   }

   /* Restore original display color attribute. */
   od_set_attrib(nChatOriginalAttrib);

   /* Record that chat mode is no longer active. */
   od_control.od_chat_active = FALSE;

#ifdef ODPLAT_WIN32
   /* Update the enabled and checked state of commands. */
   ODFrameUpdateCmdUI();
#endif /* ODPLAT_WIN32 */

#ifdef OD_MULTITHREADED
   if(bChatActivatedInternally)
   {
      ODKrnlGiveUpExclusiveControl();
   }
#endif
}

#ifdef ODPLAT_NIX
#ifdef USE_KERNEL_SIGNAL
/* ----------------------------------------------------------------------------
 * sig_run_kernel(sig)				   *** PRIVATE FUNCTION ***
 *
 * Runs od_kernel() on a SIGALRM
 *
 */
static void sig_run_kernel(int sig)
{
   od_kernel();
}

/* ----------------------------------------------------------------------------
 * sig_run_kernel(sig)				   *** PRIVATE FUNCTION ***
 *
 * Runs od_kernel() on a SIGALRM
 *
 */
static void sig_get_char(int sig)
{
   static char ch;
   /* Loop, obtaining any new characters from the serial port and */
   /* adding them to the common local/remote input queue.         */
   while(ODComGetByte(hSerialPort, &ch, FALSE) == kODRCSuccess)
   {
      ODKrnlHandleReceivedChar(ch, TRUE);
   }
}

static void sig_no_carrier(int sig)
{
   if(od_control.baud != 0 && )
   {
      if(!(od_control.od_disable&DIS_CARRIERDETECT))
      	ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_NOCARRIER);
   }
}
#endif
#endif
