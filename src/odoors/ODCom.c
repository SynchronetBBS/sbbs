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
 *        File: ODCom.c
 *
 * Description: Generic serial I/O routines, provide a single interface to
 *              serial ports on any platform.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 20, 1994  6.00  BP   Handle BIOS missing port addrs.
 *              Oct 20, 1994  6.00  BP   Standardized coding style.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Dec 07, 1994  6.00  BP   Support for RTS/CTS flow control.
 *              Dec 10, 1994  6.00  BP   Allow word frmt setting for intern I/O
 *              Dec 13, 1994  6.00  BP   Remove include of dir.h.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Jan 01, 1995  6.00  BP   Integrate in Win32 code.
 *              Jan 01, 1995  6.00  BP   Add FLOW_DEFAULT setting.
 *              Jan 01, 1995  6.00  BP   Added ODComWaitEvent().
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 21, 1995  6.00  BP   Ported to Win32.
 *              Dec 21, 1995  6.00  BP   Add ability to use already open port.
 *              Jan 09, 1996  6.00  BP   Supply actual in/out buffer size used.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Initial support for Door32 interface.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Jan 13, 1997  6.10  BP   Fixes for Door32 support.
 *              Oct 19, 2001  6.20  RS   Added TCP/IP socket (telnet) support.
 *              Oct 22, 2001  6.21  RS   Fixed disconnected socket detection.
 *              Aug 22, 2002  6.22  RS   Fixed bugs in ODComCarrier and ODComWaitEvent
 *              Aug 22, 2002  6.22  MD   Modified socket functions for non-blocking use.
 *              Sep 18, 2002  6.22  MD   Fixed bugs in ODComWaitEvent for non-blocking sockets.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "OpenDoor.h"
#ifdef ODPLAT_NIX
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#include "ODCore.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODCom.h"
#include "ODUtil.h"


/* The following define determines whether serial port function should */
/* ASSERT or return an error code on programmer erorrs (e.g. invalid   */
/* parameters.                                                         */
#define ASSERT_ON_INVALID_CALLS

/* The following code defines the VERIFY_CALL() macro, which maps to an */
/* ASSERT if ASSERT_ON_INVALID_CALLS is defined. Otherwise, this macro  */
/* maps to a test which will return an error code to the caller.        */
#ifdef ASSERT_ON_INVALID_CALLS
#define VERIFY_CALL(x) ASSERT(x)
#else /* !ASSERT_ON_INVALID_CALLS */
#define VERIFY_CALL(x) if(x) return(kODRCInvalidCall)
#endif /* !ASSERT_ON_INVALID_CALLS */


/* The following defines determine which serial I/O mechanisms should be */
/* supported.                                                            */

/* Serial I/O mechanisms supported under MS-DOS version. */
#ifdef ODPLAT_DOS
#define INCLUDE_FOSSIL_COM                      /* INT 14h FOSSIL-based I/O. */
#define INCLUDE_UART_COM                   /* Internal interrupt driven I/O. */
#endif /* ODPLAT_DOS */

/* Serial I/O mechanisms supported under Win32 version. */
#ifdef ODPLAT_WIN32
#define INCLUDE_WIN32_COM                           /* Win32 API serial I/O. */
#define INCLUDE_DOOR32_COM                          /* Door32 I/O interface. */
#define INCLUDE_SOCKET_COM                          /* TCP/IP socket I/O.    */
#endif /* ODPLAT_WIN32 */

/* Serial I/O mechanisms supported inder *nix version */
#ifdef ODPLAT_NIX
#define INCLUDE_STDIO_COM
#define INCLUDE_SOCKET_COM                          /* TCP/IP socket I/O.    */

/* Win32 Compat. Stuff */
#define SOCKET	int
#define WSAEWOULDBLOCK	EAGAIN
#define SOCKET_ERROR -1
#define WSAGetLastError() errno
#define ioctlsocket	ioctl
#define closesocket	close
#endif /* ODPLAT_NIX */

/* Include "windows.h" for Win32-API based serial I/O. */
#ifdef INCLUDE_WIN32_COM
#include "windows.h"
#endif /* INCLUDE_WIN32_COM */

/* terminal variables */
#ifdef INCLUDE_STDIO_COM
struct termios tio_default;				/* Initial term settings */
#endif


#if defined(_WIN32) && defined(INCLUDE_SOCKET_COM)
	#include <winsock.h>
	static WSADATA WSAData;		/* WinSock data */
#endif

/* ========================================================================= */
/* Serial port object structure.                                             */
/* ========================================================================= */

/* Win32-API serial I/O implementation requires current timeout setting */
/* status variable in serial port object structure.                     */
#ifdef INCLUDE_WIN32_COM
typedef enum
{
   kNotSet,
   kBlocking,
   kNonBlocking
} tReadTimeoutState;
#endif /* INCLUDE_WIN32_COM */

/* Structure associated with each serial port handle. */
typedef struct
{
   BOOL bIsOpen;
   BOOL bUsingClientsHandle;
   BYTE btFlowControlSetting;
   long lSpeed;
   BYTE btPort;
   int nPortAddress;
   BYTE btIRQLevel;
   BYTE btWordFormat;
   int nReceiveBufferSize;
   int nTransmitBufferSize;
   BYTE btFIFOSetting;
   tComMethod Method;
   void (*pfIdleCallback)(void);
#ifdef INCLUDE_WIN32_COM
   HANDLE hCommDev;
   tReadTimeoutState ReadTimeoutState;
#endif /* INCLUDE_WIN32_COM */
#ifdef INCLUDE_DOOR32_COM
   HINSTANCE hinstDoor32DLL;
   BOOL (WINAPI *pfDoorInitialize)(void);
   BOOL (WINAPI *pfDoorShutdown)(void);
   BOOL (WINAPI *pfDoorWrite)(const BYTE *pbData, DWORD dwSize);
   DWORD (WINAPI *pfDoorRead)(BYTE *pbData, DWORD dwSize);
   HANDLE (WINAPI *pfDoorGetAvailableEventHandle)(void);
   HANDLE (WINAPI *pfDoorGetOfflineEventHandle)(void);
#endif /* INCLUDE_DOOR32_COM */
#ifdef INCLUDE_SOCKET_COM
	SOCKET	socket;
	int	old_delay;
#endif
} tPortInfo;

/* ========================================================================= */
/* Internal interrupt-driven serial I/O specific defintions & functions.     */
/* ========================================================================= */

#ifdef INCLUDE_UART_COM

/* Private function prototypes, used by internal UART async serial I/O. */
static void ODComSetVect(BYTE btVector, void (INTERRUPT far *pfISR)(void));
static void (INTERRUPT far *ODComGetVect(BYTE btVector))(void);
static void INTERRUPT ODComInternalISR();
static BOOL ODComInternalTXReady(void);
static void ODComInternalResetRX(void);
static void ODComInternalResetTX(void);


/* Offsets of UART registers. */
#define TXBUFF  0                       /* Transmit buffer register. */
#define RXBUFF  0                       /* Receive buffer register. */
#define DLLSB   0                       /* Divisor latch LS byte. */
#define DLMSB   1                       /* Divisor latch MS byte. */
#define IER     1                       /* Interrupt enable register. */
#define IIR     2                       /* Interrupt ID register. */
#define LCR     3                       /* Line control register. */
#define MCR     4                       /* Modem control register. */
#define LSR     5                       /* Line status register. */
#define MSR     6                       /* Modem status register. */

/* FIFO control register bits. */
#define FE      0x01                    /* FIFO enable. */
#define RR      0x02                    /* FIFO receive buffer reset. */
#define TR      0x04                    /* FIFO transmit buffer reset. */
#define FTS_1   0x00                    /* FIFO trigger size 1 byte. */
#define FTS_4   0x40                    /* FIFO trigger size 4 bytes. */
#define FTS_8   0x80                    /* FIFO trigger size 8 bytes. */
#define FTS_14  0xc0                    /* FIFO trigger size 14 bytes. */

/* Modem control register (MCR) bits. */
#define DTR     0x01                    /* Data terminal ready. */
#define NOT_DTR 0xfe                    /* All bits other than DTR. */
#define RTS     0x02                    /* Request to send. */
#define NOT_RTS 0xfd                    /* All bits other than RTS. */
#define OUT1    0x04                    /* Output #1. */
#define OUT2    0x08                    /* Output #2. */
#define LPBK    0x10                    /* Loopback mode bit. */

/* Modem status register (MSR) bits. */
#define DCTS    0x01                    /* Delta clear to send. */
#define DDSR    0x02                    /* Delta data set ready. */
#define TERI    0x04                    /* Trailing edge ring indicator. */
#define DRLSD   0x08                    /* Delta Rx line signal detect. */
#define CTS     0x10                    /* Clear to send. */
#define DSR     0x20                    /* Data set ready. */
#define RI      0x40                    /* Ring indicator. */
#define RLSD    0x80                    /* Receive line signal detect. */

/* Line control register (LCR) bits. */
#define DATA5   0x00                    /* 5 Data bits. */
#define DATA6   0x01                    /* 6 Data bits. */
#define DATA7   0x02                    /* 7 Data bits. */
#define DATA8   0x03                    /* 8 Data bits. */

#define STOP1   0x00                    /* 1 Stop bit. */
#define STOP2   0x04                    /* 2 Stop bits. */

#define NOPAR   0x00                    /* No parity. */
#define ODDPAR  0x08                    /* Odd parity. */
#define EVNPAR  0x18                    /* Even parity. */
#define STKPAR  0x28                    /* Sticky parity. */
#define ZROPAR  0x38                    /* Zero parity. */

#define DLATCH  0x80                    /* Baud rate divisor latch. */
#define NOT_DL  0x7f                    /* Turns off divisor latch. */

/* Line status register (LSR) bits. */
#define RDR     0x01                    /* Receive data ready. */
#define ERRS    0x1E                    /* All the error bits. */
#define TXR     0x20                    /* Transmitter ready. */

/* Interrupt enable register (IER) bits. */
#define DR      0x01                    /* Data ready. */
#define THRE    0x02                    /* Transmit holding register empty. */
#define RLS     0x04                    /* Receive line status. */
#define MS      0x08                    /* Modem status. */

/* Flow control receive buffer limits. */
#define RECEIVE_LOW_NUM     1           /* Numerator for low water mark. */
#define RECEIVE_LOW_DENOM   4           /* Denominator for low water mark. */
#define RECEIVE_HIGH_NUM    3           /* Numerator for high water mark. */
#define RECEIVE_HIGH_DENOM  4           /* Denominator for high water mark. */


/* Built-in async serial I/O global variables. */

/* These variabes are shared throughout the functions that handle the      */
/* built-in UART-base serial I/O, including the interrupt service routine. */
/* Since only one copy of these variables exist, the built-in serial I/O   */
/* routines may only be used to access one port at a time.                 */

/* Default port addresses. */
/* First 4 addresses are standard addresses used for PC/AT COM1 thru COM4. */
/* Second 4 addresses are PS/2 standard addresses used for COM5 thru COM8. */
static int anDefaultPortAddr[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8,
                                  0x4220, 0x4228, 0x5220, 0x5228};

/* UART address variables. */
static int nDataRegAddr;                /* Data register address. */
static int nIntEnableRegAddr;           /* Interrupt enable register. */
static int nIntIDRegAddr;               /* Interrupt ID register address. */
static int nLineCtrlRegAddr;            /* Line control register address. */
static int nModemCtrlRegAddr;           /* Modem control register address. */
static int nLineStatusRegAddr;          /* Line status register address. */
static int nModemStatusRegAddr;         /* Modem status register address. */

/* General variables. */
static BYTE btIntVector;                /* Interrupt vector number for port. */
static char btI8259Bit;                 /* 8259 bit mask. */
static char btI8259Mask;                /* Copy as it was before open. */
static int nI8259MaskRegAddr;           /* Address of i8259 mask register. */
static int nI8259EndOfIntRegAddr;       /* Address of i8259 EOI register. */
static int nI8259MasterEndOfIntRegAddr; /* Address of master PIC EOI reg. */
static char btOldIntEnableReg;          /* Original IER contents. */
static char btOldModemCtrlReg;          /* Original MCR contents. */
static void (INTERRUPT far *pfOldISR)();/* Original ISR routine for IRQ. */
static char bUsingFIFO = FALSE;         /* Are we using 16550 FIFOs? */
static unsigned char btBaseFIFOCtrl;    /* FIFO control register byte. */


/* Transmit queue variables. */
static int nTXQueueSize;                /* Actual size of transmit queue. */
static char *pbtTXQueue;                /* Pointer to transmit queue. */
static int nTXInIndex;                  /* Location to store next byte. */
static int nTXOutIndex;                 /* Location to get next byte. */
static int nTXChars;                    /* Count of characters in queue. */

/*  Receive queue variables. */
static int nRXQueueSize;                /* Actual size of receive queue. */
static char *pbtRXQueue;                /* Pointer to receive queue. */
static int nRXInIndex;                  /* Location to store next byte. */
static int nRXOutIndex;                 /* Location to retrieve next byte. */
static int nRXChars;                    /* Count of characters in queue. */

/* Flow control variables. */
static int nRXHighWaterMark;            /* High water mark for queue size. */
static int nRXLowWaterMark;             /* Low water mark for queue size. */
static BYTE btFlowControl;              /* Flow control method. */
static BOOL bStopTrans;                 /* Flag set to stop transmitting. */

/* ----------------------------------------------------------------------------
 * ODComSetVect()                                      *** PRIVATE FUNCTION ***
 *
 * Sets the function to be called for the specified interrupt level.
 *
 * Parameters: btVector - Interrupt vector level, a value from 0 to 255.
 *
 *             pfISR    - Pointer to the ISR function to be called.
 *
 *     Return: void
 */
static void ODComSetVect(BYTE btVector, void (INTERRUPT far *pfISR)(void))
{
   ASM   push ds
   ASM   mov ah, 0x25
   ASM   mov al, btVector
   ASM   lds dx, pfISR
   ASM   int 0x21
   ASM   pop ds
}


/* ----------------------------------------------------------------------------
 * ODComGetVect()                                      *** PRIVATE FUNCTION ***
 *
 * Returns the address of the function that is currently called for the
 * specified interrupt level.
 *
 * Parameters: btVector - Interrupt vector level, a value from 0 to 255.
 *
 *     Return: A pointer to the code that is currently executed on an interrupt
 *             of the speceified level.
 */
static void (INTERRUPT far *ODComGetVect(BYTE btVector))(void)
{
   void (INTERRUPT far *pfISR)(void);

   ASM   push es
   ASM   mov ah, 0x35
   ASM   mov al, btVector
   ASM   int 0x21
   ASM   mov word ptr pfISR, bx
   ASM   mov word ptr [pfISR+2], bx
   ASM   pop es

   return(pfISR);
}


/* ----------------------------------------------------------------------------
 * ODComInternalTXReady()                              *** PRIVATE FUNCTION ***
 *
 * Returns TRUE if the internal serial I/O transmit buffer is not full.
 *
 * Parameters: none
 *
 *     Return: void
 */
static BOOL ODComInternalTXReady(void)
{
   /* Return TRUE if tx_chars is less than total tx buffer size. */
   return(nTXChars < nTXQueueSize);
}


/* ----------------------------------------------------------------------------
 * ODComInternalResetTX()                              *** PRIVATE FUNCTION ***
 *
 * Clears transmit buffer used by internal serial I/O routines.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODComInternalResetTX(void)
{
   /* Disable interrupts. */
   ASM cli

   /* If we are using 16550A FIFO buffers, then clear the FIFO transmit */
   /* buffer.                                                           */
   if(bUsingFIFO)
   {
      ASM mov al, btBaseFIFOCtrl
      ASM or al, TR
      ASM mov dx, nIntIDRegAddr
      ASM out dx, al
   }

   /* Reset start, end and total count of characters in buffer      */
   /* If buffer is still empty on next transmit interrupt, transmit */
   /* interrupts will be turned off.                                */
   nTXChars = nTXInIndex = nTXOutIndex = 0;

   /* Re-enable interrupts. */
   ASM sti
}


/* ----------------------------------------------------------------------------
 * ODComInternalResetRX()                              *** PRIVATE FUNCTION ***
 *
 * Clears receive buffer used by internal serial I/O routines.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODComInternalResetRX(void)
{
   /* Disable interrupts. */
   ASM cli

   /* If we are using 16550A FIFO buffers, then clear the FIFO receive */
   /* buffer.                                                          */
   if(bUsingFIFO)
   {
      ASM mov al, btBaseFIFOCtrl
      ASM or al, RR
      ASM mov dx, nIntIDRegAddr
      ASM out dx, al
   }

   /* Reset start, end and total count of characters in buffer           */
   /* On the next receive interrupt, data will be added at the beginning */
   /* of the buffer.                                                     */
   nRXChars = nRXInIndex = nRXOutIndex = 0;

   /* Re-enable interrupts. */
   ASM sti
}


/* ----------------------------------------------------------------------------
 * ODComInternalISR()                                  *** PRIVATE FUNCTION ***
 *
 * Interrupt service routine for internal UART-based serial I/O.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void INTERRUPT ODComInternalISR()
{
   char btIIR;
   BYTE btTemp;

   /* Loop until there are no more pending operations to perform with the */
   /* UART. */
   for(;;)
   {
      /* While bit 0 of the UART IIR is 0, there remains pending operations. */
      /* Read IIR. */
      ASM mov dx, nIntIDRegAddr
      ASM in al, dx
      ASM mov btIIR, al

      /* If IIR bit 0 is set, then UART processing is finished.             */
      if(btIIR & 0x01) break;

      /* Bits 1 and 2 of the IIR register identify the type of operation */
      /* to be performed with the UART.                                  */

      /* Switch on bits 1 and 2 of IIR register. */
      switch(btIIR & 0x06)
      {
         case 0x00:
            /* Operation: modem status has changed. */

            /* Read modem status register. */
            ASM mov dx, nModemStatusRegAddr
            ASM in al, dx
            ASM mov btTemp, al

            /* We only care about the modem status register if we are */
            /* using RTS/CTS flow control, and the CTS register has  */
            /* changed.                                               */
            if((btFlowControl & FLOW_RTSCTS) && (btTemp & DCTS))
            {
               if(btTemp & CTS)
               {
                  /* If CTS has gone high, then re-enable transmission. */
                  bStopTrans = FALSE;

                  /* Restart transmission if there is anything in the */
                  /* transmit buffer.                                 */
                  if(nTXChars > 0)
                  {
                     /* Enable transmit interrupt. */
                     ASM mov dx, nIntEnableRegAddr
                     ASM in al, dx
                     ASM or al, THRE
                     ASM out dx, al
                  }
               }
               else
               {
                  /* If CTS has gone low, then stop transmitting. */
                  bStopTrans = TRUE;
               }
            }

            break;

         case 0x02:
            /* Operation: room in transmit register/FIFO. */
            /* Check whether we can send further characters to transmit. */
            if(nTXChars <= 0 || bStopTrans)
            {
               /* If we cannot send more characters, then turn off */
               /* transmit interrupts.                             */
               ASM mov dx, nIntEnableRegAddr
               ASM in al, dx
               ASM and al, 0xfd
               ASM out dx, al
            }
            else
            {
               /* If we still have characters to transmit ... */

               /* Check line status register to determine whether transmit  */
               /* register/FIFO truly has room. Some UARTs trigger transmit */
               /* interrupts before the character has been tranmistted,     */
               /* causing transmitted characters to be lost.                */
               ASM mov dx, nLineStatusRegAddr
               ASM in al, dx
               ASM mov btTemp, al

               if(btTemp & TXR)
               {
                  /* There is room in the transmit register/FIFO. */

                  /* Get next character to transmit. */
                  btTemp = pbtTXQueue[nTXOutIndex++];

                  /* Write character to UART data register. */
                  ASM mov dx, nDataRegAddr
                  ASM mov al, btTemp
                  ASM out dx, al

                  /* Wrap-around transmit buffer pointer, if needed. */
                  if (nTXOutIndex == nTXQueueSize)
                  {
                     nTXOutIndex = 0;
                  }

                  /* Decrease count of characters in transmit buffer. */
                  nTXChars--;
               }
            }
            break;

         case 0x04:
            /* Operation: Receive Data. */

            /* Get character from receive buffer ASAP. */
            ASM mov dx, nDataRegAddr
            ASM in al, dx
            ASM mov btTemp, al

            /* If receive buffer is above high water mark. */
            if(nRXChars >= nRXHighWaterMark)
            {
               /* If we are using flow control, then stop sender from */
               /* sending.                                            */
               if(btFlowControl & FLOW_RTSCTS)
               {
                  /* If using RTS/CTS flow control, then lower RTS line. */
                  ASM mov dx, nModemCtrlRegAddr
                  ASM in al, dx
                  ASM and al, NOT_RTS
                  ASM out dx, al
               }
            }

            /* If there is room in receive buffer. */
            if(nRXChars < nRXQueueSize)
            {
               /* Store the new character in the receive buffer. */
               pbtRXQueue[nRXInIndex++] = btTemp;

               /* Wrap-around buffer index, if needed. */
               if (nRXInIndex == nRXQueueSize)
                  nRXInIndex = 0;

               /* Increment count of characters in the buffer. */
               nRXChars++;
            }
            break;

         case 0x06:
            /* Operation: Change in line status register. */

            /* We just read the register to move on to further operations. */
            ASM mov dx, nLineStatusRegAddr
            ASM in al, dx
            break;
      }
   }

   /* Send end of interrupt to interrupt controller(s). */
   ASM mov dx, nI8259EndOfIntRegAddr
   ASM mov al, 0x20
   ASM out dx, al

   if(nI8259MasterEndOfIntRegAddr != 0)
   {
      ASM mov dx, nI8259MasterEndOfIntRegAddr
      ASM mov al, 0x20
      ASM out dx, al
   }
}
#endif /* INCLUDE_UART_COM */



/* ========================================================================= */
/* Win32-API base serial I/O specific functions.                             */
/* ========================================================================= */

#ifdef INCLUDE_WIN32_COM

/* Function prototypes. */
static tODResult ODComWin32SetReadTimeouts(tPortInfo *pPortInfo,
   tReadTimeoutState RequiredTimeoutState);


/* ----------------------------------------------------------------------------
 * ODComWin32SetReadTimeouts()                         *** PRIVATE FUNCTION ***
 *
 * Ensures that read timeout state is set appropriately.
 *
 * Parameters: pPortInfo            - Pointer to serial port handle structure.
 *
 *             RequiredTimeoutState - Timeout state that should be set.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
static tODResult ODComWin32SetReadTimeouts(tPortInfo *pPortInfo,
   tReadTimeoutState RequiredTimeoutState)
{
   ASSERT(pPortInfo != NULL);

   /* If timeout state must be changed ... */
   if(RequiredTimeoutState != pPortInfo->ReadTimeoutState)
   {
      COMMTIMEOUTS CommTimeouts;

      /* Obtain current timeout settings. */
      if(!GetCommTimeouts(pPortInfo->hCommDev, &CommTimeouts))
      {
         return(kODRCGeneralFailure);
      }

      /* Setup timeout setting structure appropriately. */
      switch(RequiredTimeoutState)
      {
         case kBlocking:
            CommTimeouts.ReadIntervalTimeout = 0;
            CommTimeouts.ReadTotalTimeoutMultiplier = 0;
            CommTimeouts.ReadTotalTimeoutConstant = 0;
            break;
         case kNonBlocking:
            CommTimeouts.ReadIntervalTimeout = INFINITE;
            CommTimeouts.ReadTotalTimeoutMultiplier = 0;
            CommTimeouts.ReadTotalTimeoutConstant = 0;
            break;
         default:
            ASSERT(FALSE);
      }

      /* Write settings. */
      if(!SetCommTimeouts(pPortInfo->hCommDev, &CommTimeouts))
      {
         return(kODRCGeneralFailure);
      }

      /* Record current read timeout setting state for subsequent */
      /* calls to this function.                                  */
      pPortInfo->ReadTimeoutState = RequiredTimeoutState;
   }

   return(kODRCSuccess);
}

#endif /* INCLUDE_WIN32_COM */



/* ========================================================================= */
/* Implementation of generic serial I/O functions.                           */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODComAlloc()
 *
 * Allocates a serial port handle, which can be passed to other ODCom...()
 * functions.
 *
 * Parameters: phPort - Pointer to serial port handle.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComAlloc(tPortHandle *phPort)
{
   tPortInfo *pPortInfo;

   VERIFY_CALL(phPort != NULL);

   /* Attempt to allocate a serial port information structure. */
   pPortInfo = malloc(sizeof(tPortInfo));

   /* If memory allocation failed, return with failure. */
   if(pPortInfo == NULL)
   {
      *phPort = ODPTR2HANDLE(NULL, tPortInfo);
      return(kODRCNoMemory);
   }

   /* Initialize serial port information structure. */
   pPortInfo->bIsOpen = FALSE;
   pPortInfo->bUsingClientsHandle = FALSE;
   pPortInfo->btFlowControlSetting = FLOW_DEFAULT;
   pPortInfo->lSpeed = SPEED_UNSPECIFIED;
   pPortInfo->btWordFormat = ODPARITY_NONE | DATABITS_EIGHT | STOP_ONE;
   pPortInfo->nReceiveBufferSize = 1024;
   pPortInfo->nTransmitBufferSize = 1024;
   pPortInfo->btFIFOSetting = FIFO_ENABLE | FIFO_TRIGGER_8;
   pPortInfo->Method = kComMethodUnspecified;
   pPortInfo->pfIdleCallback = NULL;

   /* Convert serial port information structure pointer to a handle. */
   *phPort = ODPTR2HANDLE(pPortInfo, tPortInfo);

   /* Set default port number. */
   ODComSetPort(*phPort, 0);

#if defined(INCLUDE_SOCKET_COM) && defined(_WINSOCKAPI_)
	WSAStartup(MAKEWORD(1,1), &WSAData);
#endif

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComFree()
 *
 * Deallocates a serial port handle that is no longer required.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComFree(tPortHandle hPort)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   /* Deallocate port information structure. */
   free(pPortInfo);

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetIdleFunction()
 *
 * Sets function to call when serial I/O module is idle, or NULL for none.
 *
 * Parameters: hPort      - Handle to a serial port object.
 *
 *             pfCallback - Pointer to function to call when idle.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetIdleFunction(tPortHandle hPort,
   void (*pfCallback)(void))
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->pfIdleCallback = pfCallback;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetFlowControl()
 *
 * Sets the flow control method(s) to use. If this function is not called,
 * RTS/CTS flow control is used by default. This function should not be
 * called while the port is open.
 *
 * Parameters: hPort                - Handle to a serial port object.
 *
 *             btFlowControlSetting - One or more FLOW_* settings, joined
 *                                    by bitwise-or (|) operators. If
 *                                    FLOW_DEFAULT is included, all other
 *                                    settings are ignored, and the default
 *                                    settings for this serial I/O method
 *                                    are used.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetFlowControl(tPortHandle hPort, BYTE btFlowControlSetting)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->btFlowControlSetting = btFlowControlSetting;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetSpeed()
 *
 * Sets the serial port BPS (baud) rate to use. Depending upon the serial I/O
 * method being used, this setting may be controlled by the user's system
 * configuration, in which case the value passed to this function wil have
 * no effect. A setting of SPEED_UNSPECIFIED, indicates that the serial port
 * speed should not be changed, if it is possible not to do so with the serial
 * I/O method being used. This function cannot be called while the port is
 * open.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *             lSpeed - A valid BPS rate, or SPEED_UNSPECIFIED.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetSpeed(tPortHandle hPort, long lSpeed)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->lSpeed = lSpeed;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetPort()
 *
 * Sets the serial port number to be associated with this port handle. This
 * function cannot be called while the port is open. Calling this function
 * also sets the IRQ line number and serial port address to their defaults
 * for this port number, if this values can be set for the serial I/O method
 * being used.
 *
 * Parameters: hPort  - Handle to a serial port object.
 *
 *             btPort - Serial port identification, where 0 typically
 *                      corresponds to COM1, 1 to COM2, and so on.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetPort(tPortHandle hPort, BYTE btPort)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   /* Store port number in port information structure. */
   pPortInfo->btPort = btPort;


#ifdef INCLUDE_UART_COM
   /* Get default address for this port number, if possible. */
   pPortInfo->nPortAddress = 0;

   if(btPort < 4)
   {
      /* Get port address from BIOS data area. */
      pPortInfo->nPortAddress = *(((int far *)0x400) + btPort);
   }

   /* If port address is still unknown, and we know the default */
   /* address, then use that address. */
   if(pPortInfo->nPortAddress == 0
      && btPort < DIM(anDefaultPortAddr))
   {
      pPortInfo->nPortAddress = anDefaultPortAddr[btPort];
   }


   /* Set default IRQ number for this port number. */

   /* Ports 0 and 2 (COM1:, COM3:) default to IRQ 4, all others */
   /* default to IRQ 3. */
   if(btPort == 0 || btPort == 2)
   {
      pPortInfo->btIRQLevel = 4;
   }
   else
   {
      pPortInfo->btIRQLevel = 3;
   }
#endif /* INCLUDE_UART_COM */

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetPortAddress()
 *
 * Sets address of the serial port, if it can be set for the serial I/O method
 * being used. This function cannot be called when the port is open.
 *
 * Parameters: hPort        - Handle to a serial port object.
 *
 *             nPortAddress - Address of serial port.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetPortAddress(tPortHandle hPort, int nPortAddress)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->nPortAddress = nPortAddress;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetIRQ()
 *
 * Sets the IRQ line associated with this serial port, if applicable for the
 * serial I/O method being used. This function cannot be called while the port
 * is open.
 *
 * Parameters: hPort      - Handle to a serial port object.
 *
 *             btIRQLevel - A number from 1 to 15, specifying the IRQ line that
 *                          the serial port is wired to.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetIRQ(tPortHandle hPort, BYTE btIRQLevel)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->btIRQLevel = btIRQLevel;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetWordFormat()
 *
 * Determine the word format (number of data bits, stop bits and parity bits)
 * to use, if it can be set for the serial I/O method being used. If this
 * function is not called, N81 word format is used. This function can only
 * be called when the port is not open.
 *
 * Parameters: hPort        - Handle to a serial port object.
 *
 *             btWordFormat - Bitwise-or (|) of PARITY_*, STOP_* and DATABITS_*
 *                            settings which determine the word format to use.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetWordFormat(tPortHandle hPort, BYTE btWordFormat)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->btWordFormat = btWordFormat;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetRXBuf()
 *
 * Sets the desired size of the receive buffer, if possible for the
 * serial I/O method being used. Note that for some serial I/O methods, this
 * buffer size is fixed, controlled by the user's system configuration.
 * No error is generated when this function is called when such serial I/O
 * methods will be used - in this case this setting will simply have no effect.
 * This function cannot be called while the port is open.
 *
 * Parameters: hPort              - Handle to a serial port object.
 *
 *             nReceiveBufferSize - Number of bytes in the receive buffer.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetRXBuf(tPortHandle hPort, int nReceiveBufferSize)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->nReceiveBufferSize = nReceiveBufferSize;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetTXBuf()
 *
 * Sets the desired size of the transmit buffer, if possible for the
 * serial I/O method being used. Note that for some serial I/O methods, this
 * buffer size is fixed, controlled by the user's system configuration.
 * No error is generated when this function is called when such serial I/O
 * methods will be used - in this case this setting will simply have no effect.
 * This function cannot be called while the port is open.
 *
 * Parameters: hPort               - Handle to a serial port object.
 *
 *             nTransmitBufferSize - Number of bytes in the transmit buffer.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetTXBuf(tPortHandle hPort, int nTransmitBufferSize)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->nTransmitBufferSize = nTransmitBufferSize;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetFIFO()
 *
 * Enables or disables use of the UART FIFO buffers (if applicable), and also
 * sets the FIFO trigger level. This function cannot be called while the port
 * is open.
 *
 * Parameters: hPort         - Handle to a serial port object.
 *
 *             btFIFOSetting - UART FIFO setting, including FIFO_ENABLE or
 *                             FIDO_DISABLE, and a FIFO_TRIGGER_* setting,
 *                             joined by bitwise-or (|) operators.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetFIFO(tPortHandle hPort, BYTE btFIFOSetting)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->btFIFOSetting = btFIFOSetting;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetPreferredMethod()
 *
 * Sets the method to be used to perform serial I/O.
 *
 * Parameters: hPort  - Handle to a serial port object.
 *
 *             Method - The method to be used for peforming serial I/O,
 *                      or kComMethodUnspecified to have the serial I/O
 *                      routines to automatically choose the method to use.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetPreferredMethod(tPortHandle hPort, tComMethod Method)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

   pPortInfo->Method = Method;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComGetMethod()
 *
 * Returns the method being used to perform serial I/O, if this has been
 * determined. You can only assume that this value will be set after
 * ODComOpen() has been called.
 *
 * Parameters: hPort   - Handle to a serial port object.
 *
 *             pMethod - Pointer to a tComMethod, in which function will
 *                       store the method of serial I/O being used, if this
 *                       has been determined.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComGetMethod(tPortHandle hPort, tComMethod *pMethod)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pMethod != NULL);

   *pMethod = pPortInfo->Method;

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComOpen()
 *
 * Initializes serial I/O for appropriate serial I/O mechanism (e.g. FOSSIL
 * driver, internal async I/O, etc.)
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComOpen(tPortHandle hPort)
{
#if defined(INCLUDE_FOSSIL_COM) || defined(INCLUDE_UART_COM)
   unsigned int uDivisor;
   unsigned long ulQuotient, ulRemainder;
   BYTE btTemp;
#endif /* INCLUDE_FOSSIL_COM || INCLUDE_UART_COM */
#ifdef INCLUDE_STDIO_COM
	struct termios tio_raw;
#endif
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   nPort = (int)pPortInfo->btPort;

   /* Ensure that port is not already open. */
   VERIFY_CALL(!pPortInfo->bIsOpen);

   /* The following code is used to handle FOSSIL-based serial I/O open */
   /* operations.                                                       */
#ifdef INCLUDE_FOSSIL_COM
   /* If use of FOSSIL driver has not been disabled, then first attempt to */
   /* use it.                                                              */
   if(pPortInfo->Method == kComMethodFOSSIL ||
      pPortInfo->Method == kComMethodUnspecified)
   {
      /* Attempt to open port with FOSSIL DRIVER. */
      ASM    push si
      ASM    push di
      ASM    mov ah, 4
      ASM    mov dx, nPort
      ASM    mov bx, 0
      ASM    int 20
      ASM    pop di
      ASM    pop si
      ASM    cmp ax, 6484
      ASM    je fossil
      goto no_fossil;

fossil:
      pPortInfo->Method = kComMethodFOSSIL;

      /* Enable flow control, if applicable. */

      /* Generate flow control setting. All bits in high nibble of flow   */
      /* control are set to 1, because some FOSSIL driver implementations */
      /* use the high nibble as a control mask.                           */
      if(pPortInfo->btFlowControlSetting & FLOW_DEFAULT)
      {
         btTemp = FLOW_RTSCTS | 0xf0;
      }
      else
      {
         btTemp = pPortInfo->btFlowControlSetting | 0xf0;
      }

      ASM    push si
      ASM    push di
      ASM    mov ah, 0x0f
      ASM    mov al, btTemp
      ASM    mov dx, nPort
      ASM    int 20
      ASM    pop di
      ASM    pop si

      /* If serial port speed is not to be set, then return now. */
      if(pPortInfo->lSpeed == SPEED_UNSPECIFIED)
      {
         /* Set port state to open. */
         pPortInfo->bIsOpen = TRUE;

         /* Return with success. */
         return(kODRCSuccess);
      }

      /* Set to current baud rate. */
      switch(pPortInfo->lSpeed)
      {
         case 300L:
            btTemp = 0x40;
            break;
         case 600L:
            btTemp = 0x60;
            break;
         case 1200L:
            btTemp = 0x80;
            break;
         case 2400L:
            btTemp = 0xa0;
            break;
         case 4800L:
            btTemp = 0xc0;
            break;
         case 9600L:
            btTemp = 0xe0;
            break;
         case 19200L:
            btTemp = 0x00;
            break;
         case 38400L:
            btTemp = 0x20;
            break;
         default:
            /* If invalid bps rate, don't change current bps setting. */
            /* Set port state to open. */
            pPortInfo->bIsOpen = TRUE;

            /* Return with success. */
            return(kODRCSuccess);
      }

      /* Add desired word format parameters to data to be passed to fossil. */
      btTemp |= pPortInfo->btWordFormat;

      /* Initialize fossil driver. */
      ASM    push si
      ASM    push di
      ASM    mov al, btTemp
      ASM    mov ah, 0
      ASM    mov dx, nPort
      ASM    int 20
      ASM    pop di
      ASM    pop si

      /* Set port state to open. */
      pPortInfo->bIsOpen = TRUE;

      /* Return with success. */
      return(kODRCSuccess);
   }

no_fossil:
#endif /* INCLUDE_FOSSIL_COM */

   /* The following code is used to carry out the serial port I/O open */
   /* operations if built-in UART-based serial I/O is being used.      */
#ifdef INCLUDE_UART_COM
   if(pPortInfo->Method == kComMethodUART ||
      pPortInfo->Method == kComMethodUnspecified)
   {
      /* Set internal serial I/O flow control variable from pre-set */
      /* flow control options.                                      */
      if(pPortInfo->btFlowControlSetting & FLOW_DEFAULT)
      {
         btFlowControl = FLOW_RTSCTS;
      }
      else
      {
         btFlowControl = pPortInfo->btFlowControlSetting;
      }

      /* Store serial I/O method being used. */
      pPortInfo->Method = kComMethodUART;

      /* Calculate receive buffer high and low water marks for use with */
      /* flow control. */
      nRXHighWaterMark = (pPortInfo->nReceiveBufferSize * RECEIVE_HIGH_NUM)
         / RECEIVE_HIGH_DENOM;
      nRXLowWaterMark = (pPortInfo->nReceiveBufferSize * RECEIVE_LOW_NUM)
         / RECEIVE_LOW_DENOM;

      /* Allocate transmit and receive buffers */
      pbtTXQueue = malloc(nTXQueueSize = pPortInfo->nTransmitBufferSize);
      pbtRXQueue = malloc(nRXQueueSize = pPortInfo->nReceiveBufferSize);

      if(pbtTXQueue == NULL || pbtRXQueue == NULL)
      {
         return(kODRCNoMemory);
      }

      /* If serial port address is unknown. */
      if(pPortInfo->nPortAddress == 0)
      {
         return(kODRCNoPortAddress);
      }

      /* Initialize table of UART register port addresses. */
      nDataRegAddr = pPortInfo->nPortAddress;
      nIntEnableRegAddr = nDataRegAddr + IER;
      nIntIDRegAddr = nDataRegAddr + IIR;
      nLineCtrlRegAddr = nDataRegAddr + LCR;
      nModemCtrlRegAddr = nDataRegAddr + MCR;
      nLineStatusRegAddr = nDataRegAddr + LSR;
      nModemStatusRegAddr = nDataRegAddr + MSR;


      /* Store interrupt vector number and PIC interrupt information for */
      /* the specified IRQ line.                                         */
      if(pPortInfo->btIRQLevel <= 7)
      {
         btIntVector = 0x08 + (pPortInfo->btIRQLevel);
         btI8259Bit = 1 << (pPortInfo->btIRQLevel);
         nI8259MaskRegAddr = 0x21;
         nI8259EndOfIntRegAddr = 0x20;
         nI8259MasterEndOfIntRegAddr = 0x00;
      }
      else
      {
         btIntVector = 0x68 + (pPortInfo->btIRQLevel);
         btI8259Bit = 1 << (pPortInfo->btIRQLevel - 8);
         nI8259MaskRegAddr = 0xA1;
         nI8259EndOfIntRegAddr = 0xA0;
         nI8259MasterEndOfIntRegAddr = 0x20;
      }

      /* Save original state of UART IER register. */
      ASM mov dx, nIntEnableRegAddr
      ASM in al, dx
      ASM mov btOldIntEnableReg, al

      /* Test that a UART is indeed installed at this port address. */
      ASM mov dx, nIntEnableRegAddr
      ASM mov al, 0
      ASM out dx, al

      ASM mov dx, nIntEnableRegAddr
      ASM in al, dx
      ASM mov btTemp, al

      if (btTemp != 0)
      {
         return(kODRCNoUART);
      }

      /* Setup for RTS/CTS flow control, if it is to be used. */
      if(btFlowControl & FLOW_RTSCTS)
      {
         /* Read modem status register. */
         ASM mov dx, nModemStatusRegAddr
         ASM in al, dx
         ASM mov btTemp, al

         /* Enable transmission only if CTS is high. */
         bStopTrans = !(btTemp & CTS);
      }

      /* Save original PIC interrupt settings, and temporarily disable */
      /* interrupts on this IRQ line while we perform initialization.  */
      ASM cli

      ASM mov dx, nI8259MaskRegAddr
      ASM in al, dx
      ASM mov btI8259Mask, al
      ASM or  al, btI8259Bit
      ASM out dx, al

      /* Initialize transmit and recieve buffers. */
      ODComInternalResetTX();
      ODComInternalResetRX();

      /* Re-enable interrupts. */
      ASM sti

      /* Save original interrupt vector. */
      pfOldISR = ODComGetVect(btIntVector);

      /* Set interrupt vector to point to our ISR. */
#ifdef _MSC_VER
      ODComSetVect(btIntVector, (void far *)ODComInternalISR);
#else /* !_MSC_VER */
      ODComSetVect(btIntVector, ODComInternalISR);
#endif /* !_MSC_VER */

      /* Set line control register to 8 data bits, no parity bits, 1 stop */
      /* bit. */
      btTemp = pPortInfo->btWordFormat;
      ASM mov dx, nLineCtrlRegAddr
      ASM mov al, btTemp
      ASM out dx, al

      /* Save original modem control register. */
      ASM cli

      ASM mov dx, nModemCtrlRegAddr
      ASM in al, dx
      ASM mov btOldModemCtrlReg, al

      /* Keep current DTR setting, and activate RTS. */
      btTemp = (btOldModemCtrlReg & DTR) | (OUT2 + RTS);
      ASM mov dx, nModemCtrlRegAddr
      ASM mov al, btTemp
      ASM out dx, al

      /* Enable use of 16550A FIFOs, if available. */
      if(pPortInfo->btFIFOSetting & FIFO_ENABLE)
      {
         /* Set FIFO enable bit and trigger size. */
         btBaseFIFOCtrl = pPortInfo->btFIFOSetting;

         /* Attempt to enable use of FIFO buffers. */
         ASM mov al, btBaseFIFOCtrl
         ASM mov dx, nIntIDRegAddr
         ASM out dx, al

         /* Check whether a 16550A UART is actually present by reading */
         /* state of FIFO buffer. */
         ASM mov dx, nIntIDRegAddr
         ASM in al, dx
         ASM mov btTemp, al

         bUsingFIFO = btTemp & 0xc0;
      }

      ASM sti

      /* Enable receive and modem status interrupts on the UART. */
      ASM mov dx, nIntEnableRegAddr
      ASM mov al, DR + MS
      ASM out dx, al

      ASM cli

      ASM mov dx, nI8259MaskRegAddr
      ASM in al, dx
      ASM mov ah, btI8259Bit
      ASM not ah
      ASM and al, ah
      ASM out dx, al

      ASM sti

      /* Set baud rate, if possible. */

      /* Calculate baud rate divisor. */
      if(pPortInfo->lSpeed != SPEED_UNSPECIFIED)
      {
         ODDWordDivide(&ulQuotient, &ulRemainder, 115200UL, pPortInfo->lSpeed);

         /* If division results in a remainder, then this is an invalid     */
         /* baud rate. We only change the UART baud rate if we have a valid */
         /* rate to set it to. Otherwise, we cross our fingers and proceed  */
         /* with the currently set UART baud rate.                          */
         if(ulRemainder == 0L)
         {
            uDivisor = (unsigned int)ulQuotient;

            /* Disable interrupts. */
            ASM cli

            /* Set baud rate divisor latch. */
            /* The data register now becomes the lower byte of the baud rate */
            /* divisor, and the interrupt enable register becomes the upper  */
            /* byte of the divisor.                                          */
            ASM mov dx, nLineCtrlRegAddr
            ASM in al, dx
            ASM or al, DLATCH
            ASM out dx, al

            /* Write lower byte of baud rate divisor. */
            ASM mov dx, nDataRegAddr
            ASM mov ax, uDivisor
            ASM out dx, al

            /* Write upper byte of baud rate divisor. */
            ASM mov dx, nIntEnableRegAddr
            ASM mov al, ah
            ASM out dx, al

            /* Reset baud rate divisor latch. */
            ASM mov dx, nLineCtrlRegAddr
            ASM in al, dx
            ASM and al, NOT_DL
            ASM out dx, al

            /* Re-enable interrupts. */
            ASM sti
         }
      }

      /* Remember the serial I/O method that we are using. */
      pPortInfo->Method = kComMethodUART;

      /* Store port state as open. */
      pPortInfo->bIsOpen = TRUE;

      /* Return with success. */
      return(kODRCSuccess);
   }
#endif /* INCLUDE_UART_COM */

   /* The following code is used to handle I/O using the Door32 interface. */
#ifdef INCLUDE_DOOR32_COM
   if(pPortInfo->Method == kComMethodDoor32 ||
      pPortInfo->Method == kComMethodUnspecified)
   {
      /* Attempt to load the Door32 DLL. */
      pPortInfo->hinstDoor32DLL = LoadLibrary("DOOR32.DLL");
      if(pPortInfo->hinstDoor32DLL != NULL)
      {
         /* Obtain pointers to required Door32 API function entry points. */
         pPortInfo->pfDoorInitialize = (BOOL (WINAPI *)(void))
            GetProcAddress(pPortInfo->hinstDoor32DLL, "DoorInitialize");
         pPortInfo->pfDoorShutdown = (BOOL (WINAPI *)(void))
            GetProcAddress(pPortInfo->hinstDoor32DLL, "DoorShutdown");
         pPortInfo->pfDoorWrite = (BOOL (WINAPI *)(const BYTE *, DWORD))
            GetProcAddress(pPortInfo->hinstDoor32DLL, "DoorWrite");
         pPortInfo->pfDoorRead = (DWORD (WINAPI *)(BYTE *, DWORD))
            GetProcAddress(pPortInfo->hinstDoor32DLL, "DoorRead");
         pPortInfo->pfDoorGetAvailableEventHandle = (HANDLE (WINAPI *)(void))
            GetProcAddress(pPortInfo->hinstDoor32DLL,
            "DoorGetAvailableEventHandle");
         pPortInfo->pfDoorGetOfflineEventHandle = (HANDLE (WINAPI *)(void))
            GetProcAddress(pPortInfo->hinstDoor32DLL,
            "DoorGetOfflineEventHandle");

         /* Check whether we have successfully been able to obtain all the */
         /* required function entry points.                                */
         if(pPortInfo->pfDoorInitialize != NULL
            && pPortInfo->pfDoorShutdown != NULL
            && pPortInfo->pfDoorWrite != NULL
            && pPortInfo->pfDoorRead != NULL
            && pPortInfo->pfDoorGetAvailableEventHandle != NULL
            && pPortInfo->pfDoorGetOfflineEventHandle != NULL)
         {
            if((*pPortInfo->pfDoorInitialize)())
            {
               /* Set port state as open. */
               pPortInfo->bIsOpen = TRUE;

               /* Set serial I/O method. */
               pPortInfo->Method = kComMethodDoor32;

               /* Return with success. */
               return(kODRCSuccess);
            }
         }

         /* On failure to obtain all Door32 function entry points, unload */
         /* the Door32 DLL.                                               */
         FreeLibrary(pPortInfo->hinstDoor32DLL);
      }

      /* If our attempt to use the Door32 interface failed for any reason, */
      /* then proceed, attempting to use the Win32 serial I/O interface.   */
   }
#endif /* INCLUDE_DOOR32_COM */

   /* The following code is used to handle Win32 API-base serial I/O */
   /* open operations.                                               */
#ifdef INCLUDE_WIN32_COM
   if(pPortInfo->Method == kComMethodWin32 ||
      pPortInfo->Method == kComMethodUnspecified)
   {
      char szDevName[7];
      DCB dcb;

      /* Generate device name. */
      sprintf(szDevName, "COM%u", (unsigned)pPortInfo->btPort + 1);

      /* Attempt to create handle for device. */
      pPortInfo->hCommDev = CreateFile(szDevName, GENERIC_READ | GENERIC_WRITE,
         0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

      /* On open failure, return with an error code. */
      if(pPortInfo->hCommDev == INVALID_HANDLE_VALUE)
      {
         return(kODRCGeneralFailure);
      }

      /* Note that read timeout settings have not been set. */
      pPortInfo->ReadTimeoutState = kNotSet;
      
      /* Call SetupComm() to set queue sizes. */
      if(!SetupComm(pPortInfo->hCommDev, pPortInfo->nReceiveBufferSize,
         pPortInfo->nTransmitBufferSize))
      {
         CloseHandle(pPortInfo->hCommDev);
         return(kODRCGeneralFailure);
      }

      /* Get current port state. */
      if(!GetCommState(pPortInfo->hCommDev, &dcb))
      {
         CloseHandle(pPortInfo->hCommDev);
         return(kODRCGeneralFailure);
      }

      /* Fill device control block. */

      /* Set bps rate, if appropriate. */
      if(pPortInfo->lSpeed != SPEED_UNSPECIFIED)
      {
         dcb.BaudRate = pPortInfo->lSpeed;
      }

      /* Set flow control, if appropriate. */
      if(!(pPortInfo->btFlowControlSetting & FLOW_DEFAULT))
      {
         if(pPortInfo->btFlowControlSetting & FLOW_RTSCTS)
         {
            dcb.fOutxCtsFlow = 1;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
         }
         else
         {
            dcb.fOutxCtsFlow = 0;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
         }
      }

      /* Set word size. */
      if((pPortInfo->btWordFormat & DATABITS_MASK) == DATABITS_FIVE)
      {
         dcb.ByteSize = 5;
      }
      else if((pPortInfo->btWordFormat & DATABITS_MASK) == DATABITS_SIX)
      {
         dcb.ByteSize = 6;
      }
      else if((pPortInfo->btWordFormat & DATABITS_MASK) == DATABITS_SEVEN)
      {
         dcb.ByteSize = 7;
      }
      else if((pPortInfo->btWordFormat & DATABITS_MASK) == DATABITS_EIGHT)
      {
         dcb.ByteSize = 8;
      }

      /* Set parity. */
      if((pPortInfo->btWordFormat & ODPARITY_MASK) == ODPARITY_NONE)
      {
         dcb.Parity = NOPARITY;
      }
      else if((pPortInfo->btWordFormat & ODPARITY_MASK) == ODPARITY_ODD)
      {
         dcb.Parity = ODDPARITY;
      }
      else if((pPortInfo->btWordFormat & ODPARITY_MASK) == ODPARITY_EVEN)
      {
         dcb.Parity = EVENPARITY;
      }

      /* Enable DTR control. */
      dcb.fDtrControl = DTR_CONTROL_ENABLE;

      /* Set number of stop bits. */
      if((pPortInfo->btWordFormat & STOP_MASK) == STOP_ONE)
      {
         dcb.StopBits = ONESTOPBIT;
      }
      else if((pPortInfo->btWordFormat & STOP_MASK) == STOP_ONE_POINT_FIVE)
      {
         dcb.StopBits = ONE5STOPBITS;
      }
      else if((pPortInfo->btWordFormat & STOP_MASK) == STOP_TWO)
      {
         dcb.StopBits = TWOSTOPBITS;
      }

      /* Set comm state from device control block. */
      if(!SetCommState(pPortInfo->hCommDev, &dcb))
      {
         CloseHandle(pPortInfo->hCommDev);
         return(kODRCGeneralFailure);
      }

      /* Store port state as open. */
      pPortInfo->bIsOpen = TRUE;

      /* Set serial I/O method. */
      pPortInfo->Method = kComMethodWin32;

      /* Return with success. */
      return(kODRCSuccess);
   }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_STDIO_COM
   if(pPortInfo->Method == kComMethodStdIO ||
      pPortInfo->Method == kComMethodUnspecified)
   {
		if (isatty(STDIN_FILENO))  {
			tcgetattr(STDIN_FILENO,&tio_default);
			tio_raw = tio_default;
			cfmakeraw(&tio_raw);
			tcsetattr(STDIN_FILENO,TCSANOW,&tio_raw);
			setvbuf(stdout, NULL, _IONBF, 0);
		}

      /* Set port state as open. */
      pPortInfo->bIsOpen = TRUE;

      /* Set serial I/O method. */
      pPortInfo->Method = kComMethodStdIO;

      /* Return with success. */
      return(kODRCSuccess);

   }
#endif /* INCLUDE_STDIO_COM */

   /* If we get to this point, then no form of serial I/O could be */
   /* initialized.                                                 */
   return(kODRCGeneralFailure);
}


/* ----------------------------------------------------------------------------
 * ODComOpenFromExistingHandle()
 *
 * Initializes serial I/O using a serial port handle natvie to the current
 * operating system, which has already been opened by another application.
 *
 * Parameters: hPort            - Handle to a serial port object.
 *
 *             dwExistingHandle - Native operating system's handle to an
 *                                already open serial port.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComOpenFromExistingHandle(tPortHandle hPort,
   DWORD dwExistingHandle)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(!pPortInfo->bIsOpen);

#ifdef INCLUDE_SOCKET_COM
	if(pPortInfo->Method == kComMethodSocket) {
		socklen_t delay=FALSE;

		pPortInfo->socket = dwExistingHandle;

		getsockopt(pPortInfo->socket, IPPROTO_TCP, TCP_NODELAY, &(pPortInfo->old_delay), &delay);
		delay=FALSE;
		setsockopt(pPortInfo->socket, IPPROTO_TCP, TCP_NODELAY, &delay, sizeof(delay));

        pPortInfo->bIsOpen = TRUE;

		return(kODRCSuccess);
	}
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_WIN32_COM

   /* Store handle to the Win32 handle to the serial port. */
   pPortInfo->hCommDev = (HANDLE)dwExistingHandle;

   /* Remember that read timeout settings have not been set. */
   pPortInfo->ReadTimeoutState = kNotSet;

   /* Remember that we are using a handle provided by the client, rather  */
   /* than one that we opened ourself. This flag prevents the handle from */
   /* being closed by a call to ODComClose().                             */
   pPortInfo->bUsingClientsHandle = TRUE;

   /* Remember that the serial port is now open. */
   pPortInfo->bIsOpen = TRUE;

   return(kODRCSuccess);

#else /* !INCLUDE_WIN32_COM */
   UNUSED(dwExistingHandle);
   UNUSED(pPortInfo);

   /* If no form of serial I/O included in this build can use this handle, */
   /* then return with a failure.                                          */
   return(kODRCInvalidCall);

#endif /* !INCLUDE_WIN32_COM */
}


/* ----------------------------------------------------------------------------
 * ODComClose()
 *
 * Closes currently open serial port.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComClose(tPortHandle hPort)
{
#ifdef INCLUDE_UART_COM
   BYTE btTemp;
#endif /* INCLUDE_UART_COM */
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   /* If we are using the client's handle, then we should not close it. */
   if(pPortInfo->bUsingClientsHandle)
   {
      pPortInfo->bIsOpen = FALSE;
      return(kODRCSuccess);
   }

   nPort = (int)pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
         ASM    mov ah, 5
         ASM    mov dx, nPort
         ASM    int 20
         break;
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         /* Reset UART registers to their original values. */
         ASM mov dx, nModemCtrlRegAddr
         ASM mov al, btOldModemCtrlReg
         ASM out dx, al
         ASM mov dx, nIntEnableRegAddr
         ASM mov al, btOldIntEnableReg
         ASM out dx, al

         /* Disable interrupts. */
         ASM cli

         /* Reset this line's interrupt enable status on the PIC to its */
         /* original state.                                             */
         ASM mov dx, nI8259MaskRegAddr
         ASM in al, dx
         ASM mov btTemp, al

         btTemp = (btTemp  & ~btI8259Bit) | (btI8259Mask &  btI8259Bit);

         ASM mov dx, nI8259MaskRegAddr
         ASM mov al, btTemp
         ASM out dx, al

         /* Re-enable interrupts. */
         ASM sti

         /* Reset vector to original interrupt handler. */
#ifdef _MSC_VER
         ODComSetVect(btIntVector, (void far *)pfOldISR);
#else /* !_MSC_VER */
         ODComSetVect(btIntVector, pfOldISR);
#endif /* !_MSC_VER */

         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
         CloseHandle(pPortInfo->hCommDev);
         break;
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorShutdown != NULL);
         (*pPortInfo->pfDoorShutdown)();
         ASSERT(pPortInfo->hinstDoor32DLL != NULL);
         FreeLibrary(pPortInfo->hinstDoor32DLL);
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		 setsockopt(pPortInfo->socket, IPPROTO_TCP, TCP_NODELAY, &(pPortInfo->old_delay), sizeof(pPortInfo->old_delay));
         closesocket(pPortInfo->socket);
         break;
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
	  case kComMethodStdIO:
	     if(isatty(STDIN_FILENO))
		    tcsetattr(STDIN_FILENO,TCSANOW,&tio_default);
	     break;
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Store the fact that the port is now closed. */
   pPortInfo->bIsOpen = FALSE;

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComCarrier()
 *
 * Determines whether or not the carrier detect signal is present.
 *
 * Parameters: hPort       - Handle to a serial port object.
 *
 *             pbIsCarrier - Location to store result. Set to TRUE if carrier
 *                           detect signal is high, FALSE if it is low.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComCarrier(tPortHandle hPort, BOOL *pbIsCarrier)
{
#ifdef ODPLAT_NIX
   sigset_t	  sigs;
#endif
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pbIsCarrier != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
      {
         int to_return;

         ASM    mov ah, 3
         ASM    mov dx, nPort
         ASM    int 20
         ASM    and ax, 128
         ASM    mov to_return, ax

         *pbIsCarrier = to_return;

         break;
      }
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
      {
         BYTE btMSR;

         ASM mov dx, nModemStatusRegAddr
         ASM in al, dx
         ASM mov btMSR, al

         *pbIsCarrier = btMSR & RLSD;
         break;
      }
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwModemStats;

         /* Get modem status settings. */
         if(!GetCommModemStatus(pPortInfo->hCommDev, &dwModemStats))
         {
            return(kODRCGeneralFailure);
         }

         *pbIsCarrier = (dwModemStats & MS_RLSD_ON) ? TRUE : FALSE;

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorGetOfflineEventHandle != NULL);
         *pbIsCarrier = (WaitForSingleObject(
            (*pPortInfo->pfDoorGetOfflineEventHandle)(),
            0) != WAIT_OBJECT_0);
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			int		i;
			char		ch;
			fd_set	socket_set;
			struct	timeval tv;

			FD_ZERO(&socket_set);
			FD_SET(pPortInfo->socket,&socket_set);

			tv.tv_sec=0;
			tv.tv_usec=0;
			i=select(pPortInfo->socket+1,&socket_set,NULL,NULL,&tv);
			if(i==0 
				|| (i==1 && recv(pPortInfo->socket,&ch,1,MSG_PEEK)==1))
				*pbIsCarrier = TRUE;
			else
				*pbIsCarrier = FALSE;
			break;
		}
#endif

#ifdef INCLUDE_STDIO_COM
	  case kComMethodStdIO:
	    {
			sigpending(&sigs);
			if(sigismember(&sigs,SIGHUP))
				*pbIsCarrier = FALSE;
			else
				*pbIsCarrier = TRUE;
			break;
		}
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSetDTR()
 *
 * Raises or lowers the DTR signal on the port.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *             bHigh - TRUE to raise DTR, FALSE to lower it.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSetDTR(tPortHandle hPort, BOOL bHigh)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
         ASM    cmp byte ptr bHigh, 0
         ASM    je lower
         ASM    mov al, 1
         ASM    jmp set_dtr

lower:
         ASM    xor al, al

set_dtr:
         ASM    mov ah, 6
         ASM    mov dx, nPort
         ASM    int 20
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         if(bHigh)
         {
            ASM cli

            ASM mov dx, nModemCtrlRegAddr
            ASM in al, dx
            ASM or al, DTR
            ASM out dx, al

            ASM sti
         }
         else
         {
            ASM cli

            ASM mov dx, nModemCtrlRegAddr
            ASM in al, dx
            ASM and al, NOT_DTR
            ASM out dx, al

            ASM sti
         }
         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
         /* Set DTR line appropriately. */
         if(!EscapeCommFunction(pPortInfo->hCommDev, bHigh ? SETDTR : CLRDTR))
         {
            return(kODRCGeneralFailure);
         }
         break;
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         return(kODRCUnsupported);
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
         if(bHigh)
            return(kODRCUnsupported);
         closesocket(pPortInfo->socket);
         break;
#endif /* INCLUDE_SOCKET_CO */

#ifdef INCLUDE_STDIO_COM
	  case kComMethodStdIO:
	     return(kODRCUnsupported);
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComOutbound()
 *
 * Determines the number of bytes waiting in the serial port outbound buffer.
 *
 * Parameters: hPort             - Handle to a serial port object.
 *
 *             pnOutboundWaiting - Location where result the number of bytes
 *                                 waiting in the outbound buffer should be
 *                                 stored. Under some I/O methods we can
 *                                 determine whether data is still in the
 *                                 buffer, but not the number of bytes in the
 *                                 buffer. In this situation, this may be set
 *                                 to SIZE_NON_ZERO.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComOutbound(tPortHandle hPort, int *pnOutboundWaiting)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pnOutboundWaiting != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
         ASM    mov ah, 0x03
         ASM    mov dx, nPort
         ASM    int 20
         ASM    and ah, 0x40
         ASM    jz  still_sending
         *pnOutboundWaiting = 0;
         break;

still_sending:
         *pnOutboundWaiting = SIZE_NON_ZERO;
         break;
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         *pnOutboundWaiting = (int)nTXChars;
         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwErrors;
         COMSTAT ComStat;

         /* Use ClearCommError() to obtain device status. */
         if(!ClearCommError(pPortInfo->hCommDev, &dwErrors, &ComStat))
         {
            return(kODRCGeneralFailure);
         }

         /* Set pbIsInbound to TRUE if any bytes are in outbound queue. */
         *pnOutboundWaiting = (int)ComStat.cbOutQue;

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         /* Door32 doesn't currently support this functionality, so we */
         /* assume that all sent data is transmitted immediately.      */
         *pnOutboundWaiting = 0;
         return(kODRCUnsupported);
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
			*pnOutboundWaiting = 0;
			return(kODRCUnsupported);
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
	  case kComMethodStdIO:
			*pnOutboundWaiting = 0;
			return(kODRCUnsupported);
#endif	        

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComClearOutbound()
 *
 * Removes the current contents of the serial port outbound buffer.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComClearOutbound(tPortHandle hPort)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
         ASM    mov ah, 9
         ASM    mov dx, nPort
         ASM    int 20
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         ODComInternalResetTX();
         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
         if(!PurgeComm(pPortInfo->hCommDev, PURGE_TXCLEAR))
         {
            return(kODRCGeneralFailure);
         }
         break;
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         return(kODRCUnsupported);
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
			return(kODRCUnsupported);
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
			return(kODRCUnsupported);
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComClearInbound()
 *
 * Removes the current contents of the serial port inbound buffer.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComClearInbound(tPortHandle hPort)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
         ASM    mov ah, 10
         ASM    mov dx, nPort
         ASM    int 20
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         ODComInternalResetRX();
         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
         if(!PurgeComm(pPortInfo->hCommDev, PURGE_RXCLEAR))
         {
            return(kODRCGeneralFailure);
         }
         break;
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         return(kODRCUnsupported);
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
			return(kODRCUnsupported);
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
			return(kODRCUnsupported);
#endif
      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComInbound()
 *
 * Determines the number of bytes waiting in the serial port inbound buffer.
 *
 * Parameters: hPort            - Handle to a serial port object.
 *
 *             pnInboundWaiting - Location in which to store number of bytes
 *                                waiting in the inbound buffer. Under some
 *                                I/O methods (e.g. FOSSIL driver), we can
 *                                determine whether data is still in the
 *                                buffer, but not the number of bytes in the
 *                                buffer. In this situation, this may be set
 *                                to SIZE_NON_ZERO.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComInbound(tPortHandle hPort, int *pnInboundWaiting)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pnInboundWaiting != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
      {
         BOOL bDataInBuffer = FALSE;

         ASM    mov ah, 3
         ASM    mov dx, nPort
         ASM    push si
         ASM    push di
         ASM    int 20
         ASM    pop di
         ASM    pop si
         ASM    and ah, 1
         ASM    mov bDataInBuffer, ah

         *pnInboundWaiting = bDataInBuffer ? SIZE_NON_ZERO : 0;

         break;
      }
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         *pnInboundWaiting = (int)nRXChars;

         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwErrors;
         COMSTAT ComStat;

         /* Use ClearCommError() to obtain device status. */
         if(!ClearCommError(pPortInfo->hCommDev, &dwErrors, &ComStat))
         {
            return(kODRCGeneralFailure);
         }

         /* Set pbIsInbound to TRUE if there are any bytes in inbound queue. */
         *pnInboundWaiting = (int)ComStat.cbInQue;

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorGetAvailableEventHandle != NULL);
         if(WaitForSingleObject(
            (*pPortInfo->pfDoorGetAvailableEventHandle)(),
            0) == WAIT_OBJECT_0)
         {
            *pnInboundWaiting = SIZE_NON_ZERO;
         }
         else
         {
            *pnInboundWaiting = 0;
         }
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
			if(ioctlsocket(pPortInfo->socket,FIONREAD,pnInboundWaiting) != 0)
				*pnInboundWaiting = 0;
			break;
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
			if(ioctl(0,FIONREAD,pnInboundWaiting) == -1)
				*pnInboundWaiting = 0;
			break;
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComGetByte()
 *
 * Returns a single inbound byte. If there are characters waiting in the
 * inbound buffer, the next character is returned immediately. If bWait is TRUE
 * and no characters are waiting, this function will wait until a character is
 * received (possibly forever, if no characters are ever received).
 *
 * Parameters: hPort   - Handle to a serial port object.
 *
 *             pbtNext - Location to store retrieved byte.
 *
 *             bWait   - If TRUE, function will only return after a character
 *                       has been received. If FALSE, this function will return
 *                       kODRCNothingWaiting if no characters are waiting.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComGetByte(tPortHandle hPort, char *pbtNext, BOOL bWait)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pbtNext != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
      {
         BYTE btToReturn;
         int nInboundSize;

         /* If we should not wait for characters if inbound queue is empty. */
         if(!bWait)
         {
            /* Determine whether there are any inbound characterse waiting. */
            ODComInbound(hPort, &nInboundSize);

            /* If there are no inbound characters waiting, then return */
            /* without obtaining any characters.                       */
            if(nInboundSize == 0) return(kODRCNothingWaiting);
         }

         ASM     mov ah, 2
         ASM     mov dx, nPort
         ASM     push si
         ASM     push di
         ASM     int 20
         ASM     pop di
         ASM     pop si
         ASM     mov btToReturn, al

         *pbtNext = btToReturn;

         break;
      }
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         /* If we should not wait for characters if inbound queue is empty. */
         if(!bWait)
         {
            /* If there are no inbound characters waiting, then return */
            /* without obtaining any characters.                       */
            if(!nRXChars) return(kODRCNothingWaiting);
         }

         /* Loop, calling idle function, until next character arrives. */
         while(!nRXChars)
         {
            if(pPortInfo->pfIdleCallback != NULL)
            {
               (*pPortInfo->pfIdleCallback)();
            }
         }

         /* Disable interrupts. */
         ASM cli

         /* Get next character from receive queue. */
         *pbtNext = pbtRXQueue[nRXOutIndex++];

         /* Wrap queue index if needed. */
         if (nRXOutIndex == nRXQueueSize)
         {
            nRXOutIndex = 0;
         }

         /* Decrement count of total character in the receive queue. */
         nRXChars--;

         /* Re-enable interrupts. */
         ASM sti

         /* If receive buffer is below low water mark. */
         if(nRXChars <= nRXLowWaterMark)
         {
            /* If we are using flow control, then stop sender from */
            /* sending.                                            */
            if(btFlowControl & FLOW_RTSCTS)
            {
               /* If using RTS/CTS flow control, then raise RTS line. */
               ASM mov dx, nModemCtrlRegAddr
               ASM in al, dx
               ASM or al, RTS
               ASM out dx, al
            }
         }

         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwBytesRead;
         DWORD dwErrors;

         /* Ensure read timeout state is set appropriately for bWait value. */
         if(bWait)
         {
            ODComWin32SetReadTimeouts(pPortInfo, kBlocking);
         }
         else
         {
            ODComWin32SetReadTimeouts(pPortInfo, kNonBlocking);
         }

         /* Perform read operation. */
         if(!ReadFile(pPortInfo->hCommDev, pbtNext, 1, &dwBytesRead, NULL))
         {
            ClearCommError(pPortInfo->hCommDev, &dwErrors, NULL);
            return(kODRCGeneralFailure);
         }

         /* Determine whether or not a byte was read. */
         if(dwBytesRead == 0)
         {
            /* If no bytes where read, then this is a general error if bWait */
            /* is TRUE. If bWait is FALSE, then we should return             */
            /* waiting kODRCNothingWaiting.                                  */
            return(bWait ? kODRCGeneralFailure : kODRCNothingWaiting);
         }

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         if(WaitForSingleObject((*pPortInfo->pfDoorGetAvailableEventHandle)(),
            bWait ? INFINITE : 0) == WAIT_OBJECT_0)
         {
            (*pPortInfo->pfDoorRead)(pbtNext, 1);
            break;
         }

         return(bWait ? kODRCGeneralFailure : kODRCNothingWaiting);

         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			fd_set	socket_set;
			struct	timeval tv;
			int		select_ret, recv_ret;

			FD_ZERO(&socket_set);
			FD_SET(pPortInfo->socket,&socket_set);

			tv.tv_sec=0;
			tv.tv_usec=100;

			select_ret = select(pPortInfo->socket+1, &socket_set, NULL, NULL, bWait ? NULL : &tv);
			if (select_ret == SOCKET_ERROR)
				return (kODRCGeneralFailure);
			if (select_ret == 0)
				return (kODRCNothingWaiting);

			do {
				recv_ret = recv(pPortInfo->socket, pbtNext, 1, 0);
				if(recv_ret != SOCKET_ERROR)
					break;
				if(WSAGetLastError() != WSAEWOULDBLOCK)
					return (kODRCGeneralFailure);
				od_sleep(50);
			} while (bWait);

			if (recv_ret == 0)
				 return (kODRCNothingWaiting);

			break;
		}
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
		{
			fd_set	socket_set;
			struct	timeval tv;
			int		select_ret=-1;
			int		recv_ret;

			while(select_ret==-1) {
				FD_ZERO(&socket_set);
				FD_SET(STDIN_FILENO,&socket_set);

				tv.tv_sec=0;
				tv.tv_usec=100;

				select_ret = select(STDIN_FILENO+1, &socket_set, NULL, NULL, bWait ? NULL : &tv);
				if (select_ret == -1) {
					if(errno==EINTR)
						continue;
					return (kODRCGeneralFailure);
				}
				if (select_ret == 0)
					return (kODRCNothingWaiting);
			}

			recv_ret = read(STDIN_FILENO, pbtNext, 1);
			if(recv_ret == 1)
				break;
			return (kODRCGeneralFailure);

			break;
		}
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODComSendByte()
 *
 * Sends a single byte to the serial port outbound buffer.
 *
 * Parameters: hPort - Handle to a serial port object.
 *
 *             btToSend - The byte to transmit.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSendByte(tPortHandle hPort, BYTE btToSend)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
try_again:
         ASM    mov ah, 0x0b
         ASM    mov dx, nPort
         ASM    mov al, btToSend
         ASM    int 20
         ASM    cmp ax, 0
         ASM    jne keep_going

         /* Call idle function, if any. */
         if(pPortInfo->pfIdleCallback != NULL)
         {
            (*pPortInfo->pfIdleCallback)();
         }

         goto try_again;
keep_going:
         break;
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
         /* Loop, calling idle function, until characters are waiting in */
         /* the transmit buffer.                                         */
         while(!ODComInternalTXReady())
         {
            /* Call idle function, if any. */
            if(pPortInfo->pfIdleCallback != NULL)
            {
               (*pPortInfo->pfIdleCallback)();
            }
         }

         /* Disable interrupts. */
         ASM cli

         /* Place the character in the queue. */
         pbtTXQueue[nTXInIndex++] = btToSend;

         /* Wrap transmit queue index, if needed. */
         if (nTXInIndex == nTXQueueSize)
         {
            nTXInIndex = 0;
         }

         /* Increment count of total characters in the queue. */
         nTXChars++;

         /* Enable transmit interrupt on the UART. */
         ASM mov dx, nIntEnableRegAddr
         ASM in al, dx
         ASM or al, THRE
         ASM out dx, al

         ASM sti

         break;
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwErrors;
         DWORD dwBytesWritten;

         /* Attempt to perform write operation. */
         if(!WriteFile(pPortInfo->hCommDev, &btToSend, 1, &dwBytesWritten,
            NULL) || dwBytesWritten != 1)
         {
            ClearCommError(pPortInfo->hCommDev, &dwErrors, NULL);
            return(kODRCGeneralFailure);
         }
         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorWrite != NULL);
         if(!(*pPortInfo->pfDoorWrite)(&btToSend, 1))
         {
            return(kODRCGeneralFailure);
         }
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			fd_set	socket_set;
			struct	timeval tv;
			int		send_ret;

			FD_ZERO(&socket_set);
			FD_SET(pPortInfo->socket,&socket_set);

			tv.tv_sec=1;
			tv.tv_usec=0;

			if(select(pPortInfo->socket+1,NULL,&socket_set,NULL,&tv) != 1)
	         return(kODRCGeneralFailure);

			do {
				send_ret = send(pPortInfo->socket, &btToSend, 1, 0);
				if (send_ret != 1)
					od_sleep(50);
			} while ((send_ret == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK));

			if (send_ret == SOCKET_ERROR)
				return (kODRCGeneralFailure);

			break;
		}
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
	  case kComMethodStdIO:
	    {
		fd_set  fdset;
		struct  timeval tv;
		int             retval=-1;
		int	loopcount=0;

		while(retval==-1 && loopcount < 10) {
			FD_ZERO(&fdset);
			FD_SET(STDOUT_FILENO,&fdset);

			tv.tv_sec=1;
			tv.tv_usec=0;

			retval=select(STDOUT_FILENO+1,NULL,&fdset,NULL,&tv);
			if(retval!=1) {
				if(retval==0)  {
					retval=-1;
					loopcount++;
					continue;
				}
				if(retval==-1 && errno==EINTR)
					continue;
				return(kODRCGeneralFailure);
			}
		}

	    if(fwrite(&btToSend,1,1,stdout)!=1)
		   return(kODRCGeneralFailure);
		break;
		}
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComGetBuffer()
 *
 * Retreives received data into a buffer, filling the buffer with as much data
 * as possible that has been received, returning immediately.
 *
 * Parameters: hPort       - Handle to a serial port object.
 *
 *             pbtBuffer   - Pointer to a contiguous array of bytes.
 *
 *             nSize       - Size of buffer, in bytes. This is the maximum
 *                           number of characters that will be returned.
 *
 *             pnBytesRead - Pointer to an int where function will store the
 *                           number of bytes actually read.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComGetBuffer(tPortHandle hPort, BYTE *pbtBuffer, int nSize,
   int *pnBytesRead)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pbtBuffer != NULL);
   VERIFY_CALL(nSize > 0);
   VERIFY_CALL(pnBytesRead != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
      {
         int nReceived;

         ASM    push di
         ASM    mov cx, nSize
         ASM    mov dx, nPort


#ifdef LARGEDATA
         ASM    les di, pbtBuffer
#else
         ASM    mov ax, ds
         ASM    mov es, ax
         ASM    mov di, pbtBuffer
#endif

         ASM    mov ah, 0x18
         ASM    int 20
         ASM    pop di
         ASM    mov nReceived, ax

         *pnBytesRead = nReceived;

         break;
      }
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
      {
         int nTransferSize;
         int nFirstHalfSize;
         int nSecondHalfSize;
         char *pbtSource;

         /* Disable interrupts. */
         ASM cli

         /* Number of bytes to transfer is minimum of buffer size, and */
         /* number of bytes in receive queue.                          */
         nTransferSize = MIN(nRXChars, nSize);

         /* First half of transfer is minimum of number of bytes from here */
         /* to the end of the buffer, and the total transfer size.         */
         nFirstHalfSize = nRXQueueSize - nRXOutIndex;
         nFirstHalfSize = MIN(nFirstHalfSize, nTransferSize);

         /* Second half of transfer is remaining bytes, if any. */
         nSecondHalfSize = nTransferSize - nFirstHalfSize;

         /* Perform first half of transfer. */
         pbtSource = pbtRXQueue + nRXOutIndex;
         while(nFirstHalfSize--)
         {
            *pbtBuffer++ = *pbtSource++;
         }

         /* If there is a second half to transfer. */
         if(nSecondHalfSize)
         {
            /* Copy source will begin at beginning of queue. */
            pbtSource = pbtRXQueue;

            /* Set final queue out index. */
            nRXOutIndex = nSecondHalfSize;

            /* Perform second half of transfer. */
            while(nSecondHalfSize--)
            {
               *pbtBuffer++ = *pbtSource++;
            }
         }

         /* If entire transfer was performed in first half. */
         else
         {
            /* Set final queue out index. */
            nRXOutIndex += nTransferSize;

            /* Wrap queue out index, if needed. */
            if(nRXOutIndex == nRXQueueSize) nRXOutIndex = 0;
         }

         /* Subtract number of bytes retrieved from number of bytes in */
         /* receive queue.                                             */
         nRXChars -= nTransferSize;

         /* Return number of bytes copied into buffer. */
         *pnBytesRead = nTransferSize;

         /* Re-enable interrupts. */
         ASM sti

         break;
      }
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwBytesRead;
         DWORD dwErrors;

         /* Ensure read timeout state is set for non-blocking read */
         ODComWin32SetReadTimeouts(pPortInfo, kNonBlocking);

         /* Perform read operation. */
         if(!ReadFile(pPortInfo->hCommDev, pbtBuffer, nSize, &dwBytesRead,
            NULL))
         {
            ClearCommError(pPortInfo->hCommDev, &dwErrors, NULL);
            return(kODRCGeneralFailure);
         }

         /* Pass number of bytes read back to caller. */
         *pnBytesRead = (int)dwBytesRead;

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorRead != NULL);
         *pnBytesRead = (int)((*pPortInfo->pfDoorRead)(pbtBuffer, nSize));
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			fd_set	socket_set;
			struct	timeval tv;

			FD_ZERO(&socket_set);
			FD_SET(pPortInfo->socket,&socket_set);

			tv.tv_sec=0;
			tv.tv_usec=100;

			if(select(pPortInfo->socket+1,&socket_set,NULL,NULL,&tv) != 1) {
				*pnBytesRead = 0;
	         break;
			}

			*pnBytesRead = recv(pPortInfo->socket,pbtBuffer,nSize,0);
			break;
		}
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
	    {
		    for(*pnBytesRead=0;
				*pnBytesRead<nSize && (ODComGetByte(hPort, (pbtBuffer+*pnBytesRead), FALSE)==kODRCSuccess);
				*pnBytesRead++);
		}
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComSendBuffer()
 *
 * Sends the contents of an entire buffer to the serial port, waiting until
 * there is enough room in the serial port outbound buffer.
 *
 * Parameters: hPort     - Handle to a serial port object.
 *
 *             pbtBuffer - Pointer to the first byte in the buffer to transmit.
 *
 *             nSize     - Number of bytes to transmit from the buffer.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComSendBuffer(tPortHandle hPort, BYTE *pbtBuffer, int nSize)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);
   int nPort;

   VERIFY_CALL(pPortInfo != NULL);
   VERIFY_CALL(pbtBuffer != NULL);
   VERIFY_CALL(nSize >= 0);

   VERIFY_CALL(pPortInfo->bIsOpen);

   nPort = pPortInfo->btPort;

   /* If there are no characters to transmit, then there is no need to */
   /* proceed further.                                                 */
   if(nSize == 0)
   {
      return(kODRCSuccess);
   }

   switch(pPortInfo->Method)
   {
#ifdef INCLUDE_FOSSIL_COM
      case kComMethodFOSSIL:
      {
         int nCount;

try_again:
         ASM    push di
         ASM    mov cx, nSize
         ASM    mov dx, nPort


#ifdef LARGEDATA
         ASM    les di, pbtBuffer
#else
         ASM    mov ax, ds
         ASM    mov es, ax
         ASM    mov di, pbtBuffer
#endif

         ASM    mov ah, 0x19
         ASM    int 20
         ASM    pop di
         ASM    mov nCount, ax

         if(nCount<nSize)
         {
            /* Call idle function, if any. */
            if(pPortInfo->pfIdleCallback != NULL)
            {
               (*pPortInfo->pfIdleCallback)();
            }

            nSize-=nCount;
            pbtBuffer+=nCount;
            goto try_again;
         }
         break;
      }
#endif /* INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_UART_COM
      case kComMethodUART:
      {
         int nTransferSize;
         int nFirstHalfSize;
         int nSecondHalfSize;
         char *pbtDest;

         /* Loop, copying as much of buffer to transmit queue as possible, */
         /* then waiting for some characters to be transmitted, and copy   */
         /* more of buffer to transmit queue, until entire buffer has been */
         /* transferred.                                                   */
         for(;;)
         {
            /* Disable interrupts. */
            ASM cli

            /* Try to transfer all of buffer if possible. */
            nTransferSize = nSize;

            /* Adjust number of character to transfer down if there isn't */
            /* enough space in transmit queue.                            */
            if(nTransferSize > (nTXQueueSize - nTXChars))
            {
               nTransferSize = (nTXQueueSize - nTXChars);
            }

            /* Block transfer is divided into two segments - everything from */
            /* current in index to end of queue, and everything from         */
            /* beginning of queue to end of free space in queue.             */

            /* Calculate size of first half of transfer. */
            nFirstHalfSize = nTXQueueSize - nTXInIndex;
            if(nFirstHalfSize > nTransferSize) nFirstHalfSize = nTransferSize;

            /* Calculate size of second half of transfer. */
            nSecondHalfSize = nTransferSize - nFirstHalfSize;

            /* Transfer characters at current queue in index. */
            pbtDest = pbtTXQueue + nTXInIndex;
            while(nFirstHalfSize--)
            {
               *pbtDest++ = *pbtBuffer++;
            }

            /* If there is a second half to transfer. */
            if(nSecondHalfSize)
            {
               /* Copy destination will begin at beginning of queue. */
               pbtDest = pbtTXQueue;

               /* Set final queue in index. */
               nTXInIndex = nSecondHalfSize;

               /* Perform second half of transfer. */
               while(nSecondHalfSize--)
               {
                  *pbtDest++ = *pbtBuffer++;
               }
            }

            /* If entire transfer was performed in first half. */
            else
            {
               /* Set final queue in index. */
               nTXInIndex += nTransferSize;

               /* Wrap queue in index if we just happened to fill characters */
               /* up to end of physical queue. If there was one less         */
               /* character transferred, no wrap would be necessary, and if  */
               /* there was one more character to be transferred, transfer   */
               /* would have to be performed in two halves.                  */
               if(nTXInIndex == nTXQueueSize) nTXInIndex = 0;
            }

            /* Update count of total characters in the queue. */
            nTXChars += nTransferSize;

            /* Enable transmit interrupt on the UART. */
            ASM mov dx, nIntEnableRegAddr
            ASM in al, dx
            ASM or al, THRE
            ASM out dx, al

            /* Re-enable interrupts. */
            ASM sti

            /* Adjust count of characters left to transfer down by number of */
            /* characters transferred.                                       */
            nSize -= nTransferSize;

            /* If there are no characters left to transfer, then we are */
            /* done.                                                    */
            if(nSize == 0) break;

            /* Call idle function, if any. */
            if(pPortInfo->pfIdleCallback != NULL)
            {
               (*pPortInfo->pfIdleCallback)();
            }
         }
         break;
      }
#endif /* INCLUDE_UART_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwErrors;
         DWORD dwBytesWritten;

         /* Attempt to perform write operation. */
         if(!WriteFile(pPortInfo->hCommDev, pbtBuffer, nSize, &dwBytesWritten,
            NULL) || dwBytesWritten != (DWORD)nSize)
         {
            ClearCommError(pPortInfo->hCommDev, &dwErrors, NULL);
            return(kODRCGeneralFailure);
         }
         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         ASSERT(pPortInfo->pfDoorWrite != NULL);
         if(!(*pPortInfo->pfDoorWrite)(pbtBuffer, nSize))
         {
            return(kODRCGeneralFailure);
         }
         break;
         return(kODRCUnsupported);
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			fd_set	socket_set;
			struct	timeval tv;
			int     send_ret;

			FD_ZERO(&socket_set);
			FD_SET(pPortInfo->socket,&socket_set);

			tv.tv_sec=1;
			tv.tv_usec=0;

			if(select(pPortInfo->socket+1,NULL,&socket_set,NULL,&tv) != 1)
	         return(kODRCGeneralFailure);

			do {
				send_ret = send(pPortInfo->socket, pbtBuffer, nSize, 0);
				if (send_ret != SOCKET_ERROR)
					break;
				od_sleep(25);
			} while (WSAGetLastError() == WSAEWOULDBLOCK);

			if (send_ret != nSize)
				return (kODRCGeneralFailure);
      break;
		}
#endif /* INCLUDE_SOCKET_COM */

#ifdef INCLUDE_STDIO_COM
      case kComMethodStdIO:
	    {
			int pos=0;
			fd_set  fdset;
			struct  timeval tv;
			int     retval;
			int	loopcount=0;

			while(pos<nSize) {
				FD_ZERO(&fdset);
				FD_SET(STDOUT_FILENO,&fdset);

				tv.tv_sec=1;
				tv.tv_usec=0;

				retval=select(STDOUT_FILENO+1,NULL,&fdset,NULL,&tv);
				if(retval!=1) {
					if(retval==0) {
						if(++loopcount>10)
							return(kODRCGeneralFailure);
						continue;
					}
					if(retval==-1 && errno==EINTR)
						continue;
					return(kODRCGeneralFailure);
				}

				retval=fwrite(pbtBuffer+pos,1,nSize-pos,stdout);
				if(retval!=nSize-pos) {
					od_sleep(1);
				}

				pos+=retval;
			}
		    break;
		}
#endif

      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODComWaitEvent()
 *
 * Blocks until the specified serial I/O event occurs, or an error condition
 * is encountered.
 *
 * Parameters: hPort - Handle to an open port.
 *
 *             Event - Event type to wait for.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODComWaitEvent(tPortHandle hPort, tComEvent Event)
{
   tPortInfo *pPortInfo = ODHANDLE2PTR(hPort, tPortInfo);

   VERIFY_CALL(pPortInfo != NULL);

   VERIFY_CALL(pPortInfo->bIsOpen);

   switch(pPortInfo->Method)
   {
#if defined(INCLUDE_UART_COM) || defined(INCLUDE_FOSSIL_COM) || defined(INCLUDE_STDIO_COM)
      case kComMethodFOSSIL:
      case kComMethodUART:
	  case kComMethodStdIO:
         switch(Event)
         {
            case kNoCarrier:
            {
               BOOL bCarrier;
               for(;;)
               {
                  ODComCarrier(hPort, &bCarrier);
                  if(!bCarrier) break;

                  if(pPortInfo->pfIdleCallback != NULL)
                  {
                     (*pPortInfo->pfIdleCallback)();
                  }
               }
               break;
            }
            default:
               VERIFY_CALL(FALSE);
         }
         break;
#endif /* INCLUDE_UART_COM || INCLUDE_FOSSIL_COM */

#ifdef INCLUDE_WIN32_COM
      case kComMethodWin32:
      {
         DWORD dwEvtMask;

         /* Obtain current event mask. */
         if(!GetCommMask(pPortInfo->hCommDev, &dwEvtMask))
         {
            return(kODRCGeneralFailure);
         }

         /* Turn on event to be waited for. */
         switch(Event)
         {
            case kNoCarrier:
               dwEvtMask |= EV_RLSD;
               break;
            default:
               VERIFY_CALL(FALSE);
         }

         /* Write new event mask. */
         if(!SetCommMask(pPortInfo->hCommDev, dwEvtMask))
         {
            return(kODRCGeneralFailure);
         }

         /* Wait until event occurs. */
         for(;;)
         {
            /* Block until some event occurs. */
            if(!WaitCommEvent(pPortInfo->hCommDev, &dwEvtMask, NULL))
            {
               return(kODRCGeneralFailure);
            }

            /* Determine whether this is what we are waiting for. */
            switch(Event)
            {
               case kNoCarrier:
                  if(dwEvtMask | EV_RLSD)
                  {
                     BOOL bCarrier;
                     ODComCarrier(hPort, &bCarrier);
                     if(!bCarrier)
                     {
                        return(kODRCSuccess);
                     }
                  }
                  break;
            }

            /* If we get here, the event we are waiting for hasn't occurred */
            /* yet, so loop and block waiting for next event.               */
         }

         break;
      }
#endif /* INCLUDE_WIN32_COM */

#ifdef INCLUDE_DOOR32_COM
      case kComMethodDoor32:
         switch(Event)
         {
            case kNoCarrier:
               ASSERT(pPortInfo->pfDoorGetOfflineEventHandle != NULL);
               WaitForSingleObject(
                  (*pPortInfo->pfDoorGetOfflineEventHandle)(), INFINITE);
               break;
            default:
               VERIFY_CALL(FALSE);
         }
         break;
#endif /* INCLUDE_DOOR32_COM */

#ifdef INCLUDE_SOCKET_COM
      case kComMethodSocket:
		{
			if(Event == kNoCarrier)
			{
  			/* Wait for socket disconnect */
				fd_set	socket_set;
				char		ch;
				int recv_ret;

				while(1) 
				{

					FD_ZERO(&socket_set);
					FD_SET(pPortInfo->socket,&socket_set);
					if(select(pPortInfo->socket+1,&socket_set,NULL,NULL,NULL)
						==SOCKET_ERROR)
						break;
					recv_ret = recv(pPortInfo->socket, &ch, 1, MSG_PEEK);
					if(recv_ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
						continue;
					if (recv_ret != 1)
						break;
				}
			}
			else
			{
				VERIFY_CALL(FALSE);
			}
			break;
		}
#endif /* INCLUDE_SOCKET_COM */


      default:
         /* If we get here, then the current serial I/O method is not */
         /* handled by this function.                                 */
         ASSERT(FALSE);
   }

   /* Return with success. */
   return(kODRCSuccess);
}
