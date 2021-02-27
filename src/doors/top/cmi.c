/******************************************************************************
CMI.C        Channel management interface (CMI).

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

This module is an API of function calls that do all of the low-level work
involved in managing channels.  The CMI handles such tasks as maintaining the
channel index (CHANIDX.TCH), tracking which users are in each channel, and
moving users from one channel to another.  Although the CMI is not particularly
complicated, it is very delicate and should not be modified until you have a
thorough understanding of how it works.  See CHANNELS.TXT for full details.
CMI return codes are defined in TOP.H and explained in CHANNELS.TXT.
******************************************************************************/

#include "top.h"

/* The entire CMI relies on a couple of global variables.
   cmibuf - Data for the user's CURRENT channel, loaded from CHANIDX.TCH.
   cmirec - Record number inside CHANIDX.TCH that refers to the user's
            CURRENT channel.
*/

/* cmi_join() - Joins a channel.
   Parameters:  chnum - Channel number to join.
   Returns:  CMI return code.
*/
char cmi_join(unsigned long chnum)
    {
    XINT res; /* Result code. */

    /* Load channel data for the new channel. */
    res = cmi_load(chnum);
    if (res == CMI_OKAY)
        {
        /* Make sure user is allowed to join. */
        res = cmi_check();
        if (res == CMI_OKAY)
            {
            /* Add the user to the channel and save. */
            cmi_adduser();
            res = cmi_save();
            }
        else
            {
            /* Resave the channel, which unlocks its data. */
            cmi_save();
            }
        return res;
        }
    /* Only abort if a serious error occurred. */
    if (res != CMIERR_NOTFOUND)
        {
        return res;
        }

    /* If the user is trying to join another user's personal channel and
       the channel data is not found, then the other user has not yet
       entered his or her channel, which means logically that the user
       cannot have been invited.  Sysops are exempt from requiring
       invitations. */
    if (chnum >= 4000000000UL && chnum <= 4000999999UL &&
        (XINT) (chnum - 4000000000UL) != od_control.od_node &&
        user.security < cfg.sysopsec)
        {
        return CMIERR_NOINV;
        }

    /* Create new data for this channel. */
    res = cmi_make(chnum);
    if (res == CMI_OKAY)
        {
        /* Add the user to the channel and save. */
        cmi_adduser();
        res = cmi_save();
        }

    return res;
    }

/* cmi_unjoin() - Unjoins (leaves) a channel.
   Parameters:  chnum - Channel number to unjoin.
   Returns:  CMI return code.
*/
char cmi_unjoin(unsigned long chnum)
    {
    XINT res; /* Result code. */

    /* Load the channel data. */
    res = cmi_load(chnum);
    if (res != CMI_OKAY)
        {
        return res;
        }
    /* Subtract (remove) the user and save. */
    cmi_subuser();
    res = cmi_save();

    return res;
    }

/* cmi_adduser() - Adds one user to the current channel.
   Parameters:  None.
   Returns:  Nothing.
*/
void cmi_adduser(void)
    {

    /* Add one to the current user count. */
    cmibuf.usercount++;

    }

/* cmi_subuser() - Subtracts one user from the current channel.
   Parameters:  None.
   Returns:  Nothing.
*/
void cmi_subuser(void)
    {

    /* Subtract the user. */
    if (--cmibuf.usercount == 0)
        {
        /* If this user was the last, flag the channel as deleted. */
        cmibuf.type = CHAN_DELETED;
        }

    }

/* cmi_load() - Loads channel data from CHANIDX.TCH.
   Parameters:  chnum - Channel number to load data for.
   Returns:  CMI return code.
   Notes:  Always call this function to load channel data.  Never call
           cmi_loadraw() directly.  This function locks the data then retreives
           it.  Upon success, the data is NOT UNLOCKED because it is assumed it
           needs to be modified immediately.  To unlock the data, call
           cmi_save() (preferred) or cmi_unlock().
*/
char cmi_load(unsigned long chnum)
{
    XINT d, res; /* Counter, result code. */

    /* Scan each record in CHANIDX.TCH. */
    for (d = 0; d < filelength(chidxfil) / sizeof(chan_idx_typ); d++)
        {
        /* Lock the data. */
        res = cmi_lock(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        /* Load the raw data into the current channel data buffer. */
        res = cmi_loadraw(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        /* Check for a match and that the channel is not "deleted". */
        if (cmibuf.channum == chnum && cmibuf.type != CHAN_DELETED)
            {
            /* Match found, flag the current record and return okay. */
            cmirec = d;
            return CMI_OKAY;
            }
        /* This wasn't a match so the data is unlocked. */
        res = cmi_unlock(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        }

    return CMIERR_NOTFOUND;
    }

/* cmi_save() - Saves channel data to CHANIDX.TCH.
   Parameters:  None.
   Returns:  CMI return code.
   Notes:  cmirec must be set to the record the data is to be saved to.
           This is typically done with a cmi_load() call.  This function
           will NOT LOCK THE DATA!  Again, it is assumed cmi_load() already
           did this.  NEVER CALL THIS FUNCTION WITHOUT LOCKING THE DATA
           FIRST!
*/
char cmi_save(void)
    {
    XINT res; /* Result code. */

    /* Save the raw data to CHANIDX.TCH. */
    res = cmi_saveraw(cmirec);
    if (res != CMI_OKAY)
        {
        return res;
        }
    /* Unlock the data. */
    res = cmi_unlock(cmirec);
    if (res != CMI_OKAY)
        {
        return res;
        }

    return CMI_OKAY;
    }

/* cmi_loadraw() - Loads raw CMI data from CHANIDX.TCH.
   Parameters:  rec - Record number of the data to load.
   Returns:  CMI error code.
*/
char cmi_loadraw(XINT rec)
    {

    // Should use errchecking here!

    /* Load the raw data. */
    lseek(chidxfil, (long) rec * sizeof(chan_idx_typ), SEEK_SET);
    read(chidxfil, &cmibuf, sizeof(chan_idx_typ));

    return CMI_OKAY;
    }

/* cmi_saveraw() - Saves raw CMI data to CHANIDX.TCH.
   Parameters:  Record number of the data to save.
   Returns:  CMI error code.
*/
char cmi_saveraw(XINT rec)
    {

    // Should use errchecking here!

    /* Save the raw data. */
    lseek(chidxfil, (long) rec * sizeof(chan_idx_typ), SEEK_SET);
    write(chidxfil, &cmibuf, sizeof(chan_idx_typ));

    return CMI_OKAY;
    }

/* cmi_make() - Creates new channel data inside CHANIDX.TOP.
   Parameters:  chnum - Channel number being created.
   Returns:  CMI error code.
   Notes:  Like cmi_load(), the data is NOT UNLOCKED after it is created!
           This function is intended to work just like cmi_load() except
           that it creates the channel too.  In fact, it could have easily
           been called from cmi_load() but it was decided to let the calling
           function do this in case creation of a new channel was
           undesirable.
*/
char cmi_make(unsigned long chnum)
    {
    XINT d, e, res; /* Counter, counter, result code. */
    chan_idx_typ chantmp; /* Temporary data buffer. */

    /* Prepare new data. */
    chantmp.channum = chnum;
    /* Normal channels and conferences are "open". */
    if (chnum <= 3999999999UL || chnum >= 4001000000UL)
        {
        chantmp.type = CHAN_OPEN;
        }
    /* Personal channels are "closed". */
    if (chnum >= 4000000000UL && chnum <= 4000999999UL)
        {
        chantmp.type = CHAN_CLOSED;
        }
    chantmp.usercount = 0;
    chantmp.topic[0] = '\0';
    chantmp.modnode = od_control.od_node;
    /* Insert the forced topic for this channel if one is defined. */
    if ((res = findchannelrec(chnum)) > -1)
        {
        if (chan[res].topic[0] != '\0')
            {
            strcpy(chantmp.topic, chan[res].topic);
            }
        }
    /* Clear all special nodes. */
    for (e = 0; e < MAXCHANSPECNODES; e++)
        {
        chantmp.specnode[e] = -1;
        }

    /* Search for a blank (deleted) channel data record. */
    for (d = 0; d < filelength(chidxfil) / sizeof(chan_idx_typ); d++)
        {
        /* Lock the data. */
        res = cmi_lock(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        /* Load the data. */
        res = cmi_loadraw(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        /* One condition being true should never occur (it should be both
           or neither), but an OR is used in case something screwed up
           somewhere else. */
        if (cmibuf.type == CHAN_DELETED || cmibuf.usercount < 1)
            {
            /* Deleted record has been found.  Flag it. */
            cmirec = d;
            /* Copy the data to the current channel buffer. */
            memcpy(&cmibuf, &chantmp, sizeof(chan_idx_typ));
            /* Save the new data. */
            res = cmi_saveraw(cmirec);
            if (res == CMI_OKAY)
                {
                return CMI_OKAY;
                }
            }
        /* Unlock the channel data. */
        res = cmi_unlock(d);
        if (res != CMI_OKAY)
            {
            return res;
            }
        }

    /* If TOP gets to this point it means there is no blank record free so
       we have to create a new one. */

    /* d will now equal the total number of records in the file, thus the
       number of the new record. */
    cmirec = d;
    /* Copy the data to the current channel buffer. */
    memcpy(&cmibuf, &chantmp, sizeof(chan_idx_typ));
    /* Lock the new location. */
    res = cmi_lock(cmirec);
    if (res != CMI_OKAY)
        {
        return res;
        }
    /* Save the new data. */
    res = cmi_saveraw(cmirec);

    return res;
    }

/* cmi_lock() - Locks channel data in CHANIDX.TOP
   Parameters:  rec - Record number to lock.
   Returns:  CMI error code.
*/
char cmi_lock(XINT rec)
    {
    XINT res; /* Result code. */

    /* Lock the data. */
    res = rec_locking(REC_LOCK, chidxfil, (long) rec * sizeof(chan_idx_typ),
                      sizeof(chan_idx_typ));
    if (res)
        {
        return CMIERR_FILEIO;
        }

    return CMI_OKAY;
    }

/* cmi_unlock() - Unlocks channel data in CHANIDX.TOP
   Parameters:  rec - Record number to unlock.
   Returns:  CMI error code.
*/
char cmi_unlock(XINT rec)
    {
    XINT res; /* Result code. */

    /* Unlock the data. */
    res = rec_locking(REC_UNLOCK, chidxfil, (long) rec * sizeof(chan_idx_typ),
                      sizeof(chan_idx_typ));
    if (res)
        {
        return CMIERR_FILEIO;
        }

    return CMI_OKAY;
    }

/* cmi_check() - Checks if the user can join the current channel.
   Parameters:  None.
   Returns: CMI error code.
   Notes:  The current channel data must be in cmibuf.
*/
char cmi_check(void)
    {
    XINT d; /* Counter. */

    /* No checking needs to be done for sysops or if the current channel
       is the user's personal channel. */
    if (user.security >= cfg.sysopsec ||
        cmibuf.channum ==
            ((unsigned long) od_control.od_node + 4000000000UL))
        {
        return CMI_OKAY;
        }

    /* Iterate through all special nodes. */
    for (d = 0; d < MAXCHANSPECNODES; d++)
        {
        if (cmibuf.type == CHAN_OPEN &&
            cmibuf.specnode[d] == od_control.od_node)
            {
            /* In open channels, if the user exists as a special node it
               means the user has been banned from the channel. */
            return CMIERR_BANNED;
            }
        if (cmibuf.type == CHAN_CLOSED &&
            cmibuf.specnode[d] == od_control.od_node)
            {
            /* In closed channels, if the user exists as a special node it
               means they have been invited and thus may join the channel. */
            return CMI_OKAY;
            }
        }

    /* If we've made it to this point, the user has either not been banned
       or not been invited, depending on the channel type. */
    return (cmibuf.type == CHAN_OPEN ? CMI_OKAY : CMIERR_NOINV);
    }

/* cmi_setspec() - Sets a special node in the channel data.
   Parameters:  node - Node number to set the status for.
                status - TRUE to add the node to the special nodes list,
                         FALSE to remove the node.
   Returns:  CMI error code.
   Notes:  See the discussion of special nodes in CHANNELS.TXT.
*/
char cmi_setspec(XINT node, char status)
    {
    XINT d; /* Counter. */

    if (status)
        {
        /* If the node is to be added to the list, the list is checked to
           be sure the user isn't already on it, to avoid wasting specnode
           slots. */
        for (d = 0; d < MAXCHANSPECNODES; d++)
            {
            if (cmibuf.specnode[d] == node)
                {
                /* The node was found so no changes need to be made. */
                return CMI_OKAY;
                }
            }
        }

    /* Loop through each special node. */
    for (d = 0; d < MAXCHANSPECNODES; d++)
        {
        if (status)
            {
            /* If the node is being added, find the first free slot. */
            if (cmibuf.specnode[d] == -1)
                {
                /* Found a free slot, insert the node into it. */
                cmibuf.specnode[d] = node;
                return CMI_OKAY;
                }
            }
        else
            {
            /* If the node is being removed, find it in the list. */
            if (cmibuf.specnode[d] == node)
                {
                /* Found the node, clear its slot. */
                cmibuf.specnode[d] = -1;
                return CMI_OKAY;
                }
            }
        }

    /* At this point, either the special nodes list was full (if the node
       was to be added) or the node was not found (if the node was being
       deleted) so return an error code. */
    return (status ? CMIERR_FULL : CMIERR_NOTFOUND);
    }

/* cmi_busy() - Engages "busy" mode.
   Parameters:  None.
   Returns:  CMI error code.
   Notes:  This function saves the current channel, then joins the
           designated busy channel (usually 0xFFFFFFFF).  It should be
           called before the user enters an editor or some other involving
           feature (like Sysop Chat).  Manually joining the busy channel
           (i.e. without using this function) is not recommended!
*/
char cmi_busy(void)
    {
    XINT res; /* Result code. */

    /* Unjoin the current channel. */
    res = cmi_unjoin(curchannel);
    if (res != CMI_OKAY)
        {
        return res;
        }
    /* Remember the current channel. */
    cmiprevchan = curchannel;

    /* Join the busy channel. */
    res = cmi_join(BUSYCHANNEL);
    if (res != CMI_OKAY)
        {
        return res;
        }

    /* Update the NODEIDX to show the busy channel. */
    node->channel = BUSYCHANNEL;
    save_node_data(od_control.od_node, node);

    /* Set the new channel to the busy channel. */
    curchannel = BUSYCHANNEL;

    return CMI_OKAY;
    }

/* cmi_unbusy() - Disengages "busy" mode.
   Parameters:  None.
   Returns:  CMI error code.
   Notes:  This function processes any leftover messages on the busy
           channel, then remembers the original channel and joins it.  NEVER
           CALL THIS FUNCTION WITHOUT CALLING cmi_busy() FIRST!  Manually
           unbusying the user is not recommended.
*/
char cmi_unbusy(void)
    {
    XINT res; /* Result code. */

    /* Process leftover messages on the busy channel. */
    process_messages(FALSE);

    /* Unjoin the busychannel. */
    res = cmi_unjoin(BUSYCHANNEL);
    if (res != CMI_OKAY)
        {
        return res;
        }

    /* Restore the original channel. */
    curchannel = cmiprevchan;

    /* Update the NODEIDX with the original channel. */
    node->channel = curchannel;
    save_node_data(od_control.od_node, node);

    /* Rejoin the original channel. */
    res = cmi_join(curchannel);
    if (res != CMI_OKAY)
        {
        return res;
        }

	return CMI_OKAY;
    }
