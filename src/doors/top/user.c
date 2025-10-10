/******************************************************************************
USER.C       User management functions.

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

This module contains functions that manipulate the user file.  It also contains
functions like the user search function and the new user signup function.
******************************************************************************/

#include "strwrap.h"
#include "top.h"

/* The user login procedure writes to the debug log. */
extern void wdl(char *msg);

/* search_for_user() - Displays welcome and finds a user in USERS.TOP.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  The actual user searching has been moved to the
           find_user_from_name() function, leaving this function somewhat
           misnamed.  This function is intended to be called after the
           initialization has been completed.
*/
void search_for_user(void)
{
unsigned char tmp[256]; /* Temporary input buffer. */
#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
struct date mydate; /* 2-byte int date holder for 32 bit systems. */
#endif

wdl("User search function has begun");

welcomescreen();

wdl("Welcome screen displayed");

if (localmode || lanmode)
	{
    char tfix; /* Temporary name fix flag.  Used but has no effect. */

    /* Since local and LAN modes do not use a drop file, the user must
       be asked to enter a name. */
    do
    	{
        top_output(OUT_SCREEN, getlang((unsigned char *)"EnterName"));
        od_input_str((char*)tmp, 30, ' ', MAXASCII);
        }
    while(!tmp[0] || censorinput(tmp));
    filter_string(tmp, tmp);
    trim_string((unsigned char *)od_control.user_name, tmp, 0);
    /* Local users are usually sysops so they are given the highest
       possible security to start.  If the user is later found in the
       user file, their normal security value will be used instead of
       65535. */
    user.security = 65535;
    /* There were plans to make name fixing configurable until some
       problems occured relating to the door kit.  This section
       is supposed to temporarily override the configured setting to
       force fixing of the user's real name, which it does, but obviously
       with there being no FixNames setting available to the user in
       TOP.CFG there is no need to temporarily preserve the value. */
    tfix = cfg.fixnames;
    cfg.fixnames = 1;
    fixname((unsigned char *)od_control.user_name, (unsigned char *)od_control.user_name);
    cfg.fixnames = tfix;
    od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogNameUsed"),
                 od_control.user_name));
wdl("Local name obtained from user");
    }
else wdl("Local name not needed");

fixname((unsigned char*)od_control.user_name, (unsigned char*)od_control.user_name);
top_output(OUT_SCREEN, getlang((unsigned char *)"Searching"), od_control.user_name);

wdl("Pre-search completed");

/* Find the user in the user file.  As mentioned in the notes, this was
   previously done directly inside this function.  However, as TOP's
   development progressed the need arose for user name searching to occur
   elsewhere, so it was functionalized for efficiency. */
user_rec_num = find_user_from_name((unsigned char *)od_control.user_name, &user,
                                   UFIND_REALNAME);

wdl("User search completed");

if (user_rec_num < 0)
	{
    /* No valid user record found. */

wdl("New user - running new user function");
    user_rec_num = filelength(userfil) / sizeof(user_data_typ);
    make_new_user(user_rec_num);
    }
else
	{
    /* User record found. */

    XINT tries = 0;
wdl("Old user found");

    /* Get the user's TOP password if one exists. */
    if (user.password[0])
   		{
		do
    		{
            top_output(OUT_SCREEN, getlang((unsigned char *)"EnterPW"));
            get_password(tmp, 15);
            if (stricmp((char*)user.password, (char*)tmp))
            	{
                top_output(OUT_SCREEN, getlang((unsigned char *)"InvalidPW"));
                tries++;
                }
	        }
        while(stricmp((char*)user.password, (char*)tmp) && tries < MAXPWTRIES);
    	if (tries == MAXPWTRIES)
	    	{
            itoa(MAXPWTRIES, (char*)outnum[0], 10);
            top_output(OUT_SCREEN, getlang((unsigned char *)"MaxPW"), outnum[0]);
            od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogMaxPW")));
            quit_top();
        	}
wdl("Password obtained from user");
        }
else wdl("No password needed");

    fixname(user.handle, user.handle);
    itoa(user_rec_num + 1, (char*)outnum[0], 10);
    itoa(calc_days_ago(&user.last_use), (char*)outnum[1], 10);
    top_output(OUT_SCREEN, getlang((unsigned char *)"Greeting"), user.handle,
               outnum[0], outnum[1]);
    top_output(OUT_SCREEN, getlang((unsigned char *)"Setup"));
    }

wdl("Greeting displayed - now setting up");

#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
/* OS/2's and Win95's date struct use regular int variables, which under
   those compilers are 4-bytes long.  However, USERS.TOP stores a date
   with 2-byte int variables.  This is solved by simple assignment from
   a temporary 4-byte int date. */
getdate(&mydate);
/* The user file uses a 2-byte int XDATE struct, as defined in TOP.H. */
user.last_use.da_year = mydate.da_year;
user.last_use.da_day = mydate.da_day;
user.last_use.da_mon = mydate.da_mon;
#else
getdate(&user.last_use);
#endif

/* More code related to configurable name fixing. */
/*if (forcefix)
    {
    if (cfg.allownewhandles)
        {
        if (!stricmp(user.handle, od_control.user_handle))
            {
            if (strcmp(user.handle, od_control.user_handle))
                {
                strcpy(user.handle, od_control.user_handle);
                }
            }
        else
            {
            if (!stricmp(user.handle, od_control.user_name))
                {
                if (strcmp(user.handle, od_control.user_name))
                    {
                    strcpy(user.handle, od_control.user_name);
                    }
                }
            }
        }
    else
        {
        if (cfg.allowhandles && od_control.user_handle[0])
            {
            if (!stricmp(user.handle, od_control.user_handle))
                {
                if (strcmp(user.handle, od_control.user_handle))
                    {
                    strcpy(user.handle, od_control.user_handle);
                    }
                }
            }
        else
            {
            if (!stricmp(user.handle, od_control.user_name))
                {
                if (strcmp(user.handle, od_control.user_name))
                    {
                    strcpy(user.handle, od_control.user_name);
                    }
                }
            }
        }
    }*/

save_user_data(user_rec_num, &user);
wdl("User data updated");

/* Force the user to fill out their bio before proceeding if the sysop has
   so configured. */
if (cfg.forcebio)
    {
    if (!biocheckalldone())
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"BioForcePrefix"));
        while(biogetall() != numbioques)
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"BioForceAllNotDone"));
            }
        }
    }

/* Initialize personal actions. */
if (!loadpersactions())
    {
    od_exit(206, FALSE);
    }
register_node();
wdl("Node registered and logged in");
od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogHandle"), user.handle));

wdl("Post-registry completed");

if (localmode || lanmode)
	{
    /* Force some door kit variables in local/LAN mode. */
    od_control.user_timelimit = 1440;
    od_control.user_security = user.security;
    od_control.od_status_on = TRUE;
    /* Update the status line one last time to show the user's real name
       and location. */
    od_set_statusline(STATUS_NONE);
    trim_string((unsigned char *)od_control.user_location, (unsigned char *)od_control.user_location, 0);
    od_set_statusline(STATUS_NORMAL);
    od_kernel();
    od_control.od_status_on = FALSE;
wdl("Secondary local init completed");
    }
else wdl("No local init needed");

wdl("User search completed");
wdl("If TOP made it here, everything should be okay");

return;
}

/* load_user_data() - Loads a user record from USERS.TOP.
   Parameters:  recnum - Record number (0-based) of user to load.
                buffer - Pointer to buffer to hold user data.
   Returns:  TRUE on success, FALSE if an error occurred.
*/
char load_user_data(XINT recnum, user_data_typ *buffer)
{
XINT res; /* Result code. */

/* Abort if the user file can't possibly contain the requested record
   because it is too small. */
if (filelength(userfil) <= (long) recnum * (long) sizeof(user_data_typ))
	{
    return 0;
    }

/* Read in user data. */
res = lseek(userfil, (long) recnum * (long) sizeof(user_data_typ), SEEK_SET);
if (res == -1)
	{
    return 0;
    }
rec_locking(REC_LOCK, userfil, (long) recnum * (long) sizeof(user_data_typ),
            sizeof(user_data_typ));
res = read(userfil, buffer, sizeof(user_data_typ));
rec_locking(REC_UNLOCK, userfil, (long) recnum * (long) sizeof(user_data_typ),
            sizeof(user_data_typ));
if (res == -1)
	{
    return 0;
    }

return 1;
}

/* save_user_data() - Saves a user record to USERS.TOP.
   Parameters:  recnum - Record number (0-based) of user to save.
                buffer - Pointer to buffer holding user data.
   Returns:  TRUE on success, FALSE if an error occurred.
*/
char save_user_data(XINT recnum, user_data_typ *buffer)
{
XINT res; /* Result code. */

/* Write the data. */
res = lseek(userfil, (long) recnum * (long) sizeof(user_data_typ),
            SEEK_SET);
if (res == -1)
	{
    return 0;
    }
rec_locking(REC_LOCK, userfil, (long) recnum * (long) sizeof(user_data_typ),
            sizeof(user_data_typ));

res = write(userfil, buffer, sizeof(user_data_typ));
rec_locking(REC_UNLOCK, userfil, (long) recnum * (long) sizeof(user_data_typ),
            sizeof(user_data_typ));
if (res == -1)
    {
    return 0;
    }

return 1;
}

/* make_new_user() - New user signup function.
   Parameters:  recnum - User file record number to hold new user data.
   Returns:  Nothing.
*/
void make_new_user(XINT recnum)
{
XINT res; /* Result code. */
unsigned char mtmp[256]; /* Temporary input buffer. */
#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
struct date mydate; /* 2-byte int date holder for 32 bit systems. */
#endif

memset(&user, 0, sizeof(user_data_typ));

top_output(OUT_SCREEN, getlang((unsigned char *)"NewUserScrnPrefix"));
newuserscreen();
od_log_write((char *)top_output(OUT_STRING, getlang((unsigned char *)"LogStartSignOn")));

if (cfg.allownewhandles)
    {
    /* Ask the user for a new handle. */

    /* Loop until a valid handle is input. */
    do
        {
        do
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"EnterHandle"));
            od_input_str((char *)user.handle, 30, ' ', MAXASCII);
            }
        while(censorinput(user.handle));

        fixname(user.handle, user.handle);

        /* Abort if the user just hits ENTER. */
        if (user.handle[0] == '\0')
            {
            od_log_write((char *)top_output(OUT_STRING, getlang((unsigned char *)"LogAbortSignOn")));
            quit_top();
            }

        trim_string(user.handle, user.handle, 0);
        res = check_dupe_handles(user.handle);
        if (res)
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"HandleInUse"));
            }
        }
    while(res);
    }
else
    {
    /* Select the right name (real or handle) to use based on how TOP is
       configured. */
    if (cfg.allowhandles && od_control.user_handle[0])
        {
        fixname(user.handle, (unsigned char *)od_control.user_handle);
        }
    else
        {
        fixname(user.handle, (unsigned char *)od_control.user_name);
        }
    }

/* User description is no longer used. */
/* memset(user.description, '\0', 61); */

/* Under local and LAN modes it is possible for any user to login under any
   name so to protect new users signing on in these modes, TOP will ask for
   a password. */
if (localmode || lanmode)
	{
    XINT pwok = 0; /* Flag if the password is confirmed. */

    do
    	{
        top_output(OUT_SCREEN, getlang((unsigned char *)"EnterNewPW"));
        get_password(user.password, 15);
        top_output(OUT_SCREEN, getlang((unsigned char *)"ConfirmPW"));
        get_password(mtmp, 15);
        if (stricmp((char*)user.password, (char*)mtmp))
        	{
            top_output(OUT_SCREEN, getlang((unsigned char *)"PWNoMatch"));
            pwok = 0;
            }
        else
        	{
            pwok = 1;
            }
        }
    while(!pwok);
    top_output(OUT_SCREEN, getlang((unsigned char *)"LocalLANStartPref"));
    }

user.gender = 0;

/* Under CHAIN.TXT and RA 2.xx's EXITINFO.BBS, the user's gender is
   included in the drop file so there is no need to ask. */
if (od_control.od_info_type != RA2EXITINFO &&
    od_control.od_info_type != CHAINTXT)
	{
    top_output(OUT_SCREEN, getlang((unsigned char *)"GenderPrompt"));
    od_control.user_sex = od_get_answer((char*)getlang((unsigned char *)"GenderKeys"));
    if (od_control.user_sex == getlangchar((unsigned char *)"GenderKeys", 1))
    	{
        top_output(OUT_SCREEN, getlang((unsigned char *)"Female"));
        }
    if (od_control.user_sex == getlangchar((unsigned char *)"GenderKeys", 0))
    	{
        top_output(OUT_SCREEN, getlang((unsigned char *)"Male"));
        }
    top_output(OUT_SCREEN, getlang((unsigned char *)"GenderPromptSuffix"));
    }
if (od_control.user_sex == getlangchar((unsigned char *)"GenderKeys", 1))
   	{
    user.gender = 1;
    }
if (od_control.user_sex == getlangchar((unsigned char *)"GenderKeys", 0))
   	{
    user.gender = 0;
    }

/* User description is no longer used. */
/*top_output(OUT_SCREEN, getlang((unsigned char *)"EnterDescPrompt"));
od_input_str(user.description, 60, ' ', MAXASCII);*/

top_output(OUT_SCREEN, getlang((unsigned char *)"Creating"));

/*trim_string(user.description, user.description, 0);*/
// trim_string(user.realname, user.realname, 0);
fixname(user.realname, (unsigned char *)od_control.user_name);

/* See search_for_user() for explanation of this section. */
#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
getdate(&mydate);
user.last_use.da_year = mydate.da_year;
user.last_use.da_day = mydate.da_day;
user.last_use.da_mon = mydate.da_mon;
#else
getdate(&user.last_use);
#endif

/* Original preferences from before they were configurable. */
// user.pref1 = PREF1_ECHOACTIONS | PREF1_ECHOTALKTYP | PREF1_ECHOTALKMSG |
//             PREF1_MSGSENT | PREF1_ECHOGA | PREF1_TALKMSGSENT;
// user.pref2 = 0; user.pref3 = 0;

user.pref1 = cfg.defaultprefs[0];
user.pref2 = cfg.defaultprefs[1];
user.pref3 = cfg.defaultprefs[2];

if (cfg.newusersec == 0)
    {
    user.security = od_control.user_security;
    }
else
    {
    user.security = cfg.newusersec;
    }

save_user_data(recnum, &user);

return;
}

/* The active handle index used to be updated by this function, until I
   found out that I was calling it just about every time check_nodes_used()
   was called.  The index is now updated by check_nodes_used(). */
/*void load_online_handles(void)
{
XINT d;
node_idx_typ nodeon;

check_nodes_used(TRUE);

for (d = 0; d < MAXNODES; d++)
    {
    if (activenodes[d])
        {
        get_node_data(d, &nodeon);
        fixname(handles[d].string, nodeon.handle);
        }
    }

return;
}*/

/* Old pre-bio user lookup.  It is still used by a function in MAINT.C. */
void lookup(unsigned char *string)
{
user_data_typ udat;
unsigned char tmp[256]; // Dynam with MAXSTRLEN!
XINT d, u, ns = 0;
long siz;
struct file_stats_str udatdat;

if (!string[0])
	{
    top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpPrompt"));

    od_input_str((char*)tmp, 30, ' ', MAXASCII);
    }
else
	{
    strcpy((char*)tmp, (char*)string);
    top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpPrefix"));
    }

filter_string(tmp, tmp);
trim_string(tmp, tmp, 0);

siz = 0L;
sprintf((char*)outbuf, "%susers.top", cfg.toppath);
udatdat.fsize=flength((char*)outbuf);
if (!d)
	{
    siz = udatdat.fsize;
    }

if (tmp[0])
	{
    top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSearching"), tmp);
	}

d = 0; u = 0;
while((long) d * (long) sizeof(user_data_typ) < siz)
	{
    char sex[2];

    load_user_data(d, &udat);
    filter_string(udat.handle, udat.handle);
    if ((strstr(strupr((char*)udat.handle), strupr((char*)tmp)) || !tmp[0]) &&
		udat.realname[0])
    	{
        if (udat.gender == 0)
        	{
            sex[0] = 'M';
            }
        if (udat.gender == 1)
        	{
            sex[0] = 'F';
            }
        sex[1] = '\0';
        top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSep"));
        if (cfg.usehandles)
            {
            filter_string(outbuf, udat.handle);
            fixname(outbuf, udat.handle);
            }
        else
            {
            fixname(outbuf, udat.realname);
            }
        itoa(d, (char *)outnum[0], 10);
        top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpDisplay1"), outnum[0],
                   getlang(cfg.usehandles ? (unsigned char *)"LookUpHandle" :
                           (unsigned char *)"LookUpRealName"), outbuf, sex);
        itoa(udat.last_use.da_mon, (char*)outnum[0], 10);
        itoa(udat.last_use.da_day, (char*)outnum[1], 10);
        itoa(udat.last_use.da_year, (char*)outnum[2], 10);
        top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpDisplay2"), outnum[0],
                   outnum[1], outnum[2]);
/*        filter_string(outbuf, udat.description);*/
        top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpDisplay3"), outbuf);
        u++;
		}
    d++;
    if (u == 5 && !ns && ((long) d * (long) sizeof(user_data_typ) < siz))
    	{
        XINT key;

        u = 0;
        top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSep"));
        ns = moreprompt();
        if (ns == -1)
            {
            break;
            }
        }
    }

top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSep"));
top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSuffix1"));
top_output(OUT_SCREEN, getlang((unsigned char *)"PressAnyKey"));
od_get_key(TRUE);
top_output(OUT_SCREEN, getlang((unsigned char *)"LookUpSuffix2"));

return;
}

/* check_dupe_handles() - Checks for a duplicate handle in USERS.TOP.
   Parameters:  string - Handle to check for duplication.
   Returns:  TRUE if a duplicate exists, FALSE if the specified handle
             is unique.
*/
char check_dupe_handles(unsigned char *string)
{
XINT d, res; /* Counter, result code. */
unsigned char thandstr[31]; /* Temporary handle holder. */
unsigned char ctt[256]; /* Holds a filtered and trimmed version of string. */

filter_string(ctt, string);
trim_string(ctt, ctt, 0);
sprintf((char*)outbuf, "%susers.top", cfg.toppath);
if (access((char*)outbuf, 0))
	{
    return 0;
    }

/* Loop for each user record. */
for (d = 0; d < (filelength(userfil) / (long) sizeof(user_data_typ)); d++)
	{
    /* This function does not use find_user_from_name() for two reasons.
       The first is that it only needs the handle, which means it only
       needs the first 31 bytes of the user file.  The second is that
       find_user_from_name() loads the entire user record once a match
       is found, which is unneeded here. */
    res = lseek(userfil, ((long) d * (long) sizeof(user_data_typ)) + 41L,
                SEEK_SET);
    if (res == -1)
    	{
        errorabort(ERR_CANTACCESS, (unsigned char *)"users.top");
        }
    rec_locking(REC_LOCK, userfil,
                ((long) d * (long) sizeof(user_data_typ)) + 41L,
                sizeof(user_data_typ));
    res = read(userfil, thandstr, 31);
    rec_locking(REC_UNLOCK, userfil,
                ((long) d * (long) sizeof(user_data_typ)) + 41L,
                sizeof(user_data_typ));
    if (res == -1)
    	{
        errorabort(ERR_CANTREAD, (unsigned char *)"users.top");
        }
    filter_string(thandstr, thandstr);
    if (!stricmp((char*)thandstr, (char*)ctt))
    	{
        return 1;
        }
    }

return 0;
}

/* find_user_from_name() - Load a user record from a name or handle.
   Parameters:  uname - String holding the name to match.
                ubuf - Pointer to buffer to hold the loaded user data.
                nuse - Matching option (UFIND_ constant).
   Returns:  Record number of the user if one was found, or -1 on no
             match. */
XINT find_user_from_name(unsigned char *uname, user_data_typ *ubuf,
                         XINT nuse)
    {
    XINT unum = -1, d; /* User record number, counter. */
    long s; /* Size of user file, in records. */

    /* Use the configured matching criteria. */
    if (nuse == UFIND_USECFG)
        {
        nuse = cfg.usehandles + 1;
        }
    s = filelength(userfil) / sizeof(user_data_typ);
    /* Loop for each user record. */
    for (d = 0; d < s && unum < 0; d++)
        {
        rec_locking(REC_LOCK, userfil, (long) d * sizeof(user_data_typ),
                    sizeof(user_data_typ));
        lseek(userfil, (long) d * sizeof(user_data_typ), SEEK_SET);
        /* Only load the first 72 bytes for speed, which contains the
           handle and real name of the user. */
        read(userfil, ubuf, 72L);
        rec_locking(REC_UNLOCK, userfil, (long) d * sizeof(user_data_typ),
                    sizeof(user_data_typ));
        if (!stricmp((char*)uname,
                     nuse == UFIND_HANDLE ? (char*)ubuf->handle : (char*)ubuf->realname))
            {
            /* Match found, load the user data. */
            unum = d;
            if (ubuf != NULL)
                {
                load_user_data(unum, ubuf);
                }
            }
        }

    return unum;
    }
