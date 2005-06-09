/******************************************************************************
MESSAGES.C   Message writing functions.

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

This module contains the functions that handle the writing of messages.  The
reading and processing is done in PROCMSGS.C.
******************************************************************************/

#include "top.h"

/* dispatch_message() - Sends a message to one or more TOP node.
   Parameters:  type - Type of message to send (MSG_ constant).
                string - Text of message to send.
                tonode - Node the message is "done to", or -1 for none.
                priv - Whether or not to send the message privately (i.e.
                       only to the node specified in tonode).
                echo - Whether or not to echo the message to this node.
   Returns:  Nothing.
   Notes:  The contents of some parameters depend on the type of message
           being sent.  See MESSAGES.TXT for a complete message-by-message
           guide.  All messages are sent to all nodes unless the priv flag
           is TRUE, excluding the sender if the echo flag is FALSE.  There
           are globals that handle some additional data that this function
           uses.  See GLOBAL.C.  This function does not return because
           errors are treated as fatal and TOP will be aborted.
*/
void dispatch_message(XINT type, unsigned char *string, XINT tonode,
                      XINT priv, XINT echo)
{
XINT d, low, high; /* Counter, low node to send to, high node to send to. */
msg_typ msgout; /* Message data buffer. */

/* See which nodes are active. */
check_nodes_used(TRUE);

if (tonode < 0 || !priv)
    {
    /* If tonode is -1 or the message isn't private, send to all nodes. */
    low = 0;
    high = MAXNODES - 1;
    }
else
    {
    /* The message is private so only send to one node by making the low
       and high nodes the same. */
    low = high = tonode;
    }

/* Loop for the predetermined node range. */
for (d = low; d <= high; d++)
    {
    /* The message is not sent if the node is active, if the message is
       private and this isn't the right node (although this shouldn't happen
       because the range limits the sending to the one node; the test was
       leftover from a previous version of the function that didn't use a
       range), or if we're sending to this node and echoing is not on. */
    /* I think this test should have parentheses around the priv & node, and
       the echo & node tests, but it still seems to work without them. */
    if (activenodes[d] && (priv || d != od_control.od_node ||
        echo || tonode == od_control.od_node))
        {
        /* All of the message data is either copied from the user data,
           door kit, function parameters, or global variables that were
           created to maintain function calling compatibility. */
        msgout.type = type;
        msgout.from = od_control.od_node;
        msgout.doneto = tonode;
        msgout.gender = user.gender;
        fixname(msgout.handle, user.handle);
        strcpy(msgout.string, string);
        /* The global override causes the message to be sent on channel 0
           (the global channel) instead of the current one. */
        msgout.channel = (msgsendglobal ? 0UL : curchannel);
        msgout.minsec = msgminsec;
        msgout.maxsec = msgmaxsec;
        msgout.data1 = msgextradata;
        /* Write the message to disk. */
        write_message(d, &msgout);
        }
    }

/* Clear the global send flag.  The other global variables generally must
   be set if they are used, so they do not need to be cleared. */
msgsendglobal = 0;

return;
}

/* write_message() - Writes message data to disk.
   Parameters:  outnode - Node number to write the message to.
                msgbuf - Pointer to message data buffer.
   Returns:  Nothing.
   Notes:  As with dispatch_message(), all errors are treated as fatal so
           there is no need to return anything.  For a full explanation of
           how the messaging system works on a technical level, see
           MESSAGES.TXT.
*/
void write_message(XINT outnode, msg_typ *msgbuf)
{
char filnam[256], tstbit[2]; /* File name buffer, byte test buffer. */
XINT res, d; /* Result code, counter. */
XINT mrecnum = -1; /* Message record number to use. */

/* The structlength is not used but may be one day to allow for backward
   compatibility. */
msgbuf->structlength = sizeof(msg_typ);

/* Open the message index file for the receiving node. */
sprintf(filnam, "%smix%05i.tch", cfg.topworkpath, outnode);
midxoutfil = sh_open(filnam, O_RDWR | O_CREAT | O_BINARY , SH_DENYNO,
				     S_IREAD | S_IWRITE);
if (midxoutfil == -1)
	{
    errorabort(ERR_CANTOPEN, filnam);
    }

/* Loop through the message index looking for a free record. */
for (d = 0; d < filelength(midxoutfil) && mrecnum == -1; d++)
	{
    /* Read in the byte for this record index. */
    res = lseek(midxoutfil, (long) d, SEEK_SET);
    if (res == -1)
    	{
        continue;
        }
    rec_locking(REC_LOCK, midxoutfil, d, 1L);
    res = read(midxoutfil, tstbit, 1L);
    if (!tstbit[0] && res != -1)
    	{
        /* If the test byte was false and there were no errors, the
           record is available for us to use. */
        mrecnum = d;
		res = lseek(midxoutfil, (long) d, SEEK_SET);
        }
    else
    	{
	    rec_locking(REC_UNLOCK, midxoutfil, d, 1L);
        }
    }

if (mrecnum == -1)
	{
    /* If no free record was found, a new one is created. */
    mrecnum = filelength(midxoutfil);
	res = lseek(midxoutfil, (long) mrecnum, SEEK_SET);
    rec_locking(REC_LOCK, midxoutfil, mrecnum, 1L);
    }

/* Write TRUE to the index for the record being used to show we are using
   it. */
tstbit[0] = 1;
res = write(midxoutfil, tstbit, 1L);
if (res == -1)
	{
	rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
    close(midxoutfil);
    errorabort(ERR_CANTWRITE, filnam);
    }

/* Open the message data file for the receiving node. */
sprintf(filnam, "%smsg%05i.tch", cfg.topworkpath, outnode);
msgoutfil = sh_open(filnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
				  S_IREAD | S_IWRITE);
if (msgoutfil == -1)
	{
	rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
    close(midxoutfil);
    errorabort(ERR_CANTOPEN, filnam);
    }

/* Seek to the predetermined record number and write the message data. */
res = lseek(msgoutfil, (long) mrecnum * (long) sizeof(msg_typ), SEEK_SET);
if (res == -1)
	{
	rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
    close(msgoutfil);
    close(midxoutfil);
    errorabort(ERR_CANTACCESS, filnam);
    }
rec_locking(REC_LOCK, msgoutfil, (long) mrecnum * (long) sizeof(msg_typ),
			(long) sizeof(msg_typ));
res = write(msgoutfil, msgbuf, sizeof(msg_typ));
rec_locking(REC_UNLOCK, msgoutfil, (long) mrecnum * (long) sizeof(msg_typ),
			(long) sizeof(msg_typ));
if (res == -1)
	{
	rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
    close(msgoutfil);
    close(midxoutfil);
    errorabort(ERR_CANTWRITE, filnam);
    }

/* Close the files. */
res = close(msgoutfil);
if (res == -1)
	{
	rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
    close(msgoutfil);
    close(midxoutfil);
    errorabort(ERR_CANTCLOSE, filnam);
    }
rec_locking(REC_UNLOCK, midxoutfil, mrecnum, 1L);
res = close(midxoutfil);
if (res == -1)
	{
    close(midxoutfil);
    errorabort(ERR_CANTCLOSE, filnam);
	}

/* Write TRUE to the change index for this node so it knows there is a new
   message. */
tstbit[0] = 1;
// Not sure if these need errorchecking...
lseek(chgfil, outnode, SEEK_SET);
rec_locking(REC_LOCK, chgfil, outnode, 1L);
write(chgfil, tstbit, 1L);
rec_locking(REC_UNLOCK, chgfil, outnode, 1L);

return;
}

