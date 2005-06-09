/******************************************************************************
CFG.C        Configuration file processor.

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

This module contains the code to load and process TOP.CFG.
******************************************************************************/

#include "top.h"
#include "strwrap.h"

/* Shorthand macro to save typing. */
#define chkkeyword(kwd) (!stricmp(keyword, kwd))

/* processcfg() - Processes the TOP.CFG file.
   Parameters:  keyword - Keyword (first word) of the line being processed.
                options - Options (remainder) of the line being processed.
   Returns:  Nothing.
   Notes:  TOP never calls this function directly.  It is called by
           OpenDoors or Doors/2 during the od_init() function.
*/
#ifdef __OS2__
void LIBENTRY processcfg(char *keyword, char *options)
#else
void processcfg(char *keyword, char *options)
#endif
{
XINT cd;

// Set defaults

// Each keyword needs an error detection system.  Also need a USED variable.

/* See TOP.CFG for details about each keyword.  Note that except in rare
   cases, none of the described setting limitations are actually enforced. */

if (chkkeyword("BBSType"))
    {
    for (cd = 0; cd < BBSTYPES; cd++)
        {
        /* Compare options to a known BBS type name. */
        if (!stricmp(options, bbsnames[cd]))
            {
            cfg.bbstype = cd;
#ifndef __OS2__
            /* Maximus v3.0x support is the same as v2.0x support under all
               platforms except OS/2. */
            if (cfg.bbstype == BBS_MAX3)
                {
                cfg.bbstype = BBS_MAX2;
                }
#endif
            break;
            }
        }
    }
if (chkkeyword("BBSName"))
    {
    strset(cfg.bbsname, 0);
    strncpy(cfg.bbsname, options, 40);
    }
if (chkkeyword("SysopName"))
    {
    strset(cfg.sysopname, 0);
    strncpy(cfg.sysopname, options, 40);
    }
if (chkkeyword("TOPPath"))
	{
    strset(cfg.toppath, 0);
    strncpy(cfg.toppath, options, 79);
    /* strupr(cfg.toppath); */
    verifypath(cfg.toppath);
    }
if (chkkeyword("TOPWorkPath"))
	{
    strset(cfg.topworkpath, 0);
    strncpy(cfg.topworkpath, options, 79);
    /* strupr(cfg.topworkpath); */
    verifypath(cfg.topworkpath);
    }
if (chkkeyword("TOPANSIPath"))
	{
    strset(cfg.topansipath, 0);
    strncpy(cfg.topansipath, options, 79);
    /* strupr(cfg.topansipath); */
    verifypath(cfg.topansipath);
    }
if (chkkeyword("BBSPath"))
	{
    strset(cfg.bbspath, 0);
    strncpy(cfg.bbspath, options, 79);
    /* strupr(cfg.bbspath); */
    verifypath(cfg.bbspath);
    }
if (chkkeyword("BBSIPCPath"))
	{
    strset(cfg.bbsmultipath, 0);
    strncpy(cfg.bbsmultipath, options, 79);
    /* strupr(cfg.bbsmultipath); */
    verifypath(cfg.bbsmultipath);
    }
if (chkkeyword("TOPActionPath"))
	{
    strset(cfg.topactpath, 0);
    strncpy(cfg.topactpath, options, 79);
    /* strupr(cfg.topactpath); */
    verifypath(cfg.topactpath);
    }
if (chkkeyword("SecuritySysop"))
	{
    cfg.sysopsec = atol(options);
    }
if (chkkeyword("SecurityNewUser"))
	{
    cfg.newusersec = atol(options);
    }
if (chkkeyword("SecurityTalk"))
	{
    cfg.talksec = atol(options);
    }
if (chkkeyword("SecurityChangeHandle"))
	{
    cfg.handlechgsec = atol(options);
    }
if (chkkeyword("SecurityForget"))
	{
    cfg.forgetsec = atol(options);
    }
if (chkkeyword("SecurityChangeSex"))
	{
    cfg.sexchangesec = atol(options);
    }
if (chkkeyword("SecurityChangeEXMsg"))
	{
    cfg.exmsgchangesec = atol(options);
    }
if (chkkeyword("SecuritySendPrivate"))
	{
    cfg.privmessagesec = atol(options);
    }
if (chkkeyword("SecurityActionUse"))
	{
    cfg.actionusesec = atol(options);
    }
if (chkkeyword("ShowTitle"))
	{
    cfg.showtitle = seektruth(options);
    }
if (chkkeyword("ShowNews"))
	{
    cfg.shownews = seektruth(options);
    }
if (chkkeyword("AllowActions"))
	{
    cfg.allowactions = seektruth(options);
    }
if (chkkeyword("AllowActions"))
	{
    cfg.allowgas = seektruth(options);
    }
if (chkkeyword("AllowHandles"))
	{
    cfg.allowhandles = seektruth(options);
    }
if (chkkeyword("AllowNewHandles"))
	{
    cfg.allownewhandles = seektruth(options);
    }
if (chkkeyword("AllowChangeHandle"))
	{
    cfg.allowhandlechange = seektruth(options);
    }
if (chkkeyword("AllowChangeSex"))
	{
    cfg.allowsexchange = seektruth(options);
    }
if (chkkeyword("AllowChangeEXMsg"))
	{
    cfg.allowexmessages = seektruth(options);
    }
if (chkkeyword("AllowPrivateMsgs"))
	{
    cfg.allowprivmsgs = seektruth(options);
    }
if (chkkeyword("AllowForgetting"))
	{
    cfg.allowforgetting = seektruth(options);
    }
if (chkkeyword("AllowHighASCII"))
	{
    cfg.allowhighascii = seektruth(options);
    }
if (chkkeyword("ShowOpeningCredits"))
	{
    cfg.showopeningcred = seektruth(options);
    }
if (chkkeyword("ShowClosingCredits"))
	{
    cfg.showclosingcred = seektruth(options);
    }
if (chkkeyword("UseHandles"))
	{
    cfg.usehandles = seektruth(options);
    }
if (chkkeyword("NoRegName"))
	{
    cfg.noregname = seektruth(options);
    }
if (chkkeyword("InactiveTimeout"))
	{
    cfg.inactivetime = atoi(options);
    }
if (chkkeyword("LocalBeeping"))
	{
    strupr(options);
    cfg.localbeeping = BEEP_NONE;
    /* Set flags for local beeping options. */
    if (strstr(options, "NONE")) cfg.localbeeping = BEEP_NONE;
    if (strstr(options, "REMOTE")) cfg.localbeeping |= BEEP_REMOTE;
    if (strstr(options, "LOCAL")) cfg.localbeeping |= BEEP_LOCAL;
    if (strstr(options, "LAN")) cfg.localbeeping |= BEEP_LAN;
    if (strstr(options, "ALL")) cfg.localbeeping = BEEP_ALL;
    }
if (chkkeyword("LangFile"))
	{
    strset(cfg.langfile, 0);
    strncpy(cfg.langfile, options, 8);
    /* strupr(cfg.langfile); */
    strcat(cfg.langfile, ".lng");
	}
if (chkkeyword("PollDelay"))
	{
    cfg.polldelay = atol(options);
    }
if (chkkeyword("RetryDelay"))
	{
    cfg.retrydelay = atol(options);
    }
if (chkkeyword("RetryMax"))
	{
    cfg.maxretries = atol(options);
    }
if (chkkeyword("CrashProtDelay"))
	{
    cfg.crashprotdelay = atol(options);
    }
if (chkkeyword("MaxNodes"))
    {
    cfg.maxnodes = atoi(options);
    }
if (chkkeyword("MaxPWTries"))
    {
    cfg.maxpwtries = atoi(options);
    }
if (chkkeyword("ActionFiles"))
	{
    memset(cfg.actfilestr, 0, 256);
    strncpy(cfg.actfilestr, options, 255);
    }
if (chkkeyword("MaxChannelDefs"))
    {
    cfg.maxchandefs = strtol(options, NULL, 10);
    }
if (chkkeyword("PrivChatBufSize"))
    {
    cfg.privchatbufsize = strtol(options, NULL, 10);
    }
if (chkkeyword("PrivChatMaxTries"))
    {
    cfg.privchatmaxtries = strtol(options, NULL, 10);
    }
if (chkkeyword("UseCredits"))
    {
    cfg.usecredits = strtol(options, NULL, 10);
    }
if (chkkeyword("DefaultChannel"))
    {
    cfg.defaultchannel = strtoul(options, NULL, 10);
    }
if (chkkeyword("MaxCensorWarnHigh"))
    {
    cfg.maxcensorwarnhigh = atoi(options);
    }
if (chkkeyword("MaxCensorWarnLow"))
    {
    cfg.maxcensorwarnlow = atoi(options);
    }
if (chkkeyword("CensorTimerHigh"))
    {
    cfg.censortimerhigh = atoi(options);
    }
if (chkkeyword("CensorTimerLow"))
    {
    cfg.censortimerlow = atoi(options);
    }
if (chkkeyword("DefaultPrefs"))
    {
    cfg.defaultprefs[0] = 0;
    cfg.defaultprefs[1] = 0;
    cfg.defaultprefs[2] = 0;
    /* Read the first 8 characters to process PREF1_ options. */
    for (cd = 0; cd < 8; cd++)
        {
        if (options[cd] == '1' || toupper(options[cd]) == 'Y')
            {
            cfg.defaultprefs[0] |= 1 << cd;
            }
        else
            {
            cfg.defaultprefs[0] |= 0 << cd;
            }
        }
    /* Read the last 4 characters to process PREF2_ options. */
    for (cd = 0; cd < 4; cd++)
        {
        if (options[cd + 8] == '1' || toupper(options[cd + 8]) == 'Y')
            {
            cfg.defaultprefs[1] |= 1 << cd;
            }
        else
            {
            cfg.defaultprefs[1] |= 0 << cd;
            }
        }
    }
if (chkkeyword("ActionPrefix"))
    {
    if (!stricmp(options, "NONE"))
        {
        cfg.actionprefix[0] = '\0';
        }
    else
        {
        memset(cfg.actionprefix, 0, 11);
        strncpy(cfg.actionprefix, options, 10);
        }
    }
if (chkkeyword("ForceBio"))
    {
    cfg.forcebio = seektruth(options);
    }

}

/* seektruth() - Determines the setting of a boolean option.
   Parameters:  seekstr() - Options string to seek the truth from.
   Returns:  Setting of the boolean option (TRUE or FALSE).
*/
char seektruth(unsigned char *seekstr)
{

/* Compare option against known "true" words. */
if (!stricmp(seekstr, "Yes") ||
	!stricmp(seekstr, "On") ||
	!stricmp(seekstr, "1") ||
	!stricmp(seekstr, "True") ||
    !stricmp(seekstr, "Enabled"))
    {
    return 1;
    }

/* Default to false. */
return 0;
}
