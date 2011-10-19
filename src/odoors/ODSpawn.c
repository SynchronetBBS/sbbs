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
 *        File: ODSpawn.c
 *
 * Description: Implements the od_spawn...() functions for suspending this
 *              program and executing a sub-program. Can be called by the
 *              user explicitly, or invoked for sysop OS shell.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Dec 09, 1994  6.00  BP   Use new directory access functions.
 *              Dec 13, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Jan 01, 1995  6.00  BP   _waitdrain -> ODWaitDrain()
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Nov 21, 1995  6.00  BP   Ported to Win32.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 23, 1996  6.00  BP   Finished port to Win32.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 23, 1996  6.00  BP   Enable and test under Win32.
 *              Feb 27, 1996  6.00  BP   Store screen info in our own struct.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support - some functions not supported (Yet)
 */

#define BUILDING_OPENDOORS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "OpenDoor.h"
#ifdef ODPLAT_NIX
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "ODCore.h"
#include "ODGen.h"
#include "ODCom.h"
#include "ODPlat.h"
#include "ODScrn.h"
#include "ODInQue.h"
#include "ODInEx.h"
#include "ODUtil.h"
#include "ODKrnl.h"
#include "ODSwap.h"

#ifdef ODPLAT_WIN32
#include "ODFrame.h"
#endif /* ODPLAT_WIN32 */

#if defined(ODPLAT_WIN32) && defined(_MSC_VER)
#undef P_WAIT
#undef P_NOWAIT
#include <process.h>
#endif /* ODPLAT_WIN32 && _MSC_VER */

#ifdef ODPLAT_DOS

/* Local and global variables for memory swapping spawn routines. */

int _swap = 0;                     /* if 0, do swap */
char *_swappath = NULL;            /* swap path */
int _useems = 0;                   /* if 0, use EMS */
int _required = 0;                 /* child memory requirement in K */
static long swapsize;              /* swap size requirement in bytes */
static int ems = 2;                /* if 0, EMS is available */
static int mapsize;                /* size of page map information */
static unsigned int tempno = 1;    /* tempfile number */
static char errtab[] =             /* error table */
{
   0,
   EINVAL,
   ENOENT,
   ENOENT,
   EMFILE,
   EACCES,
   EBADF,
   ENOMEM,
   ENOMEM,
   ENOMEM,
   E2BIG,
   ENOEXEC,
   EINVAL,
   EINVAL,
   -1,
   EXDEV,
   EACCES,
   EXDEV,
   ENOENT,
   -1
};

static VECTOR vectab1[]=
{
    0,    1,     0,  0,
    1,    1,     0,  0,
    2,    1,     0,  0,
    3,    1,     0,  0,
    0x1B, 1,     0,  0,
    0x23, 1,     0,  0,
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    2,     0,  0,            /* free record */
    0,    3,     0,  0          /* end record */
};

static VECTOR vectab2[(sizeof vectab1)/(sizeof vectab1[0])];

/* Location function prototypes. */
int _spawnvpe(int nModeFlag, char *pszPath, char *papszArgs[],
   char *papszEnviron[]);
int _spawnve(int nModeFlag, char *pszPath, char *papszArgs[],
   char * papszEnviron[]);
static void savevect(void);


#endif /* ODPLAT_DOS */

#ifdef ODPLAT_NIX
/* Location function prototypes. */
int _spawnvpe(int nModeFlag, char *pszPath, char *papszArgs[],
   char *papszEnviron[]);
#endif /* ODPLAT_NIX */

/* ----------------------------------------------------------------------------
 * od_spawn()
 *
 * Executes the specified command line, suspending OpenDoors operations while
 * the spawned-to program is running.
 *
 * Parameters: pszCommandLine - Command to execute along with any parameters.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_spawn(const char *pszCommandLine)
{
#ifdef ODPLAT_DOS
   char *apszArgs[4];
   INT16 nReturnCode;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_spawn()");

   *apszArgs=getenv("COMSPEC");

   apszArgs[1] = "/c";
   apszArgs[2] = pszCommandLine;
   apszArgs[3] = NULL;
   
   if(*apszArgs != NULL)
   {
      if((nReturnCode = od_spawnvpe(P_WAIT, *apszArgs, apszArgs, NULL)) != -1
         || errno != ENOENT)
      {
         return(nReturnCode != -1);
      }
   }

   *apszArgs = "command.com";

   return(od_spawnvpe(P_WAIT, *apszArgs, apszArgs, NULL) != -1);
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   char *pch;
   char *apszArgs[3];
   char szProgName[80];

   /* Build command and arguments list. */
   /* Build program name. */
   ODStringCopy(szProgName, pszCommandLine, sizeof(szProgName));
   pch = strchr(szProgName, ' ');
   if(pch != NULL) *pch = '\0';
   apszArgs[0] = szProgName;

   /* Build arguments. */
   pch = strchr(pszCommandLine, ' ');
   if(pch == NULL)
   {
      apszArgs[1] = NULL;
   }
   else
   {
      apszArgs[1] = pch + 1;
      apszArgs[2] = NULL;
   }

   /* Now, call od_spawnvpe(). */
   return(od_spawnvpe(P_WAIT, *apszArgs, apszArgs, NULL) != -1);
#endif /* ODPLAT_WIN32 */

#ifdef ODPLAT_NIX
   sigset_t		block;
   int retval;

   /* Suspend kernel */
   sigemptyset(&block);
   sigaddset(&block,SIGALRM);
   sigprocmask(SIG_BLOCK,&block,NULL);
   retval=system(pszCommandLine);

   /* Restore kernel */
   sigemptyset(&block);
   sigaddset(&block,SIGALRM);
   sigprocmask(SIG_UNBLOCK,&block,NULL);

   return(retval!=-1 && retval != 127);
#endif
}


/* ----------------------------------------------------------------------------
 * od_spawnvpe()
 *
 * Executes the specified program, using the specified arguments and
 * environment variables, optionally suspending OpenDoors operations while
 * the spawned-to program is running.
 *
 * Parameters: nModeFlag - P_WAIT to for OpenDoors operations to be suspended
 *                         while the spawned-to program is running, or
 *                         P_NOWAIT if the calling program should continue to
 *                         run while the spawned-to program is running. In
 *                         non-multitasking environments, the only valid value
 *                         of this parameters is P_WAIT.
 *
 *             pszPath   - Complete path and filename of the program to
 *                         exectute.
 *
 *             papszArg  - Array of string pointers to command line arguments.
 *
 *             papszEnv  - Array of string pointers to environment variables.
 *
 *     Return: -1 on failure or the spawned-to program's return value on
 *             success.
 */
ODAPIDEF INT16 ODCALL od_spawnvpe(INT16 nModeFlag, char *pszPath,
   char *papszArg[], char *papszEnv[])
{
   INT16 nToReturn;
   time_t nStartUnixTime;
   DWORD dwQuotient;
#ifdef ODPLAT_WIN32
   void *pWindow;
#endif /* ODPLAT_WIN32 */   
#ifdef ODPLAT_DOS
   char *pszDir;
   BYTE *abtScreenBuffer;
   INT nDrive;
   tODScrnTextInfo TextInfo;
#endif /* ODPLAT_DOS */

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_spawnvpe()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

#ifdef ODPLAT_DOS
   /* Ensure the nModeFlag is P_WAIT, which is the only valid value for */
   /* the MS-DOS version of OpenDoors.                                  */
   if(nModeFlag != P_WAIT)
   {
      od_control.od_error = ERR_PARAMETER;
      return(-1);
   }

   /* Store current screen contents. */
   if((abtScreenBuffer = malloc(4000)) == NULL)
   {
      od_control.od_error = ERR_MEMORY;
      return(-1);
   }
   if((pszDir = malloc(256)) == NULL)
   {
      od_control.od_error = ERR_MEMORY;
      free(abtScreenBuffer);
      return(-1);
   }

   /* Store current display settings. */
   ODScrnGetTextInfo(&TextInfo);

   /* Set current output area to the full screen. */
   ODScrnSetBoundary(1,1,80,25);

   /* Store contents of entire screen. */   
   ODScrnGetText(1, 1, 80, 25, (char *)abtScreenBuffer);

   /* Set the current display colour to grey on black. */
   ODScrnSetAttribute(0x07);

   /* Clear the screen if required. Otherwise, move the cursor to the */
   /* upper left corner of the screen.                                */
   if(od_control.od_clear_on_exit)
   {
      ODScrnClear();
   }
   else
   {
      ODScrnSetCursorPos(1, 1);
   }

   /* Store current directory. */
   strcpy(pszDir, "X:\\");
   pszDir[0] = 'A' + (nDrive = _getdrv());
   _getcd(0, (char *)pszDir + 3);
#endif /* ODPLAT_DOS */

   /* Remember when spawned to program was executed. */
   nStartUnixTime = time(NULL);

   if(nModeFlag == P_WAIT)
   {
      /* Display the spawn message box under Win32. */
#ifdef ODPLAT_WIN32
      pWindow = ODScrnShowMessage("Running sub-program...", 0);
#endif /* ODPLAT_WIN32 */

      /* Wait for up to ten seconds for outbound buffer to drain. */
      ODWaitDrain(10000);

#ifdef OD_MULTITHREADED
      /* Mutlithreaded versions of OpenDoors must shutdown the kernel */
      /* before closing the serial port.                              */
      ODKrnlShutdown();
#endif /* OD_MULTITHREADED */

      /* Close serial port. */
      if(od_control.baud != 0)
      {
#ifdef ODPLAT_WIN32
         /* Disable DTR response by the modem before closing the serial */
         /* port, if this is required.                                  */
         ODInExDisableDTR();
#endif /* ODPLAT_WIN32 */         
         ODComClose(hSerialPort);
      }
   }

   /* Execute specified program with the specified arguments. */
   nToReturn = _spawnvpe(nModeFlag, pszPath, papszArg, papszEnv);

   if(nModeFlag == P_WAIT)
   {
      /* Re-open serial port. */
      if(od_control.baud != 0)
      {
         ODComOpen(hSerialPort);
      }

#ifdef OD_MULTITHREADED
      /* Mutlithreaded versions of OpenDoors must shutdown the kernel    */
      /* before closing the serial port, so reinitialize the kernel now. */
      ODKrnlInitialize();
#endif /* OD_MULTITHREADED */

      if(!(bIsShell || od_control.od_spawn_freeze_time))
      {
        ODDWordDivide(&dwQuotient, NULL, time(NULL) - nStartUnixTime, 60L);
        od_control.user_timelimit -= (int)dwQuotient;
      }
      else
      {
        nNextTimeDeductTime += time(NULL) - nStartUnixTime;
      }

      /* Reset the time of the last input activity to the current time. */
      /* This will prevent an immediate inactity timeout, regardless of */
      /* how long the spawned-to program was active.                    */
      ODInQueueResetLastActivity(hODInputQueue);

      /* Clear inbound buffer. */
      od_clear_keybuffer();

      /* Remove the spawn message box under Win32. */
#ifdef ODPLAT_WIN32
      ODScrnRemoveMessage(pWindow);
#endif /* ODPLAT_WIN32 */
   }

#ifdef ODPLAT_DOS
   /* Redisplay the door screen. */
   ODScrnPutText(1, 1, 80, 25, (char *)abtScreenBuffer);

   /* Restore cursor to old position. */
   ODScrnSetBoundary(TextInfo.winleft, TextInfo.wintop,
      TextInfo.winright, TextInfo.winbottom);
   ODScrnSetAttribute(TextInfo.attribute);
   ODScrnSetCursorPos(TextInfo.curx, TextInfo.cury);

   _setdrvcd(nDrive, pszDir);

   /* Free allocated space. */
   free(abtScreenBuffer);
   free(pszDir);
#endif /* ODPLAT_DOS */

   /* Return appropriate value. */
   return(nToReturn);
}


#ifdef ODPLAT_DOS

/* ----------------------------------------------------------------------------
 * _spawnvpe()                                         *** PRIVATE FUNCTION ***
 *
 * Executes a child program in the MS-DOS environment, swapping the calling
 * program out of memory if enabled.
 *
 * Parameters: nModeFlag    - Must be P_WAIT.
 *
 *             pszPath      - Name of program to execute.
 *
 *             papszArgs    - Array of command-line arguments.
 *
 *             papszEnviron - Array of environment variables.
 *
 *     Return: -1 on failure or the spawned-to program's return value on
 *             success.
 */
int _spawnvpe(int nModeFlag, char *pszPath, char *papszArgs[],
   char *papszEnviron[])
{
   char *e;
   char *p;
   char buf[80];
   int nReturnCode;


   _swappath = (char *)(strlen(od_control.od_swapping_path) == 0 ? NULL 
      : (char *)od_control.od_swapping_path);
   _useems = od_control.od_swapping_noems;
   _swap = od_control.od_swapping_disable;

   if((nReturnCode=_spawnve(nModeFlag, pszPath, papszArgs, papszEnviron))!=-1
       || errno!=ENOENT || *pszPath=='\\' || *pszPath=='/'
       || *pszPath && *(pszPath+1)==':' || (e=getenv("PATH"))==NULL)
   {
      return(nReturnCode);
   }

   for (;;e++)
   {
      if((p=strchr(e,';'))!=NULL)
      {
     if(p-e > 66)
         {
        e=p;
        continue;
         }
      }
      else if(strlen(e)>66)
      {
     return( -1 );
      }

      p=buf;

      while(*e && *e!=';') *p++=*e++;

      if(p>buf)
      {
     if(*(p-1)!='\\' && *(p-1)!='/') *p++ = '\\';
     strcpy(p,pszPath);

     if((nReturnCode=_spawnve(nModeFlag,buf,papszArgs,papszEnviron))!=-1 || errno!=ENOENT)
         {
        return(nReturnCode);
         }
      }
      if(*e=='\0') return(-1);
   }
}


/* ----------------------------------------------------------------------------
 * addvect()                                           *** PRIVATE FUNCTION ***
 *
 * Adds a vector to the vector table.
 *
 * Parameters: number - The vector number.
 *
 *             opcode - Vector flags.
 *
 *     Return: -1 on failure, or 0 on success.
 */
int addvect(int number, int opcode)
{
    VECTOR *vect = vectab1;

    if ( number < 0 || number > 0xFF ||
    ( opcode != IRET && opcode != CURRENT ))
    {
    errno = EINVAL;
    return( -1 );
    }

    /* see if number is already in table */
    while ( vect->flag != 3 && ( vect->flag == 2 ||
    vect->number != ( char )number ))
    {
    vect++;
    }

    if ( vect->flag == 3 )
    {
    /* look for a free record */
    vect = vectab1;
    while ( vect->flag == CURRENT || vect->flag == IRET )
        vect++;
    }

    if ( vect->flag != 3 )
    {
    vect->number = ( char )number;
    vect->flag = ( char )opcode;
    if ( opcode == CURRENT )
    _getvect( number, &vect->vseg, &vect->voff );
    return( 0 );
    }

    errno = ENOMEM;
    return( -1 );
}


/* ----------------------------------------------------------------------------
 * savevect()                                          *** PRIVATE FUNCTION ***
 *
 * Saves current vector in vector table.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void savevect(void)
{
    VECTOR *vect1 = vectab1;
    VECTOR *vect2 = vectab2;

    while ( vect1->flag != 3 )
    {
    if ( vect1->flag != 2 )
    {
        vect2->number = vect1->number;
        vect2->flag = CURRENT;
        _getvect( vect1->number, &vect2->vseg, &vect2->voff );
    }
    else
        vect2->flag = 2;           /* free */
    vect1++;
    vect2++;
    }
    vect2->flag = 3;               /* end */
}


/* ----------------------------------------------------------------------------
 * testfile()                                          *** PRIVATE FUNCTION ***
 *
 * Tests swap file.
 *
 * Parameters: p      - Path.
 *
 *             file   - File name.
 *
 *             handle - File handle.
 *
 *     Return: 1 on failure.
 */
static int testfile(char *p, char *file, int *handle)
{
    unsigned int startno = tempno;
    int nDrive = ( *file | 32 ) - 96;   /* a = 1, b = 2, etc. */
    int root;
    unsigned int bytes;            /* bytes per cluster */
    unsigned int clusters;         /* free clusters */
    int need;                  /* clusters needed for swap file */
    int nReturnCode;                /* return code */
    unsigned long dwQuotient;
    unsigned long remainder;

    if ( file + 2 == p )
    {
    *p++ = '\\';
    if ( _getcd( nDrive, p ))       /* get current directory */
        return( 1 );           /* invalid drive */
    p = file + strlen( file );
    }
    else
    {
    *p = '\0';
    if ( ODFileAccessMode( file, 0 ))
        return( 1 );           /* path does not exist */
    }
    if ( *( p - 1 ) != '\\' && *( p - 1 ) != '/' )
    *p++ = '\\';
    if ( p - file == 3 )
    root = 1;              /* is root directory */
    else
    root = 0;              /* is not root directory */
    strcpy( p, "swp" );
    p += 3;

    if ( _dskspace( nDrive, &bytes, &clusters ) != 0 )
    return( 1 );               /* invalid drive */



    ODDWordDivide(&dwQuotient, &remainder, swapsize, bytes);
    need = (int)dwQuotient;

    if ( remainder )
    need++;
    if ( root == 0 )               /* if subdirectory */
    need++;                /* in case the directory needs space */
    if ( clusters < ( unsigned int )need )
    return( 1 );               /* insufficient free disk space */

    do
    {
again:  tempno = ( ++tempno ) ? tempno : 1;
    if ( tempno == startno )
        return( 1 );           /* extremely unlikely */
    ltoa(( long )tempno, p, 10 );
    }
    while ( !ODFileAccessMode( file, 0 ));

/*
 *  The return code from _create will equal 80 if the user is running DOS 3.0
 *  or above and the file was created by another program between the access
 *  call and the _create call.
 */

    if (( nReturnCode = _create( file, handle )) == 80 )
    goto again;
    return( nReturnCode );
}


/* ----------------------------------------------------------------------------
 * tempfile()                                          *** PRIVATE FUNCTION ***
 *
 * Creates a temporary swap file.
 *
 * Parameters: file   - Filename
 *
 *             handle - Handle to file.
 *
 *     Return: 0 on success, or 1 on failure.
 */
static int tempfile(char *file, int *handle)
{
    char *s = _swappath;
    char *p = file;

    if ( s )
    {
    for ( ;; s++ )
    {
        while ( *s && *s != ';' )
        *p++ = *s++;
        if ( p > file )
        {
        if ( p == file + 1 || file[ 1 ] != ':' )
        {
            memmove( file + 2, file, ( int )( p - file ));
            *file = ( char )( _getdrv() + 'a' );
            file[ 1 ] = ':';
            p += 2;
        }
        if ( testfile( p, file, handle ) == 0 )
            return( 0 );
        p = file;
        }
        if ( *s == '\0' )
        break;
    }
    }
    else                   /* try the current directory */
    {
    *p++ = ( char )( _getdrv() + 'a' );
    *p++ = ':';
    if ( testfile( p, file, handle ) == 0 )
        return( 0 );
    }

    errno = EACCES;
    return( 1 );
}


/* ----------------------------------------------------------------------------
 * cmdenv()                                            *** PRIVATE FUNCTION ***
 *
 * Constructs environment.
 *
 * Parameters: papszArgs     - Array of arguments.
 *
 *             papszEnviron  - Array of environment variables to add.
 *
 *             command       - The command specified.
 *
 *             env           - Pointer to environment.
 *
 *             memory        - Allocated memory.
 *
 *     Return: Environment length.
 */
static int cmdenv(char **papszArgs, char **papszEnviron, char *command,
   char **env, char **memory)
{
    char **vp;
    unsigned int elen = 0;         /* environment length */
    char *p;
    int cnt;
    int len;

    /* construct environment */

    if ( papszEnviron == NULL )
    {
       char far *parent_env;
       char far *env_ptr;
       int nul_count;

       ASM mov ah, 0x62
       ASM int 0x21
       ASM push es
       ASM mov es, bx
       ASM mov ax, es:[0x2c]
       ASM pop es
       ASM mov word ptr parent_env, 0
       ASM mov word ptr parent_env + 2, ax

       env_ptr = parent_env;
       nul_count = 0;
       while(nul_count < 2)
       {
      if(*env_ptr)
      {
         nul_count = 0;
      }
      else
      {
         ++nul_count;
      }

      ++env_ptr;
      ++elen;
       }

       if ( elen > 32766 )    /* 32K - 2 */
       {
      errno = E2BIG;
      return( -1 );
       }

       if (( p = malloc(elen + 15 )) == NULL )
       {
       errno = ENOMEM;
       return( -1 );
       }
       *memory = p;

       *( unsigned int * )&p = *( unsigned int * )&p + 15 & ~15;
       *env = p;

       len = elen;
       while(len--)
       {
      *p++ = *parent_env++;
       }
    }
    else
    {
       for ( vp = papszEnviron; *vp; vp++ )
       {
       elen += strlen( *vp ) + 1;
       if ( elen > 32766 )        /* 32K - 2 */
       {
           errno = E2BIG;
           return( -1 );
       }
       }

       if (( p = malloc( ++elen + 15 )) == NULL )
       {
       errno = ENOMEM;
       return( -1 );
       }
       *memory = p;

       *( unsigned int * )&p = *( unsigned int * )&p + 15 & ~15;
       *env = p;

       for ( vp = papszEnviron; *vp; vp++ )
       p = strchr( strcpy( p, *vp ), '\0' ) + 1;

       *p = '\0';                         /* final element */
    }


    /* construct command-line */

    vp = papszArgs;
    p = command + 1;
    cnt = 0;

    if (vp!=NULL &&  *vp )
    {
    while ( *++vp )
    {
        *p++ = ' ';
        cnt++;
        len = strlen( *vp );
        if ( cnt + len > 125 )
        {
        errno = E2BIG;
        free( *memory );
        return( -1 );
        }
        strcpy( p, *vp );
        p += len;
        cnt += len;
    }
    }

    *p = '\r';
    *command = ( char )cnt;

    return(( int )elen );          /* return environment length */
}


/* ----------------------------------------------------------------------------
 * doxspawn()                                          *** PRIVATE FUNCTION ***
 *
 * Performs spawn using memory swapping.
 *
 * Parameters: pszPath      - Path to command to exectute.
 *
 *             papszArg     - Array of arugments.
 *
 *             papszEnviron - Pointer to the environment.
 *
 *     Return: 0 on success, or -1 on failure.
 */
static int doxspawn(char *pszPath, char *papszArgs[], char *papszEnviron[])
{
    int nReturnCode = 0;        /* assume do xspawn */
    int doswap = 0;            /* assume do swap */
    int elen;                  /* environment length */
    char *memory;
    char *env;                 /* environment */
    char command[ 128 ];           /* command-line */
    long totalsize;            /* parent and free memory in bytes */
    int handle;
    int pages;
    char file[ 79 ];
    char *mapbuf = NULL;           /* buffer for map information */

    /* construct the command-line and the environment */
    if (( elen = cmdenv( papszArgs, papszEnviron, command, &env, &memory )) == -1 )
    return( -1 );

    if ( _swap == 0 )
    {
    if ( _useems == 0 )
    {
        if ( ems == 2 )
        ems = _chkems( "EMMXXXX0", &mapsize );
        if ( ems == 0 && ( mapbuf = malloc( mapsize )) == NULL )
        {
        errno = ENOMEM;
        free( memory );
        return( -1 );
        }
    }
    if (( nReturnCode = _xsize( _psp, &swapsize, &totalsize )) == 0 )
    {
        if ( _required == 0 || totalsize - swapsize - 272
           < (long)ODDWordShiftLeft(( long )_required , 10 ))
        {
        if ( ems == 0 && _useems == 0 )
        {
            pages = ( int )ODDWordShiftRight( swapsize , 14);
            if ((long)ODDWordShiftLeft(( long )pages , 14 ) < swapsize )
            pages++;
            if ( _savemap( mapbuf ) == 0 &&
            _getems( pages, &handle ) == 0 )
            *file = '\0';  /* use EMS */
            else if ( tempfile( file, &handle ) != 0 )
            nReturnCode = -1;       /* don't do xspawn */
        }
        else if ( tempfile( file, &handle ) != 0 )
            nReturnCode = -1;           /* don't do xspawn */
        }
        else
        doswap = 1;        /* don't do swap */
    }
    else
    {
        errno = errtab[ nReturnCode ];
        nReturnCode = -1;               /* don't do xspawn */
    }
    }
    else
    doswap = 1;            /* don't do swap */

    if ( nReturnCode == 0 )
    {
    savevect();            /* save current vectors */
    nReturnCode = _xspawn( pszPath, command, env, vectab1, doswap, elen, file,
        handle );
    _setvect( vectab2 );           /* restore saved vectors */
        if ( nReturnCode == 0 )
            nReturnCode = _getrc();         /* get child return code */
    else
    {
        errno = errtab[ nReturnCode ];
        nReturnCode = -1;
    }
    /*
     *  If EMS was used, restore the page-mapping state of the expanded
     *  memory hardware.
     */
    if ( doswap == 0 && *file == '\0' && _restmap( mapbuf ) != 0 )
    {
        errno = EACCES;
        nReturnCode = -1;
    }
    }

    if ( mapbuf )
    free( mapbuf );
    free( memory );
    return( nReturnCode );
}


/* ----------------------------------------------------------------------------
 * _spawnve()                                          *** PRIVATE FUNCTION ***
 *
 * Performs a spawn.
 *
 * Parameters: nModeFlag    - Must be P_WAIT
 *
 *             pszPath      - Command to execute.
 *
 *             papszArgs    - Command line arguments.
 *
 *             papszEnviron - Pointer to environment.
 *
 *     Return: void
 */
int _spawnve(int nModeFlag, char *pszPath, char *papszArgs[],
   char * papszEnviron[])
{
    char *p;
    char *s;
    int nReturnCode = -1;
    char buf[ 80 ];

    if ( nModeFlag != P_WAIT )
    {
    errno = EINVAL;
    return( -1 );
    }

    p = strrchr( pszPath, '\\' );
    s = strrchr( pszPath, '/' );
    if ( p == NULL && s == NULL )
    p = pszPath;
    else if ( p == NULL || s > p )
    p = s;

    if ( strchr( p, '.' ))
    {
    if ( !ODFileAccessMode( pszPath, 0 ))
        nReturnCode = doxspawn( pszPath, papszArgs, papszEnviron );
    /* If file not found, access will have set errno to ENOENT. */
    }
    else
    {
    strcpy( buf, pszPath );
    strcat( buf, ".com" );
    if ( !ODFileAccessMode( buf, 0 ))
        nReturnCode = doxspawn( buf, papszArgs, papszEnviron );
    else
    {
        strcpy( strrchr( buf, '.' ), ".exe" );
        if ( !ODFileAccessMode( buf, 0 ))
        nReturnCode = doxspawn( buf, papszArgs, papszEnviron );
        /* If file not found, access will have set errno to ENOENT. */
    }
    }

    return( nReturnCode );
}

#endif /* ODPLAT_DOS */

#ifdef ODPLAT_NIX
/* ----------------------------------------------------------------------------
 * _spawnvpe()                                         *** PRIVATE FUNCTION ***
 *
 * Executes a child program in the *nix environment.
 *
 * Parameters: nModeFlag    - Must be P_WAIT or P_NOWAIT
 *
 *             pszPath      - Name of program to execute.
 *
 *             papszArgs    - Array of command-line arguments.
 *
 *             papszEnviron - Array of environment variables.
 *
 *     Return: -1 on failure or the spawned-to program's return value on
 *             success.
 */
int _spawnvpe(int nModeFlag, char *pszPath, char *papszArgs[],
   char *papszEnviron[])
{
   pid_t	child;
   int		status;
   pid_t	wret;
   struct sigaction act;


   child=fork();

   if(nModeFlag == P_WAIT)  {
      /* require wait for child */
      act.sa_handler=SIG_IGN;
      sigemptyset(&(act.sa_mask));
      act.sa_flags=SA_NOCLDSTOP;
      sigaction(SIGCHLD,&act,NULL);
   }
   else  {
      /* Ignore SIGCHLD for backgrounded spawned processes */
      act.sa_handler=SIG_IGN;
      sigemptyset(&(act.sa_mask));
      act.sa_flags=SA_NOCLDSTOP|SA_NOCLDWAIT;
      sigaction(SIGCHLD,&act,NULL);
   }

   if(!child)  {
      /* Do the exec stuff here */
	  execve(pszPath,papszArgs,papszEnviron);
	  exit(-1); /* this should never happen! */
   }
   if(nModeFlag == P_WAIT)  {
      wret=waitpid(child,&status,0);
	  if(WIFEXITED(status))  {
	     return(WEXITSTATUS(status));
	  }
	  return(-1);
   }
   return(0);
}
#endif /* ODPLAT_NIX */
