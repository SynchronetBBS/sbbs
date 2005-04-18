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
 *        File: ODKrnl.h
 *
 * Description: Contains the public definitions related to odkrnl.c
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Jan 01, 1995  6.00  BP   Split off from odcore.c and oddoor.h
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Jan 12, 1996  6.00  BP   Added bOnlyShiftArrow.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 13, 1996  6.10  BP   bOnlyShiftArrow -> nArrowUseCount.
 */

#ifndef _INC_ODKRNL
#define _INC_ODKRNL

#include "ODPlat.h"

/* Global kernel-related variables. */
extern tODTimer RunKernelTimer;
extern time_t nNextTimeDeductTime;
extern char chLastControlKey;
extern INT nArrowUseCount;
extern BOOL bForceStatusUpdate;
extern BOOL bSysopColor;
#ifdef OD_MULTITHREADED
extern tODSemaphoreHandle hODActiveSemaphore;
#endif /* OD_MULTITHREADED */

/* Chat mode global variables. */
extern BOOL bIsShell;
extern BOOL bChatted;

/* Kernel function prototypes. */
tODResult ODKrnlInitialize(void);
void ODKrnlShutdown(void);
void ODKrnlHandleLocalKey(WORD wKeyCode);
void ODKrnlEndChatMode(void);
void ODKrnlForceOpenDoorsShutdown(BYTE btReasonForShutdown);
#ifdef OD_MULTITHREADED
tODResult ODKrnlStartChatThread(BOOL bTriggeredInternally);
#endif /* OD_MULTITHREADED */

/* Macro used to generate the appropriate code (if any) to call */
/* the OpenDoors kernel from within OpenDoors code.             */
#ifdef OD_MULTITHREADED
#define CALL_KERNEL_IF_NEEDED()
#else /* !OD_MULTITHREADED */
#ifdef ODPLAT_NIX
#ifdef USE_KERNEL_SIGNAL
#define CALL_KERNEL_IF_NEEDED()
#else
#define CALL_KERNEL_IF_NEEDED()     od_kernel()
#endif
#else
#define CALL_KERNEL_IF_NEEDED()     od_kernel()
#endif /* !ODPLAT_NIX */
#endif /* !OD_MULTITHREADED */

/* Macro used to increment or decrement OpenDoors active semaphore. */
#ifdef OD_MULTITHREADED
#define OD_API_ENTRY()              ODSemaphoreUp(hODActiveSemaphore, 1);
#define OD_API_EXIT()               ODSemaphoreDown(hODActiveSemaphore, 1);
#else /* !OD_MULTITHREADED */
#define OD_API_ENTRY()
#define OD_API_EXIT()
#endif /* !OD_MULTITHREADED */

#endif /* _INC_ODKRNL */
