/* scfgsys.c */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"

void sys_cfg(void)
{
	static int sys_dflt,adv_dflt,tog_dflt,new_dflt;
	char str[81],done=0;
	int i,j,k,dflt,bar;

while(1) {
	i=0;
	sprintf(opt[i++],"%-33.33s%s","BBS Name",cfg.sys_name);
	sprintf(opt[i++],"%-33.33s%s","Location",cfg.sys_location);
	sprintf(opt[i++],"%-33.33s%s","Operator",cfg.sys_op);
	sprintf(opt[i++],"%-33.33s%s","Password","**********");

	sprintf(str,"%s Password"
		,cfg.sys_misc&SM_PWEDIT && cfg.sys_pwdays ? "Users Must Change"
		: cfg.sys_pwdays ? "Users Get New Random" : "Users Can Change");
	if(cfg.sys_pwdays)
		sprintf(tmp,"Every %u Days",cfg.sys_pwdays);
	else if(cfg.sys_misc&SM_PWEDIT)
		strcpy(tmp,"Yes");
	else
		strcpy(tmp,"No");
	sprintf(opt[i++],"%-33.33s%s",str,tmp);

	sprintf(opt[i++],"%-33.33s%u","Days to Preserve Deleted Users"
		,cfg.sys_deldays);
	sprintf(opt[i++],"%-33.33s%s","Maximum Days of Inactivity"
		,cfg.sys_autodel ? ultoa(cfg.sys_autodel,tmp,10) : "Unlimited");
    sprintf(opt[i++],"%-33.33s%s","New User Password",cfg.new_pass);

	strcpy(opt[i++],"Toggle Options...");
	strcpy(opt[i++],"New User Values...");
	strcpy(opt[i++],"Advanced Options...");
	strcpy(opt[i++],"Loadable Modules...");
	strcpy(opt[i++],"Security Level Values...");
	strcpy(opt[i++],"Expired Account Values...");
	strcpy(opt[i++],"Quick-Validation Values...");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
System Configuration:

This menu contains options and sub-menus of options that affect the
entire system.
*/
	switch(uifc.list(WIN_ORG|WIN_ACT|WIN_CHE,0,0,72,&sys_dflt,0
		,"System Configuration",opt)) {
		case -1:
			i=save_changes(WIN_MID);
			if(i==-1)
				break;
			if(!i) {
				write_main_cfg(&cfg,backup_level);
                refresh_cfg(&cfg);
            }
			return;
		case 0:
			SETHELP(WHERE);
/*
BBS Name:

This is the name of the BBS.
*/
			uifc.input(WIN_MID,0,0,"BBS Name",cfg.sys_name,40,K_EDIT);
			break;
		case 1:
			SETHELP(WHERE);
/*
System Location:

This is the location of the BBS. The format is flexible, but it is
suggested you use the City, State format for clarity.
*/
			uifc.input(WIN_MID,0,0,"Location",cfg.sys_location,40,K_EDIT);
            break;
		case 2:
			SETHELP(WHERE);
/*
System Operator:

This is the name or alias of the system operator. This does not have to
be the same as user #1. This field is used for documentary purposes
only.
*/
			uifc.input(WIN_MID,0,0,"System Operator",cfg.sys_op,40,K_EDIT);
			break;
		case 3:
			SETHELP(WHERE);
/*
System Password:

This is an extra security password required for sysop logon and certain
sysop functions. This password should be something not easily guessed
and should be kept absolutely confidential. This password must be
entered at the SY: prompt.
*/
			uifc.input(WIN_MID,0,0,"System Password",cfg.sys_pass,40,K_EDIT|K_UPPER);
			break;
		case 4:
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=1;
			SETHELP(WHERE);
/*
Allow Users to Change Their Password:

If you want the users of your system to have the option of changing
their password to a string of their choice, set this option to Yes.
For the highest level of security, set this option to No.
*/
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Allow Users to Change Their Password",opt);
			if(!i && !(cfg.sys_misc&SM_PWEDIT)) {
				cfg.sys_misc|=SM_PWEDIT;
				uifc.changes=1; }
			else if(i==1 && cfg.sys_misc&SM_PWEDIT) {
                cfg.sys_misc&=~SM_PWEDIT;
                uifc.changes=1; }
			i=0;
			SETHELP(WHERE);
/*
Force Periodic Password uifc.changes:

If you want your users to be forced to change their passwords
periodically, select Yes.
*/
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Force Periodic Password Changes",opt);
			if(!i) {
				ultoa(cfg.sys_pwdays,str,10);
			SETHELP(WHERE);
/*
Maximum Days Between Password uifc.changes:

Enter the maximum number of days allowed between password uifc.changes.
If a user has not voluntarily changed his or her password in this
many days, he or she will be forced to change their password upon
logon.
*/
				uifc.input(WIN_MID,0,0,"Maximum Days Between Password Changes"
					,str,5,K_NUMBER|K_EDIT);
				cfg.sys_pwdays=atoi(str); }
			else if(i==1 && cfg.sys_pwdays) {
				cfg.sys_pwdays=0;
				uifc.changes=1; }
			
			break;
		case 5:
			sprintf(str,"%u",cfg.sys_deldays);
			SETHELP(WHERE);
/*
Days Since Last Logon to Preserve Deleted Users:

Deleted user slots can be undeleted until the slot is written over
by a new user. If you want deleted user slots to be preserved for period
of time since their last logon, set this value to the number of days to
keep new users from taking over a deleted user's slot.
*/
			uifc.input(WIN_MID,0,0,"Days Since Last Logon to Preserve Deleted Users"
				,str,5,K_EDIT|K_NUMBER);
			cfg.sys_deldays=atoi(str);
			break;
		case 6:
			sprintf(str,"%u",cfg.sys_autodel);
			SETHELP(WHERE);
/*
Maximum Days of Inactivity Before Auto-Deletion:

If you want users that haven't logged on in certain period of time to
be automatically deleted, set this value to the maximum number of days
of inactivity before the user is deleted. Setting this value to 0
disables this feature.

Users with the P exemption will not be deleted due to inactivity.
*/
			uifc.input(WIN_MID,0,0,"Maximum Days of Inactivity Before Auto-Deletion"
				,str,5,K_EDIT|K_NUMBER);
			cfg.sys_autodel=atoi(str);
            break;
		case 7:
			SETHELP(WHERE);
/*
New User Password:

If you want callers to only be able to logon as New if they know a
certain password, enter that password here. If you want any caller to
be able to logon as New, leave this option blank.
*/
			uifc.input(WIN_MID,0,0,"New User Password",cfg.new_pass,40
				,K_EDIT|K_UPPER);
			break;
		case 8:    /* Toggle Options */
            done=0;
            while(!done) {
                i=0;
				sprintf(opt[i++],"%-33.33s%s","Allow Aliases"
					,cfg.uq&UQ_ALIASES ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Allow Time Banking"
					,cfg.sys_misc&SM_TIMEBANK ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Allow Credit Conversions"
					,cfg.sys_misc&SM_NOCDTCVT ? "No" : "Yes");
				sprintf(opt[i++],"%-33.33s%s","Allow Sysop Logins"
					,cfg.sys_misc&SM_R_SYSOP ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Echo Passwords Locally"
					,cfg.sys_misc&SM_ECHO_PW ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Short Sysop Page"
					,cfg.sys_misc&SM_SHRTPAGE ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Sound Alarm on Error"
					,cfg.sys_misc&SM_ERRALARM ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Include Sysop in Statistics"
                    ,cfg.sys_misc&SM_SYSSTAT ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Closed to New Users"
                    ,cfg.sys_misc&SM_CLOSED ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Use Location in User Lists"
					,cfg.sys_misc&SM_LISTLOC ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Use Local/System Time Zone"
					,cfg.sys_misc&SM_LOCAL_TZ ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Military (24 hour) Time Format"
					,cfg.sys_misc&SM_MILITARY ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","European Date Format (DD/MM/YY)"
					,cfg.sys_misc&SM_EURODATE ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","User Expires When Out-of-time"
					,cfg.sys_misc&SM_TIME_EXP ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Display Sys Info During Logon"
					,cfg.sys_misc&SM_NOSYSINFO ? "No" : "Yes");
				sprintf(opt[i++],"%-33.33s%s","Display Node List During Logon"
					,cfg.sys_misc&SM_NONODELIST ? "No" : "Yes");
				opt[i][0]=0;
				uifc.savnum=0;
				SETHELP(WHERE);
/*
System Toggle Options:

This is a menu of system related options that can be toggled between
two or more states, such as Yes and No.
*/
				switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,41,&tog_dflt,0
                    ,"Toggle Options",opt)) {
                    case -1:
                        done=1;
                        break;
					case 0:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.uq&UQ_ALIASES ? 0:1;
						SETHELP(WHERE);
/*
Allow Users to Use Aliases:

If you want the users of your system to be allowed to be known by a
false name, handle, or alias, set this option to Yes. If you want all
users on your system to be known only by their real names, select No.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Use Aliases",opt);
						if(!i && !(cfg.uq&UQ_ALIASES)) {
							cfg.uq|=UQ_ALIASES;
							uifc.changes=1; }
						else if(i==1 && cfg.uq&UQ_ALIASES) {
							cfg.uq&=~UQ_ALIASES;
							uifc.changes=1; }
						break;
					case 1:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_TIMEBANK ? 0:1;
						SETHELP(WHERE);
/*
Allow Time Banking:

If you want the users of your system to be allowed to be deposit
any extra time they may have left during a call into their minute bank,
set this option to Yes. If this option is set to No, then the only
way a user may get minutes in their minute bank is to purchase them
with credits.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Depost Time in Minute Bank",opt);
						if(!i && !(cfg.sys_misc&SM_TIMEBANK)) {
							cfg.sys_misc|=SM_TIMEBANK;
							uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_TIMEBANK) {
							cfg.sys_misc&=~SM_TIMEBANK;
							uifc.changes=1; }
                        break;
					case 2:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_NOCDTCVT ? 1:0;
						SETHELP(WHERE);
/*
Allow Credits to be Converted into Minutes:

If you want the users of your system to be allowed to be convert
any credits they may have into minutes for their minute bank,
set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Convert Credits into Minutes"
							,opt);
						if(!i && cfg.sys_misc&SM_NOCDTCVT) {
							cfg.sys_misc&=~SM_NOCDTCVT;
							uifc.changes=1; }
						else if(i==1 && !(cfg.sys_misc&SM_NOCDTCVT)) {
							cfg.sys_misc|=SM_NOCDTCVT;
							uifc.changes=1; }
                        break;
					case 3:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_R_SYSOP ? 0:1;
						SETHELP(WHERE);
/*
Allow Sysop Logins:

If you want to be able to login with sysop access, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Sysop Logins",opt);
						if(!i && !(cfg.sys_misc&SM_R_SYSOP)) {
							cfg.sys_misc|=SM_R_SYSOP;
							uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_R_SYSOP) {
							cfg.sys_misc&=~SM_R_SYSOP;
							uifc.changes=1; }
                        break;
					case 4:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_ECHO_PW ? 0:1;
						SETHELP(WHERE);
/*
Echo Passwords Locally:

If you want to passwords to be displayed locally, set this option to
Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Echo Passwords Locally",opt);
						if(!i && !(cfg.sys_misc&SM_ECHO_PW)) {
							cfg.sys_misc|=SM_ECHO_PW;
							uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_ECHO_PW) {
							cfg.sys_misc&=~SM_ECHO_PW;
							uifc.changes=1; }
                        break;
					case 5:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=cfg.sys_misc&SM_SHRTPAGE ? 0:1;
						SETHELP(WHERE);
/*
Short Sysop Page:

If you would like the sysop page to be a short series of beeps rather
than continuous random tones, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Short Sysop Page"
							,opt);
						if(i==1 && cfg.sys_misc&SM_SHRTPAGE) {
							cfg.sys_misc&=~SM_SHRTPAGE;
                            uifc.changes=1; }
						else if(!i && !(cfg.sys_misc&SM_SHRTPAGE)) {
							cfg.sys_misc|=SM_SHRTPAGE;
                            uifc.changes=1; }
						break;
					case 6:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=cfg.sys_misc&SM_ERRALARM ? 0:1;
						SETHELP(WHERE);
/*
Sound Alarm on Error:

If you would like to have an alarm sounded locally when a critical
system error has occured, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Sound Alarm on Error",opt);
						if(i==1 && cfg.sys_misc&SM_ERRALARM) {
							cfg.sys_misc&=~SM_ERRALARM;
                            uifc.changes=1; }
						else if(!i && !(cfg.sys_misc&SM_ERRALARM)) {
							cfg.sys_misc|=SM_ERRALARM;
                            uifc.changes=1; }
                        break;
					case 7:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=cfg.sys_misc&SM_SYSSTAT ? 0:1;
						SETHELP(WHERE);
/*
Include Sysop Activity in System Statistics:

If you want sysops to be included in the statistical data of the BBS,
set this option to Yes. The suggested setting for this option is
No so that statistical data will only reflect user usage and not
include sysop maintenance activity.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Include Sysop Activity in System Statistics"
							,opt);
                        if(!i && !(cfg.sys_misc&SM_SYSSTAT)) {
                            cfg.sys_misc|=SM_SYSSTAT;
                            uifc.changes=1; }
                        else if(i==1 && cfg.sys_misc&SM_SYSSTAT) {
                            cfg.sys_misc&=~SM_SYSSTAT;
                            uifc.changes=1; }
                        break;
					case 8:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=cfg.sys_misc&SM_CLOSED ? 0:1;
						SETHELP(WHERE);
/*
Closed to New Users:

If you want callers to be able to logon as New, set this option to No.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Closed to New Users",opt);
                        if(!i && !(cfg.sys_misc&SM_CLOSED)) {
                            cfg.sys_misc|=SM_CLOSED;
                            uifc.changes=1; }
                        else if(i==1 && cfg.sys_misc&SM_CLOSED) {
                            cfg.sys_misc&=~SM_CLOSED;
                            uifc.changes=1; }
                        break;
					case 9:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=cfg.sys_misc&SM_LISTLOC ? 0:1;
						SETHELP(WHERE);
/*
User Location in User Lists:

If you want user locations (city, state) displayed in the user lists,
set this option to Yes. If this option is set to No, the user notes
(if they exist) are displayed in the user lists.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
                            ,"User Location (Instead of Note) in User Lists"
                            ,opt);
						if(!i && !(cfg.sys_misc&SM_LISTLOC)) {
							cfg.sys_misc|=SM_LISTLOC;
                            uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_LISTLOC) {
							cfg.sys_misc&=~SM_LISTLOC;
                            uifc.changes=1; }
                        break;
					case 10:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_LOCAL_TZ ? 0:1;
						SETHELP(WHERE);
/*
Use Local/System Time Zone:

If you would like the times to be displayed adjusting for the local
time zone, set this optiont to Yes. If this option is set to Yes, then
all times will be stored in GMT/UTC representation. If this option is
set to No, then all times will be stored in local representation.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Use Local/System Time Zone",opt);
						if(!i && !(cfg.sys_misc&SM_LOCAL_TZ)) {
							cfg.sys_misc|=SM_LOCAL_TZ;
                            uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_LOCAL_TZ) {
							cfg.sys_misc&=~SM_LOCAL_TZ;
                            uifc.changes=1; }
                        break;
					case 11:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_MILITARY ? 0:1;
						SETHELP(WHERE);
/*
Military:

If you would like the time-of-day to be displayed and entered in 24 hour
format always, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Use Military Time Format",opt);
						if(!i && !(cfg.sys_misc&SM_MILITARY)) {
							cfg.sys_misc|=SM_MILITARY;
                            uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_MILITARY) {
							cfg.sys_misc&=~SM_MILITARY;
                            uifc.changes=1; }
                        break;
					case 12:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_EURODATE ? 0:1;
						SETHELP(WHERE);
/*
European Date Format:

If you would like dates to be displayed and entered in DD/MM/YY format
instead of MM/DD/YY format, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"European Date Format",opt);
						if(!i && !(cfg.sys_misc&SM_EURODATE)) {
							cfg.sys_misc|=SM_EURODATE;
                            uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_EURODATE) {
							cfg.sys_misc&=~SM_EURODATE;
                            uifc.changes=1; }
                        break;
					case 13:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_TIME_EXP ? 0:1;
						SETHELP(WHERE);
/*
User Expires When Out-of-time:

If you want users to be set to Expired User Values if they run out of
time online, then set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"User Expires When Out-of-time",opt);
						if(!i && !(cfg.sys_misc&SM_TIME_EXP)) {
							cfg.sys_misc|=SM_TIME_EXP;
                            uifc.changes=1; }
						else if(i==1 && cfg.sys_misc&SM_TIME_EXP) {
							cfg.sys_misc&=~SM_TIME_EXP;
                            uifc.changes=1; }
                        break;
					case 14:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_NOSYSINFO ? 1:0;
						SETHELP(WHERE);
/*
Display System Information During Logon:

If you want system information displayed during logon, set this option
to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Display System Information During Logon",opt);
						if(!i && cfg.sys_misc&SM_NOSYSINFO) {
							cfg.sys_misc&=~SM_NOSYSINFO;
                            uifc.changes=1; }
						else if(i==1 && !(cfg.sys_misc&SM_NOSYSINFO)) {
							cfg.sys_misc|=SM_NOSYSINFO;
                            uifc.changes=1; }
                        break;
					case 15:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=cfg.sys_misc&SM_NONODELIST ? 1:0;
						SETHELP(WHERE);
/*
Display Active Node List During Logon:

If you want the active nodes displayed during logon, set this option
to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Display Active Node List During Logon",opt);
						if(!i && cfg.sys_misc&SM_NONODELIST) {
							cfg.sys_misc&=~SM_NONODELIST;
                            uifc.changes=1; }
						else if(i==1 && !(cfg.sys_misc&SM_NONODELIST)) {
							cfg.sys_misc|=SM_NONODELIST;
                            uifc.changes=1; }
                        break;
						} }
            break;
		case 9:    /* New User Values */
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%u","Level",cfg.new_level);
				sprintf(opt[i++],"%-27.27s%s","Flag Set #1"
					,ltoaf(cfg.new_flags1,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #2"
					,ltoaf(cfg.new_flags2,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #3"
					,ltoaf(cfg.new_flags3,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #4"
					,ltoaf(cfg.new_flags4,str));
				sprintf(opt[i++],"%-27.27s%s","Exemptions"
					,ltoaf(cfg.new_exempt,str));
				sprintf(opt[i++],"%-27.27s%s","Restrictions"
					,ltoaf(cfg.new_rest,str));
				sprintf(opt[i++],"%-27.27s%s","Expiration Days"
					,ultoa(cfg.new_expire,str,10));

				ultoac(cfg.new_cdt,str);
				sprintf(opt[i++],"%-27.27s%s","Credits",str);
				ultoac(cfg.new_min,str);
				sprintf(opt[i++],"%-27.27s%s","Minutes",str);
				sprintf(opt[i++],"%-27.27s%s","Editor"
					,cfg.new_xedit);
				sprintf(opt[i++],"%-27.27s%s","Command Shell"
					,cfg.shell[cfg.new_shell]->code);
				if(cfg.new_prot!=SP)
                    sprintf(str,"%c",cfg.new_prot);
                else
                    strcpy(str,"None");
				sprintf(opt[i++],"%-27.27s%s","Download Protocol",str);
				strcpy(opt[i++],"Default Toggles...");
				strcpy(opt[i++],"Question Toggles...");
				opt[i][0]=0;
				SETHELP(WHERE);
/*
New User Values:

This menu allows you to determine the default settings for new users.
*/
				switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&new_dflt,0
					,"New User Values",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						ultoa(cfg.new_level,str,10);
						SETHELP(WHERE);
/*
New User Security Level:

This is the security level automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Security Level"
							,str,2,K_EDIT|K_NUMBER);
						cfg.new_level=atoi(str);
						break;
					case 1:
						ltoaf(cfg.new_flags1,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #1"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_flags1=aftol(str);
						break;
					case 2:
						ltoaf(cfg.new_flags2,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #2"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_flags2=aftol(str);
                        break;
					case 3:
						ltoaf(cfg.new_flags3,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #3"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_flags3=aftol(str);
                        break;
					case 4:
						ltoaf(cfg.new_flags4,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #4"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_flags4=aftol(str);
                        break;
					case 5:
						ltoaf(cfg.new_exempt,str);
						SETHELP(WHERE);
/*
New User Exemption Flags:

These are the exemptions that are automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_exempt=aftol(str);
						break;
					case 6:
						ltoaf(cfg.new_rest,str);
						SETHELP(WHERE);
/*
New User Restriction Flags:

These are the restrictions that are automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						cfg.new_rest=aftol(str);
						break;
					case 7:
						ultoa(cfg.new_expire,str,10);
						SETHELP(WHERE);
/*
New User Expiration Days:

If you wish new users to have an automatic expiration date, set this
value to the number of days before the user will expire. To disable
New User expiration, set this value to 0.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Expiration Days",str,4
							,K_EDIT|K_NUMBER);
						cfg.new_expire=atoi(str);
						break;
					case 8:
						ultoa(cfg.new_cdt,str,10);
						SETHELP(WHERE);
/*
New User Credits:

This is the amount of credits that are automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Credits",str,10
							,K_EDIT|K_NUMBER);
						cfg.new_cdt=atol(str);
                        break;
					case 9:
						ultoa(cfg.new_min,str,10);
						SETHELP(WHERE);
/*
New User Minutes:

This is the number of extra minutes automatically given to new users.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Minutes (Time Credit)"
							,str,10,K_EDIT|K_NUMBER);
						cfg.new_min=atol(str);
						break;
					case 10:
						if(!cfg.total_xedits) {
							uifc.msg("No External Editors Configured");
							break; }
						strcpy(opt[0],"Internal");
						for(i=1;i<=cfg.total_xedits;i++)
							strcpy(opt[i],cfg.xedit[i-1]->code);
						opt[i][0]=0;
						i=0;
						uifc.savnum=0;
						SETHELP(WHERE);
/*
New User Editor:

You can use this option to select the default editor for new users.
*/
						i=uifc.list(WIN_SAV|WIN_RHT,2,1,13,&i,0,"Editors",opt);
						if(i==-1)
							break;
						uifc.changes=1;
						if(i && i<=cfg.total_xedits)
							sprintf(cfg.new_xedit,"%-.8s",cfg.xedit[i-1]->code);
						else
							cfg.new_xedit[0]=0;
						break;
					case 11:
						for(i=0;i<cfg.total_shells && i<MAX_OPTS;i++)
							sprintf(opt[i],"%-.8s",cfg.shell[i]->code);
						opt[i][0]=0;
						i=0;
						uifc.savnum=0;
						SETHELP(WHERE);
/*
New User Command Shell:

You can use this option to select the default command shell for new
users.
*/
						i=uifc.list(WIN_SAV|WIN_RHT,2,1,13,&i,0
							,"Command Shells",opt);
						if(i==-1)
							break;
						cfg.new_shell=i;
						uifc.changes=1;
						break;
					case 12:
						SETHELP(WHERE);
/*
New User Default Download Protocol:

This option allows you to set the default download protocol of new users
(protocol command key or BLANK for no default).
*/
						sprintf(str,"%c",cfg.new_prot);
						uifc.input(WIN_SAV|WIN_MID,0,0
							,"Default Download Protocol (SPACE=Disabled)"
							,str,1,K_EDIT|K_UPPER);
						cfg.new_prot=str[0];
						if(cfg.new_prot<SP)
							cfg.new_prot=SP;
                        break;
					case 13:
						SETHELP(WHERE);
/*
New User Default Toggle Options:

This menu contains the default state of new user toggle options. All new
users on your system will have their defaults set according to the
settings on this menu. The user can then change them to his or her
liking.

See the Synchronet User Manual for more information on the individual
options available.
*/
						j=0;
                        while(1) {
                            i=0;
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Expert Menu Mode"
								,cfg.new_misc&EXPERT ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Screen Pause"
								,cfg.new_misc&UPAUSE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Spinning Cursor"
								,cfg.new_misc&SPIN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Clear Screen"
								,cfg.new_misc&CLRSCRN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Ask For New Scan"
								,cfg.new_misc&ASK_NSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Ask For Your Msg Scan"
								,cfg.new_misc&ASK_SSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Automatic New File Scan"
								,cfg.new_misc&ANFSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Remember Current Sub-board"
								,cfg.new_misc&CURSUB ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Batch Download File Flag"
								,cfg.new_misc&BATCHFLAG ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Extended File Descriptions"
								,cfg.new_misc&EXTDESC ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Hot Keys"
								,cfg.new_misc&COLDKEYS ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Auto Hang-up After Xfer"
								,cfg.new_misc&AUTOHANG ? "Yes":"No");
							opt[i][0]=0;
							j=uifc.list(WIN_BOT|WIN_RHT,2,1,0,&j,0
								,"Default Toggle Options",opt);
                            if(j==-1)
                                break;
                            uifc.changes=1;
                            switch(j) {
                                case 0:
									cfg.new_misc^=EXPERT;
                                    break;
								case 1:
									cfg.new_misc^=UPAUSE;
									break;
								case 2:
									cfg.new_misc^=SPIN;
									break;
								case 3:
									cfg.new_misc^=CLRSCRN;
									break;
								case 4:
									cfg.new_misc^=ASK_NSCAN;
									break;
								case 5:
									cfg.new_misc^=ASK_SSCAN;
                                    break;
								case 6:
									cfg.new_misc^=ANFSCAN;
									break;
								case 7:
									cfg.new_misc^=CURSUB;
									break;
								case 8:
									cfg.new_misc^=BATCHFLAG;
									break;
								case 9:
									cfg.new_misc^=EXTDESC;
                                    break;
								case 10:
									cfg.new_misc^=COLDKEYS;
                                    break;
								case 11:
									cfg.new_misc^=AUTOHANG;
                                    break;
									} }
						break;
					case 14:
						SETHELP(WHERE);
/*
New User Question Toggle Options:

This menu allows you to decide which questions will be asked of a new
user.
*/
						j=0;
                        while(1) {
                            i=0;
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Real Name"
								,cfg.uq&UQ_REALNAME ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Force Unique Real Name"
								,cfg.uq&UQ_DUPREAL ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Force Upper/Lower Case"
								,cfg.uq&UQ_NOUPRLWR ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Company Name"
								,cfg.uq&UQ_COMPANY ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Multinode Chat Handle"
								,cfg.uq&UQ_HANDLE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Force Unique Chat Handle"
								,cfg.uq&UQ_DUPHAND ? "Yes":"No");
                            sprintf(opt[i++],"%-27.27s %-3.3s"
                                ,"E-mail/NetMail Address"
                                ,cfg.uq&UQ_NONETMAIL ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Sex (Gender)"
								,cfg.uq&UQ_SEX ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Birthday"
								,cfg.uq&UQ_BIRTH ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Address and Zip Code"
								,cfg.uq&UQ_ADDRESS ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Location"
								,cfg.uq&UQ_LOCATION ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Require Comma in Location"
								,cfg.uq&UQ_NOCOMMAS ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Phone Number"
								,cfg.uq&UQ_PHONE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Allow EX-ASCII in Answers"
								,cfg.uq&UQ_NOEXASC ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"External Editor"
								,cfg.uq&UQ_XEDIT ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Command Shell"
								,cfg.uq&UQ_CMDSHELL ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Default Settings"
								,cfg.uq&UQ_NODEF ? "No":"Yes");
							opt[i][0]=0;
							j=uifc.list(WIN_BOT|WIN_RHT|WIN_SAV,2,1,0,&j,0
								,"New User Questions",opt);
                            if(j==-1)
                                break;
                            uifc.changes=1;
                            switch(j) {
                                case 0:
									cfg.uq^=UQ_REALNAME;
                                    break;
								case 1:
									cfg.uq^=UQ_DUPREAL;
									break;
								case 2:
									cfg.uq^=UQ_NOUPRLWR;
									break;
								case 3:
									cfg.uq^=UQ_COMPANY;
                                    break;
								case 4:
									cfg.uq^=UQ_HANDLE;
                                    break;
								case 5:
									cfg.uq^=UQ_DUPHAND;
									break;
                                case 6:
                                    cfg.uq^=UQ_NONETMAIL;
                                    break;
								case 7:
									cfg.uq^=UQ_SEX;
									break;
								case 8:
									cfg.uq^=UQ_BIRTH;
									break;
								case 9:
									cfg.uq^=UQ_ADDRESS;
									break;
								case 10:
									cfg.uq^=UQ_LOCATION;
									break;
								case 11:
									cfg.uq^=UQ_NOCOMMAS;
									break;
								case 12:
									cfg.uq^=UQ_PHONE;
									break;
								case 13:
									cfg.uq^=UQ_NOEXASC;
									break;
								case 14:
									cfg.uq^=UQ_XEDIT;
                                    break;
								case 15:
									cfg.uq^=UQ_CMDSHELL;
                                    break;
								case 16:
									cfg.uq^=UQ_NODEF;
                                    break;
									} }
						break; } }
			break;
		case 10:	/* Advanced Options */
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","New User Magic Word",cfg.new_magic);
				sprintf(opt[i++],"%-27.27s%.40s","Data Directory",cfg.data_dir);
				sprintf(opt[i++],"%-27.27s%.40s","Executables Directory"
					,cfg.exec_dir);
				sprintf(opt[i++],"%-27.27s%s","Input SIF Questionnaire"
					,cfg.new_sif);
				sprintf(opt[i++],"%-27.27s%s","Output SIF Questionnaire"
					,cfg.new_sof);
				ultoac(cfg.cdt_per_dollar,str);
				sprintf(opt[i++],"%-27.27s%s","Credits Per Dollar",str);
				sprintf(opt[i++],"%-27.27s%u","Minutes Per 100k Credits"
					,cfg.cdt_min_value);
				sprintf(opt[i++],"%-27.27s%s","Maximum Number of Minutes"
					,cfg.max_minutes ? ultoa(cfg.max_minutes,tmp,10) : "Unlimited");
				sprintf(opt[i++],"%-27.27s%u","Warning Days Till Expire"
					,cfg.sys_exp_warn);
				sprintf(opt[i++],"%-27.27s%u","Last Displayable Node"
					,cfg.sys_lastnode);
				sprintf(opt[i++],"%-27.27s%s","Phone Number Format"
					,cfg.sys_phonefmt);
				sprintf(opt[i++],"%-27.27s%.40s","Sysop Chat Requirements"
					,cfg.sys_chat_arstr);
				if(cfg.user_backup_level)
					sprintf(str,"%hu",cfg.user_backup_level);
				else
					strcpy(str,"None");
				sprintf(opt[i++],"%-27.27s%s","User Database Backups",str);
				if(cfg.mail_backup_level)
					sprintf(str,"%hu",cfg.mail_backup_level);
				else
					strcpy(str,"None");
				sprintf(opt[i++],"%-27.27s%s","Mail Database Backups",str);
				sprintf(opt[i++],"%-27.27s%lX","Control Key Pass-through"
					,cfg.ctrlkey_passthru);
				opt[i][0]=0;
				uifc.savnum=0;
				SETHELP(WHERE);
/*
System Advanced Options:

Care should be taken when modifying any of the options listed here.
*/
				switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&adv_dflt,0
					,"Advanced Options",opt)) {
					case -1:
						done=1;
                        break;

					case 0:
						SETHELP(WHERE);
/*
New User Magic Word:

If this field has a value, it is assumed the sysop has placed some
reference to this magic word in TEXT\NEWUSER.MSG and new users
will be prompted for the magic word after they enter their password.
If they do not enter it correctly, it is assumed they didn't read the
new user information displayed to them and they are disconnected.

Think of it as a password to guarantee that new users read the text
displayed to them.
*/
						uifc.input(WIN_MID,0,0,"New User Magic Word",cfg.new_magic,20
							,K_EDIT|K_UPPER);
						break;
					case 1:
						SETHELP(WHERE);
/*
Data Directory:

The Synchronet data directory contains almost all the data for your BBS.
This directory must be located where ALL nodes can access it and
MUST NOT be placed on a RAM disk or other volatile media.

This option allows you to change the location of your data directory.
*/
						strcpy(str,cfg.data_dir);
						if(uifc.input(WIN_MID|WIN_SAV,0,9,"Data Directory"
							,str,50,K_EDIT)>0) {
							backslash(str);
							SAFECOPY(cfg.data_dir,str); 
						}
                        break;
					case 2:
						SETHELP(WHERE);
/*
Exec Directory:

The Synchronet exec directory contains executable files that your BBS
executes. This directory does not need to be in your DOS search path.
If you place programs in this directory for the BBS to execute, you
should place the %! abreviation for this exec directory at the
beginning of the command line.

This option allows you to change the location of your exec directory.
*/
						strcpy(str,cfg.exec_dir);
						if(uifc.input(WIN_MID|WIN_SAV,0,9,"Exec Directory"
							,str,50,K_EDIT)>0) {
							backslash(str);
							SAFECOPY(cfg.exec_dir,str); 
						}
                        break;
					case 3:
						strcpy(str,cfg.new_sif);
						SETHELP(WHERE);
/*
SIF Questionnaire for User Input:

This is the name of a SIF questionnaire file that resides your text
directory that all users will be prompted to answer.
*/
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"SIF Questionnaire for User Input"
							,str,8,K_UPPER|K_EDIT);
						if(!str[0] || code_ok(str))
							strcpy(cfg.new_sif,str);
						else
							uifc.msg("Invalid SIF Name");
						break;
					case 4:
						strcpy(str,cfg.new_sof);
						SETHELP(WHERE);
/*
SIF Questionnaire for Reviewing User Input:

This is the SIF file used to review the input of users from the user
edit function.
*/
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"SIF Questionnaire for Reviewing User Input"
							,str,8,K_EDIT);
						if(!str[0] || code_ok(str))
							strcpy(cfg.new_sof,str);
						else
							uifc.msg("Invalid SIF Name");
						break;
					case 5:
						SETHELP(WHERE);
/*
Credits Per Dollar:

This is the monetary value of a credit (How many credits per dollar).
This value should be a power of 2 (1, 2, 4, 8, 16, 32, 64, 128, etc.)
since credits are usually converted by 100 kilobyte (102400) blocks.
To make a dollar worth two megabytes of credits, set this value to
2,097,152 (a megabyte is 1024*1024 or 1048576).
*/
						ultoa(cfg.cdt_per_dollar,str,10);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Credits Per Dollar",str,10,K_NUMBER|K_EDIT);
						cfg.cdt_per_dollar=atol(str);
						break;
					case 6:
						SETHELP(WHERE);
/*
Minutes Per 100K Credits:

This is the value of a minute of time online. This field is the number
of minutes to give the user in exchange for each 100K credit block.
*/
						sprintf(str,"%u",cfg.cdt_min_value);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Minutes Per 100K Credits",str,5,K_NUMBER|K_EDIT);
						cfg.cdt_min_value=atoi(str);
						break;
					case 7:
						SETHELP(WHERE);
/*
Maximum Number of Minutes User Can Have:

This value is the maximum total number of minutes a user can have. If a
user has this number of minutes or more, they will not be allowed to
convert credits into minutes. A sysop can add minutes to a user's
account regardless of this maximum. If this value is set to 0, users
will have no limit on the total number of minutes they can have.
*/
						sprintf(str,"%lu",cfg.max_minutes);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Maximum Number of Minutes a User Can Have "
							"(0=No Limit)"
							,str,10,K_NUMBER|K_EDIT);
						cfg.max_minutes=atol(str);
						break;
					case 8:
						SETHELP(WHERE);
/*
Warning Days Till Expire:

If a user's account will expire in this many days or less, the user will
be notified at logon. Setting this value to 0 disables the warning
completely.
*/
						sprintf(str,"%u",cfg.sys_exp_warn);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Warning Days Till Expire",str,5,K_NUMBER|K_EDIT);
						cfg.sys_exp_warn=atoi(str);
                        break;
					case 9:
						SETHELP(WHERE);
/*
Last Displayed Node:

This is the number of the last node to display to users in node lists.
This allows the sysop to define the higher numbered nodes as invisible
to users.
*/
						sprintf(str,"%u",cfg.sys_lastnode);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Last Displayed Node",str,5,K_NUMBER|K_EDIT);
						cfg.sys_lastnode=atoi(str);
                        break;
					case 10:
						SETHELP(WHERE);
/*
Phone Number Format:

This is the format used for phone numbers in your local calling
area. Use N for number positions, A for alphabetic, or ! for any
character. All other characters will be static in the phone number
format. An example for North American phone numbers is NNN-NNN-NNNN.
*/
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Phone Number Format",cfg.sys_phonefmt
							,LEN_PHONE,K_UPPER|K_EDIT);
                        break;
					case 11:
						getar("Sysop Chat",cfg.sys_chat_arstr);
						break;
					case 12:
						SETHELP(WHERE);
/*
`User Database Backups:`

Setting this option to anything but 0 will enable automatic daily
backups of the user database. This number determines how many backups
to keep on disk.
*/
						sprintf(str,"%u",cfg.user_backup_level);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Number of Daily User Database Backups to Keep"
							,str,4,K_NUMBER|K_EDIT);
						cfg.user_backup_level=atoi(str);
                        break;
					case 13:
						SETHELP(WHERE);
/*
`Mail Database Backups:`

Setting this option to anything but 0 will enable automatic daily
backups of the mail database. This number determines how many backups
to keep on disk.
*/
						sprintf(str,"%u",cfg.mail_backup_level);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Number of Daily Mail Database Backups to Keep"
							,str,4,K_NUMBER|K_EDIT);
						cfg.mail_backup_level=atoi(str);
                        break;
					case 14:
						SETHELP(WHERE);
/*
`Control Key Pass-through:`

This value is a 32-bit hexadecimal number. Each set bit represents a
control key combination that will `not` be handled internally by
Synchronet or by a Global Hot Key Event.

To disable internal handling of the `Ctrl-C` key combination (for example),
set this value to `8`. The value is determined by taking 2 to the power of
the ASCII value of the control character (Ctrl-A is 1, Ctrl-B is 2, etc).
In the case of Ctrl-C, taking 2 to the power of 3 equals 8.

To pass-through multiple control key combinations, multiple bits must be
set (or'd together) which may require the use of a hexadecimal
calculator.

If unsure, leave this value set to `0`, the default.
*/
						sprintf(str,"%lX",cfg.ctrlkey_passthru);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Control Key Pass-through"
							,str,8,K_UPPER|K_EDIT);
						cfg.ctrlkey_passthru=strtoul(str,NULL,16);
                        break;
						} }
				break;
		case 11: /* Loadable Modules */
			done=0;
			k=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-16.16s%s","Login",cfg.login_mod);
				sprintf(opt[i++],"%-16.16s%s","Logon Event",cfg.logon_mod);
				sprintf(opt[i++],"%-16.16s%s","Sync Event",cfg.sync_mod);
				sprintf(opt[i++],"%-16.16s%s","Logoff Event",cfg.logoff_mod);
				sprintf(opt[i++],"%-16.16s%s","Logout Event",cfg.logout_mod);
				sprintf(opt[i++],"%-16.16s%s","New User Event",cfg.newuser_mod);
				sprintf(opt[i++],"%-16.16s%s","Expired User",cfg.expire_mod);
				opt[i][0]=0;
				uifc.savnum=0;
				SETHELP(WHERE);
/*
Loadable Modules:

Baja modules (.BIN files) can be automatically loaded and executed
during certain system functions. The name of the module can be specified
for each of the available triggers listed here.

Login 		Required module for remote and local logins (online)
Logon 		Executed as an event during logon procedure (online)
Sync			Executed when nodes are periodically synchronized (online)
Logoff		Executed during logoff procedure (online)
Logout		Executed during logout procedure (offline)
New User		Executed at end of new user procedure (online)
Expired User	Executed during daily event when user expires (offline)
*/
				switch(uifc.list(WIN_ACT|WIN_T2B|WIN_RHT,0,0,32,&k,0
					,"Loadable Modules",opt)) {

					case -1:
						done=1;
                        break;

					case 0:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Login Module",cfg.login_mod,8
							,K_EDIT);
                        break;
					case 1:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Logon Module",cfg.logon_mod,8
							,K_EDIT);
                        break;
					case 2:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Synchronize Module"
							,cfg.sync_mod,8,K_EDIT);
                        break;
					case 3:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Logoff Module",cfg.logoff_mod,8
							,K_EDIT);
                        break;
					case 4:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Logout Module",cfg.logout_mod,8
							,K_EDIT);
                        break;
					case 5:
						uifc.input(WIN_MID|WIN_SAV,0,0,"New User Module"
							,cfg.newuser_mod,8,K_EDIT);
                        break;
					case 6:
						uifc.input(WIN_MID|WIN_SAV,0,0,"Expired User Module"
							,cfg.expire_mod,8,K_EDIT);
                        break;

					} }
			break;

		case 12: /* Security Levels */
			dflt=bar=0;
			k=0;
			while(1) {
				for(i=0;i<100;i++) {
					sprintf(tmp,"%luk",cfg.level_freecdtperday[i]/1024L);
					sprintf(opt[i],"%-2d    %5d %5d "
						"%5d %5d %5d %5d %6s %7s %2u",i
						,cfg.level_timeperday[i],cfg.level_timepercall[i]
						,cfg.level_callsperday[i],cfg.level_emailperday[i]
						,cfg.level_postsperday[i],cfg.level_linespermsg[i]
						,tmp
						,cfg.level_misc[i]&LEVEL_EXPTOVAL ? "Val Set" : "Level"
						,cfg.level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
							cfg.level_expireto[i] : cfg.expired_level); }
				opt[i][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Security Level Values:

This menu allows you to change the security options for every possible
security level from 0 to 99. The available options for each level are:

	Time Per Day		 :	Maximum online time per day
	Time Per Call		 :	Maximum online time per call
	Calls Per Day		 :	Maximum number of calls per day
	Email Per Day		 :	Maximum number of email per day
	Posts Per Day		 :	Maximum number of posts per day
	Lines Per Message	 :	Maximum number of lines per message
	Free Credits Per Day :	Number of free credits per day
	Expire To			 :	Level or validation set to Expire to
*/
				i=uifc.list(WIN_RHT|WIN_ACT,0,3,0,&dflt,&bar
					,"Level   T/D   T/C   C/D   E/D   P/D   L/M   F/D   "
						"Expire To",opt);
				if(i==-1)
					break;
				while(1) {
					sprintf(str,"Security Level %d Values",i);
					j=0;
					sprintf(opt[j++],"%-22.22s%-5u","Time Per Day"
						,cfg.level_timeperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Time Per Call"
						,cfg.level_timepercall[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Calls Per Day"
						,cfg.level_callsperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Email Per Day"
						,cfg.level_emailperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Posts Per Day"
                        ,cfg.level_postsperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Lines Per Message"
						,cfg.level_linespermsg[i]);
					sprintf(tmp,"%luk",cfg.level_freecdtperday[i]/1024L);
					sprintf(opt[j++],"%-22.22s%-6s","Free Credits Per Day"
						,tmp);
					sprintf(opt[j++],"%-22.22s%s %u","Expire To"
						,cfg.level_misc[i]&LEVEL_EXPTOVAL ? "Validation Set"
							: "Level"
						,cfg.level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
							cfg.level_expireto[i] : cfg.expired_level);
					opt[j][0]=0;
					uifc.savnum=0;
					j=uifc.list(WIN_RHT|WIN_SAV|WIN_ACT,2,1,0,&k,0
						,str,opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Total Time Allowed Per Day"
								,ultoa(cfg.level_timeperday[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.level_timeperday[i]=atoi(tmp);
							if(cfg.level_timeperday[i]>1440)
								cfg.level_timeperday[i]=1440;
							break;
						case 1:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Time Allowed Per Call"
								,ultoa(cfg.level_timepercall[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.level_timepercall[i]=atoi(tmp);
							if(cfg.level_timepercall[i]>1440)
								cfg.level_timepercall[i]=1440;
                            break;
						case 2:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Calls Allowed Per Day"
								,ultoa(cfg.level_callsperday[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.level_callsperday[i]=atoi(tmp);
                            break;
						case 3:
                            uifc.input(WIN_MID|WIN_SAV,0,0
                                ,"Email Allowed Per Day"
								,ultoa(cfg.level_emailperday[i],tmp,10),4
                                ,K_NUMBER|K_EDIT);
                            cfg.level_emailperday[i]=atoi(tmp);
                            break;
						case 4:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Posts Allowed Per Day"
								,ultoa(cfg.level_postsperday[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.level_postsperday[i]=atoi(tmp);
                            break;
						case 5:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Lines Allowed Per Message (Post/E-mail)"
								,ultoa(cfg.level_linespermsg[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.level_linespermsg[i]=atoi(tmp);
							break;
						case 6:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Free Credits Per Day (in Kilobytes)"
								,ultoa(cfg.level_freecdtperday[i]/1024L,tmp,10),8
								,K_EDIT|K_UPPER);
							cfg.level_freecdtperday[i]=atol(tmp)*1024L;
							break;
						case 7:
							j=0;
							sprintf(opt[j++],"Default Expired Level "
								"(Currently %u)",cfg.expired_level);
							sprintf(opt[j++],"Specific Level");
							sprintf(opt[j++],"Quick-Validation Set");
							opt[j][0]=0;
							j=0;
							sprintf(str,"Level %u Expires To",i);
							uifc.savnum=1;
							j=uifc.list(WIN_SAV,2,1,0,&j,0
								,str,opt);
							if(j==-1)
								break;
							if(j==0) {
								cfg.level_misc[i]&=
									~(LEVEL_EXPTOLVL|LEVEL_EXPTOVAL);
								uifc.changes=1;
								break; }
							if(j==1) {
								cfg.level_misc[i]&=~LEVEL_EXPTOVAL;
								cfg.level_misc[i]|=LEVEL_EXPTOLVL;
								uifc.changes=1;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Expired Level"
									,ultoa(cfg.level_expireto[i],tmp,10),2
									,K_EDIT|K_NUMBER);
								cfg.level_expireto[i]=atoi(tmp);
								break; }
							cfg.level_misc[i]&=~LEVEL_EXPTOLVL;
							cfg.level_misc[i]|=LEVEL_EXPTOVAL;
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Quick-Validation Set to Expire To"
								,ultoa(cfg.level_expireto[i],tmp,10),1
								,K_EDIT|K_NUMBER);
							cfg.level_expireto[i]=atoi(tmp);
							break;
							} } }
			break;
		case 13:	/* Expired Acccount Values */
			dflt=0;
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%u","Level",cfg.expired_level);
				sprintf(opt[i++],"%-27.27s%s","Flag Set #1 to Remove"
					,ltoaf(cfg.expired_flags1,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #2 to Remove"
					,ltoaf(cfg.expired_flags2,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #3 to Remove"
					,ltoaf(cfg.expired_flags3,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #4 to Remove"
					,ltoaf(cfg.expired_flags4,str));
				sprintf(opt[i++],"%-27.27s%s","Exemptions to Remove"
					,ltoaf(cfg.expired_exempt,str));
				sprintf(opt[i++],"%-27.27s%s","Restrictions to Add"
					,ltoaf(cfg.expired_rest,str));
				opt[i][0]=0;
				SETHELP(WHERE);
/*
Expired Account Values:

If a user's account expires, the security levels for that account will
be modified according to the settings of this menu. The Main Level and
Transfer Level will be set to the values listed on this menu. The Main
Flags, Transfer Flags, and Exemptions listed on this menu will be
removed from the account if they are set. The Restrictions listed will
be added to the account.
*/
				switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&dflt,0
					,"Expired Account Values",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						ultoa(cfg.expired_level,str,10);
						SETHELP(WHERE);
/*
Expired Account Security Level:

This is the security level automatically given to expired user accounts.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Security Level"
							,str,2,K_EDIT|K_NUMBER);
						cfg.expired_level=atoi(str);
						break;
					case 1:
						ltoaf(cfg.expired_flags1,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #1"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_flags1=aftol(str);
						break;
					case 2:
						ltoaf(cfg.expired_flags2,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #2"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_flags2=aftol(str);
                        break;
					case 3:
						ltoaf(cfg.expired_flags3,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #3"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_flags3=aftol(str);
                        break;
					case 4:
						ltoaf(cfg.expired_flags4,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #4"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_flags4=aftol(str);
                        break;
					case 5:
						ltoaf(cfg.expired_exempt,str);
						SETHELP(WHERE);
/*
Expired Exemption Flags to Remove:

These are the exemptions that are automatically removed from a user
account if it expires.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_exempt=aftol(str);
						break;
					case 6:
						ltoaf(cfg.expired_rest,str);
						SETHELP(WHERE);
/*
Expired Restriction Flags to Add:

These are the restrictions that are automatically added to a user
account if it expires.
*/
						uifc.input(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						cfg.expired_rest=aftol(str);
						break; } }
			break;
		case 14:	/* Quick-Validation Values */
			dflt=0;
			k=0;
			while(1) {
				for(i=0;i<10;i++)
					sprintf(opt[i],"%d  SL: %-2d  F1: %s"
						,i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],str));
				opt[i][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Quick-Validation Values:

This is a list of the ten quick-validation sets. These sets are used to
quickly set a user's security values (Level, Flags, Exemptions,
Restrictions, Expiration Date, and Credits) with one key stroke. The
user's expiration date may be extended and additional credits may also
be added using quick-validation sets.

Holding down  ALT  and one of the number keys (1-9) while a user
is online, automatically sets his or user security values to the
quick-validation set for that number key.

From within the User Edit function, a sysop can use the Validate
User command and select from this quick-validation list to change a
user's security values with very few key-strokes.
*/
				uifc.savnum=0;
				i=uifc.list(WIN_RHT|WIN_BOT|WIN_ACT|WIN_SAV,0,0,0,&dflt,0
					,"Quick-Validation Values",opt);
				if(i==-1)
                    break;
				sprintf(str,"Quick-Validation Set %d",i);
				uifc.savnum=0;
				while(1) {
					j=0;
					sprintf(opt[j++],"%-22.22s%u","Level",cfg.val_level[i]);
					sprintf(opt[j++],"%-22.22s%s","Flag Set #1"
						,ltoaf(cfg.val_flags1[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #2"
						,ltoaf(cfg.val_flags2[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #3"
						,ltoaf(cfg.val_flags3[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #4"
						,ltoaf(cfg.val_flags4[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Exemptions"
						,ltoaf(cfg.val_exempt[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Restrictions"
						,ltoaf(cfg.val_rest[i],tmp));
					sprintf(opt[j++],"%-22.22s%u days","Extend Expiration"
						,cfg.val_expire[i]);
					sprintf(opt[j++],"%-22.22s%lu","Additional Credits"
						,cfg.val_cdt[i]);
					opt[j][0]=0;
					uifc.savnum=1;
					j=uifc.list(WIN_RHT|WIN_SAV|WIN_ACT,2,1,0,&k,0
						,str,opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Level"
								,ultoa(cfg.val_level[i],tmp,10),2
								,K_NUMBER|K_EDIT);
							cfg.val_level[i]=atoi(tmp);
							break;
						case 1:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Flag Set #1"
								,ltoaf(cfg.val_flags1[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_flags1[i]=aftol(tmp);
                            break;
						case 2:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Flag Set #2"
								,ltoaf(cfg.val_flags2[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_flags2[i]=aftol(tmp);
                            break;
						case 3:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Flag Set #3"
								,ltoaf(cfg.val_flags3[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_flags3[i]=aftol(tmp);
                            break;
						case 4:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Flag Set #4"
								,ltoaf(cfg.val_flags4[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_flags4[i]=aftol(tmp);
                            break;
						case 5:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Exemption Flags"
								,ltoaf(cfg.val_exempt[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_exempt[i]=aftol(tmp);
                            break;
						case 6:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Restriction Flags"
								,ltoaf(cfg.val_rest[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							cfg.val_rest[i]=aftol(tmp);
							break;
						case 7:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Days to Extend Expiration"
								,ultoa(cfg.val_expire[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							cfg.val_expire[i]=atoi(tmp);
                            break;
						case 8:
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Additional Credits"
								,ultoa(cfg.val_cdt[i],tmp,10),10
								,K_NUMBER|K_EDIT);
							cfg.val_cdt[i]=atol(tmp);
							break; } } }
			break; } }

}
