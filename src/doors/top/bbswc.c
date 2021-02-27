#include "top.h"

/* Expirimental WildCat BBS interfacing. */

/*  Copyright 1993 - 2000 Paul J. Sidorsky

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
*/

char wc_loaduseron(XINT nodenum, bbsnodedata_typ *userdata)
{ // Get rid of the n when more time is available
XINT res;
WC_NODEINFO_typ wcnode;
long n;

n = nodenum;

if (nodenum == 0 ||
    nodenum > (filelength(useronfil) / (long) sizeof(WC_NODEINFO_typ)))
	{
    return 1;
    }

res = lseek(useronfil, n * (long) sizeof(WC_NODEINFO_typ), SEEK_SET);
if (res == -1)
	{
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(WC_NODEINFO_typ),
            sizeof(WC_NODEINFO_typ));
res = read(useronfil, &wcnode, sizeof(WC_NODEINFO_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(WC_NODEINFO_typ),
            sizeof(WC_NODEINFO_typ));
if (res == -1)
	{
    return 1;
    }

unbuild_pascal_string(35, wcnode.name);
unbuild_pascal_string(35, wcnode.handle);
unbuild_pascal_string(25, wcnode.city);
unbuild_pascal_string(10, wcnode.statdesc);

memset(userdata, 0, sizeof(bbsnodedata_typ) - 2);

fixname(userdata->realname, wcnode.name);
strncpy(userdata->handle, wcnode.handle, 30);
fixname(userdata->handle, userdata->handle);
userdata->node = wcnode.line;
userdata->speed = wcnode.baud;
strcpy(userdata->location, wcnode.city);
if (wcnode.status == 255)
	{
    strcpy(userdata->statdesc, wcnode.statdesc);
    }
else
	{
    if (wcnode.status < 8)
        {
        strcpy(userdata->statdesc, wc_statustypes[wcnode.status]);
        }
    else
        {
        userdata->statdesc[0] = 0;
        }
    }
userdata->quiet = wcnode.attribute & wc_NODISTURB;
userdata->hidden = (wcnode.attribute & wc_HIDDEN) ||
                   (wcnode.attribute & wc_READY) ||
                   (wcnode.line == 0);
userdata->attribs1 = wcnode.attribute;
userdata->numcalls = wcnode.numcalls;

return 0;
}

char wc_saveuseron(XINT nodenum, bbsnodedata_typ *userdata)
{
XINT res;
WC_NODEINFO_typ wcnode;
long n;

memset(&wcnode, 0, sizeof(WC_NODEINFO_typ));

strncpy(wcnode.name, userdata->realname, 35);
strcpy(wcnode.handle, userdata->handle);
wcnode.line = userdata->node;
wcnode.baud = userdata->speed;
strncpy(wcnode.city, userdata->location, 25);
wcnode.status = 255;
wcnode.attribute = userdata->attribs1;
if (userdata->quiet)
	{
    wcnode.attribute |= wc_NODISTURB;
    }
else
	{
    wcnode.attribute &= (0xFF - wc_NODISTURB);
    }
strncpy(wcnode.statdesc, userdata->statdesc, 10);
wcnode.numcalls = userdata->numcalls;

build_pascal_string(wcnode.name);
build_pascal_string(wcnode.handle);
build_pascal_string(wcnode.city);
build_pascal_string(wcnode.statdesc);

n = nodenum;
if (filelength(useronfil) < (long) nodenum * (long) sizeof(WC_NODEINFO_typ))
    {
    chsize(useronfil, (long) nodenum * (long) sizeof(WC_NODEINFO_typ));
    }

res = lseek(useronfil, n * (long) sizeof(WC_NODEINFO_typ), SEEK_SET);
if (res == -1)
	{
    return 1;
    }
rec_locking(REC_LOCK, useronfil, n * (long) sizeof(WC_NODEINFO_typ),
            sizeof(WC_NODEINFO_typ));
res = write(useronfil, &wcnode, sizeof(WC_NODEINFO_typ));
rec_locking(REC_UNLOCK, useronfil, n * (long) sizeof(WC_NODEINFO_typ),
            sizeof(WC_NODEINFO_typ));
if (res == -1)
	{
    return 1;
    }

return 0;
}

XINT wc_processmsgs(void)
{
// This can stay streamed for a while
FILE *pnfil = NULL;
char filnam[256];
char XFAR *buffer = NULL;
XINT pos = 0, xpos = 0;
XINT tmp, xh;
char sbbsmsgon = 0;

switch(cfg.bbstype)
	{
    case BBS_RA2:
        sprintf(filnam, "%snode%i.ra", cfg.bbsmultipath, od_control.od_node);
    	break;
    case BBS_SBBS11:
        sprintf(filnam, "%stoline%i", cfg.bbsmultipath, od_control.od_node);
        break;
    }

if (access(filnam, 4))
	{
    return 0;
    }

buffer = malloc(8000);
if (!buffer)
    {
    return 0;
    }


pnfil = _fsopen(filnam, "rb", SH_DENYNO);
if (!pnfil)
	{
    dofree(buffer);
    return 0;
    }

delprompt(TRUE);

if (user.pref1 & PREF1_DUALWINDOW)
    {
    // Different for AVT when I find out what the store/recv codes are.
    od_disp_emu("\x1B" "[u", TRUE);
    top_output(OUT_SCREEN, getlang("DWOutputPrefix"));
    }

top_output(OUT_SCREEN, getlang("RAPageRecvPrefix"));

do
	{
    memset(buffer, 0, 8000);
    fseek(pnfil, (long) xpos * 7900L, SEEK_SET);
    xh = fread(buffer, 1, 8000, pnfil);
    if (xh > 7900)
    	{
        xh = 7900;
        }

    for (pos = 0; pos < xh; pos++)
    	{
        if (!sbbsmsgon && cfg.bbstype == BBS_SBBS11)
        	{
            if (!strnicmp(&buffer[pos], "/MESSAGE", 8))
            	{
                unsigned char *eptr = NULL, *nptr = NULL;
                XINT fromnode;

                pos += 9;
                eptr = strchr(&buffer[pos], ',');
                memset(outbuf, 0, 61);
                strncpy(outbuf, &buffer[pos], eptr - &buffer[pos]);
                pos += (eptr - &buffer[pos]) + 1;
                eptr = strchr(&buffer[pos], '\r');
                nptr = strchr(&buffer[pos], '\n');
                if (nptr < eptr && nptr != NULL)
                	{
                    eptr = nptr;
                    }
                fromnode = atoi(&buffer[pos]);
                itoa(fromnode, outnum[0], 10);
                pos += (eptr - &buffer[pos]) - 1;
                top_output(OUT_SCREEN, "@c");
                top_output(OUT_SCREEN, getlang("SBBSRecvPageHdr"),
                		   outbuf, outnum[0]);
                sbbsmsgon = 1;
                }
            continue;
            }
        if (sbbsmsgon && cfg.bbstype == BBS_SBBS11)
        	{
	        if (!strnicmp(&buffer[pos], "/END/", 5))
            	{
                sbbsmsgon = 0;
                pos += 4;
                top_output(OUT_SCREEN, getlang("RAEnter"));
                while(od_get_key(TRUE) != 13);
                top_output(OUT_SCREEN, getlang("RAEnterSuffix"));
                continue;
                }
			}
        if (buffer[pos] == 1)
        	{
            while(od_get_key(TRUE) != 13);
            continue;
            }
        if (buffer[pos] == 11)
        	{
            if (buffer[pos + 1] == ']')
            	{
                tmp = atoi(&buffer[pos + 2]);
                pos += 4;

                switch(tmp)
                	{
                    case 258: top_output(OUT_SCREEN, getlang("RAEnter")); break;
                    case 496: top_output(OUT_SCREEN, getlang("RAOnNode")); break;
                    case 497: top_output(OUT_SCREEN, getlang("RAMsgFrom")); break;
                    case 628: top_output(OUT_SCREEN, getlang("RAJustPosted")); break;
                    }
                continue;
                }
            if (buffer[pos + 1] == '[')
                {
                unsigned char fgcol, bgcol;

                bgcol = toupper(buffer[pos + 2]);
                fgcol = toupper(buffer[pos + 3]);
                pos += 3;

				if (isxdigit(bgcol))
                	{
                    if (bgcol >= 'A' && bgcol <= 'F')
                    	{
                        bgcol -= ('A' - 10);
                        }
                    else
                    	{
                        bgcol -= '0';
                        }
                    }
				if (isxdigit(fgcol))
                	{
                    if (fgcol >= 'A' && fgcol <= 'F')
                    	{
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

char wc_page(XINT nodenum, unsigned char *pagebuf)
{
FILE *rapgfil = NULL;
unsigned char tmp[41];

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

if (!rapgfil)
	{
    top_output(OUT_SCREEN, getlang("CantPage"), outnum[0]);
    return 1;
    }

filter_string(tmp, cfg.usehandles ? user.handle : user.realname);
itoa(od_control.od_node, outnum[0], 10);

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

fputs(pagebuf, rapgfil);

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

return 0;
}

void wc_setexistbits(bbsnodedata_typ *userdata)
{

userdata->existbits = NEX_REALNAME |
                      NEX_NODE |
                      NEX_SPEED |
                      NEX_LOCATION |
                      NEX_STATUS;

}

void wc_login(void)
{
bbsnodedata_typ rutmp;
XINT res;

memset(&rutmp, 0, sizeof(bbsnodedata_typ) - 2);
fixname(rutmp.realname, user.realname);
fixname(rutmp.handle, user.handle);
rutmp.node = od_control.od_node;
rutmp.speed = od_control.baud;
strcpy(rutmp.location, od_control.user_location);
strcpy(rutmp.statdesc, getlang("NodeStatus"));
rutmp.attribs1 = 0; // change to use DND status!
res = (*bbs_call_saveuseron)(od_control.od_node, &rutmp);
if (!res)
    {
    FILE *rnfil = NULL;

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

void wc_logout(void)
{
bbsnodedata_typ rutmp;

if (localmode || lanmode)
	{
    memset(&rutmp, 0, sizeof(bbsnodedata_typ) - 2);
    rutmp.quiet = 1;
    rutmp.attribs1 = wc_HIDDEN & wc_NODISTURB;
	rutmp.node = 0;
    (*bbs_call_saveuseron)(od_control.od_node, &rutmp);
    if (cfg.bbstype == BBS_RA2)
    	{
        itoa(od_control.od_node, outnum[0], 10);
		unlink(top_output(OUT_STRING, "@1rabusy.@2", cfg.bbsmultipath,
					      outnum[0]));
        }
    }

}

XINT wc_openfiles(void)
{
XINT bbsres = 0;

switch(cfg.bbstype)
	{
    case BBS_RA2:
		useronfil = sh_open(top_output(OUT_STRING, "@1useron.bbs",
									   cfg.bbspath),
							O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
							S_IREAD | S_IWRITE);
        break;
    case BBS_SBBS11:
        return 0;
	}
bbsres += (useronfil == -1);

return bbsres;
}

XINT wc_updateopenfiles(void)
{

return 0;
}

void wc_init(void)
{

bbs_call_loaduseron = wc_loaduseron;
bbs_call_saveuseron = wc_saveuseron;
bbs_call_processmsgs = wc_processmsgs;
bbs_call_page = wc_page;
bbs_call_setexistbits = wc_setexistbits;
bbs_call_login = wc_login;
bbs_call_logout = wc_logout;
bbs_call_openfiles = wc_openfiles;
bbs_call_updateopenfiles = wc_updateopenfiles;

}

