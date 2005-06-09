/******************************************************************************
PRIVCHAT.C   Private chat mode functions.

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

This module implements TOP's private chat mode (CHAT command).  A complete
explanation of the private chat file system is given in PRIVCHAT.TXT.
******************************************************************************/

#include "top.h"

/* This module uses several module-wide global variables that are not needed
   in other modules. */

/* Function prototypes. */
XINT privchatreadbuf(void);
void privchatputch(XINT pckey);
XINT privchatwritebuf(void);

/* Incoming chat file handle, outgoing chat file handle. */
XINT ipchatfil, opchatfil;
/* Incoming chat file name, outgoing chat file name. */
unsigned char *pcinam = NULL, *pconam = NULL;
unsigned char *pcobuf = NULL; /* Outgoing chat buffer. */
XINT pcobufpos; /* Outgoing buffer position. */
char pccmdstr[50]; /* Command string detection buffer. */
char exitflag; /* Flag when chat is exited. */
/* Time of last private chat poll, time of last BBS poll. */
clock_t privlastpoll = 0, privlastbbspoll = 0;
/* File name of exit semaphore file. */
unsigned char exitfilenam[81];

/* privatechat() - Enters private chat mode (CHAT command).
   Parameters:  pcnode - Node entering private chat with this one.
   Returns:  Nothing.
*/
void privatechat(XINT pcnode)
    {
    XINT key, ttry; /* Keypress holder, file i/o attempt counter. */

    /* Private chats are logged for security purposes. */
    od_log_write(top_output(OUT_STRING, getlang("LogEnterPrivChat"),
                 handles[pcnode].string));

    /* Initialize variables and buffers. */
    exitflag = 0;
    pcinam = malloc(256);
    pconam = malloc(256);
    /* The private chat buffer gets overallocated by 5 bytes to give some
       breathing room in the event of a file i/o problem when the buffer
       fills up. */
    pcobuf = malloc(cfg.privchatbufsize + 5);
    if (pcinam == NULL || pconam == NULL || pcobuf == NULL)
        {
        dofree(pcinam);
        dofree(pconam);
        dofree(pcobuf);
        top_output(OUT_SCREEN, getlang("PrivChatNoMem"));
        od_log_write(top_output(OUT_STRING, getlang("LogOutOfMemory")));
        return;
        }

    /* Delete any existing exit semaphore files for this node and the
       other node. */
    sprintf(pcinam, "%sepr%05i.tch", cfg.topworkpath, od_control.od_node);
    sprintf(pconam, "%sepr%05i.tch", cfg.topworkpath, pcnode);
    sh_unlink(pcinam);
    sh_unlink(pconam);

    /* Open both the incoming chat file (this node) and the outgoing chat
       file (other node). */
    sprintf(pcinam, "%sprv%05i.tch", cfg.topworkpath, od_control.od_node);
    sprintf(pconam, "%sprv%05i.tch", cfg.topworkpath, pcnode);
    ipchatfil = sh_open(pcinam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                        S_IREAD | S_IWRITE);
    opchatfil = sh_open(pconam, O_RDWR | O_CREAT | O_BINARY | O_APPEND,
                        SH_DENYNO, S_IREAD | S_IWRITE);
    chsize(opchatfil, 1);
    if (ipchatfil == -1 || opchatfil == -1)
        {
        close(ipchatfil);
        close(opchatfil);
        dofree(pcinam);
        dofree(pconam);
        dofree(pcobuf);
        top_output(OUT_SCREEN, getlang("PrivChatCantOpen"),
                   ipchatfil == -1 ? pcinam : pconam);
        od_log_write(top_output(OUT_STRING, getlang("LogPrivChatFileErr")));
        return;
        }

    /* Prepare the screen for chat. */
    top_output(OUT_SCREEN, getlang("PrivChatPrefix"));

    /* Initialize variables. */
    pcobufpos = 0;
    sprintf(exitfilenam, "%sepr%05i.tch", cfg.topworkpath,
            od_control.od_node);

    /* Main private chat loop. */
    do
        {
        /* Private chat buffer polling is hardcoded at once every tenth of
           a second. */
        if ((((float) myclock() - (float) privlastpoll) / (float) CLK_TCK) >=
            0.10) // Configurable later!
            {
            privchatreadbuf();
            /* BBS polling is harcoded at once every second. */
            if ((((float) myclock() - (float) privlastbbspoll) / (float) CLK_TCK) >=
                1.00) // Configurable later!
                {
                /* Incoming BBS pages are still displayed in chat mode. */
                if (cfg.bbstype != BBS_UNKNOWN && bbs_call_processmsgs)
                    {
                    (*bbs_call_processmsgs)();
                    }
                privlastbbspoll = myclock();
                }
            privlastpoll = myclock();
            /* Flush the private chat buffer if there is something in it and
               if the exit semaphore file is nonexistant. */
            if (pcobufpos > 0 || !access(exitfilenam, 0))
                {
                privchatwritebuf();
                }
            }

        /* Get the next keypress, without waiting for one. */
        key = od_get_key(FALSE);
        if (key)
            {
            /* Place the key in the buffer if one was pressed. */
            privchatputch(key);
            }
        else
            {
            /* Timeslice if no activity. */
#ifndef __OS2__
            od_sleep(0);
#else
            DosSleep(10);
#endif
            }

        /* Timeslice again.  This can result in significant lag but it does
           take a lot of pressure off the processor. */
#ifndef __OS2__
        od_sleep(0);
#else
        DosSleep(10);
#endif
        }
    while(!exitflag); /* Loop until an exit is flagged. */

    /* Attempt to flush the outgoing buffer one last time. */
    ttry = 0;
    while(!privchatwritebuf() && ttry < cfg.privchatmaxtries) ttry++;

    /* Attempt to read the incoming buffer one last time. */
    ttry = 0;
    while(!privchatreadbuf() && ttry < cfg.privchatmaxtries) ttry++;

    close(ipchatfil);
    close(opchatfil);


    /* This node requested the exit. */
    if (exitflag == 1)
        {
        FILE *exitfil = NULL; /* Exit semaphore file stream. */

        /* Create a 0-length file to signal the other node that we wish to
           exit private chat. */
        sprintf(outbuf, "%sepr%05i.tch", cfg.topworkpath, pcnode);
        exitfil = _fsopen(outbuf, "wb", SH_DENYRW);
        fclose(exitfil);
        }
    /* The other node requested the exit. */
    if (exitflag == 2)
        {
        /* Inform the user of the exit. */
        top_output(OUT_SCREEN, getlang("PrivChatSuffix2"),
                   handles[pcnode].string);

        /* Wait until we have access to the exit semaphore file.  There is
           usually a very short period where this node becomes aware of the
           exit file before the other node has closed it. */
        ttry = 0;
        while(access(exitfilenam, 6) && ttry < cfg.privchatmaxtries) ttry++;

        /* Delete the file if time didn't elapse.  Deletion is not critical,
           but it does keep things cleaner. */
        if (ttry < cfg.privchatmaxtries)
            {
            sh_unlink(exitfilenam);
            }

        /* Delete the chat files as they are only good for one chat
           session. */
        sh_unlink(pcinam);
        sh_unlink(pconam);
        }

    /* Free memory. */
    dofree(pcinam);
    dofree(pconam);
    dofree(pcobuf);

    /* Clear the chat request nodes. */
    privchatin = -1;
    privchatout = -1;

    /* Log the end of the chat. */
    od_log_write(top_output(OUT_STRING, getlang("LogEndPrivChat")));

    }

/* privchatreadbuf() - Reads incoming characters in private chat mode.
   Parameters:  None.
   Returns:  TRUE on error, FALSE on success.
*/
XINT privchatreadbuf(void)
    {
    unsigned char *readbuf = NULL; /* Incoming text buffer. */
    XINT res; /* Result code. */

    /* Test if the exit semaphore exists, meaning the other node wants to
       exit private chat. */
    if (!access(exitfilenam, 0))
        {
        exitflag = 2;
        return 1;
        }

    /* If the length of the incoming chat file is 0 or 1, there is no new
       text to read. */
    if (filelength(ipchatfil) <= 1L)
        {
        /* Timeslice. */
#ifndef __OS2__
        od_sleep(0);
#else
        DosSleep(10);
#endif
        return 1;
        }

    /* The first byte of the file is locked to show the file is in use. */
    res = rec_locking(REC_LOCK, ipchatfil, 0, 1);
    if (res == -1)
        {
        return 0;
        }

    /* Allocate a buffer that will hold the entire contents of the file,
       which is normally no more than a few bytes.  Although the first byte
       is not used for anything but locking, we need an extra byte for the
       terminating \0 so it works out nicely to use the file size. */
    readbuf = malloc(filelength(ipchatfil));
    if (readbuf == NULL)
        {
        /* Signal an immediate exit if the allocation fails. */
        top_output(OUT_SCREEN, getlang("PrivChatNoMem"));
        exitflag = 1;
        return 0;
        }
    memset(readbuf, 0, filelength(ipchatfil));

    /* Read the incoming text, which starts at byte 1 (the second byte in
       the file). */
    lseek(ipchatfil, 1, SEEK_SET);
    read(ipchatfil, readbuf, filelength(ipchatfil) - 1);

    /* Select the other person's text colour. */
    top_output(OUT_SCREEN, getlang("PrivChatCol2"));

    /* Display the text, ignoring any codes that may happen to be in the
       text. */
    outproccol = FALSE; outproclang = FALSE;
    top_output(OUT_SCREEN, readbuf);
    outproccol = TRUE; outproclang = TRUE;

    /* Truncate the file to one byte, effectively clearing it. */
    chsize(ipchatfil, 1);

    /* Unlock the first byte, signifying we are done. */
    res = rec_locking(REC_UNLOCK, ipchatfil, 0, 1);
    dofree(readbuf);
    if (res == -1)
        {
        return 0;
        }

    return 1;
    }

/* privchatputch() - Places a keypress in the outgoing private chat buffer.
   Parameters:  pckey - Key value to insert.
   Returns:  Nothing.
*/
void privchatputch(XINT pckey)
    {

    /* Attempt to flush the buffer if it is full. */
    if (pcobufpos >= cfg.privchatbufsize)
        {
        if (!privchatwritebuf())
            {
            /* Abort if the buffer cannot be flushed.  This will likely lead
               to an infinite loop, forcing the user to hang up, unless the
               buffer can be cleared.  This isn't such a bad effect because
               if the buffer is filling up then there are probably bigger
               problems anyhow. */
            top_output(OUT_SCREEN, getlang("PrivChatBufProb"));
            return;
            }
        }

    /* Backspace. */
    if (pckey == 8)
        {
        /* Backspaces are sent nondestructive. */
        pcobuf[pcobufpos++] = 8;
        top_output(OUT_SCREEN, "\b");
        }
    /* New line. */
    if (pckey == 13)
        {
        /* A complete CRLF pair is sent. */
        pcobuf[pcobufpos++] = 13;
        pcobuf[pcobufpos++] = 10;
        top_output(OUT_SCREEN, getlang("PrivChatEndLine"));
        }
    /* Escape (exit private chat). */
    if (pckey == 27)
        {
        exitflag = 1;
        top_output(OUT_SCREEN, getlang("PrivChatSuffix"));
        }
    /* Normal key. */
    if (pckey >= 32 && pckey < 255)
        {
        /* Add the key to the buffer. */
        pcobuf[pcobufpos++] = pckey;

        /* Select our text colour. */
        top_output(OUT_SCREEN, getlang("PrivChatCol1"));

        /* top_output() needs a string. */
        sprintf(outbuf, "%c", pckey);

        /* Output the string, ignoring any codes. */
        outproccol = FALSE; outproclang = FALSE;
        top_output(OUT_SCREEN, outbuf);
        outproccol = TRUE; outproclang = TRUE;
        }

    }

/* privchatwritebuf() - Writes the outgoing private chat buffer to disk.
   Parameters:  None.
   Returns:  TRUE on success, FALSE on error.
*/
XINT privchatwritebuf(void)
    {
    XINT res; /* Result code. */

    /* Lock the first byte of the file while it is being updated. */
    res = rec_locking(REC_LOCK, opchatfil, 0, 1);
    if (res == -1)
        {
        return 0;
        }

    /* Write the entire contents of the buffer. */
    write(opchatfil, pcobuf, pcobufpos);
    pcobufpos = 0;

    /* Unlock the first byte. */
    res = rec_locking(REC_UNLOCK, opchatfil, 0, 1);
    if (res == -1)
        {
        return 0;
        }

    return 1;
    }

