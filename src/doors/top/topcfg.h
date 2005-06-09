
/*  Copyright 1993 - 2000 Paul J. Sidorsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define BBSTYPES       5
#define NODETYPES      3

#define BBS_UNKNOWN    0
#define BBS_RA2        1
#define BBS_MAX2       2
#define BBS_MAX3       3
#define BBS_SBBS11     4

#define NODE_REMOTE    0
#define NODE_LOCAL     1
#define NODE_LAN       2

/* Footnotes are marked as (*1) where 1 is the number of the footnote.  See
   bottom of file for actual footnotes. */

typedef struct
    {
    /* Names */
    unsigned char bbsname[41];          /* BBS Name (*1) */
    unsigned char sysopname[41];        /* Sysop Name (*1) */

    /* Site Info */
    unsigned char bbstype;              /* BBS Type (*2) */

    /* Paths */
    unsigned char toppath[81];          /* Path to TOP System files */
    unsigned char topworkpath[81];      /* Path to TOP Inter-node files */
    unsigned char topansipath[81];      /* Path to TOP .A??/.RIP files */
    unsigned char bbspath[81];          /* Path to BBS files (*4) */
    unsigned char bbsmultipath[81];     /* Path to BBS multinode files (*5) */
    unsigned char topactpath[81];       /* Path to .TAC (action) files */

    /* Security */
    unsigned XINT sysopsec;        /* Sysop's Security Level */
    unsigned XINT newusersec;
    unsigned XINT talksec;
    unsigned XINT handlechgsec;
    unsigned XINT forgetsec;
    unsigned XINT sexchangesec;
    unsigned XINT exmsgchangesec;
    unsigned XINT privmessagesec;
    unsigned XINT actionusesec;

    /* Options */
    unsigned char showtitle;            /* Boolean - Show Title? */
    unsigned char shownews;             /* Boolean - Show News? */
    unsigned char allowactions;         /* Boolean - Allow Actions? */
    unsigned char allowgas;             /* Boolean - Allow GAs? */
    unsigned char allownewhandles;      /* Boolean - Allow new users to enter
                                                     handles? */
    unsigned char allowhandles;         /* Boolean - Allow handles? */
    unsigned char allowhandlechange;    /* Boolean - Allow users to change
                                                     handles */
    unsigned char allowsexchange;       /* Boolean - Allow sex changes? */
    unsigned char allowexmessages;      /* Boolean - Allow Entry/Exit Msgs? */
    unsigned char allowprivmsgs;        /* Boolean - Allow Whispers/Secret
    												 Actions? */
    unsigned char allowforgetting;
    unsigned char usehandles;           /* Use handles in BBS NODES list? */
    unsigned char showopeningcred;
    unsigned char showclosingcred;
    unsigned char allowhighascii;
//    unsigned char allowcolinchat;
//    unsigned char allowcolinhandles;
    unsigned char noregname;
    unsigned char fixnames;
    unsigned char forcebio;

    /* General Settings */
//    unsigned XINT maxmsglen;
    unsigned XINT inactivetime;
    unsigned char localbeeping;
    unsigned char langfile[13];
    XINT          maxnodes;
    unsigned char maxpwtries;
    unsigned long defaultchannel;
    unsigned char actfilestr[256]; /* Str from cfg. */
    unsigned char **actfilenames; /* Individual file names (array) */
    unsigned XINT usecredits; /* 0=no, >0 = num to deduct per second */
    unsigned char defaultprefs[3];
    unsigned char actionprefix[11];

    /* Performance Tuning */
    unsigned XINT polldelay;
    unsigned XINT retrydelay;
    unsigned XINT maxretries;
    unsigned XINT crashprotdelay;

    /* Censor */
    unsigned XINT maxcensorwarnhigh;
    unsigned XINT maxcensorwarnlow;
    unsigned XINT censortimerhigh;
    unsigned XINT censortimerlow;

    /* Channels */
    long          maxchandefs;

    /* Private Chat */
    unsigned XINT privchatbufsize;
    unsigned XINT privchatmaxtries;
    } TOP_config_typ;

/* Footnotes:

    *1 - Must appear EXACTLY as in TOP.KEY if it exists.

    *2 - Uses the BBS_xxxxxx values defined at start of file.

    *3 - Byte 0, Bit 0 - 300
                     1 - 1200
                     2 - 2400
                     3 - 4800
                     4 - 7200
                     5 - 9600
                     6 - 12000
                     7 - 14400
		 Byte 1, Bit 0 - 16800
                     1 - 19200
                     2 - 21600
                     3 - 24000
                     4 - 28800
                     5 - 31200
                     6 - 33600

    *4 - Dependant on bbstype:

         BBS_UNKNOWN - Not Used
         BBS_RA201   - RA System Path (USERON.BBS)
         BBS_MAX201  - Not Currently Used
         BBS_EZY102  - Not Currently Used

    *5 - Dependant on bbstype:

         BBS_UNKNOWN - Not Used
         BBS_RA201   - RA Semaphore Path (RABUSY.*, NODE???.RA)
         BBS_MAX201  - Maximus IPC Path (IPC??.BBS)
         BBS_EZY102  - Ezycom Multinode Path (ONLINE.BBS)

*/

#define NODECODE_COMMENT     0
#define NODECODE_UNKNOWN     1
#define NODECODE_NUMBER      2
#define NODECODE_TYPE        3
#define NODECODE_DROPFILE    4
#define NODECODE_CFGFILE     5
#define NODECODE_LOGFILE     6
#define NODECODE_SOLID       7
#define NODECODE_FOSSIL      8
#define NODECODE_PORT        9
#define NODECODE_SPEED      10
#define NODECODE_ADDRESS    11
#define NODECODE_IRQ        12
#define NODECODE_FIFO       13
#define NODECODE_FIFOTRIG   14
#define NODECODE_RXBUF      15
#define NODECODE_TXBUF      16
#define NODECODE_RTSCTS     17
#define NODECODE_END        18

typedef struct
	{
    char          type;
    unsigned char dropfilepath[81];
    unsigned char cfgfile[81];
    unsigned char logfile[81];
    char          solidnode; /* True means use even if IDX2byte = 1 */
    char          fossil;
    char          port;
    dword         speed;
    XINT          portaddress;
    byte          portirq;
    char          usefifo;
    char          fifotrigger;
    XINT          rxbufsize;
    XINT          txbufsize;
    char          usertscts;
    char          reserved[512 - 259];
    } TOP_nodecfg_typ;
