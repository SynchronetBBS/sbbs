/******************************************************************************
KERNEL.C     Custom kernel function for TOP.

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

This module contains the custom kernel function for TOP as well as the handler
for time warnings.
******************************************************************************/

#include "top.h"

/* top_kernel() - Custom kernel.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  This function is called by the door kit's own kernel.  The door
           kit's kernel is executed every few seconds and does things like
           subtract the user's time and check for carrier.  TOP hooks a
           custom kernel function to perform some time-sensitive tasks
           specific to TOP.
*/
void top_kernel(void)
{
time_t curtime; /* Current time, in UNIX format. */
XINT kd; /* Counter. */
unsigned char tbyte; /* Temporary byte buffer. */
node_idx_typ tnode; /* Temporary node index data. */
long tcred; /* Temporary RA credit holder. */

/* Get the current time. */
curtime = time(NULL);

/* Support for RA's credit system. */
if (cfg.usecredits != 0)
    {
    /* Execute once a minute. */
    if (curtime >= lastkertime + 60)
        {
        /* Deduct the configured number of credits. */
        tcred = (((curtime - lastkertime) / 60) * (long) abs(cfg.usecredits));
        /* The user is out of time if the value to be deducted exceeds
           the number of credits the user currently has. */
        if (tcred >= od_control.user_credit)
            {
            /* Evict the user. */
            od_control.user_credit = 0;
            top_output(OUT_SCREEN, getlang("OutOfCredits"));
            od_log_write(top_output(OUT_STRING, getlang("LogOutOfCredits")));
            od_exit(9, FALSE);
            }
        /* Decrease the user's credit count. */
        od_control.user_credit -= tcred;
        /* Update the last kernel execution time. */
        lastkertime = curtime;
        }
    }

/* Crash protection.  Only executed while the node is "logged in" to TOP,
   i.e. in the node index. */
if (cfg.crashprotdelay > 0 && isregistered)
    {
    /* Check every 10 seconds.  This is why there is a minimum of 10 on
       the setting for CrashProtDelay. */
    if ((curtime - node->lastaccess) >= (cfg.crashprotdelay - 10))
        {
        /* Keep this node alive by writing the current time to the node
           index. */
        node->lastaccess = time(NULL);
        save_node_data(od_control.od_node, node);

        /* Check each node to make sure it's still alive. */
        // Reading the entire index into a dynamic array of node_idx_typs
        // and then scanning should drastically help the speed here...
        for (kd = 0; kd < MAXNODES; kd++)
            {
            /* Only check if the node is active. */
            if (!get_node_data(kd, &tnode) && tnode.handle[0] &&
                kd != od_control.od_node)
                {
                /* The node is assumed to be dead if the at least
                   CrashProtDelay seconds have elapsed since the node
                   last checked in. */
                if ((curtime - tnode.lastaccess) >= cfg.crashprotdelay)
                    { // Err!!!!!!!!!!
                    /* Delete the node from the index. */
                    lseek(nidx2fil, kd, SEEK_SET);
                    rec_locking(REC_LOCK, nidx2fil, kd, 1);
                    tbyte = 0;
                    write(nidx2fil, &tbyte, 1);
                    rec_locking(REC_UNLOCK, nidx2fil, kd, 1);
                    memset(&tnode, 0, sizeof(node_idx_typ));
                    save_node_data(kd, &tnode);

                    /* Tell all other nodes that we just killed a node
                       with Crash Protection, which will cause the other
                       nodes on this channel to recognize the node's
                       deactivation. */
                    // Should maybe be sent globally...
                    itoa(kd, outnum[0], 10);
                    dispatch_message(MSG_KILLCRASH, outnum[0],
                                     -1, 0, 1);
                    od_log_write(top_output(OUT_STRING,
                                            getlang("LogFoundCrashed"),
                                            outnum[0]));
                    }
                }
            }
        }
    }

/* Check high-level censor warnings. */
if (cfg.censortimerhigh > 0 && censorwarningshigh > 0)
    {
    /* Deduct a warning if enough time has elapsed without an offence. */
    if (curtime - lastcensorwarnhigh > cfg.censortimerhigh)
        {
        censorwarningshigh--;
        lastcensorwarnhigh = time(NULL);
        }
    }
/* Check low-level censor warnings. */
if (cfg.censortimerlow > 0 && censorwarningslow > 0)
    {
    /* Deduct a warning if enough time has elapsed without an offence. */
    if (curtime - lastcensorwarnlow > cfg.censortimerlow)
        {
        censorwarningslow--;
        lastcensorwarnlow = time(NULL);
        }
    }

#ifdef __OS2__
/* Keep us alive if we're using Maximus MCP. */
if (cfg.bbstype == BBS_MAX3 && hpMCP != 0)
    {
    /* Send a ping every minute. */
    if (curtime % 60L == 0L)
        {
        McpSendMsg(hpMCP, PMSG_PING, NULL, 0);
        }
    }
#endif

}

/* sendtimemsgs() - Handles door-kit time warning messages.
   Parameters:  string - Message being sent from the door kit.
   Returns:  Nothing.
   Notes:  Called by the door kit when needed.
*/
void sendtimemsgs(char *instring)
    {
	char string[512];

    /* Strip any extra spaces. */
    trim_string(string, instring, 0);

    /* Time messages are blocked if TOP is in the process_messages()
       function.  Remember that since this function is called by the
       door kit it can be called when any od_ function is called. */
    if (blocktimemsgs)
        {
        /* Just display the message to the screen if blocking is on.  It
           may not look pretty but at least it will get through. */
        top_output(OUT_SCREEN, getlang("TimeMsgForcePrefix"), string);
        return;
        }

    /* Send the message like a normal TOP message, which looks much
       cleaner. */
    dispatch_message(MSG_GENERIC,
                     top_output(OUT_STRINGNF, getlang("TimeMsgPrefix"),
                                string),
                     od_control.od_node, 1, 0);

    }

