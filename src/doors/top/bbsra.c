/******************************************************************************
BBSRA.C      Implements RemoteAccess-specific BBS functions.

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

The module contains all of the functions for interfacing with RA v2.xx.  It is
also used for some SuperBBS operations that are almost identical to RA.
******************************************************************************/

#include "top.h"

/* ra_init() - Initialize pointers to RA functions.
   Paremeters:  None.
   Returns:  Nothing.
*/
void ra_init(void)
{

bbs_call_loaduseron = ra_loaduseron;
bbs_call_saveuseron = ra_saveuseron;
bbs_call_processmsgs = ra_processmsgs;
bbs_call_page = ra_page;
bbs_call_setexistbits = ra_setexistbits;
bbs_call_login = ra_login;
bbs_call_logout = ra_logout;
bbs_call_openfiles = ra_openfiles;
bbs_call_updateopenfiles = ra_updateopenfiles;
bbs_call_pageedit = ra_longpageeditor;

}

/* ra_loaduseron() - Loads a node status from the USERON file.
   Parameters:  nodenum - Node number to load the data for.
                userdata - Pointer to generic BBS data structure to fill.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char ra_loaduseron(XINT nodenum, bbsnodedata_typ *userdata)
{
XINT res; /* Result code. */
RA_USERON_typ rauser; /* RA USERON record. */
long n; /* 0-based node number. */

/* In the USERON file, records are 0-based (eg. node 1 is record 0). */
n = nodenum - 1L;

/* Abort if node 0 is paged, or if the USERON file is not large enough that
   it holds the information for the node (meaning the node doesn't exist). */
if (nodenum == 0 ||
    nodenum > (filelength(useronfil) / (long) sizeof(RA_USERON_typ)))
	{
    return 1;
    }

/* Load the USERON record. */
res = lseek(useronfil, n * (long) sizeof(RA_USERON_typ), SEEK_SET);
if (res == -1)
	{
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(RA_USERON_typ),
			sizeof(RA_USERON_typ));
res = read(useronfil, &rauser, sizeof(RA_USERON_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(RA_USERON_typ),
			sizeof(RA_USERON_typ));
if (res == -1)
	{
    return 1;
    }

/* Convert Pascal strings to C. */
unbuild_pascal_string(35, rauser.name);
unbuild_pascal_string(35, rauser.handle);
unbuild_pascal_string(25, rauser.city);
unbuild_pascal_string(10, rauser.statdesc);

/* Copy the information into the Generic BBS structure. */
memset(userdata, 0, sizeof(bbsnodedata_typ) - 2);
fixname(userdata->realname, rauser.name);
strncpy(userdata->handle, rauser.handle, 30);
fixname(userdata->handle, userdata->handle);
userdata->node = rauser.line;
userdata->speed = rauser.baud;
strcpy(userdata->location, rauser.city);
/* 255 indicates a user defined status type. */
if (rauser.status == 255)
	{
    strcpy(userdata->statdesc, rauser.statdesc);
    }
else
	{
    /* Use the predefined status types from the language file. */
    if (rauser.status < 8)
        {
        strcpy(userdata->statdesc, ra_statustypes[rauser.status]);
        }
    else
        {
        /* Status type unknown.  This would occur either with a corrupt
           USERON file or with a new version of RA that uses new types. */
        userdata->statdesc[0] = 0;
        }
    }
userdata->quiet = rauser.attribute & RA_NODISTURB;
userdata->hidden = (rauser.attribute & RA_HIDDEN) ||
                   (rauser.attribute & RA_READY) ||
                   (rauser.line == 0);
userdata->attribs1 = rauser.attribute;
userdata->numcalls = rauser.numcalls;

return 0;
}

/* ra_saveuseron() - Saves a node status to the USERON file.
   Parameters:  nodenum - Node number to save the data for.
                userdata - Pointer to generic BBS data structure to use.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char ra_saveuseron(XINT nodenum, bbsnodedata_typ *userdata)
{
XINT res; /* Result code. */
RA_USERON_typ rauser; /* RA USERON buffer. */
long n; /* 0-based node number. */

/* Copy the data from the Generic BBS buffer to the USERON record. */
memset(&rauser, 0, sizeof(RA_USERON_typ));
strncpy(rauser.name, userdata->realname, 35);
strcpy(rauser.handle, userdata->handle);
rauser.line = userdata->node;
rauser.baud = userdata->speed;
strncpy(rauser.city, userdata->location, 25);
rauser.status = 255;
rauser.attribute = userdata->attribs1;
if (userdata->quiet)
	{
    rauser.attribute |= RA_NODISTURB;
    }
else
	{
    rauser.attribute &= (0xFF - RA_NODISTURB);
    }
strncpy(rauser.statdesc, userdata->statdesc, 10);
rauser.numcalls = userdata->numcalls;

/* Convert C strings to Pascal, which RA uses. */
build_pascal_string(rauser.name);
build_pascal_string(rauser.handle);
build_pascal_string(rauser.city);
build_pascal_string(rauser.statdesc);

/* Get the 0-based node number. */
n = nodenum - 1;

/* Extend the USERON file if it's not long enough.  Done for safety when
   record locking. */
if (filelength(useronfil) < (long) nodenum * (long) sizeof(RA_USERON_typ))
    {
    chsize(useronfil, (long) nodenum * (long) sizeof(RA_USERON_typ));
    }

/* Write the USERON data. */
res = lseek(useronfil, n * (long) sizeof(RA_USERON_typ), SEEK_SET);
if (res == -1)
	{
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(RA_USERON_typ),
			sizeof(RA_USERON_typ));
res = write(useronfil, &rauser, sizeof(RA_USERON_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(RA_USERON_typ),
			sizeof(RA_USERON_typ));
if (res == -1)
	{
    return 1;
    }

return 0;
}

/* ra_processmsgs() - Reads and displays messages from a NODEx.RA file.
   Parameters:  None.
   Returns:  Number of messages processed.  (0 on error.)
   Notes:  Also handles SuperBBS TOLINEx files.
*/
XINT ra_processmsgs(void)
{
FILE *pnfil = NULL; /* NODEx file stream. */
char filnam[256]; /* NODEx file name. */
char XFAR *buffer = NULL; /* Incoming message buffer. */
XINT pos = 0, xpos = 0; /* Position, position multiplier. */
XINT tmp, xh; /* ASCII-to-int conversion holder, length of message read. */
char sbbsmsgon = 0; /* Flag if a SuperBBS message is being processed. */

/* Select the filename based on BBS type. */
switch(cfg.bbstype)
	{
    case BBS_RA2:
        sprintf(filnam, "%snode%i.ra", cfg.bbsmultipath, od_control.od_node);
    	break;
    case BBS_SBBS11:
        sprintf(filnam, "%stoline%i", cfg.bbsmultipath, od_control.od_node);
        break;
    }

/* Abort if we don't get read access to the file. */
if (access(filnam, 4))
	{
    return 0;
    }

/* Allocate the page buffer. */
buffer = malloc(8000);
if (!buffer)
    {
    return 0;
    }

/* Open the file using a shared mode. */
pnfil = _fsopen(filnam, "rb", SH_DENYNO);
if (!pnfil)
	{
    dofree(buffer);
    return 0;
    }

/* Prepare the screen. */
delprompt(TRUE);
if (user.pref1 & PREF1_DUALWINDOW)
    {
    // Different for AVT when I find out what the store/recv codes are.
    od_disp_emu("\x1B" "[u", TRUE);
    top_output(OUT_SCREEN, getlang("DWOutputPrefix"));
    }
top_output(OUT_SCREEN, getlang("RAPageRecvPrefix"));

/* Loop until the whole file is processed. */
do
	{
    memset(buffer, 0, 8000);

    /* Read the file. */
    fseek(pnfil, (long) xpos * 7900L, SEEK_SET);
    xh = fread(buffer, 1, 8000, pnfil);

    /* If the file is quite long, it's handled in 7900 byte chunks.  This
       was done to stabilize memory requirements when I thought I was at
       the 640K limit.  A better way would be to just allocate the buffer
       the size of the entire file and read it all in at once.  Even in a
       low-memory situation there should be plenty of memory to read even the
       largest files, which rarely get above a few K. */
    if (xh > 7900)
    	{
        xh = 7900;
        }

    /* Loop for each character. */
    for (pos = 0; pos < xh; pos++)
    	{
        if (!sbbsmsgon && cfg.bbstype == BBS_SBBS11)
        	{
            /* In SuperBBS, messages start at a /MESSAGE token, so
               processing doesn't begin until one is found. */
            if (!strnicmp(&buffer[pos], "/MESSAGE", 8))
            	{
                /* Character location pointers. */
                char *eptr = NULL, *nptr = NULL;
                XINT fromnode; /* Node that sent the page. */

                /* The format of the message token is like this:
                   /MESSAGE User Name,node */
                pos += 9;
                /* Find the comma. */
                eptr = strchr(&buffer[pos], ',');
                memset(outbuf, 0, 61);
                /* Grab the name of the sender. */
                strncpy(outbuf, &buffer[pos], eptr - &buffer[pos]);
                pos += (eptr - &buffer[pos]) + 1;
                /* Detemine where this line ends. */
                eptr = strchr(&buffer[pos], '\r');
                nptr = strchr(&buffer[pos], '\n');
                if (nptr < eptr && nptr != NULL)
                	{
                    eptr = nptr;
                    }
                /* Get the node number. */
                fromnode = atoi(&buffer[pos]);
                itoa(fromnode, outnum[0], 10);
                /* Advance the position to the end of this line. */
                pos += (eptr - &buffer[pos]) - 1;
                /* Display the page header. */
                top_output(OUT_SCREEN, "@c");
                top_output(OUT_SCREEN, getlang("SBBSRecvPageHdr"),
                		   outbuf, outnum[0]);
                /* Now processing a SuperBBS message. */
                sbbsmsgon = 1;
                }
            continue;
            }
        if (sbbsmsgon && cfg.bbstype == BBS_SBBS11)
        	{
            /* In SuperBBS, messages end with /END/ so this needs to be
               scanned for. */
            if (!strnicmp(&buffer[pos], "/END/", 5))
            	{
                /* No longer processing a SuperBBS message. */
                sbbsmsgon = 0;
                pos += 4;
                /* Prompt the user to continue. */
                top_output(OUT_SCREEN, getlang("RAEnter"));
                while(od_get_key(TRUE) != 13);
                top_output(OUT_SCREEN, getlang("RAEnterSuffix"));
                continue;
                }
			}
        /* Handle Ctrl-A codes, which mean "wait for ENTER". */
        if (buffer[pos] == 1)
        	{
            while(od_get_key(TRUE) != 13);
            continue;
            }
        /* Ctrl-K codes display RA-specific information.  TOP only handles
           colour changes and a few page-specific language items. */
        if (buffer[pos] == 11)
        	{
            /* ^K] is the language item code. */
            if (buffer[pos + 1] == ']')
            	{
                /* The format of these codes is ^K]nnn, where nnn is a 3
                   digit number specifying the item number. */
                tmp = atoi(&buffer[pos + 2]);
                pos += 4;

                /* Display message based on the item number.  See the RA
                   language editor for the number of each item. */
                switch(tmp)
                	{
                    case 258: top_output(OUT_SCREEN, getlang("RAEnter")); break;
                    case 496: top_output(OUT_SCREEN, getlang("RAOnNode")); break;
                    case 497: top_output(OUT_SCREEN, getlang("RAMsgFrom")); break;
                    case 628: top_output(OUT_SCREEN, getlang("RAJustPosted")); break;
                    }
                continue;
                }
            /* ^K[ is a colour change. */
            if (buffer[pos + 1] == '[')
                {
                unsigned char fgcol, bgcol; /* Fore & background colours. */

                /* The format of these codes is ^K[bf, where b is the
                   background colour (in hex) and f is the foreground
                   colour (also in hex). */
                bgcol = toupper(buffer[pos + 2]);
                fgcol = toupper(buffer[pos + 3]);
                pos += 3;

                /* Process valid hex digits only. */
                if (isxdigit(bgcol))
                	{
                    if (bgcol >= 'A' && bgcol <= 'F')
                    	{
                        /* A through F represent colours 10 - 15. */
                        bgcol -= ('A' - 10);
                        }
                    else
                    	{
                        bgcol -= '0';
                        }
                    }
                /* Process valid hex digits only. */
				if (isxdigit(fgcol))
                	{
                    if (fgcol >= 'A' && fgcol <= 'F')
                    	{
                        /* A through F represent colours 10 - 15. */
                        fgcol -= ('A' - 10);
                        }
                    else
                    	{
                        fgcol -= '0';
                        }
                    }
                od_set_colour(fgcol, bgcol);
                continue;
            	}
            }
        od_putch(buffer[pos]);
        }
    xpos++;
    }
while((long) xpos * 7900L < filelength(fileno(pnfil)));

closefile(pnfil);

top_output(OUT_SCREEN, getlang("RAPageRecvSuffix"));
unlink(filnam);

dofree(buffer);

return 1;
}

/* ra_page() - Sends a page to a RA node.
   Parameters:  nodenum - Node number to page.
                pagebuf - Text to send.
   Returns:  TRUE if successful, FALSE if an error occurred.
   Notes:  Also pages SuperBBS nodes.
*/
char ra_page(XINT nodenum, unsigned char *pagebuf)
{
FILE *rapgfil = NULL; /* File stram of the receiving NODEx file. */
unsigned char tmp[41]; /* Buffer to hold the filtered username. */

/* Open the appropriate file. */
// This all needs better errorchecking
itoa(nodenum, outnum[0], 10);
switch(cfg.bbstype)
	{
    case BBS_RA2:
    	rapgfil = openfile(top_output(OUT_STRING, "@1node@2.ra",
						   cfg.bbsmultipath, outnum[0]), "at", 0);
        break;
    case BBS_SBBS11:
    	rapgfil = openfile(top_output(OUT_STRING, "@1toline@2",
						   cfg.bbsmultipath, outnum[0]), "at", 0);
        break;
    }

/* Abort if the file can't be opened. */
if (!rapgfil)
	{
    top_output(OUT_SCREEN, getlang("CantPage"), outnum[0]);
    return 0;
    }

/* The filtered username is used because handes can contain PubColour
   codes. */
filter_string(tmp, cfg.usehandles ? user.handle : user.realname);
itoa(od_control.od_node, outnum[0], 10);

/* Write the appropriate page header. */
switch(cfg.bbstype)
	{
    case BBS_RA2:
        strcpy(outbuf, top_output(OUT_STRING, getlang("RAPageHeader"),
               tmp, outnum[0]));
        break;
    case BBS_SBBS11:
        strcpy(outbuf, top_output(OUT_STRING, getlang("SBBSPageHeader"),
               tmp, outnum[0]));
        break;
    }
fputs(outbuf, rapgfil);

/* Write the message text. */
fputs(pagebuf, rapgfil);

/* Write the appropriate page footer. */
switch(cfg.bbstype)
	{
    case BBS_RA2:
        fputs(top_output(OUT_STRING, getlang("RAPageFooter")), rapgfil);
        break;
    case BBS_SBBS11:
        fputs(top_output(OUT_STRING, getlang("SBBSPageFooter")), rapgfil);
        break;
    }

closefile(rapgfil);

return 1;
}

/* ra_setexistbits() - Selects the node status fields to display.
   Parameters:  userdata - Pointer to generic BBS data structure to set the
                           bits in.
   Returns:  Nothing.
*/
void ra_setexistbits(bbsnodedata_typ *userdata)
{

userdata->existbits = NEX_HANDLE |
					  NEX_REALNAME |
                      NEX_NODE |
                      NEX_SPEED |
                      NEX_LOCATION |
                      NEX_STATUS |
                      NEX_PAGESTAT;

}

/* ra_login() - RA initialization when TOP is started.
   Parameters:  None.
   Returns:  Nothing.
*/
void ra_login(void)
{
/* Generic BBS data buffer, login check BBS data buffer. */
bbsnodedata_typ rutmp, logintmp;
XINT res; /* Result codes. */

/* Copy the user information into the generic BBS data buffer. */
memset(&rutmp, 0, sizeof(bbsnodedata_typ) - 2);

/* Check if the node is already logged in RA/SBBS. */
res = (*bbs_call_loaduseron)(od_control.od_node, &logintmp);
if (!res)
{
    if (!logintmp.hidden)
    {
        /* Set the do not disturb flag. */
        node->quiet = rutmp.quiet = logintmp.quiet;
        save_node_data(od_control.od_node, node);
    }
}

fixname(rutmp.realname, user.realname);
fixname(rutmp.handle, user.handle);
rutmp.node = od_control.od_node;
rutmp.speed = od_control.baud;
strcpy(rutmp.location, od_control.user_location);
strcpy(rutmp.statdesc, getlang("NodeStatus"));
rutmp.attribs1 = 0;

/* Update the USERON file to show the user is now in TOP. */
res = (*bbs_call_saveuseron)(od_control.od_node, &rutmp);
if (!res)
    {
    FILE *rnfil = NULL; /* RABUSY file stream. */

    /* Create RABUSY semaphore.  RA provides these for batch operations. */
    if (cfg.bbstype == BBS_RA2)
    	{
	    itoa(od_control.od_node, outnum[0], 10);
    	rnfil = fopen(top_output(OUT_STRING, "@1rabusy.@2", cfg.bbsmultipath,
				                 outnum[0]), "wb");
        fclose(rnfil);
        }
    node_added = TRUE;
    }

}

/* ra_logout() - RA deinitialization when TOP is exited.
   Parameters:  None.
   Returns:  Nothing.
*/
void ra_logout(void)
{
bbsnodedata_typ rutmp; /* Generic BBS data buffer. */

if (localmode || lanmode)
	{
    /* In local and LAN modes, TOP removes the user from the USERON file.
       In remote mode, TOP assumes RA will gain control immediately after
       the exit so it doesn't bother. */
    memset(&rutmp, 0, sizeof(bbsnodedata_typ) - 2);
    rutmp.quiet = 1;
	rutmp.attribs1 = RA_HIDDEN & RA_NODISTURB;
	rutmp.node = 0;
    (*bbs_call_saveuseron)(od_control.od_node, &rutmp);
    if (cfg.bbstype == BBS_RA2)
    	{
        /* Remove the RABUSY semaphore. */
        itoa(od_control.od_node, outnum[0], 10);
		unlink(top_output(OUT_STRING, "@1rabusy.@2", cfg.bbsmultipath,
					      outnum[0]));
        }
    }

}

/* ra_openfiles() - Opens RA-related files.
   Parameters:  None.
   Returns:  Number of errors that occurred, or 0 on success.
*/
XINT ra_openfiles(void)
{
XINT bbsres = 0; /* Error counter. */

switch(cfg.bbstype)
	{
    case BBS_RA2:
        /* TOP keeps the RA USERON file open while it is running. */
        useronfil = sh_open(top_output(OUT_STRING, "@1useron.bbs",
									   cfg.bbspath),
							O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
							S_IREAD | S_IWRITE);
        break;
    case BBS_SBBS11:
        /* TOP does not keep the SBBS SUSERON file open because it seemed
           to cause problems. */
        return 0;
	}
bbsres += (useronfil == -1);

return bbsres;
}

/* ra_updateopenfiles() - Re-opens files if the node changes in LAN mode.
   Parameters:  None.
   Returns:  Number of errors that occurred, or 0 on success. */
XINT ra_updateopenfiles(void)
{

/* As no node-specific files are kept open, nothing needs to be done. */

return 0;
}

/* ra_longpageeditor() - Editor for long (multi-line) pages.
   Parameters:  nodenum - Node number being paged.
                pagebuf - Pointer to buffer to hold the page text.
   Returns:  TRUE if a page was entered, FALSE if the user aborted.
   Notes:  Can be used with any BBS type that can handle long pages.
*/
char ra_longpageeditor(XINT nodenum, unsigned char *pagebuf)
    {
#ifndef __OS2__
    tODEditOptions edop; /* OpenDoors editor options. */
#endif
    unsigned char linebuf[81]; /* Line input buffer. */
    XINT writecount = 0; /* Write count, used to limit page length. */

/* Under DOS and Win32, OpenDoors provides a sophisticated text editor so
   TOP will take advantage of that. */
#ifndef __OS2__
    if (od_control.user_ansi || od_control.user_avatar)
        {
        /* The OpenDoors editor only works in ANSI and AVATAR modes. */

        /* Prepare the screen. */
        od_clr_scr();
        itoa(nodenum, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("EnterPageFS"), outnum[0]);
        top_output(OUT_SCREEN, getlang("EnterPageFSSep"));

        /* Initialize the OpenDoors editor data. */
        edop.nAreaLeft = 1;
        edop.nAreaTop = 4;
        edop.nAreaRight = 80;
        edop.nAreaBottom = 23;
        edop.TextFormat = FORMAT_LINE_BREAKS;
        edop.pfMenuCallback = NULL;
        edop.pfBufferRealloc = NULL;
        edop.dwEditFlags = 0;

        /* Call the editor. */
        if (od_multiline_edit(pagebuf, 1600, &edop) ==
            OD_MULTIEDIT_SUCCESS)
            {
            /* The profanity censor can currently only handle strings of
               less than 256 characters. */
            if (strlen(pagebuf) < 256)
                {
                if (censorinput(pagebuf))
                    {
                    return 0;
                    }
                }
            return 1;
            }
        else
            {
            return 0;
            }
        }
    else
/* TOP/2 uses an older port of OpenDoors which does not have the fancy page
   editor, so TOP uses less sophisticated line-by-line editor. */
#endif
        {
        /* The line editor is also used if the user is in ASCII mode. */

        /* Prepare the screen. */
        itoa(nodenum, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("EnterPage"), outnum[0]);
        top_output(OUT_SCREEN, getlang("EnterPageSep"));
        /* Put something in the input buffer to engage the loop. */
        linebuf[0] = '.';
        /* Loop until the message is too long or the user just hits ENTER. */
        while(writecount < (1600 - 81) && linebuf[0])
            {
            memset(linebuf, 0, 80);
            /* Get one line at a time.  Repeat if the input needs to be
               censored. */
            do
                {
                od_input_str(linebuf, 79, ' ',
                             MAXASCII);
                }
            while(censorinput(linebuf));
            if (linebuf[0])
                {
                /* Copy the line to the page buffer. */
                memcpy(&pagebuf[writecount], linebuf, strlen(linebuf));
                writecount += strlen(linebuf);
                memcpy(&pagebuf[writecount], "\n", 2);
                writecount++;
                }
            }
        return 1;
        }
    }

