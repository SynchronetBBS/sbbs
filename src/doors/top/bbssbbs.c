/******************************************************************************
BBSSBBS.C    Implements SuperBBS-specific BBS functions.

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

The module contains all of the functions specific to interfacing with SuperBBS
v1.1x.  The rest of the SuperBBS interfacing uses RA calls.
******************************************************************************/

#include "top.h"

/* sbbs_init() - Initialize pointers to SuperBBS functions.
   Paremeters:  None.
   Returns:  Nothing.
*/
void sbbs_init(void)
{

bbs_call_loaduseron = sbbs_loaduseron;
bbs_call_saveuseron = sbbs_saveuseron;
bbs_call_processmsgs = ra_processmsgs;
bbs_call_page = ra_page;
bbs_call_setexistbits = sbbs_setexistbits;
bbs_call_login = ra_login;
bbs_call_logout = ra_logout;
bbs_call_openfiles = ra_openfiles;
bbs_call_updateopenfiles = ra_updateopenfiles;
bbs_call_pageedit = ra_longpageeditor;

}

/* sbbs_loaduseron() - Loads a node status from the SUSERON file.
   Parameters:  nodenum - Node number to load the data for.
                userdata - Pointer to generic BBS data structure to fill.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char sbbs_loaduseron(XINT nodenum, bbsnodedata_typ *userdata)
{
XINT res; /* Result code. */
SBBS_USERON_typ sbbsuser; /* SBBS SUSERON record. */
long n; /* 0-based node number. */

/* Open the SUSERON file, which TOP does not keep open due to apparent
   conflicts. */
useronfil = sh_open(top_output(OUT_STRING, "@1suseron.bbs",
                               cfg.bbspath),
                    O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                    S_IREAD | S_IWRITE);
if (useronfil == -1)
    {
    return 1;
    }

/* In the SUSERON file, records are 0-based (eg. node 1 is record 0). */
n = nodenum - 1L;

/* Abort if node 0 is paged, or if the SUSERON file is not large enough that
   it holds the information for the node (meaning the node doesn't exist). */
if (nodenum == 0 ||
    nodenum > (filelength(useronfil) / (long) sizeof(SBBS_USERON_typ)))
	{
    close(useronfil);
    return 1;
    }

/* Load the SUSERON record. */
res = lseek(useronfil, n * (long) sizeof(SBBS_USERON_typ), SEEK_SET);
if (res == -1)
	{
    close(useronfil);
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(SBBS_USERON_typ),
			sizeof(SBBS_USERON_typ));
res = read(useronfil, &sbbsuser, sizeof(SBBS_USERON_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(SBBS_USERON_typ),
			sizeof(SBBS_USERON_typ));
if (res == -1)
	{
    close(useronfil);
    return 1;
    }

/* Convert Pascal strings to C. */
unbuild_pascal_string(35, sbbsuser.name);
unbuild_pascal_string(25, sbbsuser.city);

/* Copy the information into the Generic BBS structure. */
memset(userdata, 0, sizeof(bbsnodedata_typ) - 2);
fixname(userdata->realname, sbbsuser.name);
strncpy(userdata->handle, sbbsuser.name, 30);
fixname(userdata->handle, userdata->handle);
userdata->quiet = sbbsuser.attribute & SBBS_NODISTURB;
userdata->hidden = (sbbsuser.attribute & SBBS_HIDDEN) ||
				   !(sbbsuser.attribute & SBBS_INUSE);
userdata->attribs1 = sbbsuser.attribute;
if (sbbsuser.status < 11)
    {
    /* Use the predefined status types from the language file. */
    strcpy(userdata->statdesc, sbbs_statustypes[sbbsuser.status]);
    }
else
    {
    /* Status type unknown.  This would occur either with a corrupt
       SUSERON file or with a new version of SBBS that uses new types. */
    userdata->statdesc[0] = 0;
    }
userdata->speed = sbbsuser.baud;
strcpy(userdata->location, sbbsuser.city);
userdata->infobyte = sbbsuser.infobyte;
userdata->node = nodenum;
userdata->numcalls = 1;

close(useronfil);

return 0;
}

/* sbbs_saveuseron() - Saves a node status to the SUSERON file.
   Parameters:  nodenum - Node number to save the data for.
                userdata - Pointer to generic BBS data structure to use.
   Returns:  TRUE if an error occurred, FALSE if successful.
*/
char sbbs_saveuseron(XINT nodenum, bbsnodedata_typ *userdata)
{
XINT res; /* Result code. */
SBBS_USERON_typ sbbsuser; /* SBBS SUSERON buffer. */
long n; /* 0-based node number. */

/* Open the SUSERON file, which TOP does not keep open due to apparent
   conflicts. */
useronfil = sh_open(top_output(OUT_STRING, "@1suseron.bbs",
                               cfg.bbspath),
                    O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                    S_IREAD | S_IWRITE);
if (useronfil == -1)
    {
    return 1;
    }

/* Copy the data from the Generic BBS buffer to the SUSERON record. */
memset(&sbbsuser, 0, sizeof(SBBS_USERON_typ));
strncpy(sbbsuser.name, userdata->realname, 35);
sbbsuser.attribute = userdata->attribs1;
if (userdata->quiet)
	{
    sbbsuser.attribute |= SBBS_NODISTURB;
    }
else
	{
    sbbsuser.attribute &= (0xFF - SBBS_NODISTURB);
    }
sbbsuser.attribute |= SBBS_INUSE;
sbbsuser.status = 6;
/* Asceratin the status number since SBBS doesn't support custom statuses. */
for (res = 0; res < 11; res++)
	{
    if (!stricmp(userdata->statdesc, sbbs_statustypes[res]))
    	{
        sbbsuser.status = res;
        break;
        }
    }
sbbsuser.baud = userdata->speed;
strncpy(sbbsuser.city, userdata->location, 25);
sbbsuser.infobyte = userdata->infobyte;

/* Convert C strings to Pascal, which SBBS uses. */
build_pascal_string(sbbsuser.name);
build_pascal_string(sbbsuser.city);

/* Get the 0-based node number. */
n = nodenum - 1;

/* Extend the SUSERON file if it's not long enough.  Done for safety when
   record locking. */
if (filelength(useronfil) < (long) nodenum * (long) sizeof(SBBS_USERON_typ))
    {
    chsize(useronfil, (long) nodenum * (long) sizeof(SBBS_USERON_typ));
    }

/* Write the SUSERON data. */
res = lseek(useronfil, n * (long) sizeof(SBBS_USERON_typ), SEEK_SET);
if (res == -1)
	{
    close(useronfil);
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(SBBS_USERON_typ),
			sizeof(SBBS_USERON_typ));
res = write(useronfil, &sbbsuser, sizeof(SBBS_USERON_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(SBBS_USERON_typ),
			sizeof(SBBS_USERON_typ));
if (res == -1)
	{
    close(useronfil);
    return 1;
    }

close(useronfil);

return 0;
}

/* sbbs_setexistbits() - Selects the node status fields to display.
   Parameters:  userdata - Pointer to generic BBS data structure to set the
                           bits in.
   Returns:  Nothing.
*/
void sbbs_setexistbits(bbsnodedata_typ *userdata)
{

userdata->existbits = NEX_HANDLE |
					  NEX_REALNAME |
                      NEX_NODE |
                      NEX_LOCATION |
                      NEX_STATUS |
                      NEX_PAGESTAT;

}
