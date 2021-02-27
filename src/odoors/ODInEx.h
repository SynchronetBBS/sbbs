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
 *        File: ODInEx.h
 *
 * Description: OpenDoors initialization and shutdown operations
 *              (od_init() and od_exit()), including drop file I/O.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 22, 1995  6.00  BP   Created.
 *              Nov 23, 1995  6.00  BP   32-bit portability.
 *              Dec 03, 1995  6.00  BP   Win32 port.
 *              Jan 19, 1996  6.00  BP   Don't use atexit() under Win32.
 *              Jan 19, 1996  6.00  BP   Make ODInitError() a shared function.
 *              Jan 20, 1996  6.00  BP   Prompt for user name if force_local.
 *              Feb 02, 1996  6.00  BP   Added RA 2.50 EXITINFO.BBS support.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 20, 1996  6.00  BP   Added bParsedCmdLine.
 *              Feb 21, 1996  6.00  BP   Don't override command line options.
 *              Feb 21, 1996  6.00  BP   Force single-byte structure alignment.
 *              Feb 23, 1996  6.00  BP   Make DTR disable code shared.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODINEX
#define _INC_ODINEX

#include "ODPlat.h"

/* Drop file structures. */

/* Force byte alignment, if possible */
#ifdef __TURBOC__
#if(__TURBOC__ >= 0x295)
#pragma option -a-
#endif /* __TURBOC__ >= 0x295 */
#endif /* __TURBOC__ */
#ifdef _MSC_VER
#pragma pack(1)
#endif /* _MSC_VER */

typedef struct
{
   WORD baud;
   DWORD num_calls;
   char last_caller[36];
   char sLastHandle[36];               /* New to RA 2.50 */
   char extra1[92];
   char start_date[9];
   WORD busyperhour[24];
   WORD busyperday[7];
   char name[36];
   char location[26];
   char organisation[51];
   char address[3][51];
   char handle[36];
   char comment[81];
   DWORD password_crc;
   char dataphone[16];
   char homephone[16];
   char lasttime[6];
   char lastdate[9];
   BYTE attrib;
   BYTE attrib2;
   char flags[4];
   DWORD credit;
   DWORD pending;
   WORD posted;
   WORD sec;
   DWORD lastread;
   DWORD nocalls;
   DWORD ups;
   DWORD downs;
   DWORD upk;
   DWORD downk;
   DWORD todayk;
   INT16 elapsed;
   WORD screenlen;
   char lastpwdchange;
   WORD group;
   WORD combinedrecord[200];
   char firstcall[9];
   char birthday[9];
   char subdate[9];
   BYTE screenwidth;
   BYTE language;
   BYTE dateformat;
   char forwardto[36];
   WORD msgarea;
   WORD filearea;
   BYTE default_protocol;
   WORD file_group;
   BYTE last_dob_check;
   BYTE sex;
   DWORD xirecord;
   WORD msg_group;
   BYTE btAttribute3;                  /* New to RA 2.50. */
   char sPassword[16];                 /* New to RA 2.50. */
   BYTE extra2[31];
   char status;
   char starttime[6];
   char errorlevel;
   char days;
   char forced;
   char lasttimerun[9];
   char netmailentered;
   char echomailentered;
   char logintime[6];
   char logindate[9];
   INT16 timelimit;
   DWORD loginsec;
   WORD userrecord;
   WORD readthru;
   WORD numberpages;
   WORD downloadlimit;
   char timeofcreation[6];
   DWORD logonpasswordcrc;
   BYTE wantchat;
   INT16 deducted_time;
   char menustack[50][9];
   BYTE menustackpointer;
   char extra3[200];
   BYTE error_free;
   BYTE sysop_next;
   char emsi_session;
   char emsi_crtdef[41];
   char emsi_protocols[41];
   char emsi_capabilities[41];
   char emsi_requests[41];
   char emsi_software[41];
   BYTE hold_attr1;
   BYTE hold_attr2;
   BYTE hold_len;
   char page_reason[81];
   BYTE status_line;
   char last_cost_menu[9];
   WORD menu_cost_per_min;
   BYTE has_avatar;
   BYTE has_rip;
   BYTE btRIPVersion;                  /* New to RA 2.50. */
   BYTE btExtraSpace[85];
} tRA2ExitInfoRecord;


typedef struct
{
   WORD baud;
   DWORD num_calls;
   char last_caller[36];
   char extraspace[128];
   char start_date[9];
   WORD busyperhour[24];
   WORD busyperday[7];
   char uname[36];
   char uloc[26];
   char password[16];
   char dataphone[13];
   char homephone[13];
   char lasttime[6];
   char lastdate[9];
   BYTE attrib;
   BYTE flags[4];
   WORD credit;
   WORD pending;
   WORD posted;
   WORD lastread;
   WORD sec;
   WORD nocalls;
   WORD ups;
   WORD downs;
   WORD upk;
   WORD downk;
   WORD todayk;
   WORD elapsed;
   WORD screenlen;
   BYTE lastpwdchange;
   BYTE attrib2;
   BYTE group;
   WORD xirecord;
   char extra2[3];
   char status;
   char starttime[6];
   char errorlevel;
   char days;
   char forced;
   char lasttimerun[9];
   char netmailentered;
   char echomailentered;
   char logintime[6];
   char logindate[9];
   WORD timelimit;
   DWORD loginsec;
   DWORD net_credit;
   WORD userrecord;
   WORD readthru;
   WORD numberpages;
   WORD downloadlimint;
   union
   {
      struct
      {
         char timeofcreation[6];
         char logonpassword[16];
         char wantchat;
      } ra;
      struct
      {
         char qwantchat;
         char gosublevel;
         char menustack[20][9];
         char menu[9];
         BYTE screenclear;
         BYTE moreprompts;
         BYTE graphicsmode;
         BYTE externedit;
         INT16 screenlength;
         BYTE mnpconnect;
         char chatreason[49];
         BYTE externlogoff;
         BYTE ansicapable;
         BYTE ripactive;
         BYTE extraspace[199];
      } qbbs;
   } bbs;
} tExitInfoRecord;

typedef struct
{
   INT16 deducted_time;
   char menustack[50][9];
   char menustackpointer;
   char userhandle[36];
   char comment[81];
   char firstcall[9];
   char combinedrecord[25];
   char birthday[9];
   char subdate[9];
   BYTE screenwidth;
   BYTE msgarea;
   BYTE filearea;
   BYTE language;
   BYTE dateformat;
   char forwardto[36];
   char extra_space[43];
   char error_free;
   char sysop_next;
   char emsi_session;
   char emsi_crtdef[41];
   char emsi_protocols[41];
   char emsi_capabilities[41];
   char emsi_requests[41];
   char emsi_software[41];
   char hold_attr1;
   char hold_attr2;
   char hold_len;
   char extr_space[100];
} tExtendedExitInfo;

struct _pcbsys
{
   char display[2];       /* "-1" = On, " 0" = Off */
   char printer[2];
   char pagebell[2];
   char calleralarm[2];
   char sysopflag;        /* ' ', 'N'=sysop next, 'X'=exit to dos */
   char errorcorrection[2];
   char graphicsmode;     /* 'Y'=Yes, 'N'=No, '7'=7E1 */
   char nodechat;         /* 'A'=available, 'U'=unavailable */
   char dteportspeed[5];
   char connectspeed[5];  /* "Local"=local mode */
   WORD recordnum;
   char firstname[15];
   char password[15];
   INT16 logontimeval;    /* minutes since midnight */
   INT16 todayused;       /* -ve # of minutes */
   char logontime[5];
   INT16 timeallowed;
   WORD kallowed;
   char conference;
   char joined[5];
   char scanned[5];
   INT16 conferenceaddtime;
   INT16 creditminutes;
   char languageext[4];
   char fullname[25];
   INT16 minutesremaining;
   char node;             /* ' ' if no network */
   char eventtime[5];
   char eventactive[2];
   char slideevent[2];
   DWORD memmessage;
   char comport;          /* 0=none, 1-8 */
   char reserved1[2];
   char useansi;          /* 1 = yes, 0 = no */
   char lasteventdate[8];
   WORD lasteventminute;
   char dosexit;
   char eventupcoming;
   char stopuploads;
   WORD conferencearea;
};

struct _userssyshdr
{
   WORD     Version;           /* PCBoard version number (i.e. 145) */
   DWORD    RecNo;             /* Record number from USER's file */
   WORD     SizeOfRec;         /* Size of "fixed" user record */
   WORD     NumOfAreas;        /* Number of conference areas (Main=1) */
   WORD     NumOfBitFields;    /* Number of Bit Map fields for conferences */
   WORD     SizeOfBitFields;   /* Size of each Bit Map field */
   char     AppName[15];       /* Name of the Third Party Application */
   WORD     AppVersion;        /* Version number for the application */
   WORD     AppSizeOfRec;      /* Size of a "fixed length" record (if any) */
   WORD     AppSizeOfConfRec;  /* Size of each conference record (if any) */
   DWORD    AppRecOffset;      /* Offset of AppRec into USERS.INF record */
   char     Updated;           /* TRUE if USERS.SYS has been updated */
};

struct _pcbflags
{
   int Dirty    :1;            /* Dirty Flag (meaning file has been updated) */
   int MsgClear :1;            /* User's choice for screen clear after messages */
   int HasMail  :1;            /* Indicates if NEW mail has been left for user */
   int Reserved :5;
};

struct _pcbdate
{
   int Day   :5;               /* 5 bit integer representing the Day */
   int Month :4;               /* 4 bit integer representing the Month */
   int Year  :7;               /* 7 bit integer representing the Year MINUS 80 */
};

struct _userssysrec
{
   char     Name[26];          /* Name (NULL terminated) */
   char     City[25];          /* City (NULL terminated) */
   char     Password[13];      /* Password (NULL terminated) */
   char     BusDataPhone[14];  /* Business or Data Phone (NULL terminated) */
   char     HomeVoicePhone[14];/* Home or Voice Phone (NULL terminated) */
   WORD     LastDateOn;        /* Julian date for the Last Date On */
   char     LastTimeOn[6];     /* Last Time On (NULL Terminated) */
   char     ExpertMode;        /* 1=Expert, 0=Novice */
   char     Protocol;          /* Protocol (A thru Z) */
   struct _pcbflags PackedFlags;
   struct _pcbdate DateLastDirRead;
   INT16    SecurityLevel;     /* Security Level */
   WORD     NumTimesOn;        /* Number of times the caller has connected */
   char     PageLen;           /* Page Length when display data on the screen */
   WORD     NumUploads;        /* Total number of FILES uploaded */
   WORD NumDownloads;      /* Total number of FILES downloaded */
   DWORD    DailyDnldBytes;    /* Number of BYTES downloaded so far today */
   char     UserComment[31];   /* Comment field #1 (NULL terminated) */
   char     SysopComment[31];  /* Comment field #1 (NULL terminated) */
   INT16    ElapsedTimeOn;     /* Number of minutes online */
   WORD     RegExpDate;        /* Julian date for Registration Expiration Date */
   INT16    ExpSecurityLevel;  /* Expired Security Level */
   WORD     LastConference;    /* Number of the conference the caller was in */
   DWORD    TotDnldBytes;      /* Total number of BYTES downloaded */
   DWORD    TotUpldBytes;      /* Total number of BYTES uploaded */
   char     DeleteFlag;        /* 1=delete this record, 0=keep */
   DWORD    RecNum;            /* Record Number in USERS.INF file */
   char     Reserved[9];       /* Bytes 391-399 from the USERS file */
   DWORD    MsgsRead;          /* Number of messages the user has read in PCB */
   DWORD    MsgsLeft;          /* Number of messages the user has left in PCB */
};

/* Restore original structure alignment, if possible. */
#ifdef _MSC_VER
#pragma pack()
#endif /* _MSC_VER */


/* od_init() and od_exit() global helper functons. */
#ifndef ODPLAT_WIN32
void ODAtExitCallback(void);
#endif /* !ODPLAT_WIN32 */
INT ODWriteExitInfoPrimitive(FILE *pfDropFile, INT nCount);
BOOL ODReadExitInfoPrimitive(FILE *pfDropFile, INT nCount);
INT ODSearchForDropFile(char **papszFileNames, INT nNumFileNames,
   char *pszFound, char *pszDirectory);
void ODInitError(char *pszErrorText);
#ifdef ODPLAT_WIN32
BOOL CALLBACK ODInitLoginDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
void ODInExDisableDTR(void);
#endif /* ODPLAT_WIN32 */


/* Global variables. */
extern WORD wODNodeNumber;
extern BOOL bIsCoSysop;
extern BOOL bIsSysop;
extern char *apszDropFileInfo[25];
extern BYTE btExitReason;
extern DWORD dwForcedBPS;
extern INT nForcedPort;
extern DWORD dwFileBPS;
extern char szDropFilePath[120];
extern char szExitinfoBBSPath[120];
extern INT16 nInitialElapsed;
extern char *szOriginalDir;
extern BYTE btDoorSYSLock;
extern time_t nStartupUnixTime;
extern INT16 nInitialRemaining;
extern BOOL bSysopNameSet;
extern char szForcedSysopName[40];
extern BOOL bSystemNameSet;
extern char szForcedSystemName[40];
extern BOOL bUserFull;
extern BOOL bCalledFromConfig;
extern tRA2ExitInfoRecord *pRA2ExitInfoRecord;
extern tExitInfoRecord *pExitInfoRecord;
extern tExtendedExitInfo *pExtendedExitInfo;
extern struct _pcbsys *pPCBoardSysRecord;
extern struct _userssyshdr *pUserSysHeader;
extern struct _userssysrec *pUserSysRecord;
extern BOOL bPreOrExit;
extern BOOL bRAStatus;
extern BOOL bPromptForUserName;
extern BOOL bParsedCmdLine;
extern WORD wPreSetInfo;
#ifdef ODPLAT_WIN32
extern tODThreadHandle hFrameThread;
#endif /* ODPLAT_WIN32 */


/* wPreSetInfo flags. */
#define PRESET_BPS         0x0001
#define PRESET_PORT        0x0002
#define PRESET_REQUIRED    (PRESET_BPS | PRESET_PORT)


#endif /* _INC_ODINEX */
