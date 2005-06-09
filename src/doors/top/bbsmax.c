/******************************************************************************
BBSMAX.C     Implements Maximus-specific BBS functions.

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

The module contains all of the functions for interfacing with Maximus v2.xx and
v3.xx, excluding Maximus for OS/2 v3.0x.
******************************************************************************/

#include "top.h"

/* Blinking test used when displaying MECCA strings. */
#define blinking (blink ? 0x80 : 0x00)

/* max_init() - Initialize pointers to Maximus functions.
   Paremeters:  None.
   Returns:  Nothing.
*/
void max_init(void)
{

bbs_call_loaduseron = max_loadipcstatus;
bbs_call_saveuseron = max_saveipcstatus;
bbs_call_processmsgs = max_processipcmsgs;
bbs_call_page = max_page;
bbs_call_setexistbits = max_setexistbits;
bbs_call_login = max_login;
bbs_call_logout = max_logout;
bbs_call_openfiles = max_openfiles;
bbs_call_updateopenfiles = max_openfiles;
bbs_call_pageedit = max_shortpageeditor;

}

/* max_loadipcstatus() - Loads a node status from an IPC file.
   Parameters:  nodenum - Node number to load the data for.
                userdata - Pointer to generic BBS data structure to fill.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char max_loadipcstatus(XINT nodenum, bbsnodedata_typ *userdata)
{
char risnam[MAX_PATH]; /* IPC file name. */
XINT res; /* Result code. */
struct _cstat statbuf; /* Maximus node status buffer. */

/* Open the IPC file and read the status information. */
sprintf(risnam, "%sipc%02X.bbs", cfg.bbsmultipath, nodenum);
ipcoutfil = sh_open(risnam, O_RDONLY | O_BINARY, SH_DENYNO, S_IREAD | S_IWRITE);
if (ipcoutfil == -1)
	{
    return 1;
    }
lseek(ipcoutfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcoutfil, 0, sizeof(struct _cstat));
res = read(ipcoutfil, &statbuf, sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcoutfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    close(ipcoutfil);
    return 1;
    }
close(ipcoutfil);

/* Copy the status information to the generic structure. */
userdata->quiet = !statbuf.avail;
fixname(userdata->realname, statbuf.username);
fixname(userdata->handle, statbuf.username);
strcpy(userdata->statdesc, statbuf.status);
userdata->node = nodenum;

return 0;
}

/* max_saveipcstatus() - Saves a node status to an IPC file.
   Parameters:  nodenum - Node number to save the data for.
                userdata - Pointer to generic BBS data structure to use.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char max_saveipcstatus(XINT nodenum, bbsnodedata_typ *userdata)
{
char winam[MAX_PATH]; /* IPC file name. */
XINT res; /* Result code. */
struct _cstat nodeinf; /* Maximus node status buffer. */
char sist = 1; /* Flag indicating that there are messages in the IPC file. */

/* Transfer the data from the generic BBS structure. */
nodeinf.avail = !userdata->quiet;
strcpy(nodeinf.username,
       cfg.usehandles ? userdata->handle : userdata->realname);
strcpy(nodeinf.status, userdata->statdesc);

/* Open the file, creating it if it doesn't exist. */
sprintf(winam, "%sipc%02X.bbs", cfg.bbsmultipath, nodenum);
ipcoutfil = sh_open(winam, O_WRONLY | O_CREAT, SH_DENYNO, S_IREAD | S_IWRITE);
if (ipcoutfil == -1)
	{
    return 1;
    }

/* If TOP created the IPC file, initialize the message data. */
if (filelength(ipcoutfil) < sizeof(struct _cstat))
    {
    nodeinf.msgs_waiting = 0;
    nodeinf.next_msgofs = sizeof(struct _cstat);
    nodeinf.new_msgofs = sizeof(struct _cstat);
    chsize(ipcoutfil, sizeof(struct _cstat));
    sist = 0;
    }

/* Save the node status information. */
lseek(ipcoutfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcoutfil, 0, sizeof(struct _cstat));
/* If sist is 1 then there may be messages in the file that have not been
   sent, so the message data is not disturbed. */
res = write(ipcoutfil, &nodeinf,
            sist ? (sizeof(struct _cstat) - 10) : sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcoutfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    close(ipcoutfil);
    return 1;
    }

close(ipcoutfil);

return 0;
}

/* max_processipcmsgs() - Reads and displays messages from IPC file.
   Parameters:  None.
   Returns:  Number of messages processed.  (0 on error.)
*/
XINT max_processipcmsgs(void)
{
unsigned XINT mc = 0, mcc = 0; /* Loop counter, processing counter. */
XINT res, d; /* Result code, counter. */
unsigned long bc; /* File position tracker. */
struct file_stats_str ipctmp; /* File information buffer. */
char pinam[256]; /* IPC file name. */
char XFAR *msgdat = NULL; /* Buffer to hold incoming messages. */
struct _cstat statinf; /* Maximus node status buffer. */
struct _cdat msginf; /* Maximus message information buffer. */

/* Before the file is opened, the file information is read.  If the date,
   time, and size of the file have not changed, it is assumed there is
   nothing new in the file.  This is easier on the processor and drive. */
sprintf(pinam, "%sipc%02X.bbs", cfg.bbsmultipath, od_control.od_node);
ipctmp.fdate=fdate(pinam);
ipctmp.fsize=flength(pinam);
if (ipctmp.fdate == ipcfildat.fdate &&
    ipctmp.fsize == ipcfildat.fsize)
	{
    return 0;
    }

/* Copy the new file information for use next time. */
memcpy(&ipcfildat, &ipctmp, sizeof(struct file_stats_str));

/* Read the node status information to see how many messages there are. */
lseek(ipcinfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcinfil, 0, sizeof(struct _cstat));
res = read(ipcinfil, &statinf, sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcinfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    return 0;
    }
if (statinf.msgs_waiting == 0)
	{
    return 0;
    }

/* Process all incoming messages. */
bc = statinf.next_msgofs;
for (mc = 0; mc < statinf.msgs_waiting; mc++)
	{
    /* Prepare the screen if this is the first message. */
    if (mc == 0)
        {
        delprompt(TRUE);

        if (user.pref1 & PREF1_DUALWINDOW)
            {
            // Different for AVT when I find out what the store/recv codes are.
            od_disp_emu("\x1B" "[u", TRUE);
            top_output(OUT_SCREEN, getlang("DWOutputPrefix"));
            }
        }

    /* Read the data for the next message. */
    res = lseek(ipcinfil, bc, SEEK_SET);
    if (res == -1)
    	{
        return mcc;
        }
    rec_locking(REC_LOCK, ipcinfil, bc, sizeof(struct _cdat));
    res = read(ipcinfil, &msginf, sizeof(struct _cdat));
   	rec_locking(REC_UNLOCK, ipcinfil, bc, sizeof(struct _cdat));
    if (res == -1)
    	{
        return mcc;
        }

    /* Advance the file pointer to the location of the message text. */
    bc += (long) sizeof(struct _cdat);

    /* Allocate a buffer for the message text. */
    msgdat = malloc(msginf.len);
    if (!msgdat)
    	{
        return mcc;
        }

    /* Read the message text. */
    res = lseek(ipcinfil, bc, SEEK_SET);
    if (res == -1)
    	{
        dofree(msgdat);
        return mcc;
        }
    rec_locking(REC_LOCK, ipcinfil, bc, msginf.len);
    res = read(ipcinfil, msgdat, msginf.len);
    rec_locking(REC_UNLOCK, ipcinfil, bc, msginf.len);
    if (res == -1)
    	{
        dofree(msgdat);
        return mcc;
        }

    /* Process the message based on type.  All unrecognized messages (i.e.
       anything but a page) are ignored. */
    switch(msginf.type)
    	{
        case CMSG_HEY_DUDE:
            max_showmeccastring(msgdat);
            mcc++;
            break;
        }

    /* The next message is located after the text for this one. */
    bc += (long) msginf.len;

    dofree(msgdat);
    }

/* Clear the message data now that they've all been processed. */
statinf.msgs_waiting = 0;
statinf.next_msgofs = sizeof(struct _cstat);
statinf.new_msgofs = sizeof(struct _cstat);

/* Write the cleared message data to the file. */
lseek(ipcinfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcinfil, 0, sizeof(struct _cstat));
res = write(ipcinfil, &statinf, sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcinfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    return mcc;
	}

/* Update the file information again since it's now changed. */
ipctmp.fdate=fdate(pinam);
ipctmp.fsize=flength(pinam);
memcpy(&ipcfildat, &ipctmp, sizeof(struct file_stats_str));

return mcc;
}

/* max_page() - Sends a page to a Maximus node.
   Parameters:  nodenum - Node number to page.
                pagebuf - Text to send.
   Returns:  TRUE if successful, FALSE if an error occurred.
   Notes:  pagebuf should contain the text to send, and it must also be big
           enough to have the page header and footer appended.
*/
char max_page(XINT nodenum, unsigned char *pagebuf)
{
unsigned char tmp[256], rpt[256]; /* Output buffer, filtered name buffer. */
XINT res; /* Result code. */
struct _cdat tmpmsg; /* Maximus message information buffer. */

/* rpt, which holds the user's name filtered of colour codes, is not actually
   used.  I believe it got overlooked when I added configurable
   handle/real-name selection. */
// filter_string(rpt, od_control.user_name);
/* Prepare the page header. */
itoa(od_control.od_node, outnum[0], 10);
strcpy(tmp, top_output(OUT_STRINGNF, getlang("MaxPageHeader"),
                       cfg.usehandles ? user.handle : user.realname,
                       outnum[0]));
/* Inserts the page header in front of the page text. */
memmove(&pagebuf[strlen(tmp)], pagebuf, strlen(tmp));
memcpy(pagebuf, tmp, strlen(tmp));
/* Append the page footer. */
strcat(pagebuf, top_output(OUT_STRINGNF, getlang("MaxPageFooter")));

/* Set the message data. */
tmpmsg.tid = od_control.od_node;
tmpmsg.type = CMSG_HEY_DUDE;
tmpmsg.len = strlen(pagebuf) + 1;

/* Write the message to the IPC file. */
itoa(nodenum, outnum[0], 10);
res = max_writeipcmsg(nodenum, &tmpmsg, pagebuf);
if (!res)
	{
    top_output(OUT_SCREEN, getlang("CantPage"), outnum[0]);
    return 0;
    }
else
	{
    top_output(OUT_SCREEN, getlang("Paged"), outnum[0]);
    }

return 1;
}

/* max_writeipcmsg() - Writes a new message to an IPC file.
   Parameters:  nodenum - Node number to receive the message.
                msginf - Pointer to Maximus message information.
                message - String containing the message text.
   Returns:  TRUE on success, FALSE on failure.
*/
char max_writeipcmsg(XINT nodenum, struct _cdat *msginf, char *message)
{
XINT res; /* Result code. */
char wimpath[256]; /* IPC file name. */
struct _cstat statinf; /* Maximus node status buffer. */

/* Open the IPC file.  The file will not be created if it doesn't exist.
   This should never be a problem under normal conditions. */
sprintf(wimpath, "%sipc%02X.bbs", cfg.bbsmultipath, nodenum);
ipcoutfil = sh_open(wimpath, O_RDWR | O_BINARY, SH_DENYNO, S_IREAD | S_IWRITE);
if (ipcoutfil == -1)
	{
    return 0;
    }

/* Read the node status information to see where to put the next message. */
lseek(ipcoutfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcoutfil, 0, sizeof(struct _cstat));
res = read(ipcoutfil, &statinf, sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcoutfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }

/* Maximus includes the null terminator in its length calculations. */
msginf->len = strlen(message) + 1;

/* Extend the file size if it is too small.  This is done to make the record
   locking safer. */
if (filelength(ipcoutfil) < (long) statinf.new_msgofs +
    (long) sizeof(struct _cdat) + (long) msginf->len)
    {
    chsize(ipcoutfil, (long) statinf.new_msgofs +
           (long) sizeof(struct _cdat) + (long) msginf->len);
    }

/* Write the message information. */
res = lseek(ipcoutfil, statinf.new_msgofs, SEEK_SET);
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }
rec_locking(REC_LOCK, ipcoutfil, statinf.new_msgofs, sizeof(struct _cdat));
res = write(ipcoutfil, msginf, sizeof(struct _cdat));
rec_locking(REC_UNLOCK, ipcoutfil, statinf.new_msgofs, sizeof(struct _cdat));
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }

/* Write the message itself. */
res = lseek(ipcoutfil, statinf.new_msgofs + (long) sizeof(struct _cdat),
		    SEEK_SET);
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }
rec_locking(REC_LOCK, ipcoutfil, statinf.new_msgofs +
            (long) sizeof(struct _cdat), (long) msginf->len);
res = write(ipcoutfil, message, (long) msginf->len);
rec_locking(REC_UNLOCK, ipcoutfil, statinf.new_msgofs +
	    (long) sizeof(struct _cdat), (long) msginf->len);
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }

/* Update the message data. */
statinf.msgs_waiting++;
statinf.next_msgofs = (long) sizeof(struct _cstat);
statinf.new_msgofs += ((long) sizeof(struct _cdat) + (long) msginf->len);

/* Write the updated data. */
lseek(ipcoutfil, 0, SEEK_SET);
rec_locking(REC_LOCK, ipcoutfil, 0, sizeof(struct _cstat));
res = write(ipcoutfil, &statinf, sizeof(struct _cstat));
rec_locking(REC_UNLOCK, ipcoutfil, 0, sizeof(struct _cstat));
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
	}

res = close(ipcoutfil);
if (res == -1)
	{
    close(ipcoutfil);
    return 0;
    }

return 1;
}

/* max_setexistbits() - Selects the node status fields to display.
   Parameters:  userdata - Pointer to generic BBS data structure to set the
                           bits in.
   Returns:  Nothing.
*/
void max_setexistbits(bbsnodedata_typ *userdata)
{

userdata->existbits = NEX_HANDLE |
				      NEX_REALNAME |
                      NEX_NODE |
                      NEX_STATUS |
                      NEX_PAGESTAT;

}

/* max_login() - Maximus initialization when TOP is started.
   Parameters:  None.
   Returns:  Nothing.
*/
void max_login(void)
{
/* Generic BBS data buffer, login check BBS data buffer. */
bbsnodedata_typ maxtemp, logintmp;
XINT mres; /* Result codes. */

/* Check if the node is already logged in Maximus. */
mres = (*bbs_call_loaduseron)(od_control.od_node, &logintmp);
if (!mres)
{
    /* Set the do not disturb flag. */
    node->quiet = maxtemp.quiet = logintmp.quiet;
    save_node_data(od_control.od_node, node);
}


/* Copy the user information into the generic BBS data buffer. */
fixname(maxtemp.handle, user.handle);
fixname(maxtemp.realname, user.realname);
strcpy(maxtemp.statdesc, getlang("NodeStatus"));
maxtemp.node = od_control.od_node;
maxtemp.speed = od_control.baud;

/* Update the IPC file to show the user is now in TOP. */
mres = (*bbs_call_saveuseron)(od_control.od_node, &maxtemp);
if (!mres)
	{
    node_added = TRUE;
    }

}

/* max_logout() - Maximus deinitialization when TOP is exited.
   Parameters:  None.
   Returns:  Nothing.
*/
void max_logout(void)
{

/* Nothing is done upon logout at this time. */

}

/* max_openfiles() - Opens Maximus-related files.
   Parameters:  None.
   Returns:  Number of errors that occurred, or 0 on success.
*/
XINT max_openfiles(void)
{
unsigned char mnam[MAX_PATH]; /* IPC file name. */
XINT maxres = 0; /* Error counter. */

/* The IPC file for this node is kept open while TOP is being run. */
sprintf(mnam, "%sipc%02X.bbs", cfg.bbsmultipath, od_control.od_node);
ipcinfil = sh_open(mnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
				   S_IREAD | S_IWRITE);
maxres += (ipcinfil == -1);

return maxres;
}

/* max_shortpageeditor() - Editor for short (one-line) pages.
   Parameters:  nodenum - Node number being paged.
                pagebuf - Pointer to buffer to hold the page text.
   Returns:  TRUE if a page was entered, FALSE if the user aborted.
   Notes:  Can be used with any BBS type that needs short pages.
*/
char max_shortpageeditor(XINT nodenum, unsigned char *pagebuf)
    {

    itoa(nodenum, outnum[0], 10);
    /* TOP will retry if the input was censored. */
    do
        {
        top_output(OUT_SCREEN, getlang("EnterPageLinPrompt"), outnum[0]);
        od_input_str(pagebuf, 70, ' ', MAXASCII);
        top_output(OUT_SCREEN, getlang("EnterPageLinSuffix"));
        }
    while(censorinput(pagebuf));

    /* If nothing was entered, abort. */
    if (!pagebuf[0])
        {
        return 0;
        }

    return 1;
    }

/* max_showmeccastring() - Parses MECCA tokens in a string.
   Parameters:  str - String to process.
   Returns:  Nothing.
*/
void max_showmeccastring(unsigned char *str)
    {
    XINT d; /* Counter. */
    char blink; /* Blink toggle. */

	blink = (od_control.od_cur_attrib & 0x80);

    /* Loop for each character in the string. */
    for (d = 0; d < strlen(str); d++)
	{
    unsigned XINT cccc; /* Colour code. */

        /* Handle CRLF pair. */
        if (str[d] == 0x0A && str[d + 1] != 0x0D)
            {
            od_putch('\r'); od_putch('\n');
            continue;
            }
        /* Handle colour code. */
        if (str[d] == 0x16 && str[d + 1] == 0x01)
            {
            if (str[d + 2] <= 0xF)
                {
                /* Only change the foreground colour if the background is
                   black. */
                cccc = od_control.od_cur_attrib -
                       (od_control.od_cur_attrib % 0xF);
                cccc += str[d + 2];
				od_set_attrib(cccc + blinking);
                d += 2;
                continue;
                }
            else
                {
                /* Handle both the background and foreground. */
                if (str[d + 2] == 0x10)
                    {
                    cccc = str[d + 3];
                    if (cccc >= 0x80)
                        {
						cccc -= 0x80;
                        }
                    od_set_attrib(cccc + blinking);
                    d += 3;
                    continue;
                    }
                }
            }
        /* Handle blink code. */
        if (str[d] == 0x16 && str[d + 1] == 0x02)
            {
            blink = !blink;
		    cccc = od_control.od_cur_attrib;
            if (cccc >= 0x80)
                {
                cccc -= 0x80;
                }
            od_set_attrib(cccc + blinking);
            }

        /* Write the next character. */
        od_putch(str[d]);
        }

    }
