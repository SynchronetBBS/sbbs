/******************************************************************************
GLOBAL.C     Global variable declarations.

    Copyright 1993 - 2000 Paul J. Sidorsky

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

This module declares all program-wide global variables used in TOP.  It also
contains comments describing each variable.
******************************************************************************/

#include "top.h"

/* Version and internal revision macros.  They are used by the ver and rev
   variables below.  The defines up here are so they can be changed quickly
   and easily. */
#define TOP_VERSION "2.01b"
#define TOP_REVISION "A321"

/* Borland global variable that sets the stack size. */
unsigned _stklen = 8192L;

/* Variables below are mostly listed in the order added to TOP. */

/* Configuration data. */
TOP_config_typ cfg;
/* User's record number within USERS.TOP. */
XINT user_rec_num = -1;
/* User data. */
user_data_typ user;
/* String used by the word splitter. */
char XFAR *word_str = NULL;
/* Index of word starting positions, used by the word splitter. */
XINT XFAR *word_pos = NULL;
/* Index of word lengths, used by the word splitter. */
XINT XFAR *word_len = NULL;
/* Array of all handles of users currently in TOP. */
stringarray2 XFAR *handles = NULL;
/* Array of flags indicating which nodes are active in TOP. */
unsigned char XFAR *activenodes = NULL;
/* Time the message file was last read. */
clock_t lastpoll = 0;
/* Global output buffer. */
unsigned char XFAR *outbuf =NULL;
/* Flag indicating the node has been registered in the NODEIDX.  Do not
   confuse with the reg... variables which relate to whether or not the
   user has registered TOP.  */
char isregistered = 0;
/* Flag indicating the node has been added to the appropriate BBS useron
   file. */
char node_added = 0;
/* Array of who the user has forgotten and who has forgotten the user. */
unsigned char XFAR *forgetstatus = NULL;
/* Last node a message was sent to, usually used with forgetting. */
XINT lastsendtonode = -1;
/* Local and LAN node type flags. */
char localmode = 0, lanmode = 0;
/* Not used.  I think this was left over from before the concept of a
   "busy channel" was implemented. */
char XFAR *busynodes = NULL;
/* Holds file information for the BBS useron file, to detect changes. */
struct file_stats_str ipcfildat;
/* File handles for the following files, in order of appearance:
   Unused, USERS.TOP, NODEIDX.TCH, NODEIDX2.TCH, MSGnode.TCH,
   MSGother.TCH, USERON.BBS (oeq), IPCn.BBS (oeq), IPCother.BBS (oeq),
   NODEn.RA (oeq), MIXnode.TCH, MIXother.TCH, CHGIDX.TCH,
   CHANIDX.TCH, BIOIDX.TOP, BIORESP.TOP
   Notes:  oeq - "Or Equivalent", means the file name differs with BBS type.
           node - User's current node (also n).
           other - Receiving node.  These handles are usually closed.
*/
XINT cfgfil = -1, userfil = -1, nidxfil = -1, nidx2fil = -1, msginfil = -1,
     msgoutfil = -1, useronfil = -1, ipcinfil = -1, ipcoutfil = -1,
     rapagefil = -1, midxinfil = -1, midxoutfil = -1, chgfil = -1,
     chidxfil = -1, bioidxfil = -1, biorespfil = -1, slotsfil = -1, matchfil=-1;
/* File information for the MIX file, to detect changes. */
struct file_stats_str midxfileinfo;
/* Lowest node.  While properly initialized it is not actually used in TOP.
   It was left over from an implementation of Poker which used the lowest
   node for processing. */
XINT lowestnode = 0;
/* Language text. */
lang_text_typ XFAR **langptrs = NULL;
/* Number of language items. */
long numlang = 0;
/* Not used.  Leftover from an older, crappier version of the command
   processor. */
XINT usedcmdlen = 0;
/* Nodes configuration file handle.  Left over from beta versions which used
   a binary node configuration file. */
XINT nodecfgfil = -1;
/* Current channel the user is on. */
unsigned long curchannel = 0;
/* getword functions return buffer. */
unsigned char *wordret = NULL;
/* Node index data for this node. */
node_idx_typ *node = NULL;
/* top_output() return buffer (used with OUT_STRING). */
unsigned char *outputstr = NULL;
/* Node configuration information for this node. */
TOP_nodecfg_typ *nodecfg = NULL;
/* Flags to process language (@), colour (^), and action (%) tokens.  Used
   by top_output(). */
char outproclang = TRUE;
char outproccol = TRUE;
char outprocact = TRUE;
/* Default text attribute. */
unsigned XINT outdefattrib = 0x07;
/* Last command that was entered.  Set by checkcmdmatch() but not used
   inside TOP. */
unsigned char *lastcmd = NULL;
/* Channel definition data from CHANNELS.CFG. */
channel_data_typ *chan = NULL;
/* Sets the msg.data1 field in a dispatch_message() call.  If you're
   wondering why this is global it's because the data1 field was implemented
   a long time after the messaging system was finalized, and I didn't feel
   like changing upwards of a hundred function calls all over TOP to support
   passing it to dispatch_message(). */
long msgextradata = 0;
/* Unused poker variable. */
//|clock_t lastpokerpoll = 0;
/* Number of action files being used. */
char numactfiles = 0;
/* Action file data. */
action_file_typ **actfiles = NULL;
/* Action data. */
action_rec_typ ***actptrs = NULL;
/* Current Y and X cursor locations.  Not used anywhere.  Left over from
   a cruder version of dual window chat mode. */
XINT ystore = 0, xstore = 0;
/* Unused variable for a planned feature. */
char redrawwindows = 1;
/* Node currently waiting for a private chat, node this user has asked to
   chat with. */
XINT privchatin = -1, privchatout = -1;
/* Current channel data. */
chan_idx_typ cmibuf;
/* Current channel's record number in CHANIDX.TCH. */
XINT cmirec = -1;
/* Last kernel execution time. */
time_t lastkertime = 0;
/* Unused, left over from a feature that was removed. */
spawn_prog_typ *spawnprog = NULL;
XINT numspawnprog = 0;
/* Maximum and minimum security that can receive a message.  These are
   used by dispatch_message() and are global for the same reason
   msgextradata is. */
unsigned XINT msgminsec = 0, msgmaxsec = 0;
/* Used to store the current channel when the user enters "busy" mode. */
unsigned long cmiprevchan = 1;
/* Stores the number of credits when the user enters TOP.  Used with RA's
   credit system. */
long orignumcreds = 0;
/* Whether or not the user is typing.  Used with dual window chat mode. */
char onflag = 0;
/* Flag telling dispatch_message() to send the next message as global (i.e.
   on channel 0).  This variable is global for the same reason as
   msgextradata. */
char msgsendglobal = 0;
/* Censor word definitions from CENSOR.CFG. */
censor_word_typ *cwords = NULL;
/* Number of censor definitions. */
XINT numcensorwords = 0;
/* Number of high-grade and low-grade censor warnings. */
XINT censorwarningshigh = 0;
XINT censorwarningslow = 0;
/* Time of the last high-grade and low-grade censor warnings. */
time_t lastcensorwarnhigh = 0;
time_t lastcensorwarnlow = 0;
/* Bio question definitions from BIOQUES.CFG. */
bio_ques_typ *bioques = NULL;
/* Number of bio questions. */
XINT numbioques = 0;
/* Controls whether TOP can treat doorkit "time left" messages as actual TOP
   messages (which is cleaner) or if it should just display them on screen. */
char blocktimemsgs = 0;
/* Force TOP to fix the user's name.  Not used because some name fixing is
   built into the doorkit anyhow. */
char forcefix = 0;

/* Version and internal revision numbers, set from the macros above. */
unsigned char ver[16] = TOP_VERSION;
unsigned char rev[11] = TOP_REVISION;

/* Output buffers that are suitable for converting numeric data for use
   as parameters to top_output(), because those parameters can only be
   strings. */
unsigned char outnum[9][12];

/* BBS function pointers.  See BBS.TXT for details. */
char (XFAR *bbs_call_loaduseron)(XINT nodenum, bbsnodedata_typ *userdata) = NULL;
char (XFAR *bbs_call_saveuseron)(XINT nodenum, bbsnodedata_typ *userdata) = NULL;
XINT (XFAR *bbs_call_processmsgs)(void) = NULL;
char (XFAR *bbs_call_page)(XINT nodenum, unsigned char *pagebuf) = NULL;
void (XFAR *bbs_call_setexistbits)(bbsnodedata_typ *userdata) = NULL;
void (XFAR *bbs_call_login)(void) = NULL;
void (XFAR *bbs_call_logout)(void) = NULL;
XINT (XFAR *bbs_call_openfiles)(void) = NULL;
XINT (XFAR *bbs_call_updateopenfiles)(void) = NULL;
char (XFAR *bbs_call_pageedit)(XINT nodenum, unsigned char *pagebuf) = NULL;

/* BBS configuration setting names used in TOP.CFG. */
unsigned char *bbsnames[BBSTYPES] =
	{
    "UNKNOWN",
    "RA2",
    "MAX2",
    "MAX3",
    "SBBS11"
    };

/* Node types used in NODES.CFG. */
unsigned char *nodetypes[3] =
	{
    "Remote",
    "Local",
    "Network"
    };

/* Maximus MCP pipe, used by TOP/2 only. */
#ifdef __OS2__
HPIPE hpMCP = 0;
#endif

/* Predefined RA and SuperBBS status types. */
unsigned char XFAR *ra_statustypes[8];
unsigned char XFAR *sbbs_statustypes[11];

/* The remainder of the variables are unused poker variables. */

/*//|char XFAR *pokerlockflags = NULL;
time_t XFAR *pokeretimes = NULL;
XINT XFAR *pokerdtimes = NULL;
XINT XFAR *pokerwtimes = NULL;
char XFAR *pokeringame = NULL;

XINT pokermaingame = -1;

char XFAR pokerdefcardvals[POKERMAXDECK] =
    {
    14,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    14,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    14,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    14,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    15, 15
    };

char XFAR pokerdefcardsuits[POKERMAXDECK] =
    {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    0,  2
    };*///|

unsigned long slotsbet;
