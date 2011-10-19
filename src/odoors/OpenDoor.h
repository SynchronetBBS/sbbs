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
 *        File: OpenDoor.h
 *
 * Description: C/C++ definition of the OpenDoors API. Any program source file
 *              that uses OpenDoors must #include this file.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Dec 02, 1995  6.00  BP   New file header format.
 *              Dec 09, 1995  6.00  BP   Added od_multiline_edit() prototype.
 *              Dec 12, 1995  6.00  BP   Cleaned up, added DLL definitions.
 *              Dec 12, 1995  6.00  BP   Moved ODPLAT_??? to OpenDoor.h.
 *              Dec 21, 1995  6.00  BP   Add ability to use already open port.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Dec 23, 1995  6.00  BP   Added EDIT_FLAG_SHOW_SIZE.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 01, 1996  6.00  BP   BCC32 compatibility changes.
 *              Jan 01, 1996  6.00  BP   Added new mulitline editor options.
 *              Jan 01, 1996  6.00  BP   Added od_disable_dtr, DIS_DTR_DISABLE.
 *              Jan 03, 1996  6.00  BP   Further BCC32 compatiblity changes.
 *              Jan 04, 1996  6.00  BP   Added od_get_input() and related defs.
 *              Jan 07, 1996  6.00  BP   Added OD_GLOBAL_CONV.
 *              Jan 19, 1996  6.00  BP   Removed some unused stuff.
 *              Jan 19, 1996  6.00  BP   Added od_internal_debug.
 *              Jan 23, 1996  6.00  BP   Added od_exiting and ERR_UNSUPPORTED.
 *              Jan 30, 1996  6.00  BP   New extern "C" decl for od_control.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 31, 1996  6.00  BP   Added DIS_NAME_PROMPT.
 *              Jan 31, 1996  6.00  BP   Added tODMilliSec, OD_NO_TIMEOUT.
 *              Jan 31, 1996  6.00  BP   Added timeout for od_get_input().
 *              Feb 02, 1996  6.00  BP   Add RA 2.50-related od_control vars.
 *              Feb 03, 1996  6.00  BP   Added more editor options.
 *              Feb 06, 1996  6.00  BP   Added od_silent_mode.
 *              Feb 08, 1996  6.00  BP   Added editor buffer grow option.
 *              Feb 13, 1996  6.00  BP   Added od_get_input() flags parameter.
 *              Feb 14, 1996  6.00  BP   Recognize Borland's __WIN32__ define.
 *              Feb 17, 1996  6.00  BP   Added OD_KEY_F1 thru OD_KEY_F10.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 27, 1996  6.00  BP   Added od_max_key_latency.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 03, 1996  6.10  BP   Fixed OD_COMPONENT for medium mem mod.
 *              Mar 06, 1996  6.10  BP   Added TRIBBS.SYS support.
 *              Mar 06, 1996  6.10  BP   Added COM_DOOR32.
 *              Mar 11, 1996  6.10  BP   Added OD_VERSION.
 *              Mar 13, 1996  6.10  BP   Added od_get_cursor().
 *              Mar 13, 1996  6.10  BP   Added od_local_win_col.
 *              Mar 14, 1996  6.10  BP   Added od_config_callback.
 *              Mar 21, 1996  6.10  BP   Added od_control_get().
 *              Apr 08, 1996  6.10  BP   Added command-line parsing callbacks.
 *              Oct 19, 2001  6.20  RS   Added door32.sys and socket support.
 *              Oct 19, 2001  6.21  RS   Fixed socket disconnect bug.
 */

/* Only parse OpenDoor.h once. */
#ifndef _INC_OPENDOOR
#define _INC_OPENDOOR


/* ========================================================================= */
/* Version and platform definitions.                                         */
/* ========================================================================= */

/* OpenDoors API version number. */
#define OD_VERSION 0x624

#define DIRSEP		'\\'
#define DIRSEP_STR	"\\"

/* OpenDoors target platform. */
#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#define ODPLAT_WIN32
#undef ODPLAT_DOS
#ifdef OD_WIN32_STATIC
#pragma message("Compiling for Win32 static version of OpenDoors")
#else /* !OD_WIN32_STATIC */
#pragma message("Compiling for Win32 DLL version of OpenDoors")
#define OD_DLL
#endif /* !OD_WIN32_STATIC */
#else /* !WIN32 */
#if defined(__unix__) || defined(__NetBSD__) || defined(__APPLE__)
#define ODPLAT_NIX
#undef ODPLAT_DOS
#undef DIRSEP
#define DIRSEP '/'
#undef DIRSEP_STR
#define DIRSEP_STR "/"
#else
#define ODPLAT_DOS
#undef ODPLAT_WIN32
#pragma message("Compiling for DOS version of OpenDoors")
#endif /* !NIX */
#endif /* !WIN32 */


/* Include any other headers required by OpenDoor.h. */
#ifdef ODPLAT_WIN32 
#include "windows.h"
#endif /* ODPLAT_WIN32 */

/* For DLL versions, definitions of function or data that is exported from */
/* a module or imported into a module.                                     */
#ifdef OD_DLL
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define OD_EXPORT __declspec(dllexport)
#else /* !_MSC_VER || __BORLANDC__ */
#define OD_EXPORT _export
#endif /* !_MSC_VER */
#define OD_IMPORT DECLSPEC_IMPORT
#else /* !OD_DLL */
#define OD_EXPORT
#define OD_IMPORT
#endif /* !OD_DLL */

/* Definition of function naming convention used by OpenDoors. */
#ifdef __cplusplus
#define OD_NAMING_CONVENTION extern "C"
#else /* !__cplusplus */
#define OD_NAMING_CONVENTION
#endif /* !__cplusplus */

/* Definition of function calling convention used by OpenDoors. */
#ifdef ODPLAT_WIN32
#define ODCALL WINAPI
#define ODVCALL WINAPIV
#define OD_GLOBAL_CONV WINAPI
#else /* !ODPLAT_WIN32 */
#define ODCALL
#define ODVCALL
#define OD_GLOBAL_CONV
#endif /* !ODPLAT_WIN32 */

/* OpenDoors API function declaration type. */
#ifdef BUILDING_OPENDOORS
#define ODAPIDEF OD_NAMING_CONVENTION OD_EXPORT
#else /* !BUILDING_OPENDOORS */
#define ODAPIDEF OD_NAMING_CONVENTION OD_IMPORT
#endif /* !BUILDING_OPENDOORS */

/* OpenDoors API global variable definition and declaration types. */
#define OD_API_VAR_DEFN OD_NAMING_CONVENTION OD_EXPORT

#ifdef BUILDING_OPENDOORS
#define OD_API_VAR_DECL extern OD_EXPORT
#else /* !BUILDING_OPENDOORS */
#define OD_API_VAR_DECL extern OD_IMPORT
#endif /* !BUILDING_OPENDOORS */

/* Explicitly far pointers. */
#ifdef ODPLAT_DOS
#define ODFAR far
#else /* !ODPLAT_DOS */
#define ODFAR
#endif /* !ODPLAT_DOS */


/* ========================================================================= */
/* Primitive data types.                                                     */
/* ========================================================================= */

/* Portable types that are the same size across all platforms */
#ifndef ODPLAT_WIN32
#ifndef BYTE
typedef unsigned char      BYTE;                        /* Unsigned, 8 bits. */
#endif
#ifndef WORD
typedef unsigned short     WORD;                       /* Unsigned, 16 bits. */
#endif
#ifndef DWORD
typedef unsigned long      DWORD;                      /* Unsigned, 32 bits. */
#endif
#ifndef CHAR
typedef char               CHAR;         /* Native character representation. */
#endif
#define DWORD_DEFINED
#define WORD_DEFINED
#endif /* !ODPLAT_WIN32 */

typedef signed char        INT8;                          /* Signed, 8 bits. */
typedef signed short int   INT16;                        /* Signed, 16 bits. */
#ifndef ODPLAT_WIN32	/* avoid type redefinition from basetsd.h */
typedef signed long int    INT32;                        /* Signed, 32 bits. */
#endif


/* Types that vary in size depending on platform. These are guranteed to be */
/* at least the given size, but may be larger if this platform can          */
/* represent a larger value more efficiently (or as efficiently).           */
#ifndef ODPLAT_WIN32
typedef int                INT;                /* Integer, at least 16 bits. */
typedef unsigned int       UINT;      /* Unsigned integer, at least 16 bits. */
#ifndef BOOL
typedef char               BOOL;           /* Boolean value, at least 1 bit. */
#endif /* !BOOL */
#endif /* !ODPLAT_WIN32 */

/* TRUE and FALSE manifest constants, for use with BOOL data type. */
#ifndef FALSE
#define FALSE 0
#endif /* !FALSE */
#ifndef TRUE
#define TRUE 1
#endif /* !TRUE */


/* ========================================================================= */
/* OpenDoors complex data types and defines.                                 */
/* ========================================================================= */

/* Millisecond time type. */
typedef DWORD tODMilliSec;

/* Special value tODMilliSec value for no timeouts. */
#ifdef ODPLAT_WIN32
#define OD_NO_TIMEOUT INFINITE
#else /* !ODPLAT_WIN32 */
#define OD_NO_TIMEOUT 0xffffffffL
#endif /* !ODPLAT_WIN32 */


/* Multi-line editor defintions. */

/* Editor text formats. */
typedef enum
{
    FORMAT_PARAGRAPH_BREAKS
   ,FORMAT_LINE_BREAKS
   ,FORMAT_FTSC_MESSAGE
   ,FORMAT_NO_WORDWRAP
} tODEditTextFormat;

/* Menu callback function return values. */
typedef enum
{
    EDIT_MENU_DO_NOTHING
   ,EDIT_MENU_EXIT_EDITOR
} tODEditMenuResult;

/* Editor flags. */
#define EFLAG_NORMAL       0x00000000

/* Optional multi-line editor settings. */
typedef struct
{
   INT nAreaLeft;
   INT nAreaTop;
   INT nAreaRight;
   INT nAreaBottom;
   tODEditTextFormat TextFormat;
   tODEditMenuResult (*pfMenuCallback)(void *pUnused);
   void * (*pfBufferRealloc)(void *pOriginalBuffer, UINT unNewSize);
   DWORD dwEditFlags;
   char *pszFinalBuffer;
   UINT unFinalBufferSize;
} tODEditOptions;

/* Editor return values. */
#define OD_MULTIEDIT_ERROR          0
#define OD_MULTIEDIT_SUCCESS        1


/* Input event information. */

/* Input event types. */
typedef enum
{
    EVENT_CHARACTER
   ,EVENT_EXTENDED_KEY
} tODInputEventType;

/* Extended key codes. */
#define OD_KEY_F1          0x3b
#define OD_KEY_F2          0x3c
#define OD_KEY_F3          0x3d
#define OD_KEY_F4          0x3e
#define OD_KEY_F5          0x3f
#define OD_KEY_F6          0x40
#define OD_KEY_F7          0x41
#define OD_KEY_F8          0x42
#define OD_KEY_F9          0x43
#define OD_KEY_F10         0x44
#define OD_KEY_UP          0x48
#define OD_KEY_DOWN        0x50
#define OD_KEY_LEFT        0x4b
#define OD_KEY_RIGHT       0x4d
#define OD_KEY_INSERT      0x52
#define OD_KEY_DELETE      0x53
#define OD_KEY_HOME        0x47
#define OD_KEY_END         0x4f
#define OD_KEY_PGUP        0x49
#define OD_KEY_PGDN        0x51
#define OD_KEY_F11         0x85
#define OD_KEY_F12         0x86
#define OD_KEY_SHIFTTAB    0x0f

/* Input event structure. */
typedef struct
{
   tODInputEventType EventType;
   BOOL bFromRemote;
   char chKeyPress;
} tODInputEvent;


/* Third option (in addition to TRUE and FALSE) for tri-state options. */
#define MAYBE 2

/* od_spawnvpe() flags. */
#define P_WAIT                  0
#define P_NOWAIT                1
#define CURRENT                 0
#define IRET                    1

/* od_edit_str() flags. */
#define EDIT_FLAG_NORMAL        0x0000
#define EDIT_FLAG_NO_REDRAW     0x0001
#define EDIT_FLAG_FIELD_MODE    0x0002
#define EDIT_FLAG_EDIT_STRING   0x0004
#define EDIT_FLAG_STRICT_INPUT  0x0008
#define EDIT_FLAG_PASSWORD_MODE 0x0010
#define EDIT_FLAG_ALLOW_CANCEL  0x0020
#define EDIT_FLAG_FILL_STRING   0x0040
#define EDIT_FLAG_AUTO_ENTER    0x0080
#define EDIT_FLAG_AUTO_DELETE   0x0100
#define EDIT_FLAG_KEEP_BLANK    0x0200
#define EDIT_FLAG_PERMALITERAL  0x0400
#define EDIT_FLAG_LEAVE_BLANK   0x0800
#define EDIT_FLAG_SHOW_SIZE     0x1000

/* od_edit_str() return values. */
#define EDIT_RETURN_ERROR       0
#define EDIT_RETURN_CANCEL      1
#define EDIT_RETURN_ACCEPT      2
#define EDIT_RETURN_PREVIOUS    3
#define EDIT_RETURN_NEXT        4

/* od_popup_menu() flag values. */
#define MENU_NORMAL             0x0000
#define MENU_ALLOW_CANCEL       0x0001
#define MENU_PULLDOWN           0x0002
#define MENU_KEEP               0x0004
#define MENU_DESTROY            0x0008

/* od_autodetect() flag values. */
#define DETECT_NORMAL           0x0000

/* od_scroll() flags. */
#define SCROLL_NORMAL           0x0000
#define SCROLL_NO_CLEAR         0x0001

/* OpenDoors status line settings */
#define STATUS_NORMAL           0
#define STATUS_NONE             8
#define STATUS_ALTERNATE_1      1
#define STATUS_ALTERNATE_2      2
#define STATUS_ALTERNATE_3      3
#define STATUS_ALTERNATE_4      4
#define STATUS_ALTERNATE_5      5
#define STATUS_ALTERNATE_6      6
#define STATUS_ALTERNATE_7      7

/* OpenDoors color definitions. */
#define D_BLACK                 0
#define D_BLUE                  1
#define D_GREEN                 2
#define D_CYAN                  3
#define D_RED                   4
#define D_MAGENTA               5
#define D_BROWN                 6
#define D_GREY                  7
#define L_BLACK                 8
#define L_BLUE                  9
#define L_GREEN                 10
#define L_CYAN                  11
#define L_RED                   12
#define L_MAGENTA               13
#define L_YELLOW                14
#define L_WHITE                 15
#define B_BLACK                 L_BLACK
#define B_BLUE                  L_BLUE
#define B_GREEN                 L_GREEN
#define B_CYAN                  L_CYAN
#define B_RED                   L_RED
#define B_MAGENTA               L_MAGENTA
#define B_BROWN                 L_YELLOW
#define B_GREY                  L_WHITE

/* Door information file formats (od_control.od_info_type). */
#define DORINFO1              0                              /* DORINFO?.DEF */
#define EXITINFO              1     /* QBBS 2.6? EXITINFO.BBS & DORINFO?.DEF */
#define RA1EXITINFO           2       /* RA 1.?? EXITINFO.BBS & DORINFO?.DEF */
#define CHAINTXT              3                                 /* CHAIN.TXT */
#define SFDOORSDAT            4                               /* SFDOORS.DAT */
#define CALLINFO              5                              /* CALLINFO.BBS */
#define DOORSYS_GAP           6                     /* GAP/PC-Board DOOR.SYS */
#define DOORSYS_DRWY          7                          /* DoorWay DOOR.SYS */
#define QBBS275EXITINFO       8               /* QuickBBS 2.75+ EXITINFO.BBS */
#define CUSTOM                9                /* User-defined custom format */
#define DOORSYS_WILDCAT       10                        /* WildCat! DOOR.SYS */
#define RA2EXITINFO           11                    /* RA 2.00+ EXITINFO.BBS */
#define TRIBBSSYS             12                               /* TRIBBS.SYS */
#define DOOR32SYS             13                               /* DOOR32.SYS */
#define NO_DOOR_FILE          100      /* No door information file was found */

/* Error type (od_control.od_error). */
#define ERR_NONE              0                              /* No error yet */
#define ERR_MEMORY            1          /* Unable to allocate enough memory */
#define ERR_NOGRAPHICS        2    /* Function requires ANSI/AVATAR/RIP mode */
#define ERR_PARAMETER         3    /* Invalid value was passed to a function */
#define ERR_FILEOPEN          4                       /* Unable to open file */
#define ERR_LIMIT             5       /* An internal limit has been exceeded */
#define ERR_FILEREAD          6                  /* Unable to read from file */
#define ERR_NOREMOTE          7  /* Function may not be called in local mode */
#define ERR_GENERALFAILURE    8       /* Percise cause of failure is unknown */
#define ERR_NOTHINGWAITING    9    /* A request for data when none was ready */
#define ERR_NOMATCH           10                       /* No match was found */
#define ERR_UNSUPPORTED       11            /* Not supported in this version */

/* od_control.od_errorlevel indicies. */
#define ERRORLEVEL_ENABLE     0
#define ERRORLEVEL_CRITICAL   1
#define ERRORLEVEL_NOCARRIER  2
#define ERRORLEVEL_HANGUP     3
#define ERRORLEVEL_TIMEOUT    4
#define ERRORLEVEL_INACTIVITY 5
#define ERRORLEVEL_DROPTOBBS  6
#define ERRORLEVEL_NORMAL     7

/* Special od_popup_menu() return values. */
#define POPUP_ERROR           -1
#define POPUP_ESCAPE          0
#define POPUP_LEFT            -2
#define POPUP_RIGHT           -3

/* od_get_input() flags. */
#define GETIN_NORMAL          0x0000
#define GETIN_RAW             0x0001
#define GETIN_RAWCTRL         0x0002

/* od_control.od_box_chars array indicies. */
#define BOX_UPPERLEFT         0
#define BOX_TOP               1
#define BOX_UPPERRIGHT        2
#define BOX_LEFT              3
#define BOX_LOWERLEFT         4
#define BOX_LOWERRIGHT        5
#define BOX_BOTTOM            6
#define BOX_RIGHT             7

/* od_control.od_okaytopage settings. */
#define PAGE_DISABLE          0
#define PAGE_ENABLE           1
#define PAGE_USE_HOURS        2

/*  Method used for serial I/O (od_control.od_com_method). */
#define COM_FOSSIL            1
#define COM_INTERNAL          2
#define COM_WIN32             3
#define COM_DOOR32            4
#define COM_SOCKET				5
#define COM_STDIO			  6

/* Flow control method (od_control.od_com_flow_control). */
#define COM_DEFAULT_FLOW      0
#define COM_RTSCTS_FLOW       1
#define COM_NO_FLOW           2

/* Optional component initialization functions. */
ODAPIDEF void ODCALL ODConfigInit(void);
ODAPIDEF void ODCALL ODLogEnable(void);
ODAPIDEF void ODCALL ODMPSEnable(void);

/* Optional OpenDoors component settings. */
typedef void(ODFAR OD_COMPONENT)(void);
#define INCLUDE_CONFIG_FILE   (OD_COMPONENT *)ODConfigInit
#define NO_CONFIG_FILE        NULL
#define INCLUDE_LOGFILE       (OD_COMPONENT *)ODLogEnable
#define NO_LOGFILE            NULL
#define INCLUDE_MPS           (OD_COMPONENT *)ODMPSEnable
#define NO_MPS                NULL

/* Built-in personality defintion functions. */
ODAPIDEF void ODCALL pdef_opendoors(BYTE btOperation);
ODAPIDEF void ODCALL pdef_pcboard(BYTE btOperation);
ODAPIDEF void ODCALL pdef_ra(BYTE btOperation);
ODAPIDEF void ODCALL pdef_wildcat(BYTE btOperation);

/* Personality proc type. */
typedef void(ODFAR OD_PERSONALITY_PROC)(BYTE);

/* Personality identifiers. */
#define PER_OPENDOORS         (void *)pdef_opendoors
#define PER_PCBOARD           (void *)pdef_pcboard
#define PER_RA                (void *)pdef_ra
#define PER_WILDCAT           (void *)pdef_wildcat

/* od_control.od_disable flags. */
#define DIS_INFOFILE          0x0001
#define DIS_CARRIERDETECT     0x0002
#define DIS_TIMEOUT           0x0004
#define DIS_LOCAL_OVERRIDE    0x0008
#define DIS_BPS_SETTING       0x0010
#define DIS_LOCAL_INPUT       0x0020
#define DIS_SYSOP_KEYS        0x0040
#define DIS_DTR_DISABLE       0x0080
#define DIS_NAME_PROMPT       0x0100

/* Event status settings. */
#define ES_DELETED            0
#define ES_ENABLED            1
#define ES_DISABLED           2

/* Personality proceedure operations. */
#define PEROP_DISPLAY1        0
#define PEROP_DISPLAY2        1
#define PEROP_DISPLAY3        2
#define PEROP_DISPLAY4        3
#define PEROP_DISPLAY5        4
#define PEROP_DISPLAY6        5
#define PEROP_DISPLAY7        6
#define PEROP_DISPLAY8        7
#define PEROP_UPDATE1         10
#define PEROP_UPDATE2         11
#define PEROP_UPDATE3         12
#define PEROP_UPDATE4         13
#define PEROP_UPDATE5         14
#define PEROP_UPDATE6         15
#define PEROP_UPDATE7         16
#define PEROP_UPDATE8         17
#define PEROP_INITIALIZE      20
#define PEROP_CUSTOMKEY       21
#define PEROP_DEINITIALIZE    22


/* ========================================================================= */
/* The OpenDoors control structure (od_control)                              */
/* ========================================================================= */

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
   /* Location or name of door information file (if one is to be used). */
   char          info_path[60];

   /* Serial port settings. */
   DWORD         baud;
   DWORD         od_connect_speed;
   INT16         od_com_address;
   BYTE          od_com_irq;
   BYTE          od_com_method;
   BYTE          od_com_flow_control;
   WORD          od_com_rx_buf;
   WORD          od_com_tx_buf;
   BYTE          od_com_fifo_trigger;
   BOOL          od_com_no_fifo;
   BOOL          od_no_fossil;
   BOOL          od_use_socket;
   INT16         port;
   DWORD         od_open_handle;

   /* Caller and system information. */
   char          system_name[40];
   char          sysop_name[40];
   INT32         system_calls;
   char          system_last_caller[36];
   char          timelog_start_date[9];
   INT16         timelog_busyperhour[24];
   INT16         timelog_busyperday[7];

   char          user_name[36];
   char          user_location[26];
   char          user_password[16];
   char          user_dataphone[16];
   char          user_homephone[16];
   char          user_lasttime[6];
   char          user_lastdate[9];
   BYTE          user_attribute;
   BYTE          user_flags[4];
   DWORD         user_net_credit;
   DWORD         user_pending;
   WORD          user_messages;
   DWORD         user_lastread;
   WORD          user_security;
   DWORD         user_numcalls;
   DWORD         user_uploads;
   DWORD         user_downloads;
   DWORD         user_upk;
   DWORD         user_downk;
   DWORD         user_todayk;
   WORD          user_time_used;
   WORD          user_screen_length;
   BYTE          user_last_pwdchange;
   BYTE          user_attrib2;
   WORD          user_group;

   BYTE          event_status;
   char          event_starttime[6];
   BYTE          event_errorlevel;
   BYTE          event_days;
   BYTE          event_force;
   char          event_last_run[9];

   BYTE          user_netmailentered;
   BYTE          user_echomailentered;
   char          user_logintime[6];
   char          user_logindate[9];
   INT16         user_timelimit;
   INT32         user_loginsec;
   INT32         user_credit;
   WORD          user_num;
   INT16         user_readthru;
   INT16         user_numpages;
   INT16         user_downlimit;
   char          user_timeofcreation[6];
   char          user_logonpassword[16];
   BYTE          user_wantchat;
   BYTE          user_ansi;
   INT16         user_deducted_time;
   char          user_menustack[50][9];
   BYTE          user_menustackpointer;
   char          user_handle[36];
   char          user_comment[81];
   char          user_firstcall[9];
   BYTE          user_combinedrecord[200];
   char          user_birthday[9];
   char          user_subdate[9];
   BYTE          user_screenwidth;
   BYTE          user_language;
   BYTE          user_date_format;
   char          user_forward_to[36];
   BYTE          user_error_free;
   BYTE          sysop_next;
   BYTE          user_emsi_session;
   char          user_emsi_crtdef[41];
   char          user_emsi_protocols[41];
   char          user_emsi_capabilities[41];
   char          user_emsi_requests[41];
   char          user_emsi_software[41];
   BYTE          user_hold_attr1;
   BYTE          user_hold_attr2;
   BYTE          user_hold_len;
   char          user_reasonforchat[78];
   char          user_callsign[12];
   WORD          user_msg_area;
   WORD          user_file_area;
   char          user_protocol;
   WORD          user_file_group;
   BYTE          user_last_birthday_check;
   char          user_sex;
   DWORD         user_xi_record;
   WORD          user_msg_group;
   BYTE          user_avatar;
   char          user_org[51];
   char          user_address[3][51];
   INT32         user_pwd_crc;
   INT32         user_logon_pwd_crc;
   char          user_last_cost_menu[9];
   WORD          user_menu_cost;
   BYTE          user_rip;
   BYTE          user_rip_ver;
   BYTE          user_attrib3;
   BOOL          user_expert;
   char          system_last_handle[36];

   /* Door information file statistics. */
   BYTE          od_info_type;
   BYTE          od_extended_info;
   WORD          od_node;
   BYTE          od_ra_info;

   /* Current program settings. */
   BOOL          od_always_clear;
   BOOL          od_force_local;
   BOOL          od_chat_active;
   BOOL          od_current_statusline;
   INT16         od_error;
   BYTE          od_last_input;
   BOOL          od_logfile_disable;
   char          od_logfile_name[80];
   WORD          od_maxtime;
   INT16         od_maxtime_deduction;
   BOOL          od_okaytopage;
   INT16         od_pagestartmin;
   INT16         od_pageendmin;
   BOOL          od_page_pausing;
   INT16         od_page_statusline;
   BOOL          od_user_keyboard_on;
   BOOL          od_update_status_now;
   INT16         od_cur_attrib;

   /* OpenDoors customization settings. */
   char          od_box_chars[8];
   char          od_cfg_text[48][33];
   char          od_cfg_lines[25][33];
   OD_COMPONENT  *od_config_file;
   const char *  od_config_filename;
   void          (*od_config_function)(char *keyword, char *options);
   char          od_color_char;
   char          od_color_delimiter;
   char          od_color_names[12][33];
   BOOL          od_clear_on_exit;
   void          (*od_cmd_line_handler)(char *pszKeyword, char *pszOptions);
   void          (*od_cmd_line_help_func)(void);
   void          (*od_default_personality)(BYTE operation);
   BOOL          od_default_rip_win;
   WORD          od_disable;
   char          od_disable_dtr[40];
   BOOL          od_disable_inactivity;
   BOOL          od_emu_simulate_modem;
   BYTE          od_errorlevel[8];
   BOOL          od_full_color;
   BOOL          od_full_put;
   WORD          od_in_buf_size;
   INT16         od_inactivity;
   INT16         od_inactive_warning;
   BOOL          od_internal_debug;
   tODMilliSec   od_max_key_latency;
   char          od_list_pause;
   char          od_list_stop;
   OD_COMPONENT  *od_logfile;
   char          *od_logfile_messages[14];
   OD_COMPONENT  *od_mps;
   BOOL          od_nocopyright;
   BOOL          od_noexit;
   BOOL          od_no_ra_codes;
   BYTE          od_page_len;
   char          od_prog_copyright[40];
   char          od_prog_name[40];
   char          od_prog_version[40];
   DWORD         od_reg_key;
   char          od_reg_name[36];
   BOOL          od_silent_mode;
   BOOL          od_status_on;
   BOOL          od_spawn_freeze_time;
   BOOL          od_swapping_disable;
   BOOL          od_swapping_noems;
   char          od_swapping_path[80];

   /* Custom function hooks. */
   void          (*od_no_file_func)(void);
   void          (*od_before_exit)(void);
   void          (*od_cbefore_chat)(void);
   void          (*od_cafter_chat)(void);
   void          (*od_cbefore_shell)(void);
   void          (*od_cafter_shell)(void);
   void          (*od_config_callback)(void);
   void          (*od_help_callback)(void);
   void          (*od_ker_exec)(void);
   void          (*od_local_input)(INT16 key);
   void          (*od_time_msg_func)(char *string);

   /* OpenDoors function key customizations. */
   WORD          key_chat;
   WORD          key_dosshell;
   WORD          key_drop2bbs;
   WORD          key_hangup;
   WORD          key_keyboardoff;
   WORD          key_lesstime;
   WORD          key_lockout;
   WORD          key_moretime;
   WORD          key_status[9];
   WORD          key_sysopnext;

   /* Additional function keys. */
   BYTE          od_num_keys;
   INT16         od_hot_key[16];
   INT16         od_last_hot;
   void          (*od_hot_function[16])(void);

   /* OpenDoors prompt customizations. */
   char *        od_after_chat;
   char *        od_after_shell;
   char *        od_before_chat;
   char *        od_before_shell;
   char *        od_chat_reason;
   char *        od_continue;
   char          od_continue_yes;
   char          od_continue_no;
   char          od_continue_nonstop;
   char *        od_day[7];
   char *        od_hanging_up;
   char *        od_exiting;
   char *        od_help_text;
   char *        od_help_text2;
   char *        od_inactivity_timeout;
   char *        od_inactivity_warning;
   char *        od_month[12];
   char *        od_no_keyboard;
   char *        od_no_sysop;
   char *        od_no_response;
   char *        od_no_time;
   char *        od_offline;
   char *        od_paging;
   char *        od_press_key;
   char *        od_sending_rip;
   char *        od_status_line[3];
   char *        od_sysop_next;
   char *        od_time_left;
   char *        od_time_warning;
   char *        od_want_chat;
   char *        od_cmd_line_help;

   /* OpenDoors color customizations. */
   BYTE          od_chat_color1;
   BYTE          od_chat_color2;
   BYTE          od_list_comment_col;
   BYTE          od_list_name_col;
   BYTE          od_list_offline_col;
   BYTE          od_list_size_col;
   BYTE          od_list_title_col;
   BYTE          od_local_win_col;
   BYTE          od_continue_col;
   BYTE          od_menu_title_col;
   BYTE          od_menu_border_col;
   BYTE          od_menu_text_col;
   BYTE          od_menu_key_col;
   BYTE          od_menu_highlight_col;
   BYTE          od_menu_highkey_col;

   /* Platform-specific settings. */
#ifdef ODPLAT_WIN32
   HICON         od_app_icon;
   int           od_cmd_show;
#endif /* ODPLAT_WIN32 */
} tODControl;

/* Restore original structure alignment, if possible. */
#ifdef _MSC_VER
#pragma pack()
#endif /* _MSC_VER */


/* The od_control external variable. */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
OD_API_VAR_DECL tODControl 
#ifndef _WIN32	/* warning C4229: anachronism used : modifiers on data are ignored */
OD_GLOBAL_CONV 
#endif
od_control;
#ifdef __cplusplus
}
#endif /* __cplusplus */


/* ========================================================================= */
/* OpenDoors API function prototypes.                                        */
/* ========================================================================= */

/* Programs interface with OpenDoors by calling any of the OpenDoors API
 * functions. A summary of these functions appears below, followed by the
 * function definition prototypes. Full information on these functions is
 * provided by the OpenDoors manual. Functions denoted (ANS/AVT) require ANSI
 * or AVATAR display modes to be active.
 *
 * OUTPUT FUNCTIONS - TEXT DISPLAY
 *    od_printf()             - Performs formatted output, with color settings
 *    od_disp_str()           - Displays a normal, nul-terminated string.
 *    od_disp()               - Sends chars to modem, with/without local echo
 *    od_disp_emu()           - Displays a string, translating ANSI/AVT codes
 *    od_repeat()             - Efficiently displays a character repeatedly
 *    od_putch()              - Displays a single character.
 *
 * OUTPUT FUNCTIONS - COLOUR AND CURSOR CONTROL
 *    od_set_color()          - Sets color according to fore/background values
 *    od_set_attrib()         - Sets color to specified IBM-PC attribute
 *    od_set_cursor()         - Positions cursor on screen            (ANS/AVT)
 *    od_get_cursor()         - Estimates the current cursor position on screen
 *
 * OUTPUT FUNCTIONS - SCREEN MANIPULATION
 *    od_clr_scr()            - Clears the screen
 *    od_save_screen()        - Saves the contents of entire screen
 *    od_restore_screen()     - Restores the contents of entire screen
 *
 * OUTPUT FUNCTIONS - BLOCK MANIPULATION
 *    od_clr_line()           - Clears the remainder of the current line
 *    od_gettext()            - Gets the contents a block of screen   (ANS/AVT)
 *    od_puttext()            - Displays block stored by gettext()    (ANS/AVT)
 *    od_scroll()             - Scrolls a portion of the screen       (ANS/AVT)
 *
 * OUTPUT FUNCTIONS - WINDOWS & MENUS
 *    od_draw_box()           - Draws a box on the screen             (ANS/AVT)
 *    od_window_create()      - Creates a window, storing underlying  (ANS/AVT)
 *    od_window_remove()      - Removes window, restoring underlying  (ANS/AVT)
 *    od_popup_menu()         - Displays popup menu with "light" bar  (ANS/AVT)
 *
 * OUTPUT FUNCTIONS - FILE DISPLAY
 *    od_send_file()          - Displays an ASCII/ANSI/AVATAR/RIP file
 *    od_hotkey_menu()        - Displays ASC/ANS/AVATAR/RIP menu, with hotkeys
 *    od_list_files()         - Lists files avail for download using FILES.BBS
 *
 * INPUT FUNCTIONS
 *    od_get_answer()         - Inputs a key, allowing only specified responses
 *    od_get_key()            - Inputs a key, optionally waiting for next key
 *    od_get_input()          - Obtains next input, with translation
 *    od_input_str()          - Inputs string of specified length from keyboard
 *    od_edit_str()           - Fancy formatted string input function (ANS/AVT)
 *    od_clear_keybuffer()    - Removes any waiting keys in keyboard buffer
 *    od_multiline_edit()     - Edits text that spans multiple lines  (ANS/AVT)
 *    od_key_pending()        - Returns TRUE if a key is waiting to be processed
 *
 * COMMON DOOR ACTIVITY FUNCTIONS
 *    od_page()               - Allows user to page sysop
 *    od_spawn()              - Suspends OpenDoors & starts another program
 *    od_spawnvpe()           - Like od_spawn, but with more options
 *    od_log_write()          - Writes a logfile entry
 *    od_parse_cmd_line()     - Handles standard command-line parameters
 *
 * SPECIAL CONTROL FUNCTIONS
 *    od_init()               - Forces OpenDoors initialization
 *    od_color_config()       - Translates color description to color value
 *    od_add_personality()    - Adds another local interface personality
 *    od_set_statusline()     - Sets the current status line setting
 *    od_autodetect()         - Determines the remote system terminal type
 *    od_kernel()             - Call when not calling other functions
 *    od_exit()               - Ends OpenDoors program
 *    od_carrier()            - Indicates whether remote connection is present
 *    od_set_dtr()            - Raises / lowers the DTR signal to the modem
 *    od_chat()               - Manually starts chat mode
 *    od_sleep()              - Yield to other processes
 *    od_control_get()        - Returns a pointer to the od_control structure.
 */
ODAPIDEF BOOL ODCALL   od_add_personality(const char *pszName, BYTE btOutputTop,
                          BYTE btOutputBottom,
                          OD_PERSONALITY_PROC *pfPerFunc);
ODAPIDEF void ODCALL   od_autodetect(INT nFlags);
ODAPIDEF BOOL ODCALL   od_carrier(void);
ODAPIDEF void ODCALL   od_chat(void);
ODAPIDEF void ODCALL   od_clear_keybuffer(void);
ODAPIDEF void ODCALL   od_clr_line(void);
ODAPIDEF void ODCALL   od_clr_scr(void);
ODAPIDEF BYTE ODCALL   od_color_config(char *pszColorDesc);
ODAPIDEF tODControl *  ODCALL od_control_get(void);
ODAPIDEF void ODCALL   od_disp(const char *pachBuffer, INT nSize, BOOL bLocalEcho);
ODAPIDEF void ODCALL   od_disp_emu(const char *pszToDisplay, BOOL bRemoteEcho);
ODAPIDEF void ODCALL   od_disp_str(const char *pszToDisplay);
ODAPIDEF BOOL ODCALL   od_draw_box(BYTE btLeft, BYTE btTop, BYTE btRight,
                          BYTE btBottom);
ODAPIDEF WORD ODCALL   od_edit_str(char *pszInput, char *pszFormat, INT nRow,
                          INT nColumn, BYTE btNormalColour,
                          BYTE btHighlightColour, char chBlank,
                          WORD nFlags);
ODAPIDEF void ODCALL   od_exit(INT nErrorLevel, BOOL bTermCall);
ODAPIDEF char ODCALL   od_get_answer(const char *pszOptions);
ODAPIDEF void ODCALL   od_get_cursor(INT *pnRow, INT *pnColumn);
ODAPIDEF BOOL ODCALL   od_get_input(tODInputEvent *pInputEvent,
                          tODMilliSec TimeToWait, WORD wFlags);
ODAPIDEF BOOL ODCALL   od_key_pending(void);
ODAPIDEF char ODCALL   od_get_key(BOOL bWait);
ODAPIDEF BOOL ODCALL   od_gettext(INT nLeft, INT nTop, INT nRight,
                          INT nBottom, void *pBlock);
ODAPIDEF char ODCALL   od_hotkey_menu(char *pszFileName, char *pszHotKeys,
                          BOOL bWait);
ODAPIDEF void ODCALL   od_init(void);
ODAPIDEF void ODCALL   od_input_str(char *pszInput, INT nMaxLength,
                          unsigned char chMin, unsigned char chMax);
ODAPIDEF void ODCALL   od_kernel(void);
ODAPIDEF BOOL ODCALL   od_list_files(char *pszFileSpec);
ODAPIDEF BOOL ODCALL   od_log_write(char *pszMessage);
ODAPIDEF INT ODCALL    od_multiline_edit(char *pszBufferToEdit,
                          UINT unBufferSize, tODEditOptions *pEditOptions);
ODAPIDEF void ODCALL   od_page(void);
#ifdef ODPLAT_WIN32
ODAPIDEF void ODCALL   od_parse_cmd_line(LPSTR pszCmdLine);
#else /* !ODPLAT_WIN32 */
ODAPIDEF void ODCALL   od_parse_cmd_line(INT nArgCount,
                          char *papszArguments[]);
#endif /* !ODPLAT_WIN32 */
ODAPIDEF INT ODCALL    od_popup_menu(char *pszTitle, char *pszText,
                          INT nLeft, INT nTop, INT nLevel, WORD uFlags);
ODAPIDEF void ODVCALL  od_printf(const char *pszFormat, ...);
ODAPIDEF void ODCALL   od_putch(char chToDisplay);
ODAPIDEF BOOL ODCALL   od_puttext(INT nLeft, INT nTop, INT nRight,
                          INT nBottom, void *pBlock);
ODAPIDEF void ODCALL   od_repeat(char chValue, BYTE btTimes);
ODAPIDEF BOOL ODCALL   od_restore_screen(void *pBuffer);
ODAPIDEF BOOL ODCALL   od_save_screen(void *pBuffer);
ODAPIDEF BOOL ODCALL   od_scroll(INT nLeft, INT nTop, INT nRight,
                          INT nBottom, INT nDistance, WORD nFlags);
ODAPIDEF BOOL ODCALL   od_send_file(const char *pszFileName);
ODAPIDEF BOOL ODCALL   od_send_file_section(char *pszFileName, char *pszSectionName);
ODAPIDEF void ODCALL   od_set_attrib(INT nColour);
ODAPIDEF void ODCALL   od_set_color(INT nForeground, INT nBackground);
ODAPIDEF void ODCALL   od_set_cursor(INT nRow, INT nColumn);
ODAPIDEF void ODCALL   od_set_dtr(BOOL bHigh);
ODAPIDEF BOOL ODCALL   od_set_personality(const char *pszName);
ODAPIDEF void ODCALL   od_set_statusline(INT nSetting);
ODAPIDEF void ODCALL   od_sleep(tODMilliSec Milliseconds);
ODAPIDEF BOOL ODCALL   od_spawn(const char *pszCommandLine);
ODAPIDEF INT16 ODCALL  od_spawnvpe(INT16 nModeFlag, char *pszPath,
                          char *papszArg[], char *papszEnv[]);
ODAPIDEF void * ODCALL od_window_create(INT nLeft, INT nTop, INT nRight,
                          INT nBottom, char *pszTitle, BYTE btBorderCol,
                          BYTE btTitleCol, BYTE btInsideCol, INT nReserved);
ODAPIDEF BOOL ODCALL   od_window_remove(void *pWinInfo);


/* ========================================================================= */
/* Definitions for compatibility with previous versions.                     */
/* ========================================================================= */

/* Alternative spelling for the word color (colour). */
#define od_chat_colour1                od_chat_color1
#define od_chat_colour2                od_chat_color2
#define od_colour_char                 od_color_char
#define od_colour_delimiter            od_color_delimiter
#define od_colour_names                od_color_names
#define od_full_colour                 od_full_color
#define od_colour_config               od_color_config
#define od_set_colour                  od_set_color

/* Definitions for renamed od_control members and manifest constants. */
#define key_help                       key_status[6]
#define key_nohelp                     key_status[0]
#define user_credit                    user_net_credit
#define caller_netmailentered          user_netmailentered
#define caller_echomailentered         user_echomailentered
#define caller_logintime               user_logintime
#define caller_logindate               user_logindate
#define caller_timelimit               user_timelimit
#define caller_loginsec                user_loginsec
#define caller_credit                  user_credit
#define caller_userrecord              user_num
#define caller_readthru                user_readthru
#define caller_numpages                user_numpages
#define caller_downlimit               user_downlimit
#define caller_timeofcreation          user_timeofcreation
#define caller_logonpassword           user_logonpassword
#define caller_wantchat                user_wantchat
#define caller_ansi                    user_ansi
#define ra_deducted_time               user_deducted_time
#define ra_menustack                   user_menustack
#define ra_menustackpointer            user_menustackpointer
#define ra_userhandle                  user_handle
#define ra_comment                     user_comment
#define ra_firstcall                   user_firstcall
#define ra_combinedrecord              user_combinedrecord
#define ra_birthday                    user_birthday
#define ra_subdate                     user_subdate
#define ra_screenwidth                 user_screenwidth
#define ra_msg_area                    user_msg_area
#define ra_file_area                   user_file_area
#define ra_language                    user_language
#define ra_date_format                 user_date_format
#define ra_forward_to                  user_forward_to
#define ra_error_free                  user_error_free
#define ra_sysop_next                  sysop_next
#define ra_emsi_session                user_emsi_session
#define ra_emsi_crtdef                 user_emsi_crtdef
#define ra_emsi_protocols              user_emsi_protocols
#define ra_emsi_capabilities           user_emsi_capabilities
#define ra_emsi_requests               user_emsi_requests
#define ra_emsi_software               user_emsi_software
#define ra_hold_attr1                  user_hold_attr1
#define ra_hold_attr2                  user_hold_attr2
#define ra_hold_len                    user_hold_len
#define caller_usernum                 user_num
#define caller_callsign                user_callsign
#define caller_sex                     user_sex
#define od_avatar                      user_avatar
#define B_YELLOW                       L_YELLOW
#define B_WHITE                        L_WHITE
#define od_rbbs_node                   od_node
#define STATUS_USER1                   STATUS_ALTERNATE_1
#define STATUS_USER2                   STATUS_ALTERNATE_2
#define STATUS_USER3                   STATUS_ALTERNATE_3
#define STATUS_USER4                   STATUS_ALTERNATE_4
#define STATUS_SYSTEM                  STATUS_ALTERNATE_5
#define STATUS_HELP                    STATUS_ALTERNATE_7
#define od_registered_to               od_control.od_reg_name
#define od_registration_key            od_control.od_reg_key
#define od_program_name                od_control.od_prog_name
#define od_log_messages                od_control.od_logfile_messages
#define od_config_text                 od_control.od_cfg_text
#define od_config_lines                od_control.od_cfg_lines
#define od_config_colours              od_control.od_colour_names
#define od_config_colors               od_control.od_colour_names
#define config_file                    od_config_file
#define config_filename                od_config_filename
#define config_function                od_config_function
#define default_personality            od_default_personality
#define logfile                        od_logfile
#define mps                            od_mps
#define od_kernal                      od_kernel

/* Obsolete functions. */
#define od_init_with_config(filename,function)\
                                  od_control.config_file=INCLUDE_CONFIG_FILE;\
                                  od_control.config_filename=filename;\
                                  od_control.config_function=function;\
                                  od_init()
ODAPIDEF BOOL ODCALL                   od_log_open(void);
ODAPIDEF void ODCALL                   od_emulate(register char in_char);

#endif /* _INC_OPENDOOR */
