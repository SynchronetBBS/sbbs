/* $Id: scfgsys.c,v 1.62 2020/04/23 02:40:59 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

static void configure_dst(void)
{
	strcpy(opt[0],"Yes");
	strcpy(opt[1],"No");
	strcpy(opt[2],"Automatic");
	opt[3][0]=0;
	int i=1;
	uifc.helpbuf=
		"`Daylight Saving Time (DST):`\n"
		"\n"
		"If your system is using a U.S. standard time zone, and you would like\n"
		"to have the daylight saving time `flag` automatically toggled for you,\n"
		"set this option to ~Automatic~ (recommended).\n"
		"\n"			
		"The ~DST~ `flag` is used for display purposes only (e.g. to display \"PDT\"\n"
		"instead of \"PST\" and calculate the correct offset from UTC), it does not\n"
		"actually change the time on your computer system(s) for you.\n"
	;
	i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
		,"Daylight Saving Time (DST)",opt);
	if(i==-1)
        return;
	cfg.sys_misc&=~SM_AUTO_DST;
	switch(i) {
		case 0:
			cfg.sys_timezone|=DAYLIGHT;
			break;
		case 1:
			cfg.sys_timezone&=~DAYLIGHT;
			break;
		case 2:
			cfg.sys_misc|=SM_AUTO_DST;
			sys_timezone(&cfg);
			break;
	}
}

void sys_cfg(void)
{
	static int sys_dflt,adv_dflt,tog_dflt,new_dflt;
	char str[81],done=0;
	int i,j,k,dflt,bar;
	char sys_pass[sizeof(cfg.sys_pass)];
	SAFECOPY(sys_pass, cfg.sys_pass);
	char* cryptlib_syspass_helpbuf =
		"`Changing the System Password requires new Cryptlib key and certificate:`\n"
		"\n"
		"The Cryptlib private key (`cryptlib.key`) and TLS certificate (`ssl.cert`)\n"
		"files, located in the Synchronet `ctrl` directory, are encrypted with the\n"
		"current `System Password`.\n"
		"\n"
		"Changing the System Password will require that the Cryptlib Private Key\n"
		"and Certificate files be regenerated.  The Cryptlib key and certificate\n"
		"regeneration should occur automatically after the files are deleted and\n"
		"the Synchronet servers are recycled.";

	while(1) {
		i=0;
		sprintf(opt[i++],"%-33.33s%s","BBS Name",cfg.sys_name);
		sprintf(opt[i++],"%-33.33s%s","Location",cfg.sys_location);
		sprintf(opt[i++],"%-33.33s%s %s","Local Time Zone"
			,smb_zonestr(cfg.sys_timezone,NULL)
			,SMB_TZ_HAS_DST(cfg.sys_timezone) && cfg.sys_misc&SM_AUTO_DST ? "(Auto-DST)" : "");
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
		uifc.helpbuf=
			"`System Configuration:`\n"
			"\n"
			"This menu contains options and sub-menus of options that affect the\n"
			"entire BBS system and the Synchronet Terminal Server in particular.\n"
		;
		switch(uifc.list(WIN_ORG|WIN_ACT|WIN_CHE,0,0,72,&sys_dflt,0
			,"System Configuration",opt)) {
			case -1:
				i=save_changes(WIN_MID);
				if(i==-1)
					break;
				if(!i) {
					cfg.new_install=new_install;
					if(strcmp(sys_pass, cfg.sys_pass) != 0) {
						uifc.helpbuf = cryptlib_syspass_helpbuf;
						if((fexist("ssl.cert") || fexist("cryptlib.key"))
							&& uifc.confirm("System Password Changed. Delete Cryptlib Key and Certificate?")) {
							if(remove("ssl.cert") != 0)
								uifc.msgf("Error %d removing ssl.cert", errno);
							if(remove("cryptlib.key") != 0)
								uifc.msgf("Error %d removing cryptlib.key", errno);
						}
					}
					save_main_cfg(&cfg,backup_level);
					refresh_cfg(&cfg);
				}
				return;
			case 0:
				uifc.helpbuf=
					"`BBS Name:`\n"
					"\n"
					"This is the name of the BBS.\n"
				;
				uifc.input(WIN_MID,0,0,"BBS Name",cfg.sys_name,sizeof(cfg.sys_name)-1,K_EDIT);
				break;
			case 1:
				uifc.helpbuf=
					"`System Location:`\n"
					"\n"
					"This is the location of the BBS. The format is flexible, but it is\n"
					"suggested you use the `City, State` format for clarity.\n"
				;
				uifc.input(WIN_MID,0,0,"Location",cfg.sys_location,sizeof(cfg.sys_location)-1,K_EDIT);
				break;
			case 2:
				i=0;
				uifc.helpbuf=
					"`United States Time Zone:`\n"
					"\n"
					"If your local time zone is the United States, select `Yes`.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"United States Time Zone",uifcYesNoOpts);
				if(i==-1)
					break;
				if(i==0) {
					strcpy(opt[i++],"Atlantic");
					strcpy(opt[i++],"Eastern");
					strcpy(opt[i++],"Central");
					strcpy(opt[i++],"Mountain");
					strcpy(opt[i++],"Pacific");
					strcpy(opt[i++],"Yukon");
					strcpy(opt[i++],"Hawaii/Alaska");
					strcpy(opt[i++],"Bering");
					opt[i][0]=0;
					i=0;
					uifc.helpbuf=
						"`U.S. Time Zone:`\n"
						"\n"
						"Choose the region which most closely reflects your local U.S. time zone.\n"
					;
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"U.S. Time Zone",opt);
					if(i==-1)
						break;
					uifc.changes=1;
					switch(i) {
						case 0:
							cfg.sys_timezone=AST;
							break;
						case 1:
							cfg.sys_timezone=EST;
							break;
						case 2:
							cfg.sys_timezone=CST;
							break;
						case 3:
							cfg.sys_timezone=MST;
							break;
						case 4:
							cfg.sys_timezone=PST;
							break;
						case 5:
							cfg.sys_timezone=YST;
							break;
						case 6:
							cfg.sys_timezone=HST;
							break;
						case 7:
							cfg.sys_timezone=BST;
							break; 
					}
					configure_dst();
					break; 
				}
				i=0;
				strcpy(opt[i++],"Midway");
				strcpy(opt[i++],"Vancouver");
				strcpy(opt[i++],"Edmonton");
				strcpy(opt[i++],"Winnipeg");
				strcpy(opt[i++],"Bogota");
				strcpy(opt[i++],"Caracas");
				strcpy(opt[i++],"Rio de Janeiro");
				strcpy(opt[i++],"Fernando de Noronha");
				strcpy(opt[i++],"Azores");
				strcpy(opt[i++],"Western Europe (WET)");
				strcpy(opt[i++],"Central Europe (CET)");
				strcpy(opt[i++],"Eastern Europe (EET)");
				strcpy(opt[i++],"Moscow");
				strcpy(opt[i++],"Dubai");
				strcpy(opt[i++],"Kabul");
				strcpy(opt[i++],"Karachi");
				strcpy(opt[i++],"Bombay");
				strcpy(opt[i++],"Kathmandu");
				strcpy(opt[i++],"Dhaka");
				strcpy(opt[i++],"Bangkok");
				strcpy(opt[i++],"Hong Kong");
				strcpy(opt[i++],"Tokyo");
				strcpy(opt[i++],"Australian Central");
				strcpy(opt[i++],"Australian Eastern");
				strcpy(opt[i++],"Noumea");
				strcpy(opt[i++],"New Zealand");
				strcpy(opt[i++],"Other...");
				opt[i][0]=0;
				i=0;
				uifc.helpbuf=
					"`Non-U.S. Time Zone:`\n"
					"\n"
					"Choose the region which most closely reflects your local time zone.\n"
					"\n"
					"Choose `Other...` if a region representing your local time zone is\n"
					"not listed (you will be able to set the UTC offset manually)."
				;
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"None-U.S. Time Zone",opt);
				if(i==-1)
					break;
				uifc.changes=1;
				switch(i) {
					case 0:
						cfg.sys_timezone=MID;
						break;
					case 1:
						cfg.sys_timezone=VAN;
						break;
					case 2:
						cfg.sys_timezone=EDM;
						break;
					case 3:
						cfg.sys_timezone=WIN;
						break;
					case 4:
						cfg.sys_timezone=BOG;
						break;
					case 5:
						cfg.sys_timezone=CAR;
						break;
					case 6:
						cfg.sys_timezone=RIO;
						break;
					case 7:
						cfg.sys_timezone=FER;
						break;
					case 8:
						cfg.sys_timezone=AZO;
						break;
					case 9:
						cfg.sys_timezone=WET;
						break;
					case 10:
						cfg.sys_timezone=CET;
						break;
					case 11:
						cfg.sys_timezone=EET;
						break;
					case 12:
						cfg.sys_timezone=MOS;
						break;
					case 13:
						cfg.sys_timezone=DUB;
						break;
					case 14:
						cfg.sys_timezone=KAB;
						break;
					case 15:
						cfg.sys_timezone=KAR;
						break;
					case 16:
						cfg.sys_timezone=BOM;
						break;
					case 17:
						cfg.sys_timezone=KAT;
						break;
					case 18:
						cfg.sys_timezone=DHA;
						break;
					case 19:
						cfg.sys_timezone=BAN;
						break;
					case 20:
						cfg.sys_timezone=HON;
						break;
					case 21:
						cfg.sys_timezone=TOK;
						break;
					case 22:
						cfg.sys_timezone=ACST;
						break;
					case 23:
						cfg.sys_timezone=AEST;
						break;
					case 24:
						cfg.sys_timezone=NOU;
						break;
					case 25:
						cfg.sys_timezone=NZST;
						break;
					default:
						if(cfg.sys_timezone>720 || cfg.sys_timezone<-720)
							cfg.sys_timezone=0;
						if(cfg.sys_timezone==0)
							str[0]=0;
						else
							sprintf(str,"%02d:%02d"
								,cfg.sys_timezone/60,cfg.sys_timezone<0
								? (-cfg.sys_timezone)%60 : cfg.sys_timezone%60);
						uifc.helpbuf=
							"`Time Zone Offset:`\n"
							"\n"
							"Enter your local time zone offset from Universal Time (UTC/GMT)\n"
							"in `HH:MM` format.\n"
						;
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Time (HH:MM) East (+) or West (-) of Universal "
								"Time"
							,str,6,K_EDIT|K_UPPER);
						cfg.sys_timezone=atoi(str)*60;
						char *p=strchr(str,':');
						if(p) {
							if(cfg.sys_timezone<0)
								cfg.sys_timezone-=atoi(p+1);
							else
								cfg.sys_timezone+=atoi(p+1); 
						}
						break;
				}
				if(SMB_TZ_HAS_DST(cfg.sys_timezone))
					configure_dst();
				break;
			case 3:
				uifc.helpbuf=
					"`System Operator:`\n"
					"\n"
					"This is the name or alias of the system operator. This does not have to\n"
					"be the same as user #1. This field is used for informational purposes\n"
					"only.\n"
				;
				uifc.input(WIN_MID,0,0,"System Operator",cfg.sys_op,sizeof(cfg.sys_op)-1,K_EDIT);
				break;
			case 4:
				uifc.helpbuf=cryptlib_syspass_helpbuf;
				if(uifc.deny("Changing SysPass requires new Cryptlib key/cert. Continue?"))
					break;
				uifc.helpbuf=
					"`System Password:`\n"
					"\n"
					"This is an extra security password required for sysop logon and certain\n"
					"sysop functions. This password should be something not easily guessed\n"
					"and should be kept absolutely confidential. This password must be\n"
					"entered at the Terminal Server `SY:` prompt.\n"
				;
				uifc.input(WIN_MID,0,0,"System Password",cfg.sys_pass,sizeof(cfg.sys_pass)-1,K_EDIT|K_UPPER);
				break;
			case 5:
				i=1;
				uifc.helpbuf=
					"`Allow Users to Change Their Password:`\n"
					"\n"
					"If you want the users of your system to have the option of changing\n"
					"their password to a string of their choice, set this option to `Yes`.\n"
					"For the highest level of security, set this option to `No.`\n"
				;
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Users to Change Their Password",uifcYesNoOpts);
				if(!i && !(cfg.sys_misc&SM_PWEDIT)) {
					cfg.sys_misc|=SM_PWEDIT;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_PWEDIT) {
					cfg.sys_misc&=~SM_PWEDIT;
					uifc.changes=1; 
				}
				i=0;
				uifc.helpbuf=
					"`Force Periodic Password uifc.changes:`\n"
					"\n"
					"If you want your users to be forced to change their passwords\n"
					"periodically, select `Yes`.\n"
				;
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Force Periodic Password Changes",opt);
				if(!i) {
					ultoa(cfg.sys_pwdays,str,10);
				uifc.helpbuf=
					"`Maximum Days Between Password uifc.changes:`\n"
					"\n"
					"Enter the maximum number of days allowed between password uifc.changes.\n"
					"If a user has not voluntarily changed his or her password in this\n"
					"many days, he or she will be forced to change their password upon\n"
					"logon.\n"
				;
					uifc.input(WIN_MID,0,0,"Maximum Days Between Password Changes"
						,str,5,K_NUMBER|K_EDIT);
					cfg.sys_pwdays=atoi(str); 
				}
				else if(i==1 && cfg.sys_pwdays) {
					cfg.sys_pwdays=0;
					uifc.changes=1; 
				}
			
				break;
			case 6:
				sprintf(str,"%u",cfg.sys_deldays);
				uifc.helpbuf=
					"`Days Since Last Logon to Preserve Deleted Users:`\n"
					"\n"
					"Deleted user slots can be `undeleted` until the slot is written over\n"
					"by a new user. If you want deleted user slots to be preserved for period\n"
					"of time since their last logon, set this value to the number of days to\n"
					"keep new users from taking over a deleted user's slot.\n"
				;
				uifc.input(WIN_MID,0,0,"Days Since Last Logon to Preserve Deleted Users"
					,str,5,K_EDIT|K_NUMBER);
				cfg.sys_deldays=atoi(str);
				break;
			case 7:
				sprintf(str,"%u",cfg.sys_autodel);
				uifc.helpbuf=
					"`Maximum Days of Inactivity Before Auto-Deletion:`\n"
					"\n"
					"If you want users that haven't logged on in certain period of time to\n"
					"be automatically deleted, set this value to the maximum number of days\n"
					"of inactivity before the user is deleted. Setting this value to `0`\n"
					"disables this feature.\n"
					"\n"
					"Users with the `P` exemption will not be deleted due to inactivity.\n"
				;
				uifc.input(WIN_MID,0,0,"Maximum Days of Inactivity Before Auto-Deletion"
					,str,5,K_EDIT|K_NUMBER);
				cfg.sys_autodel=atoi(str);
				break;
			case 8:
				uifc.helpbuf=
					"`New User Password:`\n"
					"\n"
					"If you want callers to only be able to logon as `New` if they know a\n"
					"certain password, enter that password here. If you want any caller to\n"
					"be able to logon as New, leave this option blank.\n"
				;
				uifc.input(WIN_MID,0,0,"New User Password",cfg.new_pass,sizeof(cfg.new_pass)-1
					,K_EDIT|K_UPPER);
				break;
			case 9:    /* Toggle Options */
				done=0;
				while(!done) {
					i=0;
					sprintf(opt[i++],"%-33.33s%s","Allow User Aliases"
						,cfg.uq&UQ_ALIASES ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Allow Time Banking"
						,cfg.sys_misc&SM_TIMEBANK ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Allow Credit Conversions"
						,cfg.sys_misc&SM_NOCDTCVT ? "No" : "Yes");
					sprintf(opt[i++],"%-33.33s%s","Allow Sysop Logins"
						,cfg.sys_misc&SM_R_SYSOP ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Display/Log Passwords Locally"
						,cfg.sys_misc&SM_ECHO_PW ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Short Sysop Page"
						,cfg.sys_misc&SM_SHRTPAGE ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Include Sysop in Statistics"
						,cfg.sys_misc&SM_SYSSTAT ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Closed to New Users"
						,cfg.sys_misc&SM_CLOSED ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Use Location in User Lists"
						,cfg.sys_misc&SM_LISTLOC ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Military (24 hour) Time Format"
						,cfg.sys_misc&SM_MILITARY ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","European Date Format (DD/MM/YY)"
						,cfg.sys_misc&SM_EURODATE ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","User Expires When Out-of-time"
						,cfg.sys_misc&SM_TIME_EXP ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Require Sys Pass During Login"
						,cfg.sys_misc&SM_SYSPASSLOGIN ? "Yes" : "No");
					sprintf(opt[i++],"%-33.33s%s","Display Sys Info During Logon"
						,cfg.sys_misc&SM_NOSYSINFO ? "No" : "Yes");
					sprintf(opt[i++],"%-33.33s%s","Display Node List During Logon"
						,cfg.sys_misc&SM_NONODELIST ? "No" : "Yes");
					opt[i][0]=0;
					uifc.helpbuf=
						"`System Toggle Options:`\n"
						"\n"
						"This is a menu of system related options that can be toggled between\n"
						"two or more states, such as `Yes` and `No`.\n"
					;
					switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,41,&tog_dflt,0
						,"Toggle Options",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							i=cfg.uq&UQ_ALIASES ? 0:1;
							uifc.helpbuf=
								"`Allow Users to Use Aliases:`\n"
								"\n"
								"If you want the users of your system to be allowed to be known by a\n"
								"false name, handle, or alias, set this option to `Yes`. If you want all\n"
								"users on your system to be known only by their real names, select `No`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Allow Users to Use Aliases",uifcYesNoOpts);
							if(!i && !(cfg.uq&UQ_ALIASES)) {
								cfg.uq|=UQ_ALIASES;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.uq&UQ_ALIASES) {
								cfg.uq&=~UQ_ALIASES;
								uifc.changes=1; 
							}
							break;
						case 1:
							i=cfg.sys_misc&SM_TIMEBANK ? 0:1;
							uifc.helpbuf=
								"`Allow Time Banking:`\n"
								"\n"
								"If you want the users of your system to be allowed to deposit\n"
								"any extra time they may have left during a call into their minute bank,\n"
								"set this option to `Yes`. If this option is set to `No`, then the only\n"
								"way a user may get minutes in their minute bank is to purchase them\n"
								"with credits.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Allow Users to Deposit Time in Minute Bank",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_TIMEBANK)) {
								cfg.sys_misc|=SM_TIMEBANK;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_TIMEBANK) {
								cfg.sys_misc&=~SM_TIMEBANK;
								uifc.changes=1; 
							}
							break;
						case 2:
							i=cfg.sys_misc&SM_NOCDTCVT ? 1:0;
							uifc.helpbuf=
								"`Allow Credits to be Converted into Minutes:`\n"
								"\n"
								"If you want the users of your system to be allowed to convert\n"
								"any credits they may have into minutes for their minute bank,\n"
								"set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Allow Users to Convert Credits into Minutes"
								,uifcYesNoOpts);
							if(!i && cfg.sys_misc&SM_NOCDTCVT) {
								cfg.sys_misc&=~SM_NOCDTCVT;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.sys_misc&SM_NOCDTCVT)) {
								cfg.sys_misc|=SM_NOCDTCVT;
								uifc.changes=1; 
							}
							break;
						case 3:
							i=cfg.sys_misc&SM_R_SYSOP ? 0:1;
							uifc.helpbuf=
								"`Allow Sysop Logins:`\n"
								"\n"
								"If you want to be able to login with sysop access, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Allow Sysop Logins",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_R_SYSOP)) {
								cfg.sys_misc|=SM_R_SYSOP;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_R_SYSOP) {
								cfg.sys_misc&=~SM_R_SYSOP;
								uifc.changes=1; 
							}
							break;
						case 4:
							i=cfg.sys_misc&SM_ECHO_PW ? 0:1;
							uifc.helpbuf=
								"`Display/Log Passwords Locally:`\n"
								"\n"
								"If you want to passwords to be displayed locally and/or logged to disk\n"
								"(e.g. when there's a failed login attempt), set this option to `Yes`.\n"
								"\n"
								"For elevated security, set this option to `No`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Display/Log Passwords Locally",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_ECHO_PW)) {
								cfg.sys_misc|=SM_ECHO_PW;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_ECHO_PW) {
								cfg.sys_misc&=~SM_ECHO_PW;
								uifc.changes=1; 
							}
							break;
						case 5:
							i=cfg.sys_misc&SM_SHRTPAGE ? 0:1;
							uifc.helpbuf=
								"`Short Sysop Page:`\n"
								"\n"
								"If you would like the sysop page to be a short series of beeps rather\n"
								"than continuous random tones, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Short Sysop Page"
								,uifcYesNoOpts);
							if(i==1 && cfg.sys_misc&SM_SHRTPAGE) {
								cfg.sys_misc&=~SM_SHRTPAGE;
								uifc.changes=1; 
							}
							else if(!i && !(cfg.sys_misc&SM_SHRTPAGE)) {
								cfg.sys_misc|=SM_SHRTPAGE;
								uifc.changes=1; 
							}
							break;
						case 6:
							i=cfg.sys_misc&SM_SYSSTAT ? 0:1;
							uifc.helpbuf=
								"`Include Sysop Activity in System Statistics:`\n"
								"\n"
								"If you want sysops to be included in the statistical data of the BBS,\n"
								"set this option to `Yes`. The suggested setting for this option is\n"
								"`No` so that statistical data will only reflect user usage and not\n"
								"include sysop maintenance activity.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Include Sysop Activity in System Statistics"
								,uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_SYSSTAT)) {
								cfg.sys_misc|=SM_SYSSTAT;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_SYSSTAT) {
								cfg.sys_misc&=~SM_SYSSTAT;
								uifc.changes=1; 
							}
							break;
						case 7:
							i=cfg.sys_misc&SM_CLOSED ? 0:1;
							uifc.helpbuf=
								"`Closed to New Users:`\n"
								"\n"
								"If you want callers to be able to logon as `New`, set this option to `No`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Closed to New Users",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_CLOSED)) {
								cfg.sys_misc|=SM_CLOSED;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_CLOSED) {
								cfg.sys_misc&=~SM_CLOSED;
								uifc.changes=1; 
							}
							break;
						case 8:
							i=cfg.sys_misc&SM_LISTLOC ? 0:1;
							uifc.helpbuf=
								"`User Location in User Lists:`\n"
								"\n"
								"If you want user locations (city, state) displayed in the user lists,\n"
								"set this option to `Yes`. If this option is set to `No`, the user notes\n"
								"(if they exist) are displayed in the user lists.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"User Location (Instead of Note) in User Lists"
								,uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_LISTLOC)) {
								cfg.sys_misc|=SM_LISTLOC;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_LISTLOC) {
								cfg.sys_misc&=~SM_LISTLOC;
								uifc.changes=1; 
							}
							break;
						case 9:
							i=cfg.sys_misc&SM_MILITARY ? 0:1;
							uifc.helpbuf=
								"`Military:`\n"
								"\n"
								"If you would like the time-of-day to be displayed and entered in 24 hour\n"
								"format always, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Use Military Time Format",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_MILITARY)) {
								cfg.sys_misc|=SM_MILITARY;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_MILITARY) {
								cfg.sys_misc&=~SM_MILITARY;
								uifc.changes=1; 
							}
							break;
						case 10:
							i=cfg.sys_misc&SM_EURODATE ? 0:1;
							uifc.helpbuf=
								"`European Date Format:`\n"
								"\n"
								"If you would like dates to be displayed and entered in `DD/MM/YY` format\n"
								"instead of `MM/DD/YY` format, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"European Date Format",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_EURODATE)) {
								cfg.sys_misc|=SM_EURODATE;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_EURODATE) {
								cfg.sys_misc&=~SM_EURODATE;
								uifc.changes=1; 
							}
							break;
						case 11:
							i=cfg.sys_misc&SM_TIME_EXP ? 0:1;
							uifc.helpbuf=
								"`User Expires When Out-of-time:`\n"
								"\n"
								"If you want users to be set to `Expired User Values` if they run out of\n"
								"time online, then set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"User Expires When Out-of-time",uifcYesNoOpts);
							if(!i && !(cfg.sys_misc&SM_TIME_EXP)) {
								cfg.sys_misc|=SM_TIME_EXP;
								uifc.changes=1; 
							}
							else if(i==1 && cfg.sys_misc&SM_TIME_EXP) {
								cfg.sys_misc&=~SM_TIME_EXP;
								uifc.changes=1; 
							}
							break;
						case 12:
							i=cfg.sys_misc&SM_SYSPASSLOGIN ? 0:1;
							uifc.helpbuf=
								"`Require System Password for Sysop Login:`\n"
								"\n"
								"If you want to require the correct system password to be provided during\n"
								"system operator logins (in addition to the sysop's personal user account\n"
								"password), set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Require System Password for Sysop Logon",uifcYesNoOpts);
							if(i==1 && cfg.sys_misc&SM_SYSPASSLOGIN) {
								cfg.sys_misc&=~SM_SYSPASSLOGIN;
								uifc.changes=1; 
							}
							else if(i==0 && !(cfg.sys_misc&SM_SYSPASSLOGIN)) {
								cfg.sys_misc|=SM_SYSPASSLOGIN;
								uifc.changes=1; 
							}
							break;
						case 13:
							i=cfg.sys_misc&SM_NOSYSINFO ? 1:0;
							uifc.helpbuf=
								"`Display System Information During Logon:`\n"
								"\n"
								"If you want system information displayed during logon, set this option\n"
								"to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Display System Information During Logon",uifcYesNoOpts);
							if(!i && cfg.sys_misc&SM_NOSYSINFO) {
								cfg.sys_misc&=~SM_NOSYSINFO;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.sys_misc&SM_NOSYSINFO)) {
								cfg.sys_misc|=SM_NOSYSINFO;
								uifc.changes=1; 
							}
							break;
						case 14:
							i=cfg.sys_misc&SM_NONODELIST ? 1:0;
							uifc.helpbuf=
								"`Display Active Node List During Logon:`\n"
								"\n"
								"If you want the active nodes displayed during logon, set this option\n"
								"to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
								,"Display Active Node List During Logon",uifcYesNoOpts);
							if(!i && cfg.sys_misc&SM_NONODELIST) {
								cfg.sys_misc&=~SM_NONODELIST;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.sys_misc&SM_NONODELIST)) {
								cfg.sys_misc|=SM_NONODELIST;
								uifc.changes=1; 
							}
							break;
						} 
					}
				break;
			case 10:    /* New User Values */
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
					if(cfg.new_prot!=' ')
						sprintf(str,"%c",cfg.new_prot);
					else
						strcpy(str,"None");
					sprintf(opt[i++],"%-27.27s%s","Download Protocol",str);
					sprintf(opt[i++],"%-27.27s%hu","Days of New Messages", cfg.new_msgscan_init);
					strcpy(opt[i++],"Default Toggles...");
					strcpy(opt[i++],"Question Toggles...");
					opt[i][0]=0;
					uifc.helpbuf=
						"`New User Values:`\n"
						"\n"
						"This menu allows you to determine the default settings for new users.\n"
					;
					switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&new_dflt,0
						,"New User Values",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							ultoa(cfg.new_level,str,10);
							uifc.helpbuf=
								"`New User Security Level:`\n"
								"\n"
								"This is the security level automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Security Level"
								,str,2,K_EDIT|K_NUMBER);
							cfg.new_level=atoi(str);
							break;
						case 1:
							truncsp(ltoaf(cfg.new_flags1,str));
							uifc.helpbuf=
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #1"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_flags1=aftol(str);
							break;
						case 2:
							truncsp(ltoaf(cfg.new_flags2,str));
							uifc.helpbuf=
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #2"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_flags2=aftol(str);
							break;
						case 3:
							truncsp(ltoaf(cfg.new_flags3,str));
							uifc.helpbuf=
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #3"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_flags3=aftol(str);
							break;
						case 4:
							truncsp(ltoaf(cfg.new_flags4,str));
							uifc.helpbuf=
								"`New User Security Flags:`\n"
								"\n"
								"These are the security flags automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flag Set #4"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_flags4=aftol(str);
							break;
						case 5:
							truncsp(ltoaf(cfg.new_exempt,str));
							uifc.helpbuf=
								"`New User Exemption Flags:`\n"
								"\n"
								"These are the exemptions that are automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
								,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_exempt=aftol(str);
							break;
						case 6:
							truncsp(ltoaf(cfg.new_rest,str));
							uifc.helpbuf=
								"`New User Restriction Flags:`\n"
								"\n"
								"These are the restrictions that are automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
								,K_EDIT|K_UPPER|K_ALPHA);
							cfg.new_rest=aftol(str);
							break;
						case 7:
							ultoa(cfg.new_expire,str,10);
							uifc.helpbuf=
								"`New User Expiration Days:`\n"
								"\n"
								"If you wish new users to have an automatic expiration date, set this\n"
								"value to the number of days before the user will expire. To disable\n"
								"New User expiration, set this value to `0`.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Expiration Days",str,4
								,K_EDIT|K_NUMBER);
							cfg.new_expire=atoi(str);
							break;
						case 8:
							ultoa(cfg.new_cdt,str,10);
							uifc.helpbuf=
								"`New User Credits:`\n"
								"\n"
								"This is the amount of credits that are automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Credits",str,10
								,K_EDIT|K_NUMBER);
							cfg.new_cdt=atol(str);
							break;
						case 9:
							ultoa(cfg.new_min,str,10);
							uifc.helpbuf=
								"`New User Minutes:`\n"
								"\n"
								"This is the number of extra minutes automatically given to new users.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Minutes (Time Credit)"
								,str,10,K_EDIT|K_NUMBER);
							cfg.new_min=atol(str);
							break;
						case 10:
							if(!cfg.total_xedits) {
								uifc.msg("No External Editors Configured");
								break; 
							}
							strcpy(opt[0],"Internal");
							for(i=1;i<=cfg.total_xedits;i++)
								strcpy(opt[i],cfg.xedit[i-1]->code);
							opt[i][0]=0;
							i=0;
							uifc.helpbuf=
								"`New User Editor:`\n"
								"\n"
								"You can use this option to select the default editor for new users.\n"
							;
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
							uifc.helpbuf=
								"`New User Command Shell:`\n"
								"\n"
								"You can use this option to select the default command shell for new\n"
								"users.\n"
							;
							i=uifc.list(WIN_SAV|WIN_RHT,2,1,13,&i,0
								,"Command Shells",opt);
							if(i==-1)
								break;
							cfg.new_shell=i;
							uifc.changes=1;
							break;
						case 12:
							uifc.helpbuf=
								"`New User Default Download Protocol:`\n"
								"\n"
								"This option allows you to set the default download protocol of new users\n"
								"(protocol command key or `BLANK` for no default).\n"
							;
							sprintf(str,"%c",cfg.new_prot);
							uifc.input(WIN_SAV|WIN_MID,0,0
								,"Default Download Protocol (SPACE=Disabled)"
								,str,1,K_EDIT|K_UPPER);
							cfg.new_prot=str[0];
							if(cfg.new_prot<' ')
								cfg.new_prot=' ';
							break;
						case 13:
							uifc.helpbuf=
								"`New User Days of New Messages:`\n"
								"\n"
								"This option allows you to set the number of days worth of messages\n"
								"which will be included in the new user's first `new message scan`.\n"
								"\n"
								"The value `0` means there will be `no` new messages for the new user.\n"
							;
							sprintf(str,"%hu",cfg.new_msgscan_init);
							uifc.input(WIN_SAV|WIN_MID,0,0
								,"Days of New Messages for New User's first new message scan"
								,str,4,K_EDIT|K_NUMBER);
							cfg.new_msgscan_init=atoi(str);
							break;
						case 14:
							uifc.helpbuf=
								"`New User Default Toggle Options:`\n"
								"\n"
								"This menu contains the default state of new user toggle options. All new\n"
								"users on your system will have their defaults set according to the\n"
								"settings on this menu. The user can then change them to his or her\n"
								"liking.\n"
								"\n"
								"See the Synchronet User Manual for more information on the individual\n"
								"options available.\n"
							;
							j=0;
							k=0;
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
								j=uifc.list(WIN_BOT|WIN_RHT,2,1,0,&j,&k
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
									
								} 
							}
							break;
						case 15:
							uifc.helpbuf=
								"`New User Question Toggle Options:`\n"
								"\n"
								"This menu allows you to decide which questions will be asked of a new\n"
								"user.\n"
							;
							j=0;
							k=0;
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
									,"Chat Handle / Call Sign"
									,cfg.uq&UQ_HANDLE ? "Yes":"No");
								sprintf(opt[i++],"%-27.27s %-3.3s"
									,"Force Unique Handle / Call Sign"
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
								sprintf(opt[i++],"%-27.27s %-3.3s"
									,"Color Terminal"
									,cfg.uq&UQ_COLORTERM ? "Yes":"No");
								opt[i][0]=0;
								j=uifc.list(WIN_BOT|WIN_RHT,2,1,0,&j,&k
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
									case 17:
										cfg.uq^=UQ_COLORTERM;
										break;
								} 
							}
						break; 
					} 
				}
				break;
			case 11:	/* Advanced Options */
				done=0;
				while(!done) {
					i=0;
					sprintf(opt[i++],"%-27.27s%s","New User Magic Word",cfg.new_magic);
					sprintf(opt[i++],"%-27.27s%.40s","Data Directory"
						,cfg.data_dir);
					sprintf(opt[i++],"%-27.27s%.40s","Logs Directory"
						,cfg.logs_dir);
					sprintf(opt[i++],"%-27.27s%.40s","Exec Directory"
						,cfg.exec_dir);
					sprintf(opt[i++],"%-27.27s%.40s","Mods Directory"
						,cfg.mods_dir);
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
					sprintf(opt[i++],"%-27.27s%.40s","Sysop Chat Override"
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
					sprintf(opt[i++],"%-27.27s%"PRIX32,"Control Key Pass-through"
						,cfg.ctrlkey_passthru);
					opt[i][0]=0;
					uifc.helpbuf=
						"`System Advanced Options:`\n"
						"\n"
						"Care should be taken when modifying any of the options listed here.\n"
					;
					switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&adv_dflt,0
						,"Advanced Options",opt)) {
						case -1:
							done=1;
							break;

						case 0:
							uifc.helpbuf=
								"`New User Magic Word:`\n"
								"\n"
								"If this field has a value, it is assumed the sysop has placed some\n"
								"reference to this `magic word` in `text/newuser.msg` and new users\n"
								"will be prompted for the magic word after they enter their password.\n"
								"If they do not enter it correctly, it is assumed they didn't read the\n"
								"new user information displayed to them and they are disconnected.\n"
								"\n"
								"Think of it as a password to guarantee that new users read the text\n"
								"displayed to them.\n"
							;
							uifc.input(WIN_MID,0,0,"New User Magic Word",cfg.new_magic,sizeof(cfg.new_magic)-1
								,K_EDIT|K_UPPER);
							break;
						case 1:
							uifc.helpbuf=
								"`Data File Directory:`\n"
								"\n"
								"The Synchronet data directory contains almost all the data for your BBS.\n"
								"This directory must be located where `ALL` nodes can access it and\n"
								"`MUST NOT` be placed on a RAM disk or other volatile media.\n"
								"\n"
								"This option allows you to change the location of your data directory.\n"
							;
							strcpy(str,cfg.data_dir);
							if(uifc.input(WIN_MID|WIN_SAV,0,9,"Data Directory"
								,str,sizeof(cfg.data_dir)-1,K_EDIT)>0) {
								backslash(str);
								SAFECOPY(cfg.data_dir,str); 
							}
							break;
						case 2:
							uifc.helpbuf=
								"`Log File Directory:`\n"
								"\n"
								"Log files will be stored in this directory.\n"
							;
							strcpy(str,cfg.logs_dir);
							if(uifc.input(WIN_MID|WIN_SAV,0,9,"Logs Directory"
								,str,sizeof(cfg.logs_dir)-1,K_EDIT)>0) {
								backslash(str);
								SAFECOPY(cfg.logs_dir,str); 
							}
							break;
						case 3:
							uifc.helpbuf=
								"`Executable/Module File Directory:`\n"
								"\n"
								"The Synchronet exec directory contains executable files that your BBS\n"
								"executes. This directory does `not` need to be in your DOS search path.\n"
								"If you place programs in this directory for the BBS to execute, you\n"
								"should place the `%!` abbreviation for the exec directory at the\n"
								"beginning of the configured command-lines.\n"
								"\n"
								"This option allows you to change the location of your exec directory.\n"
							;
							strcpy(str,cfg.exec_dir);
							if(uifc.input(WIN_MID|WIN_SAV,0,9,"Exec Directory"
								,str,sizeof(cfg.exec_dir)-1,K_EDIT)>0) {
								backslash(str);
								SAFECOPY(cfg.exec_dir,str); 
							}
							break;
						case 4:
							uifc.helpbuf=
								"`Modified Modules Directory:`\n"
								"\n"
								"This optional directory can be used to specify a location where modified\n"
								"module files are stored. These modified modules will take precedence over\n"
								"stock modules with the same filename (in the exec directory) and will\n"
								"not be overwritten by future updates/upgrades.\n"
								"\n"
								"If this directory is `blank`, then this feature is not used and all modules\n"
								"are assumed to be located in the `exec` directory.\n"
							;
							strcpy(str,cfg.mods_dir);
							if(uifc.input(WIN_MID|WIN_SAV,0,9,"Mods Directory"
								,str,sizeof(cfg.mods_dir)-1,K_EDIT)>0) {
								backslash(str);
								SAFECOPY(cfg.mods_dir,str); 
							}
							break;
						case 5:
							strcpy(str,cfg.new_sif);
							uifc.helpbuf=
								"`SIF Questionnaire for User Input:`\n"
								"\n"
								"This is the name of a SIF questionnaire file that resides your text\n"
								"directory that all users will be prompted to answer.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"SIF Questionnaire for User Input"
								,str,LEN_CODE,K_UPPER|K_EDIT);
							if(!str[0] || code_ok(str))
								strcpy(cfg.new_sif,str);
							else
								uifc.msg("Invalid SIF Name");
							break;
						case 6:
							strcpy(str,cfg.new_sof);
							uifc.helpbuf=
								"`SIF Questionnaire for Reviewing User Input:`\n"
								"\n"
								"This is the SIF file used to review the input of users from the user\n"
								"edit function.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"SIF Questionnaire for Reviewing User Input"
								,str,LEN_CODE,K_EDIT);
							if(!str[0] || code_ok(str))
								strcpy(cfg.new_sof,str);
							else
								uifc.msg("Invalid SIF Name");
							break;
						case 7:
							uifc.helpbuf=
								"`Credits Per Dollar:`\n"
								"\n"
								"This is the monetary value of a credit (How many credits per dollar).\n"
								"This value should be a power of 2 (1, 2, 4, 8, 16, 32, 64, 128, etc.)\n"
								"since credits are usually converted by 100 kilobyte (102400) blocks.\n"
								"To make a dollar worth two megabytes of credits, set this value to\n"
								"2,097,152 (a megabyte is 1024*1024 or 1048576).\n"
							;
							ultoa(cfg.cdt_per_dollar,str,10);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Credits Per Dollar",str,10,K_NUMBER|K_EDIT);
							cfg.cdt_per_dollar=atol(str);
							break;
						case 8:
							uifc.helpbuf=
								"`Minutes Per 100K Credits:`\n"
								"\n"
								"This is the value of a minute of time online. This field is the number\n"
								"of minutes to give the user in exchange for each 100K credit block.\n"
							;
							sprintf(str,"%u",cfg.cdt_min_value);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Minutes Per 100K Credits",str,5,K_NUMBER|K_EDIT);
							cfg.cdt_min_value=atoi(str);
							break;
						case 9:
							uifc.helpbuf=
								"`Maximum Number of Minutes User Can Have:`\n"
								"\n"
								"This value is the maximum total number of minutes a user can have. If a\n"
								"user has this number of minutes or more, they will not be allowed to\n"
								"convert credits into minutes. A sysop can add minutes to a user's\n"
								"account regardless of this maximum. If this value is set to `0`, users\n"
								"will have no limit on the total number of minutes they can have.\n"
							;
							sprintf(str,"%"PRIu32,cfg.max_minutes);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Maximum Number of Minutes a User Can Have "
								"(0=No Limit)"
								,str,10,K_NUMBER|K_EDIT);
							cfg.max_minutes=atol(str);
							break;
						case 10:
							uifc.helpbuf=
								"`Warning Days Till Expire:`\n"
								"\n"
								"If a user's account will expire in this many days or less, the user will\n"
								"be notified at logon. Setting this value to `0` disables the warning\n"
								"completely.\n"
							;
							sprintf(str,"%u",cfg.sys_exp_warn);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Warning Days Till Expire",str,5,K_NUMBER|K_EDIT);
							cfg.sys_exp_warn=atoi(str);
							break;
						case 11:
							uifc.helpbuf=
								"`Last Displayed Node:`\n"
								"\n"
								"This is the number of the last node to display to users in node lists.\n"
								"This allows the sysop to define the higher numbered nodes as `invisible`\n"
								"to users.\n"
							;
							sprintf(str,"%u",cfg.sys_lastnode);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Last Displayed Node",str,5,K_NUMBER|K_EDIT);
							cfg.sys_lastnode=atoi(str);
							break;
						case 12:
							uifc.helpbuf=
								"`Phone Number Format:`\n"
								"\n"
								"This is the format used for phone numbers in your local calling\n"
								"area. Use `N` for number positions, `A` for alphabetic, or `!` for any\n"
								"character. All other characters will be static in the phone number\n"
								"format. An example for North American phone numbers is `NNN-NNN-NNNN`.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Phone Number Format",cfg.sys_phonefmt
								,LEN_PHONE,K_UPPER|K_EDIT);
							break;
						case 13:
							getar("Sysop Chat Override",cfg.sys_chat_arstr);
							break;
						case 14:
							uifc.helpbuf=
								"`User Database Backups:`\n"
								"\n"
								"Setting this option to anything but 0 will enable automatic daily\n"
								"backups of the user database. This number determines how many backups\n"
								"to keep on disk.\n"
							;
							sprintf(str,"%u",cfg.user_backup_level);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Number of Daily User Database Backups to Keep"
								,str,4,K_NUMBER|K_EDIT);
							cfg.user_backup_level=atoi(str);
							break;
						case 15:
							uifc.helpbuf=
								"`Mail Database Backups:`\n"
								"\n"
								"Setting this option to anything but 0 will enable automatic daily\n"
								"backups of the mail database. This number determines how many backups\n"
								"to keep on disk.\n"
							;
							sprintf(str,"%u",cfg.mail_backup_level);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Number of Daily Mail Database Backups to Keep"
								,str,4,K_NUMBER|K_EDIT);
							cfg.mail_backup_level=atoi(str);
							break;
						case 16:
							uifc.helpbuf=
								"`Control Key Pass-through:`\n"
								"\n"
								"This value is a 32-bit hexadecimal number. Each set bit represents a\n"
								"control key combination that will `not` be handled internally by\n"
								"Synchronet or by a Global Hot Key Event.\n"
								"\n"
								"To disable internal handling of the `Ctrl-C` key combination (for example)\n"
								"set this value to `8`. The value is determined by taking 2 to the power of\n"
								"the ASCII value of the control character (Ctrl-A is 1, Ctrl-B is 2, \n"
								"etc.). In the case of Ctrl-C, taking 2 to the power of 3 equals 8.\n"
								"\n"
								"To pass-through multiple control key combinations, multiple bits must be\n"
								"set (or'd together) to create the necessary value, which may require the\n"
								"use of a hexadecimal calculator.\n"
								"\n"
								"If unsure, leave this value set to `0`, the default.\n"
							;
							sprintf(str,"%"PRIX32,cfg.ctrlkey_passthru);
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Control Key Pass-through"
								,str,8,K_UPPER|K_EDIT);
							cfg.ctrlkey_passthru=strtoul(str,NULL,16);
							break;
						} 
					}
					break;
			case 12: /* Loadable Modules */
				done=0;
				k=0;
				while(!done) {
					i=0;
					sprintf(opt[i++],"%-16.16s%s","Login",cfg.login_mod);
					sprintf(opt[i++],"%-16.16s%s","Logon",cfg.logon_mod);
					sprintf(opt[i++],"%-16.16s%s","Sync",cfg.sync_mod);
					sprintf(opt[i++],"%-16.16s%s","Logoff",cfg.logoff_mod);
					sprintf(opt[i++],"%-16.16s%s","Logout",cfg.logout_mod);
					sprintf(opt[i++],"%-16.16s%s","New User",cfg.newuser_mod);
					sprintf(opt[i++],"%-16.16s%s","Expired User",cfg.expire_mod);
					sprintf(opt[i++],"%-16.16s%s","Auto Message",cfg.automsg_mod);
					sprintf(opt[i++],"%-16.16s%s","Text Section",cfg.textsec_mod);
					sprintf(opt[i++],"%-16.16s%s","Xtrn Section",cfg.xtrnsec_mod);
					sprintf(opt[i++],"%-16.16s%s","Read Mail",cfg.readmail_mod);
					sprintf(opt[i++],"%-16.16s%s","Scan Msgs",cfg.scanposts_mod);
					sprintf(opt[i++],"%-16.16s%s","Scan Subs",cfg.scansubs_mod);
					sprintf(opt[i++],"%-16.16s%s","List Msgs",cfg.listmsgs_mod);
					sprintf(opt[i++],"%-16.16s%s","List Logons",cfg.logonlist_mod);
					sprintf(opt[i++],"%-16.16s%s","List Nodes",cfg.nodelist_mod);
					sprintf(opt[i++],"%-16.16s%s","Who's Online",cfg.whosonline_mod);
					sprintf(opt[i++],"%-16.16s%s","Private Msg",cfg.privatemsg_mod);
					opt[i][0]=0;
					uifc.helpbuf=
						"`Loadable Modules:`\n"
						"\n"
						"Baja modules (`.bin` files) or JavaScript modules (`.js` files) can be\n"
						"automatically loaded and executed during certain Terminal Server\n"
						"operations. The name (root filename) of the module can be specified for\n"
						"each of the available operations listed below:\n"
						"\n"
						"`Login`        Required module for interactive terminal logins (answer)\n"
						"`Logon`        Executed during terminal logon procedure\n"
						"`Sync`         Executed when terminal nodes are periodically synchronized\n"
						"`Logoff`       Executed during terminal logoff procedure (interactive)\n"
						"`Logout`       Executed during terminal logout procedure (offline)\n"
						"`New User`     Executed at end of new terminal user creation process\n"
						"`Expired User` Executed during daily event when user expires (offline)\n"
						"`Auto Message` Executed when a user chooses to edit the auto-message\n"
						"`Text Section` Executed to handle general text file (viewing) section\n"
						"`Xtrn Section` Executed to handle external programs (doors) section\n"
						"\n"
						"Full module command-lines may be used for the operations listed below:\n"
						"\n"
						"`Read Mail`    Executed when a user reads email/netmail\n"
						"`Scan Msgs`    Executed when a user reads or scans a message sub-board\n"
						"`Scan Subs`    Executed when a user scans one or more sub-boards for msgs\n"
						"`List Msgs`    Executed when a user lists msgs from the msg read prompt\n"
						"`List Logons`  Executed when a user lists logons (i.e. '-y' for yesterday)\n"
						"`List Nodes`   Executed when a user lists all nodes\n"
						"`Who's Online` Executed when a user lists the nodes in-use (e.g. `^U`)\n"
						"`Private Msg`  Executed when a user sends a private node msg (e.g. `^P`)\n"
						"\n"
						"`Note:` JavaScript modules take precedence over Baja modules if both exist\n"
						"in your `exec` or `mods` directories.\n"
					;
					switch(uifc.list(WIN_ACT|WIN_T2B|WIN_RHT,0,0,32,&k,0
						,"Loadable Modules",opt)) {

						case -1:
							done=1;
							break;

						case 0:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Login Module"
								,cfg.login_mod,sizeof(cfg.login_mod)-1,K_EDIT);
							break;
						case 1:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Logon Module"
								,cfg.logon_mod,sizeof(cfg.logon_mod)-1,K_EDIT);
							break;
						case 2:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Synchronize Module"
								,cfg.sync_mod,sizeof(cfg.sync_mod)-1,K_EDIT);
							break;
						case 3:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Logoff Module"
								,cfg.logoff_mod,sizeof(cfg.logoff_mod)-1,K_EDIT);
							break;
						case 4:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Logout Module"
								,cfg.logout_mod,sizeof(cfg.logout_mod)-1,K_EDIT);
							break;
						case 5:
							uifc.input(WIN_MID|WIN_SAV,0,0,"New User Module"
								,cfg.newuser_mod,sizeof(cfg.newuser_mod)-1,K_EDIT);
							break;
						case 6:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Expired User Module"
								,cfg.expire_mod,sizeof(cfg.expire_mod)-1,K_EDIT);
							break;
						case 7:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Auto Message Module"
								,cfg.automsg_mod,sizeof(cfg.automsg_mod)-1,K_EDIT);
							break;
						case 8:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Text File Section Module"
								,cfg.textsec_mod,sizeof(cfg.textsec_mod)-1,K_EDIT);
							break;
						case 9:
							uifc.input(WIN_MID|WIN_SAV,0,0,"External Program Section Module"
								,cfg.xtrnsec_mod,sizeof(cfg.xtrnsec_mod)-1,K_EDIT);
							break;
						case 10:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Read Mail Command"
								,cfg.readmail_mod,sizeof(cfg.readmail_mod)-1,K_EDIT);
							break;
						case 11:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Scan Msgs Command"
								,cfg.scanposts_mod,sizeof(cfg.scanposts_mod)-1,K_EDIT);
							break;
						case 12:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Scan Subs Command"
								,cfg.scansubs_mod,sizeof(cfg.scansubs_mod)-1,K_EDIT);
							break;
						case 13:
							uifc.input(WIN_MID|WIN_SAV,0,0,"List Msgs Command"
								,cfg.listmsgs_mod,sizeof(cfg.listmsgs_mod)-1,K_EDIT);
							break;
						case 14:
							uifc.input(WIN_MID|WIN_SAV,0,0,"List Logons Command"
								,cfg.logonlist_mod,sizeof(cfg.logonlist_mod)-1,K_EDIT);
							break;
						case 15:
							uifc.input(WIN_MID|WIN_SAV,0,0,"List Nodes Command"
								,cfg.nodelist_mod,sizeof(cfg.nodelist_mod)-1,K_EDIT);
							break;
						case 16:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Who's Online Command"
								,cfg.whosonline_mod,sizeof(cfg.whosonline_mod)-1,K_EDIT);
							break;
						case 17:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Private Message Command"
								,cfg.privatemsg_mod,sizeof(cfg.privatemsg_mod)-1,K_EDIT);
							break;
					} 
				}
				break;

			case 13: /* Security Levels */
				dflt=bar=0;
				k=0;
				while(1) {
					for(i=0;i<100;i++) {
						byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
						sprintf(opt[i],"%-2d    %5d %5d "
							"%5d %5d %5d %5d %6s %7s %2u",i
							,cfg.level_timeperday[i],cfg.level_timepercall[i]
							,cfg.level_callsperday[i],cfg.level_emailperday[i]
							,cfg.level_postsperday[i],cfg.level_linespermsg[i]
							,tmp
							,cfg.level_misc[i]&LEVEL_EXPTOVAL ? "Val Set" : "Level"
							,cfg.level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
								cfg.level_expireto[i] : cfg.expired_level); 
					}
					opt[i][0]=0;
					i=0;
					uifc.helpbuf=
						"`Security Level Values:`\n"
						"\n"
						"This menu allows you to change the security options for every possible\n"
						"security level from 0 to 99. The available options for each level are:\n"
						"\n"
						"    Time Per Day           Maximum online time per day\n"
						"    Time Per Call          Maximum online time per call\n"
						"    Calls Per Day          Maximum number of calls per day\n"
						"    Email Per Day          Maximum number of email per day\n"
						"    Posts Per Day          Maximum number of posts per day\n"
						"    Lines Per Message      Maximum number of lines per message\n"
						"    Free Credits Per Day   Number of free credits per day\n"
						"    Expire To              Level or validation set to Expire to\n"
					;
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
						byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
						sprintf(opt[j++],"%-22.22s%-6s","Free Credits Per Day"
							,tmp);
						sprintf(opt[j++],"%-22.22s%s %u","Expire To"
							,cfg.level_misc[i]&LEVEL_EXPTOVAL ? "Validation Set"
								: "Level"
							,cfg.level_misc[i]&(LEVEL_EXPTOVAL|LEVEL_EXPTOLVL) ?
								cfg.level_expireto[i] : cfg.expired_level);
						opt[j][0]=0;
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
									,ultoa(cfg.level_linespermsg[i],tmp,10),5
									,K_NUMBER|K_EDIT);
								cfg.level_linespermsg[i]=atoi(tmp);
								break;
							case 6:
								byte_count_to_str(cfg.level_freecdtperday[i], tmp, sizeof(tmp));
								if(uifc.input(WIN_MID|WIN_SAV,0,0
									,"Free Credits Per Day"
									,tmp,10
									,K_EDIT|K_UPPER) > 0)
									cfg.level_freecdtperday[i] = (int32_t)parse_byte_count(tmp, 1);
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
								j=uifc.list(WIN_SAV,2,1,0,&j,0
									,str,opt);
								if(j==-1)
									break;
								if(j==0) {
									cfg.level_misc[i]&=
										~(LEVEL_EXPTOLVL|LEVEL_EXPTOVAL);
									uifc.changes=1;
									break; 
								}
								if(j==1) {
									cfg.level_misc[i]&=~LEVEL_EXPTOVAL;
									cfg.level_misc[i]|=LEVEL_EXPTOLVL;
									uifc.changes=1;
									uifc.input(WIN_MID|WIN_SAV,0,0
										,"Expired Level"
										,ultoa(cfg.level_expireto[i],tmp,10),2
										,K_EDIT|K_NUMBER);
									cfg.level_expireto[i]=atoi(tmp);
									break; 
								}
								cfg.level_misc[i]&=~LEVEL_EXPTOLVL;
								cfg.level_misc[i]|=LEVEL_EXPTOVAL;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Quick-Validation Set to Expire To"
									,ultoa(cfg.level_expireto[i],tmp,10),1
									,K_EDIT|K_NUMBER);
								cfg.level_expireto[i]=atoi(tmp);
								break;
						} 
					} 
				}
				break;
			case 14:	/* Expired Acccount Values */
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
					uifc.helpbuf=
						"`Expired Account Values:`\n"
						"\n"
						"If a user's account expires, the security levels for that account will\n"
						"be modified according to the settings of this menu. The `Main Level` and\n"
						"`Transfer Level` will be set to the values listed on this menu. The `Main\n"
						"Flags`, `Transfer Flags`, and `Exemptions` listed on this menu will be\n"
						"removed from the account if they are set. The `Restrictions` listed will\n"
						"be added to the account.\n"
					;
					switch(uifc.list(WIN_ACT|WIN_BOT|WIN_RHT,0,0,60,&dflt,0
						,"Expired Account Values",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							ultoa(cfg.expired_level,str,10);
							uifc.helpbuf=
								"`Expired Account Security Level:`\n"
								"\n"
								"This is the security level automatically given to expired user accounts.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Security Level"
								,str,2,K_EDIT|K_NUMBER);
							cfg.expired_level=atoi(str);
							break;
						case 1:
							truncsp(ltoaf(cfg.expired_flags1,str));
							uifc.helpbuf=
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #1"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_flags1=aftol(str);
							break;
						case 2:
							truncsp(ltoaf(cfg.expired_flags2,str));
							uifc.helpbuf=
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #2"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_flags2=aftol(str);
							break;
						case 3:
							truncsp(ltoaf(cfg.expired_flags3,str));
							uifc.helpbuf=
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #3"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_flags3=aftol(str);
							break;
						case 4:
							truncsp(ltoaf(cfg.expired_flags4,str));
							uifc.helpbuf=
								"`Expired Security Flags to Remove:`\n"
								"\n"
								"These are the security flags automatically removed when a user account\n"
								"has expired.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Flags Set #4"
								,str,26,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_flags4=aftol(str);
							break;
						case 5:
							truncsp(ltoaf(cfg.expired_exempt,str));
							uifc.helpbuf=
								"`Expired Exemption Flags to Remove:`\n"
								"\n"
								"These are the exemptions that are automatically removed from a user\n"
								"account if it expires.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Exemption Flags",str,26
								,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_exempt=aftol(str);
							break;
						case 6:
							truncsp(ltoaf(cfg.expired_rest,str));
							uifc.helpbuf=
								"`Expired Restriction Flags to Add:`\n"
								"\n"
								"These are the restrictions that are automatically added to a user\n"
								"account if it expires.\n"
							;
							uifc.input(WIN_SAV|WIN_MID,0,0,"Restriction Flags",str,26
								,K_EDIT|K_UPPER|K_ALPHA);
							cfg.expired_rest=aftol(str);
							break; 
						} 
				}
				break;
			case 15:	/* Quick-Validation Values */
				dflt=0;
				k=0;
				while(1) {
					for(i=0;i<10;i++)
						sprintf(opt[i],"%d  SL: %-2d  F1: %s"
							,i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],str));
					opt[i][0]=0;
					i=0;
					uifc.helpbuf=
						"`Quick-Validation Values:`\n"
						"\n"
						"This is a list of the ten quick-validation sets. These sets are used to\n"
						"quickly set a user's security values (Level, Flags, Exemptions,\n"
						"Restrictions, Expiration Date, and Credits) with one key stroke. The\n"
						"user's expiration date may be extended and additional credits may also\n"
						"be added using quick-validation sets.\n"
						"\n"
						"Holding down ~ ALT ~ and one of the number keys (`1-9`) while a user\n"
						"is online, automatically sets his or user security values to the\n"
						"quick-validation set for that number key.\n"
						"\n"
						"From within the `User Edit` function, a sysop can use the `V`alidate\n"
						"User command and select from this quick-validation list to change a\n"
						"user's security values with very few key-strokes.\n"
					;
					i=uifc.list(WIN_RHT|WIN_BOT|WIN_ACT|WIN_SAV,0,0,0,&dflt,0
						,"Quick-Validation Values",opt);
					if(i==-1)
						break;
					sprintf(str,"Quick-Validation Set %d",i);
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
						sprintf(opt[j++],"%-22.22s%u","Additional Credits"
							,cfg.val_cdt[i]);
						opt[j][0]=0;
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
									,truncsp(ltoaf(cfg.val_flags1[i],tmp)),26
									,K_UPPER|K_ALPHA|K_EDIT);
								cfg.val_flags1[i]=aftol(tmp);
								break;
							case 2:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Flag Set #2"
									,truncsp(ltoaf(cfg.val_flags2[i],tmp)),26
									,K_UPPER|K_ALPHA|K_EDIT);
								cfg.val_flags2[i]=aftol(tmp);
								break;
							case 3:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Flag Set #3"
									,truncsp(ltoaf(cfg.val_flags3[i],tmp)),26
									,K_UPPER|K_ALPHA|K_EDIT);
								cfg.val_flags3[i]=aftol(tmp);
								break;
							case 4:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Flag Set #4"
									,truncsp(ltoaf(cfg.val_flags4[i],tmp)),26
									,K_UPPER|K_ALPHA|K_EDIT);
								cfg.val_flags4[i]=aftol(tmp);
								break;
							case 5:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Exemption Flags"
									,truncsp(ltoaf(cfg.val_exempt[i],tmp)),26
									,K_UPPER|K_ALPHA|K_EDIT);
								cfg.val_exempt[i]=aftol(tmp);
								break;
							case 6:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Restriction Flags"
									,truncsp(ltoaf(cfg.val_rest[i],tmp)),26
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
								break; 
						} 
					} 
				}
				break; 
		} 
	}
}
