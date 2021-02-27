/******************************************************************************
NODES.C      Node handling functions.

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

This module handles just about everything related to nodes.  It contains
functions to manage the two node indexes (NODEIDX.TCH and NODEIDX2.TCH) as
well as more user-interactive functions that show who else is in TOP.
******************************************************************************/

#include "top.h"

/* whos_in_pub() - Displays a short summary of people in current channel.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  This function is called when the user hits ENTER by itself, or
           when the user enters a new channel.  Incidentally, this function is
           now misnamed as it only shows people in the current channel.  The
           function name is left over from before TOP had multi-channels.
           The scan_pub() function shows users in all channels.
*/
void whos_in_pub(void)
{
XINT ucount = 0, d; /* Flag if other users are present, counter. */

/* Refresh the index of nodes online. */
check_nodes_used(TRUE);

/* Count the number of nodes in this channel. */
// Should flag then abort this loop when a node is found...
for (d = 0; d < MAXNODES && !ucount; d++)
	{
    if (activenodes[d] == 1 && d != od_control.od_node)
    	{
        ucount = 1;
        }
    }

if (ucount)
    {
    XINT comma = 0; /* Flag for when to display a comma. */

    /* Show header. */
    top_output(OUT_SCREEN, getlang("LookAround"));

    /* Loop for each node. */
    for (d = 0; d < MAXNODES; d++)
    	{
        /* Only show active nodes in our channel, except this node. */
        if (activenodes[d] == 1 && d != od_control.od_node)
        	{
            /* Display the user's handle, followed by either a separator
               (usually a comma) or a prefix, depending on if this is
               the first node to be shown. */
            top_output(OUT_SCREEN, getlang("LookAroundList"),
                       getlang(comma ? "LookAroundSep" : "LookAroundFirst"),
                       handles[d].string);
            /* Flag that a node has been shown so the prefix isn't used
               again. */
            comma = 1;
            }
        }
    }
else
	{
    /* Nobody's here. */
    top_output(OUT_SCREEN, getlang("LookAroundNobody"));
    }
/* Show footer. */
top_output(OUT_SCREEN, getlang("LookAroundAfter"));

return;
}

/* scan_pub() - Displays who is in all of TOP (SCAN command).
   Parameters:  None.
   Returns:  Nothing.
*/
void scan_pub(void)
{
node_idx_typ nodeinfo; /* Node data buffer. */
XINT sd; /* Counter. */
unsigned char chnam[51]; /* Channel name buffer. */

/* Show header. */
top_output(OUT_SCREEN, getlang("ScanHeader"));
top_output(OUT_SCREEN, getlang("ScanHeaderSep"));

/* The active node index (NODEIDX2.TCH) isn't used as a node's active status
   can be determined directly from the node data.  Using it may save some
   time but in my mind it wasn't worth the extra code. */

/* Loop for each node. */
for (sd = 0; sd < MAXNODES; sd++)
	{
    /* If the current node to be displayed cannot possibly be in the
       node index (because the index is too small), break out of the loop. */
    if ((long) sd * (long) sizeof(node_idx_typ) > filelength(nidxfil))
		{
		break;
		}
    /* Clear the data buffer. */
    memset(&nodeinfo, 0, sizeof(node_idx_typ));
    /* If the data was loaded and the handle is not blank, display the
       node. */
    if (!get_node_data(sd, &nodeinfo) && nodeinfo.handle[0])
    	{
        long tcrec; /* Channel definition number. */

        /* Attempt to find a definition for the user's channel. */
        tcrec = findchannelrec(nodeinfo.channel);

        if (!nodeinfo.channellisted || (tcrec > -1 && !chan[tcrec].listed))
            {
            /* Don't display the channel if the user doesn't wish it or if
               the channel is marked unlisted in its definition. */
            strcpy(chnam,
                   top_output(OUT_STRINGNF, getlang("ScanUnlistedChan")));
            }
        else
            {
            /* Display the channel shortname. */
            strcpy(chnam, channelname(nodeinfo.channel));
            }

        /* Effectively 0-length outbuf so the string appends below will
           work properly. */
        outbuf[0] = '\0';
        /* Prepare task information. */
        strcat(outbuf, top_output(OUT_STRINGNF,
                                  getlang((nodeinfo.task & TASK_EDITING) ?
                                  "ScanEditing" : "ScanNotEditing")));
        strcat(outbuf, top_output(OUT_STRINGNF,
                           getlang((nodeinfo.task & TASK_PRIVATECHAT) ?
                           "ScanPrivChat" : "ScanNotPrivChat")));

        /* Display the node and related information. */
        top_output(OUT_SCREEN, getlang("ScanInfo"), nodeinfo.handle,
                   chnam, outbuf);
        }
    }

}

/* register_node() - Initializes the node when logging into TOP.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  Any error is considered fatal and the function will abort instead
           of returning.
*/
void register_node(void)
{
char tstflg; /* Flag for testing the active node index. */
char tmp; /* Byte to write. */
XINT res; /* Result code. */

/* Make sure the node index can hold all nodes. */
chsize(nidx2fil, MAXNODES);

/* Read the active status for this node from NODEIDX2.TCH. */
res = lseek(nidx2fil, od_control.od_node, SEEK_SET);
if (res == -1)
	{
    errorabort(ERR_CANTACCESS, top_output(OUT_STRING, "@1nodeidx2.top",
			   cfg.topworkpath));
    }
rec_locking(REC_LOCK, nidx2fil, od_control.od_node, 1L);
res = read(nidx2fil, &tstflg, 1L);
rec_locking(REC_UNLOCK, nidx2fil, od_control.od_node, 1L);
if (res == -1)
	{
    errorabort(ERR_CANTREAD, top_output(OUT_STRING, "@1nodeidx2.top",
			   cfg.topworkpath));
    }

/* If the flag was set and the node is not solid, initialization cannot
   proceed. */
if (tstflg && !nodecfg->solidnode)
	{
    /* The node is in use and cannot be overrided, so inform the user
       and quit. */
    itoa(od_control.od_node, outnum[0], 10);
    top_output(OUT_SCREEN, getlang("NodeInUse"), outnum[0]);
    top_output(OUT_SCREEN, getlang("NodeInUseContact"));
    od_log_write(top_output(OUT_STRING, getlang("LogNodeInUse"), outnum[0]));
    quit_top();
    }

/* If the node is solid and the flag is set, it is assumed the node
   crashed on its last run, so a message is logged to let the sysop know
   of a potential problem. */
if (tstflg && nodecfg->solidnode)
    {
    od_log_write(top_output(OUT_STRING, getlang("LogPrevCrashDetect")));
    }

/* Superfluous NODEIDX2 size extension. */
if (filelength(nidx2fil) < MAXNODES)
	{
    chsize(nidx2fil, MAXNODES);
    }

/* Write TRUE to this node's active byte in NODEIDX2.  This effectively logs
   the node into TOP, making it visible to other nodes. */
res = lseek(nidx2fil, od_control.od_node, SEEK_SET);
if (res == -1)
	{
    errorabort(ERR_CANTACCESS, top_output(OUT_STRING, "@1nodeidx2.top",
			   cfg.topworkpath));
    }
rec_locking(REC_LOCK, nidx2fil, od_control.od_node, 1L);
tmp = 1;
res = write(nidx2fil, &tmp, 1L);
rec_locking(REC_UNLOCK, nidx2fil, od_control.od_node, 1L);
if (res == -1)
	{
    errorabort(ERR_CANTWRITE, top_output(OUT_STRING, "@1nodeidx2.top",
			   cfg.topworkpath));
    }

/* Update the active node and active handle arrays. */
activenodes[od_control.od_node] = 1;
fixname(handles[od_control.od_node].string, user.handle);

/* Initialize the NODEIDX information from the user file and door kit
   information. */
memset(node, 0, sizeof(node_idx_typ));
fixname(node->handle, user.handle);
fixname(node->realname, user.realname);
node->speed = od_control.baud;
strcpy(node->location, od_control.user_location);
node->gender = user.gender;
node->channel = curchannel;
node->channellisted = (user.pref2 & PREF2_CHANLISTED);
node->security = user.security;

/* Write the node data to the NODEIDX. */
save_node_data(od_control.od_node, node);

/* Perform any BBS-specific node initialization if needed. */
if (cfg.bbstype != BBS_UNKNOWN)
	{
    if (bbs_call_login)
        (*bbs_call_login)();
    }

/* Flag the node as logged in. */
isregistered = TRUE;

if (localmode || lanmode)
	{
    /* Under local and LAN modes, a final display of the status line
       is performed, which will update the name. */
    od_control.od_status_on = TRUE;
    od_kernel();
    od_set_statusline(STATUS_NORMAL);
    od_control.od_status_on = FALSE;
    }

/* Clear this node's change index byte.  This is done to block out any
   old unreceived messages that may have been left over if the node
   crashed. */
tmp = 0;
lseek(chgfil, od_control.od_node, SEEK_SET);
rec_locking(REC_LOCK, chgfil, od_control.od_node, 1L);
write(chgfil, &tmp, 1L);
rec_locking(REC_UNLOCK, chgfil, od_control.od_node, 1L);
chsize(midxinfil, 0); chsize(msginfil, 0);

/* Attempt to join the default channel. */
res = cmi_join(curchannel);
if (res != CMI_OKAY)
    {
    /* If for some reason the default channel cannot be joined, attempt to
       join the user's personal channel.  This should never occur.  If the
       main channel cannot be joined it likely means the CMI is unstable
       (i.e. CHANIDX.TCH has been corrupted. */
    res = cmi_join(4000000000UL + (unsigned long) od_control.od_node);
    if (res != CMI_OKAY)
        {
        /* If that doesn't work it's pretty much a hopeless situation. */
        top_output(OUT_SCREEN, getlang("SevereJoinError"));
        od_log_write(top_output(OUT_STRING, getlang("LogSevereJoinErr")));
        od_exit(230, FALSE);
        }
    else
        {
        /* We're now in the user's personal channel.  This is only a backup
           condition, and as mentioned there is probably a problem with
           the CMI. */
        curchannel = 4000000000UL + (unsigned long) od_control.od_node;
        }
    }

/* Now that we are logged into TOP, time messages from the door kit can
   be processed as if they were TOP messages, which looks nicer. */
od_control.od_time_msg_func = sendtimemsgs;

return;
}

/* unregister_node() - Logs the node out of TOP.
   Parameters:  errabort - Whether or not to handle errors (see notes).
   Returns:  Nothing.
   Notes:  The errabort flag only controls whether the calling function
           cares if an error occurs or not.  If the flag is TRUE, this
           function will abort on error and display a message.  If it is
           FALSE, the function simply returns.  The flag is usually set FALSE
           when deregistering in an error situation.
*/
void unregister_node(char errabort)
{
char tmp; /* Byte to write. */
XINT res; /* Result code. */
long d; /* Counter. */

/* Adjust NODEIDX2.TCH size if needed. */
if (filelength(nidx2fil) < MAXNODES)
	{
    chsize(nidx2fil, MAXNODES);
    }

/* Write FALSE to this node's byte in NODEIDX2.TCH, effectively logging
   the node out of TOP and making it invisible to other nodes. */
res = lseek(nidx2fil, od_control.od_node, SEEK_SET);
if (res == -1)
	{
    if (errabort)
    	{
	    errorabort(ERR_CANTACCESS, top_output(OUT_STRING, "@1nodeidx2.top",
				   cfg.topworkpath));
        }
    return;
    }
rec_locking(REC_LOCK, nidx2fil, od_control.od_node, 1);
tmp = 0;
res = write(nidx2fil, &tmp, 1);
rec_locking(REC_UNLOCK, nidx2fil, od_control.od_node, 1);
if (res == -1)
	{
    if (errabort)
    	{
	    errorabort(ERR_CANTWRITE, top_output(OUT_STRING, "@1nodeidx2.top",
				   cfg.topworkpath));
        }
    return;
    }

/* Perform any BBS-specific node deactivation if needed. */
if ((cfg.bbstype != BBS_UNKNOWN) && node_added)
	{
    if (bbs_call_logout)
        (*bbs_call_logout)();
    node_added = FALSE;
    }

/* Remove this node from the current channel. */
cmi_unjoin(curchannel);

/* This loop iterates through all channel data in CHANIDX.TCH and removes
   this node from all special node lists, so any bans and/or invites won't
   carry over to the next user on this node. */
for (d = 0L; d < (filelength(chidxfil) / sizeof(chan_idx_typ)); d++)
    {
    /* The CMI data is locked and loaded directly (rather than using
       cmi_load() to save a bit of processing time. */
    cmi_lock(d);
    cmi_loadraw(d);
    cmi_setspec(od_control.od_node, 0);
    cmi_saveraw(d);
    cmi_unlock(d);
    }

/* Blank this node's NODEIDX data and write the blank data to disk. */
memset(node, 0, sizeof(node_idx_typ));
save_node_data(od_control.od_node, node);
  
/* Flag the node as logged off and disconnect function handlers that are
   only needed when the node is logged in. */
isregistered = FALSE;
od_control.od_ker_exec = NULL;
od_control.od_time_msg_func = NULL;

return;
}

/* check_nodes_used() - Updates the active node index from NODEIDX2.TCH.
   Parameters:  fatal - Whether or not to abort on error (see notes).
   Returns:  Nothing.
   Notes:  Like unregister_node(), this function's error handling can be
           controlled with the fatal flag.  This flag should be set to TRUE
           if an error is to be handled, and to FALSE if the calling
           function does not care about errors.  Normally this flag is set to
           TRUE when this function is called during initialization or during
           important system routines like message dispatching.  It is normally
           set FALSE when an updated node list is requested before displaying
           information to the user (eg. SCAN command), where inaccurate
           information will only cause minor problems.  The function does not
           return anything for this reason.  The active node index is stored
           in the global activenodes array.
*/
void check_nodes_used(char fatal)
{
XINT res, d; /* Result code, counter. */
node_idx_typ tnbuf; /* Node data buffer. */

/* Read the entire active node index into memory. */
lseek(nidx2fil, 0L, SEEK_SET);
rec_locking(REC_LOCK, nidx2fil, 0L, MAXNODES);
res = read(nidx2fil, activenodes, MAXNODES);
rec_locking(REC_UNLOCK, nidx2fil, 0L, MAXNODES);
if (res == -1)
    {
    if (fatal)
        {
	    errorabort(ERR_CANTREAD, top_output(OUT_STRING, "@1nodeidx2.top",
				   cfg.topworkpath));
        }
    return;
    }

/* The lowest active node is stored but not currently used.  It was
   used by poker, which was removed from TOP. */
lowestnode = -1;
/* This loop goes through each node and modifies the active node flag if
   the node is active but not on our current channel.  This provides
   function with a quick way to check if a node is in our channel or not.
   It also updates the active handles index, which although is not really
   needed anymore (it was left over from an older version of the TOP-to-TOP
   file system) is still maintained for speed in some functions. */
for (d = 0; d < MAXNODES; d++)
	{
    /* Blank out the node's active handle. */
    memset(handles[d].string, 0, 31);
    if (activenodes[d])
        {
        /* If the node is active, get the node data. */
        if (get_node_data(d, &tnbuf) == 0)
            {
            /* If the data was obtained successfully, update the active
               handle. */
            fixname(handles[d].string, tnbuf.handle);
            /* If the user's not on our channel, adjust the flag so this
               condition can be detected easily. */
            if (tnbuf.channel != curchannel)
                {
                activenodes[d] = 2;
                }
            }
        }
    /* Clear the forget status for any node that is no longer active.  This
       probably isn't needed but it keeps things cleaner. */
    if (!activenodes[d])
    	{
        forgetstatus[d] = 0;
        }
    /* Set the lowest node, which again is not currently used. */
    if (activenodes[d] && lowestnode < 0)
    	{
        lowestnode = d;
        }
    }

return;
}

/* get_node_data() - Reads node data from NODEIDX.TCH.
   Parameters:  recnum - Node number to load data for.
                nodebuf - Pointer to node data buffer.
   Returns:  FALSE on success, or an ERR_ code plus one on error.
   Notes:  ERR_ codes are zero based so to provide a proper boolean
           return value, 1 has to be added.
*/
char get_node_data(XINT recnum, node_idx_typ *nodebuf)
{
XINT res; /* Result code. */

/* This function's operation should be straightforward. */

res = lseek(nidxfil, (long) recnum * (long) sizeof(node_idx_typ), SEEK_SET);
if (res == -1)
	{
    return (ERR_CANTACCESS + 1);
    }
rec_locking(REC_LOCK, nidxfil, (long) recnum * (long) sizeof(node_idx_typ),
            sizeof(node_idx_typ));
res = read(nidxfil, nodebuf, sizeof(node_idx_typ));
rec_locking(REC_UNLOCK, nidxfil, (long) recnum * (long) sizeof(node_idx_typ),
            sizeof(node_idx_typ));
if (res == -1)
	{
    return (ERR_CANTREAD + 1);
    }

return 0;
}

/* save_node_data() - Saves node data to NODEIDX.TCH.
   Parameters:  recnum - Node number to save data for.
                nodebuf - Pointer to node data buffer.
   Returns:  Nothing.
   Notes:  Errors are treated as fatal and the function will abort TOP if
           one occurs.
*/
void save_node_data(XINT recnum, node_idx_typ *nodebuf)
{
XINT res; /* Result code. */

/* Set the node data length for future backward compatibility. */
nodebuf->structlength = sizeof(node_idx_typ);

/* The rest of this function's operation should be straightforward. */

res = lseek(nidxfil, (long) recnum * (long) sizeof(node_idx_typ), SEEK_SET);
if (res == -1)
	{
    errorabort(ERR_CANTACCESS, top_output(OUT_STRING, "@1NODEIDX.TOP",
			   cfg.topworkpath));
    }
rec_locking(REC_LOCK, nidxfil, (long) recnum * (long) sizeof(node_idx_typ),
            sizeof(node_idx_typ));
res = write(nidxfil, nodebuf, sizeof(node_idx_typ));
rec_locking(REC_UNLOCK, nidxfil, (long) recnum * (long) sizeof(node_idx_typ),
            sizeof(node_idx_typ));
if (res == -1)
	{
    errorabort(ERR_CANTWRITE, top_output(OUT_STRING, "@1NODEIDX.TOP",
			   cfg.topworkpath));
    }

 return;
}

/* Byte-level functions which were left over from an older TOP-to-TOP file
   system that only used one node index.  They could still be used with
   NODEIDX2.TCH by changing "nidxfil" to "nidx2fil".  Since
   check_nodes_used() is used as a shell and handles I/O to the NODEIDX2
   directly, these functions should not be needed. */
/*char set_idx_byte(unsigned char value, unsigned XINT nodenum)
    {
    XINT res;

    res = rec_locking(REC_LOCK, nidxfil, (long) nodenum, 1L);
    if (res)
        {
        return 0;
        }
    res = lseek(nidxfil, (long) nodenum, SEEK_SET);
    if (res == -1)
        {
        res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
        return 0;
        }
    res = write(nidxfil, &value, 1L);
    if (res == -1)
        {
        res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
        return 0;
        }
    res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
    if (res)
        {
        return 0;
        }

    return 1;
    }

char get_idx_byte(unsigned char *valbuf, unsigned XINT nodenum)
    {
    XINT res;

    res = rec_locking(REC_LOCK, nidxfil, (long) nodenum, 1L);
    if (res)
        {
        return 0;
        }
    res = lseek(nidxfil, (long) nodenum, SEEK_SET);
    if (res == -1)
        {
        res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
        return 0;
        }
    res = read(nidxfil, valbuf, 1L);
    if (res == -1)
        {
        res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
        return 0;
        }
    res = rec_locking(REC_UNLOCK, nidxfil, (long) nodenum, 1L);
    if (res)
        {
        return 0;
        }

    return 1;
    }*/
