#include "top.h"

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

void usersec(void);

void useredit(void)
{
user_data_typ udat;
XINT res, udel;
char ustr[5];

top_output(OUT_SCREEN, "\r\n^pTOP User Deletor");
lookup("\0");

top_output(OUT_SCREEN, "^oEnter a user number to delete, S to change a user's security,@cor ENTER to abort.\r\n");
top_output(OUT_SCREEN, "^j---> ^k");

od_input_str(ustr, 4, '0', 'z');
if (!stricmp(ustr, "S"))
    {
    usersec();
    od_exit(0, FALSE);
    }
res = 0; udel = (atol(ustr) - 1U);
top_output(OUT_SCREEN, "\r\n");

if (udel > 0)
	{
    res = load_user_data(udel, &udat);
    }

if (!res)
	{
    top_output(OUT_SCREEN, "^nAborted.");
    }
else
	{
    sprintf(outbuf, "^mDelete ^p%s^m (^p%s^m) (y/N)? ",
    		udat.realname, udat.handle);
    top_output(OUT_SCREEN, outbuf);
	res = toupper(od_get_key(TRUE));
    if (res == 'Y')
    	{
        memset(&udat, 0, sizeof(user_data_typ));
        save_user_data(udel, &udat);
        top_output(OUT_SCREEN, "^kYes!\r\n\r\n^lDeleted!");
        od_log_write(top_output(OUT_STRING, getlang("LogDelUser"),
                                udat.handle));
        }
	else
    	{
        top_output(OUT_SCREEN, "^kNo!\r\n\r\n^nAborted.");
        }
    }

top_output(OUT_SCREEN, "\r\n");

od_exit(0, FALSE);

return;
}

void userpack(void)
{
user_data_typ udat;
XINT uc, udc, unc, res, unewfil, tfil;
char tnam1[256], tnam2[256]; /////////Both dynamic with MAXSTRLEN!
long cashreset = -1, secreset = -1;
top_output(OUT_SCREEN, "\r\n^pPerforming user packing...\r\n");
od_log_write(top_output(OUT_STRING, getlang("LogPacking")));
close(userfil);

if (word_str[0])
    {
    secreset = atol(word_str);
    }

if (outbuf[0])
	{
    cashreset = atol(outbuf);
    }

sprintf(tnam1, "%susers.top", cfg.toppath);
sprintf(tnam2, "%susers.old", cfg.toppath);

if (!access(tnam2, 0))
	{
    sprintf(outbuf, "^nDeleting old file - %s\r\n", tnam2);
    top_output(OUT_SCREEN, outbuf);
    unlink(tnam2);
    }

sprintf(outbuf, "^nRenaming %s to %s...", tnam1, tnam2);
top_output(OUT_SCREEN, outbuf);
rename(tnam1, tnam2);

top_output(OUT_SCREEN, "^n\r\nOpening new files...");
userfil = sh_open(tnam2, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
				  S_IREAD | S_IWRITE);
unewfil = sh_open(tnam1, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
				  S_IREAD | S_IWRITE);

if (userfil == -1 || unewfil == -1)
	{
    close(userfil); close(unewfil);
    top_output(OUT_SCREEN, "^I^mCAN'T DO!!!^A^n\r\n");
    top_output(OUT_SCREEN, "Aborting.\r\n");
    od_exit(0, FALSE);
    }

top_output(OUT_SCREEN, "\r\n^nPacking old user file...    0");
for (uc = 0, udc = 0, unc = 0;
	 uc < ((long) filelength(userfil) / (long) sizeof(user_data_typ));
	 uc++)
     {
     res = load_user_data(uc, &udat);
     if (res && !udat.realname[0])
     	{
        udc++;
        }
     if (res && udat.realname[0])
     	{
        tfil = userfil; userfil = unewfil;
        if (secreset != -1)
        	{
            udat.security = secreset;
            }
        if (cashreset != -1)
        	{
            udat.cybercash = cashreset;
            }
        save_user_data(unc, &udat);
        unc++;
        userfil = tfil;
		}
    sprintf(outbuf, "\b\b\b\b\b%5i", uc);
    top_output(OUT_SCREEN, outbuf);
	}

close(userfil); close(unewfil);
top_output(OUT_SCREEN, "\b\b\b\b\b^I^kDone!^A^o\r\n\r\n");

sprintf(outbuf, "^oRecords processed:  ^l%-5i   ^oRecords deleted:  "
		"^l%-5i   ^oNew user count:  ^l%-5i\r\n", uc, udc, unc);
top_output(OUT_SCREEN, outbuf);

od_exit(0, FALSE);

return;
}

void usersec(void)
    {
    char tstr[10];
    user_data_typ usu;

    top_output(OUT_SCREEN, "RECORD NUMBER of user to upgrade to 65535 security (usually will be 0).@c");
    top_output(OUT_SCREEN, "---> ");

    od_input_str(tstr, 9, '0', '9');
    if (!tstr[0])
        {
        top_output(OUT_SCREEN, "@c@cAborted.@c@c");
        return;
        }

    if (!load_user_data(atoi(tstr), &usu))
        {
        top_output(OUT_SCREEN, "@c@cInvalid user record number!@c@c");
        return;
        }

    usu.security = 65535;

    save_user_data(atoi(tstr), &usu);

    top_output(OUT_SCREEN, "@c@cSecurity for \"@1\" upgraded to 65535.@cThis user can now use the SYSOP SETSEC command.@c@c",
        usu.realname);

    }
