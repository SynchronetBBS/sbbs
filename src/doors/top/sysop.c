/******************************************************************************
SYSOP.C      SYSOP command processor.

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

This module processes all SYSOP commands.
******************************************************************************/

#include "top.h"

/* Command constants used internally by the sysop_proc() function.  The
   text after SYSOP_ corresponds to the command name. */
#define SYSOP_CLEAR       0     /* Clears a stuck node. */
#define SYSOP_DELETE      1     /* Reserved. */
#define SYSOP_LINK        2     /* Reserved. */
#define SYSOP_MAINT       3     /* Reserved. */
#define SYSOP_TOSS        4     /* Toss a user out of TOP, back to the BBS. */
#define SYSOP_UEDIT       5     /* Reserved. */
#define SYSOP_ZAP         6     /* Hang up a user. */
#define SYSOP_CDGIVE      7     /* Reserved. */
#define SYSOP_CDTAKE      8     /* Reserved. */
#define SYSOP_CDSET       9     /* Reserved. */
#define SYSOP_EDITUSER   10     /* Reserved. */
#define SYSOP_SETSEC     11     /* Change a user's security level. */

/* sysop_proc() - SYSOP command processor. */
void sysop_proc(void)
{
/* Command was used flag, command (SYSOP_ constant), flag if there is a node
   to do the command to. */
char sused = 0, scom = -1, yessend = 1;

/* This module uses a different approach than other command procesors,
   because most SYSOP commands are very similar.  It first determines the
   command, setting a SYSOP_ constant if one is found.  Then a common
   processor acts. */

if (checkcmdmatch(get_word(1), getlang("CmdsSysopToss")) > 0)
	{
    scom = MSG_TOSSED;
    }
if (checkcmdmatch(get_word(1), getlang("CmdsSysopZap")) > 0)
	{
    scom = MSG_ZAPPED;
    }
if (checkcmdmatch(get_word(1), getlang("CmdsSysopClear")) > 0)
	{
    scom = MSG_CLEARNODE;
    yessend = 0;
    }
/* CDxxxx commands are not used.  They are intended for use with games. */
/*if (checkcmdmatch(get_word(1), getlang("CmdsSysopCDGive")) > 0)
	{
    scom = MSG_SYSGIVECD;
    }
if (checkcmdmatch(get_word(1), getlang("CmdsSysopCDTake")) > 0)
	{
    scom = MSG_SYSTAKECD;
    }
if (checkcmdmatch(get_word(1), getlang("CmdsSysopCDSet")) > 0)
	{
    scom = MSG_SYSSETCD;
    }*/
if (checkcmdmatch(get_word(1), getlang("CmdsSysopSetSec")) > 0)
	{
    scom = MSG_SYSSETSEC;
    }

if (scom != -1)
	{
    XINT sendto; /* Node to send to. */
    /* Temporary text buffer, temporary "send to" handle buffer. */
    unsigned char tmpstr[256], tmphand[256];

    sused = 1;

    if (yessend)
    	{
        /* Find the node from a name if the command requires one. */
        sendto = find_node_from_name(tmpstr, tmphand,
                                     &word_str[word_pos[2]]);
        }
    else
    	{
        /* Command does not require a name. */
        sendto = 0;
        }

    if (sendto == -1)
    	{
        top_output(OUT_SCREEN, getlang("NotLoggedIn"), tmphand);
        }
    if (sendto == -2)
	   	{
        top_output(OUT_SCREEN, getlang("NotSpecific"), tmphand);
        }
    /* -3 (HasForgotYou) is not handled because sysops cannot be forgotten. */
    /* Sysop commands are channel-independent. */
    if (sendto == -4)
	   	{
        sendto = lastsendtonode;
        }
    if (sendto >= 0)
    	{
        unsigned long cdc; /* Temporary value holder. */

        /* find_node_from_name() returns an untrimmed string, which
           includes the space that follows the name of the user.  For this
           reason, if tmpstr has nothing in it, we need to make sure the
           second character is a \0 as well, because the processor starts
           looking at the second character. */
        if (tmpstr[0] == '\0')
            {
            tmpstr[1] = '\0';
            }

        /* Get the value of the rest of the string, not including the
           leading space mentioned above. */
        cdc = strtoul(&tmpstr[1], NULL, 10);

        /* SYSOP TOSS command. */
        if (scom == MSG_TOSSED)
            {
            msgsendglobal = 1;
	        dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            top_output(OUT_SCREEN, getlang("HasBeenTossed"),
                       handles[sendto].string);
            od_log_write(top_output(OUT_STRING, getlang("LogTossed"),
                                    handles[sendto].string));
            }
        /* SYSOP ZAP command. */
        if (scom == MSG_ZAPPED)
            {
            msgsendglobal = 1;
	        dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            top_output(OUT_SCREEN, getlang("HasBeenZapped"),
                       handles[sendto].string);
            od_log_write(top_output(OUT_STRING, getlang("LogZapped"),
                                    handles[sendto].string));
            }
        /* SYSOP CLEAR command. */
        if (scom == MSG_CLEARNODE)
        	{
            char tmpbit = 0; /* Temporary NODEIDX2 byte. */
            XINT clnod; /* Node to clear. */

            strcpy(tmpstr, &word_str[word_pos[2]]);

            clnod = atoi(tmpstr);
            if (clnod < 1 || clnod > MAXNODES)
            	{
                itoa(MAXNODES, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("InvalidNode"), outnum[0]);
                }
            else
            	{
                /* Clear the "receiving" node's NODEIDX2 byte, which should
                   make the node invisible. */
                lseek(nidx2fil, (long) clnod, SEEK_SET);
                rec_locking(REC_LOCK, nidx2fil, clnod, 1L);
    	        write(nidx2fil, &tmpbit, 1L);
                rec_locking(REC_UNLOCK, nidx2fil, clnod, 1L);
                msgsendglobal = 1;
                dispatch_message(MSG_CLEARNODE, tmpstr, -1, 0, 1);
				// Clear the who's online file!
                itoa(clnod, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("NodeCleared"), outnum[0]);
                od_log_write(top_output(OUT_STRING, getlang("LogClearedNode"),
                                        outnum[0]));
                }
        	}
        /* The remaining commands use numeric values. */
        ultoa(cdc, outnum[0], 10);
/* Unused CyberCash commands. */
/*        if (scom == MSG_SYSGIVECD)
        	{
            top_output(OUT_SCREEN, getlang("CDHasBeenGiven"), outnum[0],
                       getlang (cdc == 1 ? "SingularHas" : "PluralHave"),
                       handles[sendto].string);
            dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            }
        if (scom == MSG_SYSTAKECD)
        	{
            top_output(OUT_SCREEN, getlang("CDHasBeenTaken"), outnum[0],
                       getlang (cdc == 1 ? "SingularHas" : "PluralHave"),
                       handles[sendto].string);
            dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            }
        if (scom == MSG_SYSSETCD)
        	{
            top_output(OUT_SCREEN, getlang("CDHasBeenSetAt"),
                       handles[sendto].string, outnum[0]);
            dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            }*/
        /* SYSOP SETSEC command. */
        if (scom == MSG_SYSSETSEC)
            {
            top_output(OUT_SCREEN, getlang("SecHasBeenSetAt"),
                       handles[sendto].string, outnum[0]);
            msgsendglobal = 1;
            dispatch_message(scom, &tmpstr[1], sendto, 1, 0);
            }

        }
    }

/* There was an online user editor at one point, but it was never finished. */
/*if (checkcmdmatch(get_word(1), getlang("CmdsSysopEditUser")))
	{
    // Check ANSI bigtime!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    sused = 1;
    od_disp_emu("\x1B[s", TRUE);  // will screw up screen in AVT mode
    usereditor();
    od_set_attrib(0x07);
    od_disp_emu("\x1B[u\r\n", TRUE);
    }*/

if (!sused) ///!!!!!!!!
	{
    /* Command not recognized, show a help file instead. */
    show_helpfile("helpsys0");
    }

return;
}
