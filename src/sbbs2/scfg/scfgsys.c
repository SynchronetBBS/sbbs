#line 2 "SCFGSYS.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

void sys_cfg(void)
{
	static int sys_dflt,adv_dflt,tog_dflt,new_dflt;
	char str[81],str2[81],done=0,*dupehelp;
	int i,j,k,dflt,bar,savchanges;

while(1) {
	i=0;
	sprintf(opt[i++],"%-33.33s%s","BBS Name",sys_name);
	sprintf(opt[i++],"%-33.33s%s","Location",sys_location);
	sprintf(opt[i++],"%-33.33s%s","Operator",sys_op);
	sprintf(opt[i++],"%-33.33s%s","Password",sys_pass);

	sprintf(str,"%s Password"
		,sys_misc&SM_PWEDIT && sys_pwdays ? "Users Must Change"
		: sys_pwdays ? "Users Get New Random" : "Users Can Change");
	if(sys_pwdays)
		sprintf(tmp,"Every %u Days",sys_pwdays);
	else if(sys_misc&SM_PWEDIT)
		strcpy(tmp,"Yes");
	else
		strcpy(tmp,"No");
	sprintf(opt[i++],"%-33.33s%s",str,tmp);

	sprintf(opt[i++],"%-33.33s%u","Days to Preserve Deleted Users"
		,sys_deldays);
	sprintf(opt[i++],"%-33.33s%s","Maximum Days of Inactivity"
		,sys_autodel ? itoa(sys_autodel,tmp,10) : "Unlimited");
    sprintf(opt[i++],"%-33.33s%s","New User Password",new_pass);

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
	switch(ulist(WIN_ORG|WIN_ACT|WIN_CHE,0,0,72,&sys_dflt,0
		,"System Configuration",opt)) {
		case -1:
			i=save_changes(WIN_MID);
			if(i==-1)
				break;
			if(!i)
				write_main_cfg();
			return;
		case 0:
			SETHELP(WHERE);
/*
BBS Name:

This is the name of the BBS.
*/
			uinput(WIN_MID,0,0,"BBS Name",sys_name,40,K_EDIT);
			break;
		case 1:
			SETHELP(WHERE);
/*
System Location:

This is the location of the BBS. The format is flexible, but it is
suggested you use the City, State format for clarity.
*/
			uinput(WIN_MID,0,0,"Location",sys_location,40,K_EDIT);
            break;
		case 2:
			SETHELP(WHERE);
/*
System Operator:

This is the name or alias of the system operator. This does not have to
be the same as user #1. This field is used for documentary purposes
only.
*/
			uinput(WIN_MID,0,0,"System Operator",sys_op,40,K_EDIT);
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
			uinput(WIN_MID,0,0,"System Password",sys_pass,40,K_EDIT|K_UPPER);
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
			i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Allow Users to Change Their Password",opt);
			if(!i && !(sys_misc&SM_PWEDIT)) {
				sys_misc|=SM_PWEDIT;
				changes=1; }
			else if(i==1 && sys_misc&SM_PWEDIT) {
                sys_misc&=~SM_PWEDIT;
                changes=1; }
			i=0;
			SETHELP(WHERE);
/*
Force Periodic Password Changes:

If you want your users to be forced to change their passwords
periodically, select Yes.
*/
			i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Force Periodic Password Changes",opt);
			if(!i) {
				itoa(sys_pwdays,str,10);
			SETHELP(WHERE);
/*
Maximum Days Between Password Changes:

Enter the maximum number of days allowed between password changes.
If a user has not voluntarily changed his or her password in this
many days, he or she will be forced to change their password upon
logon.
*/
				uinput(WIN_MID,0,0,"Maximum Days Between Password Changes"
					,str,5,K_NUMBER|K_EDIT);
				sys_pwdays=atoi(str); }
			else if(i==1 && sys_pwdays) {
				sys_pwdays=0;
				changes=1; }
			
			break;
		case 5:
			sprintf(str,"%u",sys_deldays);
			SETHELP(WHERE);
/*
Days Since Last Logon to Preserve Deleted Users:

Deleted user slots can be undeleted until the slot is written over
by a new user. If you want deleted user slots to be preserved for period
of time since their last logon, set this value to the number of days to
keep new users from taking over a deleted user's slot.
*/
			uinput(WIN_MID,0,0,"Days Since Last Logon to Preserve Deleted Users"
				,str,5,K_EDIT|K_NUMBER);
			sys_deldays=atoi(str);
			break;
		case 6:
			sprintf(str,"%u",sys_autodel);
			SETHELP(WHERE);
/*
Maximum Days of Inactivity Before Auto-Deletion:

If you want users that haven't logged on in certain period of time to
be automatically deleted, set this value to the maximum number of days
of inactivity before the user is deleted. Setting this value to 0
disables this feature.

Users with the P exemption will not be deleted due to inactivity.
*/
			uinput(WIN_MID,0,0,"Maximum Days of Inactivity Before Auto-Deletion"
				,str,5,K_EDIT|K_NUMBER);
			sys_autodel=atoi(str);
            break;
		case 7:
			SETHELP(WHERE);
/*
New User Password:

If you want callers to only be able to logon as New if they know a
certain password, enter that password here. If you want any caller to
be able to logon as New, leave this option blank.
*/
			uinput(WIN_MID,0,0,"New User Password",new_pass,40
				,K_EDIT|K_UPPER);
			break;
		case 8:    /* Toggle Options */
            done=0;
            while(!done) {
                i=0;
				sprintf(opt[i++],"%-33.33s%s","Allow Aliases"
					,uq&UQ_ALIASES ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Allow Time Banking"
					,sys_misc&SM_TIMEBANK ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Allow Credit Conversions"
					,sys_misc&SM_NOCDTCVT ? "No" : "Yes");
				sprintf(opt[i++],"%-33.33s%s","Allow Local Sysop Access"
					,sys_misc&SM_L_SYSOP ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Allow Remote Sysop Access"
					,sys_misc&SM_R_SYSOP ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Echo Passwords Locally"
					,sys_misc&SM_ECHO_PW ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Require Passwords Locally"
					,sys_misc&SM_REQ_PW ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Short Sysop Page"
					,sys_misc&SM_SHRTPAGE ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Sound Alarm on Error"
					,sys_misc&SM_ERRALARM ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Include Sysop in Statistics"
                    ,sys_misc&SM_SYSSTAT ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Closed to New Users"
                    ,sys_misc&SM_CLOSED ? "Yes" : "No");
                sprintf(opt[i++],"%-33.33s%s","Use Location in User Lists"
					,sys_misc&SM_LISTLOC ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Military (24 hour) Time Format"
					,sys_misc&SM_MILITARY ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","European Date Format (DD/MM/YY)"
					,sys_misc&SM_EURODATE ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","User Expires When Out-of-time"
					,sys_misc&SM_TIME_EXP ? "Yes" : "No");
				sprintf(opt[i++],"%-33.33s%s","Quick Validation Hot-Keys"
					,sys_misc&SM_QVALKEYS ? "Yes" : "No");
				opt[i][0]=0;
				savnum=0;
				SETHELP(WHERE);
/*
System Toggle Options:

This is a menu of system related options that can be toggled between
two or more states, such as Yes and No.
*/
				switch(ulist(WIN_ACT|WIN_BOT|WIN_RHT,0,0,41,&tog_dflt,0
                    ,"Toggle Options",opt)) {
                    case -1:
                        done=1;
                        break;
					case 0:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Allow Users to Use Aliases:

If you want the users of your system to be allowed to be known by a
false name, handle, or alias, set this option to Yes. If you want all
users on your system to be known only by their real names, select No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Use Aliases",opt);
						if(!i && !(uq&UQ_ALIASES)) {
							uq|=UQ_ALIASES;
							changes=1; }
						else if(i==1 && uq&UQ_ALIASES) {
							uq&=~UQ_ALIASES;
							changes=1; }
						break;
					case 1:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Allow Time Banking:

If you want the users of your system to be allowed to be deposit
any extra time they may have left during a call into their minute bank,
set this option to Yes. If this option is set to No, then the only
way a user may get minutes in their minute bank is to purchase them
with credits.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Depost Time in Minute Bank",opt);
						if(!i && !(sys_misc&SM_TIMEBANK)) {
							sys_misc|=SM_TIMEBANK;
							changes=1; }
						else if(i==1 && sys_misc&SM_TIMEBANK) {
							sys_misc&=~SM_TIMEBANK;
							changes=1; }
                        break;
					case 2:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Allow Credits to be Converted into Minutes:

If you want the users of your system to be allowed to be convert
any credits they may have into minutes for their minute bank,
set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Users to Convert Credits into Minutes"
							,opt);
						if(!i && sys_misc&SM_NOCDTCVT) {
							sys_misc&=~SM_NOCDTCVT;
							changes=1; }
						else if(i==1 && !(sys_misc&SM_NOCDTCVT)) {
							sys_misc|=SM_NOCDTCVT;
							changes=1; }
                        break;
					case 3:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Allow Local Sysop Access:

If you want to be able to logon locally with sysop access, set this
option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Local Sysop Access",opt);
						if(!i && !(sys_misc&SM_L_SYSOP)) {
							sys_misc|=SM_L_SYSOP;
							changes=1; }
						else if(i==1 && sys_misc&SM_L_SYSOP) {
							sys_misc&=~SM_L_SYSOP;
							changes=1; }
                        break;
					case 4:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Allow Remote Sysop Access:

If you want to be able to logon remotely with sysop access, set this
option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Allow Remote Sysop Access",opt);
						if(!i && !(sys_misc&SM_R_SYSOP)) {
							sys_misc|=SM_R_SYSOP;
							changes=1; }
						else if(i==1 && sys_misc&SM_R_SYSOP) {
							sys_misc&=~SM_R_SYSOP;
							changes=1; }
                        break;
					case 5:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Echo Passwords Locally:

If you want to passwords to be displayed locally, set this option to
Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Echo Passwords Locally",opt);
						if(!i && !(sys_misc&SM_ECHO_PW)) {
							sys_misc|=SM_ECHO_PW;
							changes=1; }
						else if(i==1 && sys_misc&SM_ECHO_PW) {
							sys_misc&=~SM_ECHO_PW;
							changes=1; }
                        break;
					case 6:
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						i=0;
						SETHELP(WHERE);
/*
Require Passwords Locally:

If you want to passwords to be required when logged on locally, set this
option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Require Passwords Locally",opt);
						if(!i && !(sys_misc&SM_REQ_PW)) {
							sys_misc|=SM_REQ_PW;
							changes=1; }
						else if(i==1 && sys_misc&SM_REQ_PW) {
							sys_misc&=~SM_REQ_PW;
							changes=1; }
                        break;
					case 7:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=0;
						SETHELP(WHERE);
/*
Short Sysop Page:

If you would like the sysop page to be a short series of beeps rather
than continuous random tones, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Short Sysop Page"
							,opt);
						if(i==1 && sys_misc&SM_SHRTPAGE) {
							sys_misc&=~SM_SHRTPAGE;
                            changes=1; }
						else if(!i && !(sys_misc&SM_SHRTPAGE)) {
							sys_misc|=SM_SHRTPAGE;
                            changes=1; }
						break;
					case 8:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=0;
						SETHELP(WHERE);
/*
Sound Alarm on Error:

If you would like to have an alarm sounded locally when a critical
system error has occured, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Sound Alarm on Error",opt);
						if(i==1 && sys_misc&SM_ERRALARM) {
							sys_misc&=~SM_ERRALARM;
                            changes=1; }
						else if(!i && !(sys_misc&SM_ERRALARM)) {
							sys_misc|=SM_ERRALARM;
                            changes=1; }
                        break;
					case 9:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=1;
						SETHELP(WHERE);
/*
Include Sysop Activity in System Statistics:

If you want sysops to be included in the statistical data of the BBS,
set this option to Yes. The suggested setting for this option is
No so that statistical data will only reflect user usage and not
include sysop maintenance activity.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Include Sysop Activity in System Statistics"
							,opt);
                        if(!i && !(sys_misc&SM_SYSSTAT)) {
                            sys_misc|=SM_SYSSTAT;
                            changes=1; }
                        else if(i==1 && sys_misc&SM_SYSSTAT) {
                            sys_misc&=~SM_SYSSTAT;
                            changes=1; }
                        break;
					case 10:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=1;
						SETHELP(WHERE);
/*
Closed to New Users:

If you want callers to be able to logon as New, set this option to No.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Closed to New Users",opt);
                        if(!i && !(sys_misc&SM_CLOSED)) {
                            sys_misc|=SM_CLOSED;
                            changes=1; }
                        else if(i==1 && sys_misc&SM_CLOSED) {
                            sys_misc&=~SM_CLOSED;
                            changes=1; }
                        break;
					case 11:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
                        i=0;
						SETHELP(WHERE);
/*
User Location in User Lists:

If you want user locations (city, state) displayed in the user lists,
set this option to Yes. If this option is set to No, the user notes
(if they exist) are displayed in the user lists.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
                            ,"User Location (Instead of Note) in User Lists"
                            ,opt);
						if(!i && !(sys_misc&SM_LISTLOC)) {
							sys_misc|=SM_LISTLOC;
                            changes=1; }
						else if(i==1 && sys_misc&SM_LISTLOC) {
							sys_misc&=~SM_LISTLOC;
                            changes=1; }
                        break;
					case 12:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=1;
						SETHELP(WHERE);
/*
Military:

If you would like the time-of-day to be displayed and entered in 24 hour
format always, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Use Military Time Format",opt);
						if(!i && !(sys_misc&SM_MILITARY)) {
							sys_misc|=SM_MILITARY;
                            changes=1; }
						else if(i==1 && sys_misc&SM_MILITARY) {
							sys_misc&=~SM_MILITARY;
                            changes=1; }
                        break;
					case 13:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=1;
						SETHELP(WHERE);
/*
European Date Format:

If you would like dates to be displayed and entered in DD/MM/YY format
instead of MM/DD/YY format, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"European Date Format",opt);
						if(!i && !(sys_misc&SM_EURODATE)) {
							sys_misc|=SM_EURODATE;
                            changes=1; }
						else if(i==1 && sys_misc&SM_EURODATE) {
							sys_misc&=~SM_EURODATE;
                            changes=1; }
                        break;

					case 14:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=1;
						SETHELP(WHERE);
/*
User Expires When Out-of-time:

If you want users to be set to Expired User Values if they run out of
time online, then set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"User Expires When Out-of-time",opt);
						if(!i && !(sys_misc&SM_TIME_EXP)) {
							sys_misc|=SM_TIME_EXP;
                            changes=1; }
						else if(i==1 && sys_misc&SM_TIME_EXP) {
							sys_misc&=~SM_TIME_EXP;
                            changes=1; }
                        break;

					case 15:
                        strcpy(opt[0],"Yes");
                        strcpy(opt[1],"No");
						opt[2][0]=0;
						i=1;
						SETHELP(WHERE);
/*
Quick Validation Hot-Keys:

If you would like to enable the Alt-# hot-keys for quick user
validation, set this option to Yes.
*/
						i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0
							,"Quick Validation Hot-Keys",opt);
						if(!i && !(sys_misc&SM_QVALKEYS)) {
							sys_misc|=SM_QVALKEYS;
                            changes=1; }
						else if(i==1 && sys_misc&SM_QVALKEYS) {
							sys_misc&=~SM_QVALKEYS;
                            changes=1; }
                        break;

						} }
            break;
		case 9:    /* New User Values */
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%u","Level",new_level);
				sprintf(opt[i++],"%-27.27s%s","Flag Set #1"
					,ltoaf(new_flags1,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #2"
					,ltoaf(new_flags2,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #3"
					,ltoaf(new_flags3,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #4"
					,ltoaf(new_flags4,str));
				sprintf(opt[i++],"%-27.27s%s","Exemptions"
					,ltoaf(new_exempt,str));
				sprintf(opt[i++],"%-27.27s%s","Restrictions"
					,ltoaf(new_rest,str));
				sprintf(opt[i++],"%-27.27s%s","Expiration Days"
					,itoa(new_expire,str,10));

				ultoac(new_cdt,str);
				sprintf(opt[i++],"%-27.27s%s","Credits",str);
				ultoac(new_min,str);
				sprintf(opt[i++],"%-27.27s%s","Minutes",str);
				sprintf(opt[i++],"%-27.27s%s","Editor"
					,new_xedit);
				sprintf(opt[i++],"%-27.27s%s","Command Shell"
					,shell[new_shell]->code);
				if(new_prot!=SP)
                    sprintf(str,"%c",new_prot);
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
				switch(ulist(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&new_dflt,0
					,"New User Values",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						itoa(new_level,str,10);
						SETHELP(WHERE);
/*
New User Security Level:

This is the security level automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Security Level"
							,str,2,K_EDIT|K_NUMBER);
						new_level=atoi(str);
						break;
					case 1:
						ltoaf(new_flags1,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flag Set #1"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						new_flags1=aftol(str);
						break;
					case 2:
						ltoaf(new_flags2,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flag Set #2"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						new_flags2=aftol(str);
                        break;
					case 3:
						ltoaf(new_flags3,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flag Set #3"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						new_flags3=aftol(str);
                        break;
					case 4:
						ltoaf(new_flags4,str);
						SETHELP(WHERE);
/*
New User Security Flags:

These are the security flags automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flag Set #4"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						new_flags4=aftol(str);
                        break;
					case 5:
						ltoaf(new_exempt,str);
						SETHELP(WHERE);
/*
New User Exemption Flags:

These are the exemptions that are automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						new_exempt=aftol(str);
						break;
					case 6:
						ltoaf(new_rest,str);
						SETHELP(WHERE);
/*
New User Restriction Flags:

These are the restrictions that are automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						new_rest=aftol(str);
						break;
					case 7:
						itoa(new_expire,str,10);
						SETHELP(WHERE);
/*
New User Expiration Days:

If you wish new users to have an automatic expiration date, set this
value to the number of days before the user will expire. To disable
New User expiration, set this value to 0.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Expiration Days",str,4
							,K_EDIT|K_NUMBER);
						new_expire=atoi(str);
						break;
					case 8:
						ultoa(new_cdt,str,10);
						SETHELP(WHERE);
/*
New User Credits:

This is the amount of credits that are automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Credits",str,10
							,K_EDIT|K_NUMBER);
						new_cdt=atol(str);
                        break;
					case 9:
						ultoa(new_min,str,10);
						SETHELP(WHERE);
/*
New User Minutes:

This is the number of extra minutes automatically given to new users.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Minutes (Time Credit)"
							,str,10,K_EDIT|K_NUMBER);
						new_min=atol(str);
						break;
					case 10:
						if(!total_xedits) {
							umsg("No External Editors Configured");
							break; }
						strcpy(opt[0],"Internal");
						for(i=1;i<=total_xedits;i++)
							strcpy(opt[i],xedit[i-1]->code);
						opt[i][0]=0;
						i=0;
						savnum=0;
						SETHELP(WHERE);
/*
New User Editor:

You can use this option to select the default editor for new users.
*/
						i=ulist(WIN_SAV|WIN_RHT,2,1,13,&i,0,"Editors",opt);
						if(i==-1)
							break;
						changes=1;
						if(i && i<=total_xedits)
							sprintf(new_xedit,"%-.8s",xedit[i-1]->code);
						else
							new_xedit[0]=0;
						break;
					case 11:
						for(i=0;i<total_shells && i<MAX_OPTS;i++)
							sprintf(opt[i],"%-.8s",shell[i]->code);
						opt[i][0]=0;
						i=0;
						savnum=0;
						SETHELP(WHERE);
/*
New User Command Shell:

You can use this option to select the default command shell for new
users.
*/
						i=ulist(WIN_SAV|WIN_RHT,2,1,13,&i,0
							,"Command Shells",opt);
						if(i==-1)
							break;
						new_shell=i;
						changes=1;
						break;
					case 12:
						SETHELP(WHERE);
/*
New User Default Download Protocol:

This option allows you to set the default download protocol of new users
(protocol command key or BLANK for no default).
*/
						sprintf(str,"%c",new_prot);
						uinput(WIN_SAV|WIN_MID,0,0
							,"Default Download Protocol (SPACE=Disabled)"
							,str,1,K_EDIT|K_UPPER);
						new_prot=str[0];
						if(new_prot<SP)
							new_prot=SP;
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
								,new_misc&EXPERT ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Screen Pause"
								,new_misc&UPAUSE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Spinning Cursor"
								,new_misc&SPIN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Clear Screen"
								,new_misc&CLRSCRN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Ask For New Scan"
								,new_misc&ASK_NSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Ask For Your Msg Scan"
								,new_misc&ASK_SSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Automatic New File Scan"
								,new_misc&ANFSCAN ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Remember Current Sub-board"
								,new_misc&CURSUB ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Batch Download File Flag"
								,new_misc&BATCHFLAG ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Extended File Descriptions"
								,new_misc&EXTDESC ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Hot Keys"
								,new_misc&COLDKEYS ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Auto Hang-up After Xfer"
								,new_misc&AUTOHANG ? "Yes":"No");
							opt[i][0]=0;
							j=ulist(WIN_BOT|WIN_RHT,2,1,0,&j,0
								,"Default Toggle Options",opt);
                            if(j==-1)
                                break;
                            changes=1;
                            switch(j) {
                                case 0:
									new_misc^=EXPERT;
                                    break;
								case 1:
									new_misc^=UPAUSE;
									break;
								case 2:
									new_misc^=SPIN;
									break;
								case 3:
									new_misc^=CLRSCRN;
									break;
								case 4:
									new_misc^=ASK_NSCAN;
									break;
								case 5:
									new_misc^=ASK_SSCAN;
                                    break;
								case 6:
									new_misc^=ANFSCAN;
									break;
								case 7:
									new_misc^=CURSUB;
									break;
								case 8:
									new_misc^=BATCHFLAG;
									break;
								case 9:
									new_misc^=EXTDESC;
                                    break;
								case 10:
									new_misc^=COLDKEYS;
                                    break;
								case 11:
									new_misc^=AUTOHANG;
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
								,uq&UQ_REALNAME ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Force Unique Real Name"
								,uq&UQ_DUPREAL ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Company Name"
								,uq&UQ_COMPANY ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Multinode Chat Handle"
								,uq&UQ_HANDLE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Force Unique Chat Handle"
								,uq&UQ_DUPHAND ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Sex (Gender)"
								,uq&UQ_SEX ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Birthday"
								,uq&UQ_BIRTH ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Address and Zip Code"
								,uq&UQ_ADDRESS ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Location"
								,uq&UQ_LOCATION ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Require Comma in Location"
								,uq&UQ_NOCOMMAS ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Phone Number"
								,uq&UQ_PHONE ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Computer Type"
								,uq&UQ_COMP ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Multiple Choice Computer"
								,uq&UQ_MC_COMP ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Allow EX-ASCII in Answers"
								,uq&UQ_NOEXASC ? "No":"Yes");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"External Editor"
								,uq&UQ_XEDIT ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Command Shell"
								,uq&UQ_CMDSHELL ? "Yes":"No");
							sprintf(opt[i++],"%-27.27s %-3.3s"
								,"Default Settings"
								,uq&UQ_NODEF ? "No":"Yes");
							opt[i][0]=0;
							j=ulist(WIN_BOT|WIN_RHT|WIN_SAV,2,1,0,&j,0
								,"New User Questions",opt);
                            if(j==-1)
                                break;
                            changes=1;
                            switch(j) {
                                case 0:
									uq^=UQ_REALNAME;
                                    break;
								case 1:
									uq^=UQ_DUPREAL;
									break;
								case 2:
									uq^=UQ_COMPANY;
                                    break;
								case 3:
									uq^=UQ_HANDLE;
                                    break;
								case 4:
									uq^=UQ_DUPHAND;
									break;
								case 5:
									uq^=UQ_SEX;
									break;
								case 6:
									uq^=UQ_BIRTH;
									break;
								case 7:
									uq^=UQ_ADDRESS;
									break;
								case 8:
									uq^=UQ_LOCATION;
									break;
								case 9:
									uq^=UQ_NOCOMMAS;
									break;
								case 10:
									uq^=UQ_PHONE;
									break;
								case 11:
									uq^=UQ_COMP;
									break;
								case 12:
									uq^=UQ_MC_COMP;
									break;
								case 13:
									uq^=UQ_NOEXASC;
									break;
								case 14:
									uq^=UQ_XEDIT;
                                    break;
								case 15:
									uq^=UQ_CMDSHELL;
                                    break;
								case 16:
									uq^=UQ_NODEF;
                                    break;
									} }
						break; } }
			break;
		case 10:	/* Advanced Options */
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","New User Magic Word",new_magic);
				sprintf(opt[i++],"%-27.27s%.40s","Data Directory",data_dir);
				sprintf(opt[i++],"%-27.27s%.40s","Executables Directory"
					,exec_dir);
				sprintf(opt[i++],"%-27.27s%s","Input SIF Questionnaire"
					,new_sif);
				sprintf(opt[i++],"%-27.27s%s","Output SIF Questionnaire"
					,new_sof);
				ultoac(cdt_per_dollar,str);
				sprintf(opt[i++],"%-27.27s%s","Credits Per Dollar",str);
				sprintf(opt[i++],"%-27.27s%u","Minutes Per 100k Credits"
					,cdt_min_value);
				sprintf(opt[i++],"%-27.27s%s","Maximum Number of Minutes"
					,max_minutes ? ltoa(max_minutes,tmp,10) : "Unlimited");
				sprintf(opt[i++],"%-27.27s%u","Warning Days Till Expire"
					,sys_exp_warn);
				sprintf(opt[i++],"%-27.27s%u","Default Status Line"
					,sys_def_stat);
				sprintf(opt[i++],"%-27.27s%u","Last Displayable Node"
					,sys_lastnode);
				sprintf(opt[i++],"%-27.27s%u","First Local Auto-Node"
					,sys_autonode);
				sprintf(opt[i++],"%-27.27s%s","Phone Number Format"
					,sys_phonefmt);
				sprintf(opt[i++],"%-27.27s%.40s","Sysop Chat Requirements"
					,sys_chat_ar);
				opt[i][0]=0;
				savnum=0;
				SETHELP(WHERE);
/*
System Advanced Options:

Care should be taken when modifying any of the options listed here.
*/
				switch(ulist(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&adv_dflt,0
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
						uinput(WIN_MID,0,0,"New User Magic Word",new_magic,20
							,K_EDIT|K_UPPER);
						break;
					case 1:
						SETHELP(WHERE);
/*
Data Directory Parent:

The Synchronet data directory contains almost all the data for your BBS.
This directory must be located where ALL nodes can access it and
MUST NOT be placed on a RAM disk or other volatile media.

This option allows you to change the parent of your data directory.
The \DATA\ suffix (sub-directory) cannot be changed or removed.
*/
						strcpy(str,data_dir);
						if(strstr(str,"\\DATA\\")!=NULL)
							*strstr(str,"\\DATA\\")=0;
						if(uinput(WIN_MID|WIN_SAV,0,9,"Data Dir Parent"
							,str,50,K_EDIT|K_UPPER)>0) {
							if(str[strlen(str)-1]!='\\')
								strcat(str,"\\");
							strcat(str,"DATA\\");
							strcpy(data_dir,str); }
                        break;
					case 2:
						SETHELP(WHERE);
/*
Exec Directory Parent:

The Synchronet exec directory contains executable files that your BBS
executes. This directory does not need to be in your DOS search path.
If you place programs in this directory for the BBS to execute, you
should place the %! abreviation for this exec directory at the
beginning of the command line.

This option allows you to change the parent of your exec directory.
The \EXEC\ suffix (sub-directory) cannot be changed or removed.
*/
						strcpy(str,exec_dir);
						if(strstr(str,"\\EXEC\\")!=NULL)
							*strstr(str,"\\EXEC\\")=0;
						if(uinput(WIN_MID|WIN_SAV,0,9,"Exec Dir Parent"
							,str,50,K_EDIT|K_UPPER)>0) {
							if(str[strlen(str)-1]!='\\')
								strcat(str,"\\");
							strcat(str,"EXEC\\");
							strcpy(exec_dir,str); }
                        break;
					case 3:
						strcpy(str,new_sif);
						SETHELP(WHERE);
/*
SIF Questionnaire for User Input:

This is the name of a SIF questionnaire file that resides your text
directory that all users will be prompted to answer.
*/
						uinput(WIN_MID|WIN_SAV,0,0
							,"SIF Questionnaire for User Input"
							,str,8,K_UPPER|K_EDIT);
						if(!str[0] || code_ok(str))
							strcpy(new_sif,str);
						else
							umsg("Invalid SIF Name");
						break;
					case 4:
						strcpy(str,new_sof);
						SETHELP(WHERE);
/*
SIF Questionnaire for Reviewing User Input:

This is the SIF file used to review the input of users from the user
edit function.
*/
						uinput(WIN_MID|WIN_SAV,0,0
							,"SIF Questionnaire for Reviewing User Input"
							,str,8,K_UPPER|K_EDIT);
						if(!str[0] || code_ok(str))
							strcpy(new_sof,str);
						else
							umsg("Invalid SIF Name");
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
						ultoa(cdt_per_dollar,str,10);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Credits Per Dollar",str,10,K_NUMBER|K_EDIT);
						cdt_per_dollar=atol(str);
						break;
					case 6:
						SETHELP(WHERE);
/*
Minutes Per 100K Credits:

This is the value of a minute of time online. This field is the number
of minutes to give the user in exchange for each 100K credit block.
*/
						sprintf(str,"%u",cdt_min_value);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Minutes Per 100K Credits",str,5,K_NUMBER|K_EDIT);
						cdt_min_value=atoi(str);
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
						sprintf(str,"%lu",max_minutes);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Maximum Number of Minutes a User Can Have "
							"(0=No Limit)"
							,str,10,K_NUMBER|K_EDIT);
						max_minutes=atol(str);
						break;
					case 8:
						SETHELP(WHERE);
/*
Warning Days Till Expire:

If a user's account will expire in this many days or less, the user will
be notified at logon. Setting this value to 0 disables the warning
completely.
*/
						sprintf(str,"%u",sys_exp_warn);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Warning Days Till Expire",str,5,K_NUMBER|K_EDIT);
						sys_exp_warn=atoi(str);
                        break;
					case 9:
						SETHELP(WHERE);
/*
Default Status Line:

This is the number of the status line format that will be the default
display on the bottom line of the screen. For explanation of the
available status lines, see the sysop documentation.
*/
						sprintf(str,"%u",sys_def_stat);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Default Status Line",str,5,K_NUMBER|K_EDIT);
						sys_def_stat=atoi(str);
						break;
					case 10:
						SETHELP(WHERE);
/*
Last Displayed Node:

This is the number of the last node to display to users in node lists.
This allows the sysop to define the higher numbered nodes as invisible
to users.
*/
						sprintf(str,"%u",sys_lastnode);
						uinput(WIN_MID|WIN_SAV,0,0
							,"Last Displayed Node",str,5,K_NUMBER|K_EDIT);
						sys_lastnode=atoi(str);
                        break;
					case 11:
						SETHELP(WHERE);
/*
First Local Auto-Node:

This is the number of the first node in the search for an available
node for local login using the AUTONODE utility.
*/
						sprintf(str,"%u",sys_autonode);
						uinput(WIN_MID|WIN_SAV,0,0
							,"First Local Auto-Node",str,5,K_NUMBER|K_EDIT);
						sys_autonode=atoi(str);
                        break;
					case 12:
						SETHELP(WHERE);
/*
Phone Number Format:

This is the format used for phone numbers in your local calling
area. Use N for number positions, A for alphabetic, or ! for any
character. All other characters will be static in the phone number
format. An example for North American phone numbers is NNN-NNN-NNNN.
*/
						uinput(WIN_MID|WIN_SAV,0,0
							,"Phone Number Format",sys_phonefmt
							,LEN_PHONE,K_UPPER|K_EDIT);
                        break;
					case 13:
						getar("Sysop Chat",sys_chat_ar);
						break;
						} }
				break;
		case 11: /* Loadable Modules */
			done=0;
			k=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-16.16s%s","Login",login_mod);
				sprintf(opt[i++],"%-16.16s%s","Logon Event",logon_mod);
				sprintf(opt[i++],"%-16.16s%s","Sync Event",sync_mod);
				sprintf(opt[i++],"%-16.16s%s","Logoff Event",logoff_mod);
				sprintf(opt[i++],"%-16.16s%s","Logout Event",logout_mod);
				sprintf(opt[i++],"%-16.16s%s","New User Event",newuser_mod);
				sprintf(opt[i++],"%-16.16s%s","Expired User",expire_mod);
				opt[i][0]=0;
				savnum=0;
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
				switch(ulist(WIN_ACT|WIN_T2B|WIN_RHT,0,0,32,&k,0
					,"Loadable Modules",opt)) {

					case -1:
						done=1;
                        break;

					case 0:
						uinput(WIN_MID|WIN_SAV,0,0,"Login Module",login_mod,8
							,K_EDIT|K_UPPER);
                        break;
					case 1:
						uinput(WIN_MID|WIN_SAV,0,0,"Logon Module",logon_mod,8
							,K_EDIT|K_UPPER);
                        break;
					case 2:
						uinput(WIN_MID|WIN_SAV,0,0,"Synchronize Module"
							,sync_mod,8,K_EDIT|K_UPPER);
                        break;
					case 3:
						uinput(WIN_MID|WIN_SAV,0,0,"Logoff Module",logoff_mod,8
							,K_EDIT|K_UPPER);
                        break;
					case 4:
						uinput(WIN_MID|WIN_SAV,0,0,"Logout Module",logout_mod,8
							,K_EDIT|K_UPPER);
                        break;
					case 5:
						uinput(WIN_MID|WIN_SAV,0,0,"New User Module"
							,newuser_mod,8,K_EDIT|K_UPPER);
                        break;
					case 6:
						uinput(WIN_MID|WIN_SAV,0,0,"Expired User Module"
							,expire_mod,8,K_EDIT|K_UPPER);
                        break;

					} }
			break;

		case 12: /* Security Levels */
			dflt=bar=0;
			k=0;
			while(1) {
				for(i=0;i<100;i++) {
					sprintf(tmp,"%luk",level_freecdtperday[i]/1024L);
					sprintf(opt[i],"%-2d    %5d %5d "
						"%5d %5d %5d %5d %6s %7s %2u",i
						,level_timeperday[i],level_timepercall[i]
						,level_callsperday[i],level_emailperday[i]
						,level_postsperday[i],level_linespermsg[i]
						,tmp
						,level_misc[i]&LEVEL_EXPTOVAL ? "Val Set" : "Level"
						,level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
							level_expireto[i] : expired_level); }
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
				i=ulist(WIN_RHT|WIN_ACT,0,3,0,&dflt,&bar
					,"Level   T/D   T/C   C/D   E/D   P/D   L/M   F/D   "
						"Expire To",opt);
				if(i==-1)
					break;
				while(1) {
					sprintf(str,"Security Level %d Values",i);
					j=0;
					sprintf(opt[j++],"%-22.22s%-5u","Time Per Day"
						,level_timeperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Time Per Call"
						,level_timepercall[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Calls Per Day"
						,level_callsperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Email Per Day"
						,level_emailperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Posts Per Day"
                        ,level_postsperday[i]);
					sprintf(opt[j++],"%-22.22s%-5u","Lines Per Message"
						,level_linespermsg[i]);
					sprintf(tmp,"%luk",level_freecdtperday[i]/1024L);
					sprintf(opt[j++],"%-22.22s%-6s","Free Credits Per Day"
						,tmp);
					sprintf(opt[j++],"%-22.22s%s %u","Expire To"
						,level_misc[i]&LEVEL_EXPTOVAL ? "Validation Set"
							: "Level"
						,level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
							level_expireto[i] : expired_level);
					opt[j][0]=0;
					savnum=0;
					j=ulist(WIN_RHT|WIN_SAV|WIN_ACT,2,1,0,&k,0
						,str,opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Total Time Allowed Per Day"
								,itoa(level_timeperday[i],tmp,10),3
								,K_NUMBER|K_EDIT);
							level_timeperday[i]=atoi(tmp);
							if(level_timeperday[i]>500)
								level_timeperday[i]=500;
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Time Allowed Per Call"
								,itoa(level_timepercall[i],tmp,10),3
								,K_NUMBER|K_EDIT);
							level_timepercall[i]=atoi(tmp);
							if(level_timepercall[i]>500)
								level_timepercall[i]=500;
                            break;
						case 2:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Calls Allowed Per Day"
								,itoa(level_callsperday[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							level_callsperday[i]=atoi(tmp);
                            break;
						case 3:
                            uinput(WIN_MID|WIN_SAV,0,0
                                ,"Email Allowed Per Day"
								,itoa(level_emailperday[i],tmp,10),4
                                ,K_NUMBER|K_EDIT);
                            level_emailperday[i]=atoi(tmp);
                            break;
						case 4:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Posts Allowed Per Day"
								,itoa(level_postsperday[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							level_postsperday[i]=atoi(tmp);
                            break;
						case 5:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Lines Allowed Per Message (Post/E-mail)"
								,itoa(level_linespermsg[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							level_linespermsg[i]=atoi(tmp);
							break;
						case 6:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Free Credits Per Day (in Kilobytes)"
								,ultoa(level_freecdtperday[i]/1024L,tmp,10),8
								,K_EDIT|K_UPPER);
							level_freecdtperday[i]=atol(tmp)*1024L;
							break;
						case 7:
							j=0;
							sprintf(opt[j++],"Default Expired Level "
								"(Currently %u)",expired_level);
							sprintf(opt[j++],"Specific Level");
							sprintf(opt[j++],"Quick-Validation Set");
							opt[j][0]=0;
							j=0;
							sprintf(str,"Level %u Expires To",i);
							savnum=1;
							j=ulist(WIN_SAV,2,1,0,&j,0
								,str,opt);
							if(j==-1)
								break;
							if(j==0) {
								level_misc[i]&=
									~(LEVEL_EXPTOLVL|LEVEL_EXPTOVAL);
								changes=1;
								break; }
							if(j==1) {
								level_misc[i]&=~LEVEL_EXPTOVAL;
								level_misc[i]|=LEVEL_EXPTOLVL;
								changes=1;
								uinput(WIN_MID|WIN_SAV,0,0
									,"Expired Level"
									,itoa(level_expireto[i],tmp,10),2
									,K_EDIT|K_NUMBER);
								level_expireto[i]=atoi(tmp);
								break; }
							level_misc[i]&=~LEVEL_EXPTOLVL;
							level_misc[i]|=LEVEL_EXPTOVAL;
							uinput(WIN_MID|WIN_SAV,0,0
								,"Quick-Validation Set to Expire To"
								,itoa(level_expireto[i],tmp,10),1
								,K_EDIT|K_NUMBER);
							level_expireto[i]=atoi(tmp);
							break;
							} } }
			break;
		case 13:	/* Expired Acccount Values */
			dflt=0;
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%u","Level",expired_level);
				sprintf(opt[i++],"%-27.27s%s","Flag Set #1 to Remove"
					,ltoaf(expired_flags1,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #2 to Remove"
					,ltoaf(expired_flags2,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #3 to Remove"
					,ltoaf(expired_flags3,str));
				sprintf(opt[i++],"%-27.27s%s","Flag Set #4 to Remove"
					,ltoaf(expired_flags4,str));
				sprintf(opt[i++],"%-27.27s%s","Exemptions to Remove"
					,ltoaf(expired_exempt,str));
				sprintf(opt[i++],"%-27.27s%s","Restrictions to Add"
					,ltoaf(expired_rest,str));
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
				switch(ulist(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&dflt,0
					,"Expired Account Values",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						itoa(expired_level,str,10);
						SETHELP(WHERE);
/*
Expired Account Security Level:

This is the security level automatically given to expired user accounts.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Security Level"
							,str,2,K_EDIT|K_NUMBER);
						expired_level=atoi(str);
						break;
					case 1:
						ltoaf(expired_flags1,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flags Set #1"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						expired_flags1=aftol(str);
						break;
					case 2:
						ltoaf(expired_flags2,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flags Set #2"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						expired_flags2=aftol(str);
                        break;
					case 3:
						ltoaf(expired_flags3,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flags Set #3"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						expired_flags3=aftol(str);
                        break;
					case 4:
						ltoaf(expired_flags4,str);
						SETHELP(WHERE);
/*
Expired Security Flags to Remove:

These are the security flags automatically removed when a user account
has expired.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Flags Set #4"
							,str,26,K_EDIT|K_UPPER|K_ALPHA);
						expired_flags4=aftol(str);
                        break;
					case 5:
						ltoaf(expired_exempt,str);
						SETHELP(WHERE);
/*
Expired Exemption Flags to Remove:

These are the exemptions that are automatically removed from a user
account if it expires.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						expired_exempt=aftol(str);
						break;
					case 6:
						ltoaf(expired_rest,str);
						SETHELP(WHERE);
/*
Expired Restriction Flags to Add:

These are the restrictions that are automatically added to a user
account if it expires.
*/
						uinput(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
							,K_EDIT|K_UPPER|K_ALPHA);
						expired_rest=aftol(str);
						break; } }
			break;
		case 14:	/* Quick-Validation Values */
			dflt=0;
			k=0;
			while(1) {
				for(i=0;i<10;i++)
					sprintf(opt[i],"%d  SL: %-2d  F1: %s"
						,i,val_level[i],ltoaf(val_flags1[i],str));
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
				savnum=0;
				i=ulist(WIN_RHT|WIN_BOT|WIN_ACT|WIN_SAV,0,0,0,&dflt,0
					,"Quick-Validation Values",opt);
				if(i==-1)
                    break;
				sprintf(str,"Quick-Validation Set %d",i);
				savnum=0;
				while(1) {
					j=0;
					sprintf(opt[j++],"%-22.22s%u","Level",val_level[i]);
					sprintf(opt[j++],"%-22.22s%s","Flag Set #1"
						,ltoaf(val_flags1[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #2"
						,ltoaf(val_flags2[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #3"
						,ltoaf(val_flags3[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Flag Set #4"
						,ltoaf(val_flags4[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Exemptions"
						,ltoaf(val_exempt[i],tmp));
					sprintf(opt[j++],"%-22.22s%s","Restrictions"
						,ltoaf(val_rest[i],tmp));
					sprintf(opt[j++],"%-22.22s%u days","Extend Expiration"
						,val_expire[i]);
					sprintf(opt[j++],"%-22.22s%lu","Additional Credits"
						,val_cdt[i]);
					opt[j][0]=0;
					savnum=1;
					j=ulist(WIN_RHT|WIN_SAV|WIN_ACT,2,1,0,&k,0
						,str,opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Level"
								,itoa(val_level[i],tmp,10),2
								,K_NUMBER|K_EDIT);
							val_level[i]=atoi(tmp);
							break;
						case 1:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Flag Set #1"
								,ltoaf(val_flags1[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_flags1[i]=aftol(tmp);
                            break;
						case 2:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Flag Set #2"
								,ltoaf(val_flags2[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_flags2[i]=aftol(tmp);
                            break;
						case 3:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Flag Set #3"
								,ltoaf(val_flags3[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_flags3[i]=aftol(tmp);
                            break;
						case 4:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Flag Set #4"
								,ltoaf(val_flags4[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_flags4[i]=aftol(tmp);
                            break;
						case 5:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Exemption Flags"
								,ltoaf(val_exempt[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_exempt[i]=aftol(tmp);
                            break;
						case 6:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Restriction Flags"
								,ltoaf(val_rest[i],tmp),26
								,K_UPPER|K_ALPHA|K_EDIT);
							val_rest[i]=aftol(tmp);
							break;
						case 7:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Days to Extend Expiration"
								,itoa(val_expire[i],tmp,10),4
								,K_NUMBER|K_EDIT);
							val_expire[i]=atoi(tmp);
                            break;
						case 8:
							uinput(WIN_MID|WIN_SAV,0,0
								,"Additional Credits"
								,ultoa(val_cdt[i],tmp,10),10
								,K_NUMBER|K_EDIT);
							val_cdt[i]=atol(tmp);
							break; } } }
			break; } }

}
