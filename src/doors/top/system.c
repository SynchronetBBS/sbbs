/******************************************************************************
SYSTEM.C     Miscellaneous system functions.

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

This module contains miscellaneous technical functions and low-level file I/O
functions.
******************************************************************************/

#include "top.h"

/* calc_days_ago() - Calculate number of days between a date and today.
   Parameters:  lastdate - Pointer to date to be tested.
   Returns:  Number of days since the given date.
   Notes:  Normally used in the logon sequence to tell the user how long
           since their last call.
*/
XINT calc_days_ago(XDATE *lastdate)
{
struct date thisdate; /* Today's date. */
long d1 = 0, d2 = 0; /* Days in first date, days in today's date. */
long daysago = 0; /* Return value. */
XINT d; /* Counter. */

getdate(&thisdate);

/* This function converts each date into the number of days elapsed since
   what is technically 01/01/0001.  (In other words, 01/01/0001 is the
   base date and gets a value of 0).  This provides a numeric basis by which
   to compare the two dates. */

/* This function was written when I was new to C and was unfamiliar with
   UNIX-style time (the time_t type).  Using time_t would no doubt be
   much easier. */

/* Convert each year into days. */
for (d = 1; d < lastdate->da_year; d++)
	{
	d1 += 365;
    /* Adjust for leap year. */
	if (d % 4 == 0)
		{
		d1++;
		}
    }
/* Convert each month into days. */
for (d = 1; d < lastdate->da_mon; d++)
	{
    switch(d)
    	{
        case 1 : d1 += 31; break;
        case 2 :
			d1 += 28;
            /* Adjust for current leap year. */
            if (lastdate->da_year % 4 == 0)
            	{
                d1++;
                }
			break;
        case 3 : d1 += 31; break;
        case 4 : d1 += 30; break;
        case 5 : d1 += 31; break;
        case 6 : d1 += 30; break;
        case 7 : d1 += 31; break;
        case 8 : d1 += 31; break;
        case 9 : d1 += 30; break;
        case 10: d1 += 31; break;
        case 11: d1 += 30; break;
		}
    }
/* Count each day for the current month. */
for (d = 1; d < lastdate->da_day; d++)
	{
    d1++;
    }

/* The same counting procedure above is repeated for today's date. */
for (d = 1; d < thisdate.da_year; d++)
	{
	d2 += 365;
	if (d % 4 == 0)
		{
		d2++;
		}
    }
for (d = 1; d < thisdate.da_mon; d++)
	{
    switch(d)
    	{
        case 1 : d2 += 31; break;
        case 2 :
			d2 += 28;
            if (thisdate.da_year % 4 == 0)
            	{
                d2++;
                }
			break;
        case 3 : d2 += 31; break;
        case 4 : d2 += 30; break;
        case 5 : d2 += 31; break;
        case 6 : d2 += 30; break;
        case 7 : d2 += 31; break;
        case 8 : d2 += 31; break;
        case 9 : d2 += 30; break;
        case 10: d2 += 31; break;
        case 11: d2 += 30; break;
		}
    }
for (d = 1; d < thisdate.da_day; d++)
	{
    d2++;
    }

/* The difference is now a simple subtraction operation. */
daysago = d2 - d1;

return daysago;
}

/* This is the old name processor that was used up to and including TOP
   v2.00g1.  The new one (used in v2.00 and up) appears later.  This old
   processor also used an external scoring function called find_max_score()
   which follows the main processor function below. */
/*XINT find_node_from_name(char *newstring, char *handle, char *oldstring)
{
XINT x, y;
XINT *count = NULL;
XINT cc;
XINT maxs, themax;
char fntt[256]; // Dynam with MAXSTRLEN!

count = calloc(MAXNODES, 2);
if (!count)
    {
    dispatch_message(MSG_GENERIC, top_output(OUT_STRINGNF,
                                             getlang("OutOfMemory")),
                     od_control.od_node, 0, 0);
    od_log_write(top_output(OUT_STRING, getlang("LogOutOfMemory")));
    return -1;
    }
memset(count, 0, MAXNODES * 2);

check_nodes_used(TRUE);

for (x = 0, cc = 1; x < strlen(oldstring) && x < 30 && cc > 0; x++)
	{
	for (y = 0, cc = 0; y < MAXNODES; y++)
    	{
        filter_string(fntt, handles[y].string);
        if (toupper(fntt[x]) == toupper(oldstring[x]) && count[y] == x
			&& activenodes[y])
        	{
            count[y]++;
            cc++;
            }
        }
	}

maxs = find_max_score(count, &themax);

dofree(count);

if (maxs >= 0 && (forgetstatus[maxs] & FGT_HASFORGOTME))
	{
    lastsendtonode = maxs;
    return -3;
    }
if (maxs >= 0)
    {
    if (activenodes[maxs] == 2)
        {
        lastsendtonode = maxs;
        return -4;
        }
    }

if (maxs == -1)
	{
	fixname(handle, oldstring);
    newstring[0] = '\0';
    return -1;
    }

if (maxs == -2)
	{
	fixname(handle, oldstring);
    newstring[0] = '\0';
    return -2;
    }

strncpy(handle, oldstring, themax);
if (themax >= strlen(oldstring))
    {
    newstring[0] = '\0';
    }
else
    {
    strcpy(newstring, &oldstring[themax]);
    }

return maxs;
}

XINT find_max_score(XINT *check, XINT *thescore)
{
XINT dd;
XINT maxmax = 0, maxmaxset, highnode;
char fmtt[256]; // Dynam with MAXSTRLEN!

for (dd = 0, maxmaxset = 0, highnode = -1; dd < MAXNODES; dd++)
	{
    filter_string(fmtt, handles[dd].string);
    if (check[dd] > 0 && check[dd] == strlen(fmtt))
    	{
        memcpy(thescore, &check[dd], 2);
        return dd;
        }

	if (check[dd] == maxmax && maxmaxset)
    	{
        return -2;
        }

    if (check[dd] > maxmax)
    	{
        maxmaxset = 1;
        maxmax = check[dd];
        highnode = dd;
        }
    }

if (maxmax == 0)
	{
    return -1;
    }

memcpy(thescore, &maxmax, 2);

return highnode;
}*/
/* End of old name processor. */

/* show_time() - Displays the current time and time remaining.
   Parameters:  None.
   Returns:  Nothing.
*/
void show_time(void)
{
struct date dat; /* Current date. */
struct time tim; /* Current time. */
clock_t tmpc, mins; /* Time in TOP in ticks, time in TOP in minutes. */

/* In local and LAN modes, adjust the time limit in case it was deducted
   before it was "frozen".  It looks a little better if it is consistent. */
if (localmode || lanmode)
	{
    od_control.user_deducted_time = 0;
    od_control.user_timelimit = 1440;
    }

/* Initialize variables. */
getdate(&dat); gettime(&tim);
tmpc = myclock();
mins = (tmpc / CLK_TCK) / 60L;

/* top_output() needs all numeric values to be strings. */
itoa(dat.da_mon, outnum[0], 10);
itoa(dat.da_day, outnum[1], 10);
itoa(dat.da_year, outnum[2], 10);
itoa(tim.ti_hour, outnum[3], 10);
itoa(tim.ti_min, outnum[4], 10);
itoa(tim.ti_sec, outnum[5], 10);
/* Show the current time and date. */
top_output(OUT_SCREEN, getlang("DateTime"), outnum[0], outnum[1], outnum[2],
           outnum[3], outnum[4], outnum[5]);
/* Show how long the user has been in TOP. */
itoa(mins, outnum[0], 10);
itoa(od_control.user_timelimit, outnum[1], 10);
top_output(OUT_SCREEN, getlang("TimeIn"), outnum[0], outnum[1]);
if (cfg.usecredits > 0)
    {
    /* Show number of credits left if the sysop is using RA credits. */
    ltoa(od_control.user_credit, outnum[0], 10);
    top_output(OUT_SCREEN, getlang("CreditDisplay"), outnum[0]);
    }

return;
}

/* quit_top() - Exits TOP normally, displaying a farewell to the user.
   Parameters:  None.
   Returns:  Nothing.
*/
void quit_top(void)
    {
    XINT qtd, qpnum; /* Unused poker variables. */
//|    poker_game_typ qgame;

/* Unused poker code that folds all hands this node is playing in. */
/*//|    for (qtd = 0; qtd < cfg.pokermaxgames; qtd++)
        {
        if (pokeringame[qtd])
            {
            poker_lockgame(qtd);
            poker_loadgame(qtd, &qgame);
            if (poker_checkturn(POKSTG_ANYTURN, od_control.od_node, &qgame))
                {
                poker_foldplayer(od_control.od_node, qtd, &qgame);
                }

            qpnum = poker_getplayernum(POKPL_WAITING, od_control.od_node,
                                       &qgame);
            qgame.wanttoplay[qpnum] = -1;
            if (--qgame.wtpcount < 1)
                {
                qgame.inuse = 0;
                }
            poker_savegame(qtd, &qgame);
            poker_unlockgame(qtd);
            }
        }*///|

    /* Show closing credits. */
    quit_top_screen();

    /* Adjust RA credit value if needed. */
    fixcredits();

    /* Exit normally. */
    od_log_write(top_output(OUT_STRING, getlang("LogEnded"), ver));
    od_exit(0, FALSE);

    }

/* before_exit() - Cleanup function before TOP is exited.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  Never called directly.  Called by the door kit during an
           od_exit() function call.
*/
void _USERENTRY before_exit(void)
{

/* Show any final messages before we logout. */
process_messages(TRUE);

/* Remove the node from TOP. */
if (isregistered)
	{
    unregister_node(1);
    dispatch_message(MSG_TOSSOUT, "\0", -1, 0, 0);
    }

/* De-init. */
closefiles();
memfree();

return;
}

/* errorabort() - Aborts TOP with an error code.
   Parameters:  code - Code to abort with (ERR_ constant).
                info - Error information, usually a file name.
   Returns:  Nothing.
   Notes:  This function should be called whenever a critical TOP file
           (such as USERS.TOP or NODEIDX.TCH) cannot be accessed.
*/
void errorabort(XINT code, char *info)
{
char logent[256]; /* Log entry buffer. */
XINT d; /* Not used. */

strcpy(outbuf, top_output(OUT_STRINGNF, getlang("FileErrorPrefix")));

/* Select proper message to show based on the error code. */
switch(code)
	{
    case ERR_CANTOPEN:
        strcat(outbuf, top_output(OUT_STRINGNF, getlang("CantOpen"),
               info));
        break;
    case ERR_CANTREAD:
        strcat(outbuf, top_output(OUT_STRINGNF, getlang("CantRead"),
               info));
        break;
    case ERR_CANTWRITE:
        strcat(outbuf, top_output(OUT_STRINGNF, getlang("CantWrite"),
               info));
        break;
    case ERR_CANTACCESS:
        strcat(outbuf, top_output(OUT_STRINGNF, getlang("CantAccess"),
               info));
        break;
    case ERR_CANTCLOSE:
        strcat(outbuf, top_output(OUT_STRINGNF, getlang("CantClose"),
               info));
        break;
	}

/* Log the error code. */
strcpy(logent, getlang("LogAlert"));
strcat(logent, &outbuf[4]);
filter_string(logent, logent);
od_log_write(logent);

/* Inform the user we have to exit. */
strcat(outbuf, getlang("FileErrorMsgPrefix"));
strcat(outbuf, getlang("InformSysop"));
strcat(outbuf, getlang("FileErrorMsgSuffix"));
top_output(OUT_SCREEN, outbuf);

od_exit(220 + code, FALSE);
}

/* closefile() - Closes a file.
   Parameters:  thefil - Pointer to file stream to close.
   Returns:  Error condition from fclose().
*/
XINT closefile(FILE *thefil)
{

/* This function is currently just a wrapper for fclose().  It is provided
   in case some common pre-close procedure needs to be performed. */

return fclose(thefil);
}

/* rec_locking() - Performs record-locking operations.
   Parameters:  op - Operation to perform (REC_ constant).
                hand - File handle to perform locking on.
                ofs - Starting position of data to lock.
                len - Length of data to lock.
   Returns:  TRUE if the operation failed, FALSE on success.
   Notes:  This function is called for _ALL_ record locking done inside
           TOP.  It provides try-counting and prevents an infinite loop
           from occuring.  It also notifies the user of a problem.  It is
           currently impossible for this function to return TRUE as it
           will exit TOP if a problem occurs.
*/
XINT rec_locking(char op, XINT hand, long ofs, long len)
{
/* Attempt counter, return code, flag if attempt limit exceeded,
   keypress holder. */
XINT d, failed, past = 0, key;
clock_t cl; /* Current time (used for interval determination). */

/* Loop until the operation succeeds, counting each attempt. */
for (d = 0, failed = 1; failed; d++)
	{
    /* If an attempt has just failed, wait the configured interval
       before trying again. */
    if (failed == -1)
    	{
        for(cl = myclock();
            ((float) myclock() - (float) cl) / (float) CLK_TCK <
            (float) RETRYINTERVAL;);
        }
    /* Perform the record locking. */
    switch(op)
    	{
        case REC_LOCK: failed = lock(hand, ofs, len); break;
        case REC_UNLOCK: failed = unlock(hand, ofs, len); break;
        }
    /* Once the configured number of tries has elapsed, inform the
       user of a problem. */
    if (d == MAXRETRIES)
	    {
        top_output(OUT_SCREEN, getlang("AccessProblem"));
        top_output(OUT_SCREEN, getlang("AccessPrompt"));
        past = 1;
    	}
    /* If we are passed the try limit, the user can abort back to the
       BBS. */
    if (past)
    	{
        /* Check for a key without waiting. */
        key = od_get_key(FALSE);
        /* ESC aborts to the BBS. */
        if (key == 27)
        	{
            top_output(OUT_SCREEN, getlang("AccessShutDown"));
            od_exit(250, FALSE);
            }
        }
    }

return failed;
}

/* sh_open() - Open a file handle in shared mode.
   Parameters:  nam - File name to open.
                acc - Access flags.
                sh - Sharing flags.
                mod - Mode flags.
   Returns:  File handle, or -1 on failure.
   Notes:  This function is used for _ALL_ non-streamed file opening
           in TOP.  It contains a loop to retry the configured number
           of times.
*/
XINT sh_open(char *nam, XINT acc, XINT sh, XINT mod)
{
XINT d, rethand = -1; /* Try counter, file handle. */

/* Loop until success or until the configured number of tries passes. */
for (d = 0; d < MAXOPENRETRIES && rethand == -1; d++);
    {
	rethand = sopen(nam, acc, sh, mod);
    }

return rethand;
}

/* openfile() - Opens a file stream.
   Parameters:  name - File name to open.
                mode - Mode string.
                errabort - Flag if TOP should abort on error.
   Returns:  Pointer to a file stream for the new file, or NULL on failure.
   Notes:  This is an older function from an early version of TOP that used
           a streamed communication system.  It is still used for some BBS
           interface files.  It contains code to retry and to abort on
           failure.
*/
FILE XFAR *openfile(char *name, char *mode, char errabort)
{
FILE XFAR *tmpfil = NULL; /* File handle holder. */
XINT cl; /* Retry counter. */
unsigned XINT sharebits; /* Sharing mode bits. */

strlwr(mode);
/* TOP used to need a different share mode based on how the file was
   opened. */
/*if (strstr(mode, "r") && !strstr(mode, "+"))
	{
    sharebits = SH_DENYWR;
    }
else
	{*/
	sharebits = SH_DENYRW;
/*    }*/

tmpfil = NULL;
/* Loop until success or until the maximum number of tries elapses. */
for (cl = 0; cl < MAXRETRIES && !tmpfil; cl++)
    {
    tmpfil = _fsopen(name, mode, sharebits);
    }

if (!tmpfil && errabort && errno == EACCES)
	{
    /* Abort on error, if requested. */
    top_output(OUT_SCREEN, getlang("SharingVioError"), name);
    unregister_node(0);
    od_log_write(top_output(OUT_STRING, getlang("LogSharingVioError")));
    od_log_write(top_output(OUT_STRING, getlang("LogSharingVioFile"), name));
    od_exit(251, FALSE);
    }

return tmpfil;
}

/* dofree() - Safely frees previously allocated memory.
   Parameters:  pfree() - Pointer to memory to free.
   Returns:  Nothing.
   Notes:  Used to free _ALL_ memory in TOP safely.  TOP does not always
           use all of its memory buffers so it is possible some will be
           NULL when they are freed.  Borland C does not check for this
           and it can cause problems if it occurs.  This function checks
           for NULL before freeing. */
void dofree(void XFAR *pfree)
{

if (pfree != NULL)
    {
    /* Only free non-NULL buffers. */
    free(pfree);
    pfree = NULL;
    }

return;
}

/* closefiles() - Closes TOP files before exiting.
   Parameters:  None.
   Returns:  Nothing.
*/
void closefiles(void)
{

/* Only major TOP files are closed because Borland C will close all files
   upon exit anyhow.  If the compiler is changed then all files will have
   to be closed here. */

close(userfil); close(nidxfil); close(nidx2fil); close(msginfil);
close(msgoutfil); close(useronfil); close(ipcinfil); close(ipcoutfil);
close(midxinfil); close(midxoutfil); close(chgfil); close(chidxfil);
close(bioidxfil); close(biorespfil);

return;
}

/* memfree() - Frees TOP memory before exiting.
   Parameters:  None.
   Returns:  Nothing.
*/
void memfree(void)
{
XINT mfd; /* Counter. */

/* Only major TOP memory is freed because Borland C will free all memory
   upon exit anyhow.  If the compiler is changed then all memory will need
   to be freed here. */

dofree(word_str); dofree(word_pos); dofree(word_len);
dofree(handles); dofree(activenodes);
dofree(outbuf); dofree(forgetstatus);
dofree(busynodes); dofree(wordret); dofree(node); dofree(outputstr);
dofree(nodecfg); dofree(lastcmd);
dofree(chan); dofree(bioques);

/* Free language items. */
for (mfd = 0; mfd < numlang; mfd++)
   	{
    dofree(langptrs[mfd]);
    }
dofree(langptrs);

return;
}

/* moreprompt() - Pauses between screens, prompting the user to continue.
   Parameters:  None.
   Returns:  Negative number if the user aborted, positive number if the
             user requested nonstop mode, or 0 for normal continuation.
*/
char moreprompt(void)
    {
    char nsmp = 0; /* Return code. */
    XINT key; /* Keypress holder. */

    /* All more prompts default to Yes. */

    top_output(OUT_SCREEN, getlang("MorePrompt"));
    key = od_get_answer(top_output(OUT_STRING, getlang("MoreKeys")));
    top_output(OUT_SCREEN, getlang("MorePromptSuffix"));

    /* The keys are held in the MoreKeys language item. */

    /* Abort (no more). */
    if (key == getlangchar("MoreKeys", 1))
        {
        nsmp = -1;
        top_output(OUT_SCREEN, getlang("MoreAbortSuffix"));
        }
    /* Nonstop mode. */
    if (key == getlangchar("MoreKeys", 2))
        {
        nsmp = 1;
        }

    return nsmp;
    }

/* get_extension() - Determines file extension based on terminal type.
   Parameters:  ename - Base name of file to test.
                ebuf - Buffer to hold extension.
   Returns:  Nothing.
   Notes:  This function gets an extension for an external display file,
           based on the user's terminal type.  Either ".asc", ".ans",
           ".avt", or ".rip" is returned.  This function is used with
           pictorial actions only, which can't be displayed by the door
           kit.  All other external files are shown through the doorkit,
           which determines the type of extension itself.  See TOP.DOC's
           section on External Files for information.  This function needs
           a valid base file name in the ename parameter to properly test
           for nonexistent files.
*/
void get_extension(char *ename, char *ebuf)
    {
    char ework[256]; /* Working file name buffer. */

    /* Test for RIP. */
    strcpy(ework, ename);
    if (od_control.user_rip)
        {
        strcat(ework, ".rip");
        if (!access(ework, 0))
            {
            strcpy(ebuf, ".rip");
            return;
            }
        }

    /* Test for AVATAR. */
    strcpy(ework, ename);
    if (od_control.user_avatar)
        {
        strcat(ework, ".avt");
        if (!access(ework, 0))
            {
            strcpy(ebuf, ".avt");
            return;
            }
        }

    /* Test for ANSI. */
    strcpy(ework, ename);
    if (od_control.user_ansi)
        {
        strcat(ework, ".ans");
        if (!access(ework, 0))
            {
            strcpy(ebuf, ".ans");
            return;
            }
        }

    /* Default to ASCII. */
    strcpy(ebuf, ".asc");

    }

/* myclock() - Midnight clock wrap compensator for DOS.
   Parameters:  None.
   Returns:  Current clock tick value from the clock() function.
   Notes:  This function is a replacement for the clock() function.  It
           corrects a bug which wraps the number of elapsed clock ticks
           since program start if the program runs through midnight.  It
           is not defined for TOP for OS/2 as this bug does not occur
           under OS/2.  (TOP/2 can still operate properly if this
           function is defined for it, but it would add overhead.)  It is
           not clear whether the bug is in DOS or in the Borland runtime
           library.
*/
#ifndef __OS2__
clock_t myclock(void)
    {
    clock_t retclock; /* Clock tick value to return. */

    /* Get the current clock tick value. */
    retclock = clock();

    /* What happens is that after midnight, a day's worth of clock ticks
       (1 573 040) is somehow subtracted from the clock tick value.  These
       just have to be added back to restore the proper clock value. */
    if (retclock < 0)
        {
        retclock += 1573040L;
        }

    return retclock;
    }
#endif

/* fixcredits() - Adjusts number of RA credits upon exit.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  In some cases, RA seems to do its own credit subtracting upon
           return from a door, so the original number of credits needs to be
           restored before exiting or the credits would be subtracted twice.
           This is done if the value of cfg.usecredits is positive.  If it is
           negative, TOP will subtract the credits itself (i.e. the value
           will not be restored).
*/
void fixcredits(void)
    {

    if (cfg.usecredits > 0)
        {
        od_control.user_credit = orignumcreds;
        }

    }

/* find_node_from_name() - Determines a node number based on a name.
   Parameters:  newstring - Buffer to hold the remainder of the string.
                handle - Buffer holding the full handle of the user.
                oldstring - String to test.
   Returns:  A node number (>= 0) if a match was found, or a negative
             number on error:  -1 if no match could be made, -2 if the
             handle is not specific enough, -3 if a match was made but the
             user was forgotten by the user who's name matches, or -4 if
             a match was made but the user who matches is not on our
             current channel.  (-3 and -4 are "recoverable" conditions.)
             See notes.
   Notes:  This function is the TOP "name processor", which attempts to find
           a user currently in TOP based on a full or partial name input
           (passed to it in oldstring).  If at least a partially matching
           name is found, on return handle will contain the text in
           oldstring that matched, and newstring will contain everything
           after the part that was used to make the match (including any
           leading spaces).  If no match can be found, newstring will be
           blank and handle will contain the part of the string that failed
           to match.  If -3 or -4 is returned (for reasons described in the
           return code information above), handle and newstring will contain
           the proper values, and the global variable lastsendtonode will
           contain the node that matched. This allows the caller to either
           report the condition to the user, or ignore the condition if it
           is not relevant.  For example, BAN and INVITE commands must span
           channels so it does not matter if a -4 is returned.  This
           function works by comparing "scores" for each node.  The scoring
           process is described inside the function body.
*/
XINT find_node_from_name(char *newstring, char *handle, char *oldstring)
    {
    /* Node counter, length of current handle, length of oldstring. */
    XINT x, lhan, lold;
    XINT cursco = 0, maxs = 0; /* Current score, best score to date. */
    XINT highnode = -1; /* Current node that has the high score. */
    char tied = 0; /* Flag if there is a tie score. */

    /* All matching is case-insensitive! */

    lold = strlen(oldstring);

    check_nodes_used(TRUE);

    /* Test exact matches for each node. */
    for (x = 0; x < MAXNODES; x++)
        {
        lhan = strlen(handles[x].string);
        /* There can only be an exact match if oldstring is at least as
           long as the entire handle. */
        if (lhan >= lold)
            {
            /* An exact match means the start oldstring contains all of
               the characters of the handle being tested.  It may contain
               more characters after (for text or other purposes). */
            if (activenodes[x] &&
                !strnicmp(oldstring, handles[x].string, lhan))
                {
                /* The longest exact match is saved. */
                if (lhan > maxs)
                    {
                    maxs = lhan;
                    highnode = x;
                    }
                }
            }
        }

    /* Only check for a partial match if there wasn't an exact match above.
       This is done because if for example one user was named "Joe" and
       another was named "Joey", it would be impossible to speak to "Joe"
       because the score for both names would tie and return a -2
       (not-specific enough).  This is one of the shortcomings of the
       old name processor that appears earlier in this module. */
    if (maxs == 0)
        {
        /* Test each node. */
        for (x = 0; x < MAXNODES; x++)
            {
            cursco = 0;
            lhan = strlen(handles[x].string);

            /* The "score" for a node is simply how many consecutive
               characters match the start of oldstring.  For example, if
               oldstring contained "al Hey there!" and there was a user
               inside TOP named "Allan", the "score" for that node
               would be 2. */
            while (toupper(handles[x].string[cursco]) ==
                   toupper(oldstring[cursco]) &&
                   cursco < lhan && cursco < lold)
                {
                cursco++;
                }
            /* Check for a tie. */
            if (maxs > 0 && cursco == maxs)
                {
                tied = 1;
                }
            if (cursco > maxs)
                {
                /* This is now the new "high score". */
                tied = 0;
                maxs = cursco;
                highnode = x;
                }
            }

        /* To guard against inadvertant matching, there must be a
           whitespace character after the matched part of the name, unless
           there is a whitespace character at the end of the match part
           (which usually means the user specified one full word of a
           multi-word name).  For example, if the string contained "Jose"
           and there was only a user named "Joe" that matched, the match
           would be negated so messages for "Jose" would not accidentally
           go to "Joe".  This most commonly occurs when there are two or
           more similarly named users inside TOP and one of them leaves.
           The old name processor, included earlier in this module, does
           not fix this problem and is prone to such unwanted matches. */
        if (maxs > 0)
            {
            if (!iswhite(oldstring[maxs - 1]) && !iswhite(oldstring[maxs]))
                {
                maxs = 0;
                tied = 0;
                }
            }

        /* Tie situation. */
        if (tied)
            {
            memset(handle, 0, 31);
            strncpy(handle, oldstring, 30);
            fixname(handle, handle);
            newstring[0] = '\0';
            return -2;
            }

        /* No match was found. */
        if (maxs == 0)
            {
            memset(handle, 0, 31);
            strncpy(handle, oldstring, 30);
            fixname(handle, handle);
            newstring[0] = '\0';
            return -1;
            }
        }

    /* As mentioned in the notes for this function, the cases below are
       "recoverable" errors. */

    /* A match was found but the user who matches has forgotten this
       user. */
    if (forgetstatus[highnode] & FGT_HASFORGOTME)
        {
        lastsendtonode = highnode;
        return -3;
        }

    /* A match was found but the user who matches is not in our current
       channel. */
    if (activenodes[highnode] == 2)
        {
        lastsendtonode = highnode;
        return -4;
        }

    /* handle only contains the text used to make the match.  It is up to
       the calling function to get the entire handle from the master handle
       index (using the returned node number from this function) if it is
       needed for display purposes. */
    strncpy(handle, oldstring, maxs);

    if (maxs >= strlen(oldstring))
        {
        /* If oldstring only contained a name, newstring will contain
           nothing. */
        newstring[0] = '\0';
        }
    else
        {
        /* Compensate if the user specified a full word of a multi-word
           handle.  TOP considers the space as part of the matched handle
           and, without compensation, would not include it in newstring.
           However, many routines expect the space at the front so the
           compensation puts it back there. */
        if (iswhite(handle[maxs - 1]))
            {
            maxs--;
            }
        /* Newstring contains everything after the text used for the
           match. */
        strcpy(newstring, &oldstring[maxs]);
        }

    return highnode;
    }

/* sh_unlink() - Share-aware file deletion function.
   Parameters:  filname - Name of file to delete.
   Returns:  FALSE on success, TRUE on error.
   Notes:  This function is needed because on some systems, if a program
           tries to delete a file while it is in use, a message will pop
           up on the system which will require a human response before
           the program can continue.  This is obviously undesirable in
           an automatic BBS that may never even have a human on the
           premises where the system is located.
*/
XINT sh_unlink(char *filnam)
    {

    /* Don't delete if we don't have full access to the file. */
    if (access(filnam, 6))
        {
        return -1;
        }

    return (unlink(filnam));
    }

