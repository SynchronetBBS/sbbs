/* scfgxtrn.c */

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
/****************************************************************************/
/* Synchronet configuration utility 										*/
/****************************************************************************/

#include "scfg.h"

char *daystr(char days);
static void hotkey_cfg(void);

static char* mdaystr(long mdays)
{
	int		i;
	char	tmp[16];
	static	char str[256];

	if(mdays==0 || mdays==1)
		return("Any");

	str[0]=0;
	for(i=1;i<32;i++) {
		if((mdays&(1<<i))==0)
			continue;
		if(str[0])
			strcat(str," ");
		sprintf(tmp,"%u",i);
		strcat(str,tmp);
	}
	
	return(str);
}

static char* dropfile(int type, ulong misc)
{
	static char str[128];
	char fname[64]="";

	switch(type) {
		case XTRN_SBBS:
			strcpy(fname,"XTRN.DAT");
			break;
		case XTRN_WWIV:
			strcpy(fname,"CHAIN.TXT");
			break;
		case XTRN_GAP:
			strcpy(fname,"DOOR.SYS");
			break;
		case XTRN_RBBS:
			strcpy(fname,"DORINFO#.DEF");
			break;
		case XTRN_RBBS1:
			strcpy(fname,"DORINFO1.DEF");
            break;
		case XTRN_WILDCAT:
			strcpy(fname,"CALLINFO.BBS");
			break;
		case XTRN_PCBOARD:
			strcpy(fname,"PCBOARD.SYS");
			break;
		case XTRN_SPITFIRE:
			strcpy(fname,"SFDOORS.DAT");
			break;
		case XTRN_UTI:
			strcpy(fname,"UTIDOOR.TXT");
			break;
		case XTRN_SR:
			strcpy(fname,"DOORFILE.SR");
			break;
		case XTRN_TRIBBS:
			strcpy(fname,"TRIBBS.SYS");
			break;
        case XTRN_DOOR32:
            strcpy(fname,"DOOR32.SYS");
            break;
	}

	if(misc&XTRN_LWRCASE)
		strlwr(fname);

	switch(type) {
		case XTRN_SBBS:
			sprintf(str,"%-15s %s","Synchronet",fname);
			break;
		case XTRN_WWIV:
			sprintf(str,"%-15s %s","WWIV",fname);
			break;
		case XTRN_GAP:
			sprintf(str,"%-15s %s","GAP",fname);
			break;
		case XTRN_RBBS:
			sprintf(str,"%-15s %s","RBBS/QuickBBS",fname);
			break;
		case XTRN_RBBS1:
			sprintf(str,"%-15s %s","RBBS/QuickBBS",fname);
            break;
		case XTRN_WILDCAT:
			sprintf(str,"%-15s %s","Wildcat",fname);
			break;
		case XTRN_PCBOARD:
			sprintf(str,"%-15s %s","PCBoard",fname);
			break;
		case XTRN_SPITFIRE:
			sprintf(str,"%-15s %s","SpitFire",fname);
			break;
		case XTRN_UTI:
			sprintf(str,"%-15s %s","MegaMail",fname);
			break;
		case XTRN_SR:
			sprintf(str,"%-15s %s","Solar Realms",fname);
			break;
		case XTRN_TRIBBS:
			sprintf(str,"%-15s %s","TriBBS",fname);
			break;
        case XTRN_DOOR32:
            sprintf(str,"%-15s %s","Mystic",fname);
            break;
		default:
			strcpy(str,"None");
			break; 
	}
	return(str);
}

void xprogs_cfg()
{
	static int xprogs_dflt;
	int 	i;

while(1) {
	i=0;
	strcpy(opt[i++],"Fixed Events");
	strcpy(opt[i++],"Timed Events");
	strcpy(opt[i++],"Native Program List");
	strcpy(opt[i++],"External Editors");
	strcpy(opt[i++],"Global Hot Key Events");
	strcpy(opt[i++],"Online Programs (Doors)");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
Online External Programs:

From this menu, you can configure external events, external editors, or
online external programs (doors).
*/
	switch(uifc.list(WIN_ORG|WIN_CHE|WIN_ACT,0,0,0,&xprogs_dflt,0
		,"External Programs",opt)) {
		case -1:
			i=save_changes(WIN_MID);
            if(i==-1)
                break;
			if(!i) {
                write_xtrn_cfg(&cfg,backup_level);
				write_main_cfg(&cfg,backup_level);
                refresh_cfg(&cfg);
            }
			return;
		case 0:
			fevents_cfg();
			break;
		case 1:
			tevents_cfg();
			break;
		case 2:
			natvpgm_cfg();
			break;
		case 3:
			xedit_cfg();
			break;
        case 4:
            hotkey_cfg();
            break;
		case 5:
			xtrnsec_cfg();
			break; } }
}

void fevents_cfg()
{
	static int event_dflt;
	int i;

while(1) {
	i=0;
	sprintf(opt[i++],"%-32.32s%.40s","Logon Event",cfg.sys_logon);
	sprintf(opt[i++],"%-32.32s%.40s","Logout Event",cfg.sys_logout);
	sprintf(opt[i++],"%-32.32s%.40s","Daily Event",cfg.sys_daily);
	opt[i][0]=0;
	uifc.savnum=0;
	SETHELP(WHERE);
/*
External Events:

From this menu, you can configure the logon and logout events, and the
system daily event.
*/
	switch(uifc.list(WIN_ACT|WIN_SAV|WIN_CHE|WIN_BOT|WIN_RHT,0,0,60,&event_dflt,0
		,"Fixed Events",opt)) {
		case -1:
			return;
		case 0:
			SETHELP(WHERE);
/*
Logon Event:

This is the command line for a program that will execute during the
logon sequence of every user. The program cannot have user interaction.
The program will be executed after the LOGON message is displayed and
before the logon user list is displayed. If you wish to place a program
in the logon sequence of users that includes interaction or requires
account information, you probably want to use an online external
program configured to run as a logon event.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Logon Event"
				,cfg.sys_logon,50,K_EDIT);
			break;
		case 1:
			SETHELP(WHERE);
/*
Logout Event:

This is the command line for a program that will execute during the
logout sequence of every user. This program cannot have user
interaction because it is executed after carrier is dropped. If you
wish to have a program execute before carrier is dropped, you probably
want to use an Online External Program configured to run as a logoff
event.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Logout Event"
				,cfg.sys_logout,50,K_EDIT);
			break;
		case 2:
			SETHELP(WHERE);
/*
Daily Event:

This is the command line for a program that will run after the first
user that logs on after midnight, logs off (regardless of what node).
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Daily Event"
				,cfg.sys_daily,50,K_EDIT);

			break; } }
}

void tevents_cfg()
{
	static int dflt,dfltopt,bar;
	char str[81],done=0,*p;
	int j,k;
	uint i;
	static event_t savevent;

while(1) {
	for(i=0;i<cfg.total_events && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-8.8s      %.50s",cfg.event[i]->code,cfg.event[i]->cmd);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_RHT;
	uifc.savnum=0;
	if(cfg.total_events)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_events<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savevent.code[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Timed Events:

This is a list of the configured timed external events.

To add an event hit  INS .

To delete an event, select it and hit  DEL .

To configure an event, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&dflt,&bar,"Timed Events",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i=cfg.total_events;
		SETHELP(WHERE);
/*
Timed Event Internal Code:

This is the internal code for the timed event.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Event Internal Code",str,8
			,K_UPPER)<1)
            continue;
		if((cfg.event=(event_t **)REALLOC(cfg.event
			,sizeof(event_t *)*(cfg.total_events+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_events+1);
			cfg.total_events=0;
			bail(1);
            continue; }
		if((cfg.event[i]=(event_t *)MALLOC(sizeof(event_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(event_t));
			continue; }
		memset((event_t *)cfg.event[i],0,sizeof(event_t));
		strcpy(cfg.event[i]->code,str);
		cfg.event[i]->node=1;
		cfg.event[i]->days=0xff;
		cfg.total_events++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.event[i]);
		cfg.total_events--;
		for(j=i;j<cfg.total_events;j++)
			cfg.event[j]=cfg.event[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savevent=*cfg.event[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.event[i]=savevent;
		uifc.changes=1;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-32.32s%s","Internal Code",cfg.event[i]->code);
		sprintf(opt[k++],"%-32.32s%.40s","Start-up Directory",cfg.event[i]->dir);
		sprintf(opt[k++],"%-32.32s%.40s","Command Line",cfg.event[i]->cmd);
		sprintf(opt[k++],"%-32.32s%u","Execution Node",cfg.event[i]->node);
		sprintf(opt[k++],"%-32.32s%s","Execution Days of Month"
			,mdaystr(cfg.event[i]->mdays));
		sprintf(opt[k++],"%-32.32s%s","Execution Days of Week",daystr(cfg.event[i]->days));
        if(cfg.event[i]->freq) {
            sprintf(str,"%u times a day",1440/cfg.event[i]->freq);
            sprintf(opt[k++],"%-32.32s%s","Execution Frequency",str);
        } else {
            sprintf(str,"%2.2u:%2.2u"
                ,cfg.event[i]->time/60,cfg.event[i]->time%60);
            sprintf(opt[k++],"%-32.32s%s","Execution Time",str);
        }
		sprintf(opt[k++],"%-32.32s%s","Requires Exclusive Execution"
			,cfg.event[i]->misc&EVENT_EXCL ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Force Users Off-line For Event"
			,cfg.event[i]->misc&EVENT_FORCE ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Native (32-bit) Executable"
			,cfg.event[i]->misc&EX_NATIVE ? "Yes" : "No");
		sprintf(opt[k++],"%-32.32s%s","Use Shell to Execute"
			,cfg.event[i]->misc&XTRN_SH ? "Yes" : "No");
		sprintf(opt[k++],"%-32.32s%s","Background Execution"
			,cfg.event[i]->misc&EX_BG ? "Yes" : "No");

		opt[k][0]=0;
		SETHELP(WHERE);
/*
Timed Event:

This is the configuration menu for a timed event. An event is an
external program that performs some type of automated function on the
system. Use this menu to configure how and when this event will be
executed.

If you need the BBS to swap out of memory for this event (to make more
available memory), add the program name (first word of the command line)
to Global Swap List from the External Programs menu.
*/
		uifc.savnum=1;
		sprintf(str,"%s Timed Event",cfg.event[i]->code);
		switch(uifc.list(WIN_SAV|WIN_ACT|WIN_L2R|WIN_BOT,0,0,70,&dfltopt,0
			,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				strcpy(str,cfg.event[i]->code);
				SETHELP(WHERE);
/*
Timed Event Internal Code:

Every timed event must have its own unique internal code for Synchronet
to reference it by. It is helpful if this code is an abreviation of the
command line.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(cfg.event[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
					uifc.helpbuf=0; }
                break;
			case 1:
				SETHELP(WHERE);
/*
Timed Event Start-up Directory:

This is the DOS drive/directory where the event program is located.
If a path is specified here, it will be made the current directory
before the event's command line is executed. This eliminates the need
for batch files that just change the current drive and directory before
executing the event.

If this option is not used, the current NODE's directory will be the
current DOS drive/directory before the command line is executed.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Directory"
					,cfg.event[i]->dir,50,K_EDIT);
                break;
			case 2:
				SETHELP(WHERE);
/*
Timed Event Command Line:

This is the command line to execute upon this timed event.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Command"
					,cfg.event[i]->cmd,50,K_EDIT);
				break;
			case 3:
				SETHELP(WHERE);
/*
Timed Event Node:

This is the node number to execute the timed event.
*/
				sprintf(str,"%u",cfg.event[i]->node);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Node Number"
					,str,3,K_EDIT|K_NUMBER);
				cfg.event[i]->node=atoi(str);
				break;
			case 4:
				SETHELP(WHERE);
/*
`Days of Month to Execute Event:`

Specifies the days of the month (`1-31`, separated by spaces) on which 
to execute this event, or `Any` to execute event on any and all days of
the month.
*/
				SAFECOPY(str,mdaystr(cfg.event[i]->mdays));
				uifc.input(WIN_MID|WIN_SAV,0,0,"Days of Month to Execute Event (or Any)"
					,str,16,K_EDIT);
				cfg.event[i]->mdays=0;
				for(p=str;*p;p++) {
					if(!isdigit(*p))
						continue;
					cfg.event[i]->mdays|=(1<<atoi(p));
					while(*p && isdigit(*p))
						p++;
				}
				break;
			case 5:
				j=0;
				while(1) {
					for(k=0;k<7;k++)
						sprintf(opt[k],"%-30s %3s"
							,wday[k],(cfg.event[i]->days&(1<<k)) ? "Yes":"No");
					strcpy(opt[k++],"All");
					strcpy(opt[k++],"None");
					opt[k][0]=0;
					uifc.savnum=2;
					SETHELP(WHERE);
/*
Days of Week to Execute Event:

These are the days of the week that this event will be executed.
*/
					k=uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,0,&j,0
						,"Days of Week to Execute Event",opt);
					if(k==-1)
						break;
					if(k==7)
						cfg.event[i]->days=0xff;
					else if(k==8)
						cfg.event[i]->days=0;
					else
						cfg.event[i]->days^=(1<<k);
					uifc.changes=1; 
				}
				break;
			case 6:
                if(cfg.event[i]->freq==0)
                    k=0;
                else
                    k=1;
                strcpy(opt[0],"Yes");
                strcpy(opt[1],"No");
                opt[2][0]=0;
                uifc.savnum=2;
                SETHELP(WHERE);
/*
Execute Event at a Specific Time:

If you want the system execute this event at a specific time, set
this option to Yes. If you want the system to execute this event more
than once a day at predetermined intervals, set this option to No.
*/
                k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
                    ,"Execute Event at a Specific Time",opt);
                if(k==0) {
                    sprintf(str,"%2.2u:%2.2u",cfg.event[i]->time/60
                        ,cfg.event[i]->time%60);
                    SETHELP(WHERE);
/*
Time to Execute Event:

This is the time (in 24 hour HH:MM format) to execute the event.
*/
                    if(uifc.input(WIN_MID|WIN_SAV,0,0
                        ,"Time to Execute Event (HH:MM)"
                        ,str,5,K_UPPER|K_EDIT)>0) {
                        cfg.event[i]->freq=0;
                        cfg.event[i]->time=atoi(str)*60;
                        if((p=strchr(str,':'))!=NULL)
                            cfg.event[i]->time+=atoi(p+1); } }
                else if(k==1) {
                    sprintf(str,"%u"
                        ,cfg.event[i]->freq && cfg.event[i]->freq<=1440
                            ? 1440/cfg.event[i]->freq : 0);
                    SETHELP(WHERE);
/*
Number of Executions Per Day:

This is the maximum number of times the system will execute this event
per day.
*/
                    if(uifc.input(WIN_MID|WIN_SAV,0,0
                        ,"Number of Executions Per Day"
                        ,str,4,K_NUMBER|K_EDIT)>0) {
                        cfg.event[i]->time=0;
                        k=atoi(str);
                        if(k && k<=1440)
                            cfg.event[i]->freq=1440/k;
                        else
                            cfg.event[i]->freq=0;
                        }
                    }
                break;
			case 7:
				k=cfg.event[i]->misc&EVENT_EXCL ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Exclusive Event Execution:

If this event must be run exclusively (all nodes inactive), set this
option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Exclusive Execution"
					,opt);
				if(!k && !(cfg.event[i]->misc&EVENT_EXCL)) {
					cfg.event[i]->misc|=EVENT_EXCL;
					uifc.changes=1; }
				else if(k==1 && cfg.event[i]->misc&EVENT_EXCL) {
					cfg.event[i]->misc&=~EVENT_EXCL;
					uifc.changes=1; }
                break;
			case 8:
				k=cfg.event[i]->misc&EVENT_FORCE ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Force Users Off-line for Event:

If you want to have your users' on-line time reduced so the event can
execute precisely on time, set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Force Users Off-line for Event",opt);
				if(!k && !(cfg.event[i]->misc&EVENT_FORCE)) {
					cfg.event[i]->misc|=EVENT_FORCE;
					uifc.changes=1; }
				else if(k==1 && cfg.event[i]->misc&EVENT_FORCE) {
					cfg.event[i]->misc&=~EVENT_FORCE;
                    uifc.changes=1; }
                break;

			case 9:
				k=cfg.event[i]->misc&EX_NATIVE ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Native (32-bit) Executable:

If this event program is a native 32-bit executable,
set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Native (32-bit)",opt);
				if(!k && !(cfg.event[i]->misc&EX_NATIVE)) {
					cfg.event[i]->misc|=EX_NATIVE;
					uifc.changes=TRUE;
                }
				else if(k==1 && cfg.event[i]->misc&EX_NATIVE) {
					cfg.event[i]->misc&=~EX_NATIVE;
					uifc.changes=TRUE;
                }
                break;

			case 10:
				k=cfg.event[i]->misc&XTRN_SH ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
`Use Shell to Execute Command:`

If this command-line requires the system command shell to execute, (Unix 
shell script or DOS batch file), set this option to ~Yes~.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Use Shell",opt);
				if(!k && !(cfg.event[i]->misc&XTRN_SH)) {
					cfg.event[i]->misc|=XTRN_SH;
					uifc.changes=TRUE;
                }
				else if(k==1 && cfg.event[i]->misc&XTRN_SH) {
					cfg.event[i]->misc&=~XTRN_SH;
					uifc.changes=TRUE;
                }
                break;

			case 11:
				k=cfg.event[i]->misc&EX_BG ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Execute Event in Background (Asynchronously):

If you would like this event to run simultaneously with other events,
set this option to Yes. Exclusive events will not run in the background.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Background (Asynchronous) Execution",opt);
				if(!k && !(cfg.event[i]->misc&EX_BG)) {
					cfg.event[i]->misc|=EX_BG;
					uifc.changes=TRUE;
                }
				else if(k==1 && cfg.event[i]->misc&EX_BG) {
					cfg.event[i]->misc&=~EX_BG;
                    uifc.changes=TRUE;
                }
                break;

				} } }
}


void xtrn_cfg(uint section)
{
	static int ext_dflt,ext_bar,opt_dflt,time_dflt;
	char str[128],code[9],done=0,*p;
	int j,k;
	uint i,n,xtrnnum[MAX_OPTS+1];
	static xtrn_t savxtrn;

while(1) {
	for(i=0,j=0;i<cfg.total_xtrns && j<MAX_OPTS;i++)
		if(cfg.xtrn[i]->sec==section) {
			sprintf(opt[j],"%-25s",cfg.xtrn[i]->name);
			xtrnnum[j++]=i; }
	xtrnnum[j]=cfg.total_xtrns;
	opt[j][0]=0;
	uifc.savnum=2;
	i=WIN_ACT|WIN_CHE|WIN_SAV|WIN_RHT;
	if(j)
		i|=WIN_DEL|WIN_GET;
	if(cfg.total_xtrns<MAX_OPTS)
		i|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savxtrn.name[0])
		i|=WIN_PUT;
	SETHELP(WHERE);
/*
Online External Programs:

This is a list of the configured online external programs (doors).

To add a program, select the desired location with the arrow keys and
hit  INS .

To delete a program, select it with the arrow keys and hit  DEL .

To configure a program, select it with the arrow keys and hit  ENTER .
*/
	sprintf(str,"%s Online Programs",cfg.xtrnsec[section]->name);
	i=uifc.list(i,0,0,45,&ext_dflt,&ext_bar,str,opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Online Program Name:

This is the name or description of the online program (door).
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Online Program Name",str,25
			,0)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Online Program Internal Code:

Every online program must have its own unique code for Synchronet to
refer to it internally. This code is usually an abreviation of the
online program name.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.xtrn=(xtrn_t **)REALLOC(cfg.xtrn,sizeof(xtrn_t *)*(cfg.total_xtrns+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_xtrns+1);
			cfg.total_xtrns=0;
			bail(1);
            continue; }
		if(j)
			for(n=cfg.total_xtrns;n>xtrnnum[i];n--)
				cfg.xtrn[n]=cfg.xtrn[n-1];
		if((cfg.xtrn[xtrnnum[i]]=(xtrn_t *)MALLOC(sizeof(xtrn_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xtrn_t));
			continue; }
		memset((xtrn_t *)cfg.xtrn[xtrnnum[i]],0,sizeof(xtrn_t));
		strcpy(cfg.xtrn[xtrnnum[i]]->name,str);
		strcpy(cfg.xtrn[xtrnnum[i]]->code,code);
		cfg.xtrn[xtrnnum[i]]->sec=section;
		cfg.total_xtrns++;
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.xtrn[xtrnnum[i]]);
		cfg.total_xtrns--;
		for(j=xtrnnum[i];j<cfg.total_xtrns;j++)
			cfg.xtrn[j]=cfg.xtrn[j+1];
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxtrn=*cfg.xtrn[xtrnnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.xtrn[xtrnnum[i]]=savxtrn;
		cfg.xtrn[xtrnnum[i]]->sec=section;
		uifc.changes=TRUE;
        continue; }
	done=0;
	i=xtrnnum[i];
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",cfg.xtrn[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.xtrn[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Start-up Directory",cfg.xtrn[i]->path);
		sprintf(opt[k++],"%-27.27s%.40s","Command Line",cfg.xtrn[i]->cmd);
		sprintf(opt[k++],"%-27.27s%.40s","Clean-up Command Line",cfg.xtrn[i]->clean);
		if(cfg.xtrn[i]->cost)
			sprintf(str,"%lu credits",cfg.xtrn[i]->cost);
		else
			strcpy(str,"None");
		sprintf(opt[k++],"%-27.27s%s","Execution Cost",str);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",cfg.xtrn[i]->arstr);
		sprintf(opt[k++],"%-27.27s%.40s","Execution Requirements"
			,cfg.xtrn[i]->run_arstr);
		sprintf(opt[k++],"%-27.27s%s","Multiple Concurrent Users"
			,cfg.xtrn[i]->misc&MULTIUSER ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s%s","Intercept Standard I/O"
			,cfg.xtrn[i]->misc&IO_INTS ? "Yes" : "No"
			,cfg.xtrn[i]->misc&WWIVCOLOR ? ", WWIV" : nulstr);
		sprintf(opt[k++],"%-27.27s%s","Native (32-bit) Executable"
			,cfg.xtrn[i]->misc&XTRN_NATIVE ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Use Shell to Execute"
			,cfg.xtrn[i]->misc&XTRN_SH ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Modify User Data"
            ,cfg.xtrn[i]->misc&MODUSERDAT ? "Yes" : "No");
		switch(cfg.xtrn[i]->event) {
			case EVENT_LOGON:
				strcpy(str,"Logon");
				break;
			case EVENT_LOGOFF:
				strcpy(str,"Logoff");
				break;
			case EVENT_NEWUSER:
				strcpy(str,"New User");
				break;
			case EVENT_BIRTHDAY:
				strcpy(str,"Birthday");
				break;
			default:
				strcpy(str,"No");
				break; }
		if(cfg.xtrn[i]->misc&EVENTONLY && cfg.xtrn[i]->event)
			strcat(str,", Only");
		sprintf(opt[k++],"%-27.27s%s","Execute on Event",str);
		sprintf(opt[k++],"%-23.23s%-4s%s","BBS Drop File Type"
			,cfg.xtrn[i]->misc&REALNAME ? "(R)":nulstr
			,dropfile(cfg.xtrn[i]->type,cfg.xtrn[i]->misc));
		sprintf(opt[k++],"%-27.27s%s","Place Drop File In"
			,cfg.xtrn[i]->misc&STARTUPDIR ? "Start-Up Directory":"Node Directory");
		sprintf(opt[k++],"Time Options...");
		opt[k][0]=0;
		uifc.savnum=3;
		SETHELP(WHERE);
/*
Online Program Configuration:

This menu is for configuring the selected online program.
*/
		switch(uifc.list(WIN_SAV|WIN_ACT|WIN_MID,0,0,60,&opt_dflt,0,cfg.xtrn[i]->name
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Online Program Name:

This is the name or description of the online program (door).
*/
				strcpy(str,cfg.xtrn[i]->name);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Online Program Name"
					,cfg.xtrn[i]->name,25,K_EDIT))
					strcpy(cfg.xtrn[i]->name,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Online Program Internal Code:

Every online program must have its own unique code for Synchronet to
refer to it internally. This code is usually an abreviation of the
online program name.
*/
				strcpy(str,cfg.xtrn[i]->code);
				uifc.input(WIN_MID|WIN_SAV,0,10,"Internal Code"
					,str,8,K_UPPER|K_EDIT);
				if(code_ok(str))
					strcpy(cfg.xtrn[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
                    uifc.helpbuf=0; }
                break;
			case 2:
                SETHELP(WHERE);
/*
Online Program Start-up Directory:

This is the DOS drive/directory where the online program is located.
If a path is specified here, it will be made the current directory
before the program's command line is executed. This eliminates the need
for batch files that just change the current drive and directory before
executing the program.

If this option is not used, the current NODE's directory will be the
current DOS drive/directory before the command line is executed.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Directory"
					,cfg.xtrn[i]->path,50,K_EDIT);
                break;
			case 3:
				SETHELP(WHERE);
/*
Online Program Command Line:

This is the command line to execute to run the online program.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Command"
					,cfg.xtrn[i]->cmd,50,K_EDIT);
				break;
			case 4:
				SETHELP(WHERE);
/*
Online Program Clean-up Command:

This is the command line to execute after the main command line. This
option is usually only used for multiuser online programs.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Clean-up"
					,cfg.xtrn[i]->clean,50,K_EDIT);
                break;
			case 5:
                ultoa(cfg.xtrn[i]->cost,str,10);
                SETHELP(WHERE);
/*
Online Program Cost to Run:

If you want users to be charged credits to run this online program,
set this value to the number of credits to charge. If you want this
online program to be free, set this value to 0.
*/
                uifc.input(WIN_MID|WIN_SAV,0,0,"Cost to Run (in Credits)"
                    ,str,10,K_EDIT|K_NUMBER);
                cfg.xtrn[i]->cost=atol(str);
                break;
			case 6:
				uifc.savnum=4;
				sprintf(str,"%s Access",cfg.xtrn[i]->name);
				getar(str,cfg.xtrn[i]->arstr);
                break;
			case 7:
				uifc.savnum=4;
				sprintf(str,"%s Execution",cfg.xtrn[i]->name);
				getar(str,cfg.xtrn[i]->run_arstr);
                break;
			case 8:
				k=cfg.xtrn[i]->misc&MULTIUSER ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Supports Multiple Users:

If this online program supports multiple simultaneous users (nodes),
set this option to Yes.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Supports Multiple Users"
					,opt);
				if(!k && !(cfg.xtrn[i]->misc&MULTIUSER)) {
					cfg.xtrn[i]->misc|=MULTIUSER;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xtrn[i]->misc&MULTIUSER) {
					cfg.xtrn[i]->misc&=~MULTIUSER;
					uifc.changes=TRUE; }
                break;
			case 9:
				k=cfg.xtrn[i]->misc&IO_INTS ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Intercept Standard I/O:

If this online program uses a FOSSIL driver or SOCKET communications,
set this option to No.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept Standard I/O"
					,opt);
				if(!k && !(cfg.xtrn[i]->misc&IO_INTS)) {
					cfg.xtrn[i]->misc|=IO_INTS;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xtrn[i]->misc&IO_INTS) {
					cfg.xtrn[i]->misc&=~(IO_INTS|WWIVCOLOR);
					uifc.changes=TRUE; }
                else if(k==-1)
                    break;
				if(!(cfg.xtrn[i]->misc&IO_INTS))
					break;
				k=cfg.xtrn[i]->misc&WWIVCOLOR ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Program Uses WWIV Color Codes:

If this program was written for use exclusively under WWIV, set this
option to Yes.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Program Uses WWIV Color Codes"
					,opt);
				if(!k && !(cfg.xtrn[i]->misc&WWIVCOLOR)) {
					cfg.xtrn[i]->misc|=WWIVCOLOR;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xtrn[i]->misc&WWIVCOLOR) {
					cfg.xtrn[i]->misc&=~WWIVCOLOR;
                    uifc.changes=TRUE; }
                break;
			case 10:
				k=cfg.xtrn[i]->misc&XTRN_NATIVE ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Native (32-bit) Executable:

If this online program is a native 32-bit executable,
set this option to Yes.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Native (32-bit)",opt);
				if(!k && !(cfg.xtrn[i]->misc&XTRN_NATIVE)) {
					cfg.xtrn[i]->misc|=XTRN_NATIVE;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xtrn[i]->misc&XTRN_NATIVE) {
					cfg.xtrn[i]->misc&=~XTRN_NATIVE;
					uifc.changes=TRUE; }
				break;
			case 11:
				k=cfg.xtrn[i]->misc&XTRN_SH ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
`Use Shell to Execute Command:`

If this command-line requires the system command shell to execute, (Unix 
shell script or DOS batch file), set this option to ~Yes~.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Use Shell",opt);
				if(!k && !(cfg.xtrn[i]->misc&XTRN_SH)) {
					cfg.xtrn[i]->misc|=XTRN_SH;
					uifc.changes=TRUE; 
				}
				else if(k==1 && cfg.xtrn[i]->misc&XTRN_SH) {
					cfg.xtrn[i]->misc&=~XTRN_SH;
					uifc.changes=TRUE; 
				}
				break;
			case 12:
				k=cfg.xtrn[i]->misc&MODUSERDAT ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Program Can Modify User Data:

If this online programs recognizes the Synchronet MODUSER.DAT format
or the RBBS/QuickBBS EXITINFO.BBS format and you want it to be able to
modify the data of users who run the program, set this option to Yes.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Program Can Modify User Data",opt);
				if(!k && !(cfg.xtrn[i]->misc&MODUSERDAT)) {
					cfg.xtrn[i]->misc|=MODUSERDAT;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xtrn[i]->misc&MODUSERDAT) {
					cfg.xtrn[i]->misc&=~MODUSERDAT;
					uifc.changes=TRUE; }
                break;
			case 13:
				k=0;
				strcpy(opt[k++],"No");
				strcpy(opt[k++],"Logon");
				strcpy(opt[k++],"Logoff");
				strcpy(opt[k++],"New User");
				strcpy(opt[k++],"Birthday");
				opt[k][0]=0;
				switch(cfg.xtrn[i]->event) {
					default:
						k=0;
						break;
					case EVENT_LOGON:
						k=1;
						break;
					case EVENT_LOGOFF:
						k=2;
						break;
					case EVENT_NEWUSER:
						k=3;
						break;
					case EVENT_BIRTHDAY:
						k=4;
						break; }
				SETHELP(WHERE);
/*
Execute Online Program on Event:

If you would like this online program to automatically execute on a
specific user event, select the event. Otherwise, select No.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Execute on Event",opt);
				if(k==-1)
					break;
				if(cfg.xtrn[i]->event!=k) {
					cfg.xtrn[i]->event=k;
					uifc.changes=TRUE; }
				if(!cfg.xtrn[i]->event)
					break;
				k=cfg.xtrn[i]->misc&EVENTONLY ? 0:1;
                strcpy(opt[0],"Yes");
                strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Execute Online Program as Event Only:

If you would like this online program to execute as an event only
(not available to users on the online program menu), set this option
to Yes.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k
					,0,"Execute as Event Only"
                    ,opt);
                if(!k && !(cfg.xtrn[i]->misc&EVENTONLY)) {
                    cfg.xtrn[i]->misc|=EVENTONLY;
                    uifc.changes=TRUE; }
                else if(k==1 && cfg.xtrn[i]->misc&EVENTONLY) {
                    cfg.xtrn[i]->misc&=~EVENTONLY;
                    uifc.changes=TRUE; }
                break;
			case 14:
				k=0;
				strcpy(opt[k++],"None");
				sprintf(opt[k++],"%-15s %s","Synchronet","XTRN.DAT");
				sprintf(opt[k++],"%-15s %s","WWIV","CHAIN.TXT");
				sprintf(opt[k++],"%-15s %s","GAP","DOOR.SYS");
				sprintf(opt[k++],"%-15s %s","RBBS/QuickBBS","DORINFO#.DEF");
				sprintf(opt[k++],"%-15s %s","Wildcat","CALLINFO.BBS");
				sprintf(opt[k++],"%-15s %s","PCBoard","PCBOARD.SYS");
				sprintf(opt[k++],"%-15s %s","SpitFire","SFDOORS.DAT");
				sprintf(opt[k++],"%-15s %s","MegaMail","UTIDOOR.TXT");
				sprintf(opt[k++],"%-15s %s","Solar Realms","DOORFILE.SR");
				sprintf(opt[k++],"%-15s %s","RBBS/QuickBBS","DORINFO1.DEF");
				sprintf(opt[k++],"%-15s %s","TriBBS","TRIBBS.SYS");
				sprintf(opt[k++],"%-15s %s","Mystic","DOOR32.SYS");
				opt[k][0]=0;
				k=cfg.xtrn[i]->type;
				SETHELP(WHERE);
/*
Online Program BBS Drop File Type:

If this online program requires a specific BBS data (drop) file
format, select the file format from the list.
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"BBS Drop File Type",opt);
				if(k==-1)
					break;
				if(cfg.xtrn[i]->type!=k) {
					cfg.xtrn[i]->type=k;
                    if(cfg.xtrn[i]->type==XTRN_DOOR32)
                        cfg.xtrn[i]->misc|=XTRN_NATIVE;
					uifc.changes=TRUE; }
				if(cfg.xtrn[i]->type && cfg.uq&UQ_ALIASES) {
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					k=cfg.xtrn[i]->misc&REALNAME ? 0:1;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Use Real Names",opt);
					if(k==0 && !(cfg.xtrn[i]->misc&REALNAME)) {
						cfg.xtrn[i]->misc|=REALNAME;
						uifc.changes=TRUE; }
					else if(k==1 && cfg.xtrn[i]->misc&REALNAME) {
						cfg.xtrn[i]->misc&=~REALNAME;
						uifc.changes=TRUE; 
					} 
				}
				if(cfg.xtrn[i]->type) {
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					k=cfg.xtrn[i]->misc&XTRN_LWRCASE ? 0:1;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Lowercase Filename",opt);
					if(k==0 && !(cfg.xtrn[i]->misc&XTRN_LWRCASE)) {
						cfg.xtrn[i]->misc|=XTRN_LWRCASE;
						uifc.changes=TRUE; 
					}
					else if(k==1 && cfg.xtrn[i]->misc&XTRN_LWRCASE) {
						cfg.xtrn[i]->misc&=~XTRN_LWRCASE;
						uifc.changes=TRUE; 
					} 
				}
				break;
			case 15:
				k=0;
				strcpy(opt[0],"Node Directory");
				strcpy(opt[1],"Start-up Directory");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Directory for Drop File:

You can have the data file created in the current Node Directory or the
Start-up Directory (if one is specified).
*/
				uifc.savnum=4;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Create Drop File In"
                    ,opt);
				if(!k && cfg.xtrn[i]->misc&STARTUPDIR) {
					cfg.xtrn[i]->misc&=~STARTUPDIR;
                    uifc.changes=TRUE; }
				else if(k==1 && !(cfg.xtrn[i]->misc&STARTUPDIR)) {
					cfg.xtrn[i]->misc|=STARTUPDIR;
                    uifc.changes=TRUE; }
				break;
			case 16:
				while(1) {
					k=0;
					if(cfg.xtrn[i]->textra)
						sprintf(str,"%u minutes",cfg.xtrn[i]->textra);
					else
						strcpy(str,"None");
					sprintf(opt[k++],"%-25.25s%s","Extra Time",str);
					if(cfg.xtrn[i]->maxtime)
						sprintf(str,"%u minutes",cfg.xtrn[i]->maxtime);
					else
						strcpy(str,"None");
					sprintf(opt[k++],"%-25.25s%s","Maximum Time",str);
					sprintf(opt[k++],"%-25.25s%s","Suspended (Free) Time"
						,cfg.xtrn[i]->misc&FREETIME ? "Yes" : "No");
					sprintf(opt[k++],"%-25.25s%s","Monitor Time Left"
						,cfg.xtrn[i]->misc&XTRN_CHKTIME ? "Yes" : "No");
					opt[k][0]=0;
					SETHELP(WHERE);
/*
Online Program Time Options:

This sub-menu allows you to define specific preferences regarding the
time users spend running this program.
*/
					uifc.savnum=4;
					k=uifc.list(WIN_SAV|WIN_ACT|WIN_RHT|WIN_BOT,0,0,40
						,&time_dflt,0
						,"Online Program Time Options",opt);
					if(k==-1)
						break;
					uifc.savnum=5;
					switch(k) {
						case 0:
							ultoa(cfg.xtrn[i]->textra,str,10);
							SETHELP(WHERE);
/*
Extra Time to Give User in Program:

If you want to give users extra time while in this online program,
set this value to the number of minutes to add to their current time
left online.
*/
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Extra Time to Give User (in minutes)"
								,str,2,K_EDIT|K_NUMBER);
							cfg.xtrn[i]->textra=atoi(str);
							break;
						case 1:
							ultoa(cfg.xtrn[i]->maxtime,str,10);
							SETHELP(WHERE);
/*
Maximum Time Allowed in Program:

If this program supports a drop file that contains the number of minutes
left online for the current user, this option allows the sysop to set
the maximum number of minutes that will be allowed in the drop file.

Setting this option to 0, disables this feature.
*/
							uifc.input(WIN_MID|WIN_SAV,0,0
								,"Maximum Time (in minutes, 0=disabled)"
								,str,2,K_EDIT|K_NUMBER);
							cfg.xtrn[i]->maxtime=atoi(str);
							break;
						case 2:
							k=cfg.xtrn[i]->misc&FREETIME ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Suspended (Free) Time:

If you want the user's time online to be suspended while running this
online program (e.g. Free Time), set this option to Yes.
*/
							k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Suspended (Free) Time",opt);
							if(!k && !(cfg.xtrn[i]->misc&FREETIME)) {
								cfg.xtrn[i]->misc|=FREETIME;
								uifc.changes=TRUE; }
							else if(k==1 && cfg.xtrn[i]->misc&FREETIME) {
								cfg.xtrn[i]->misc&=~FREETIME;
								uifc.changes=TRUE; }
							break; 
						case 3:
							k=cfg.xtrn[i]->misc&XTRN_CHKTIME ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Monitor Time Left:

If you want Synchronet to monitor the user's time left onlnie while this
program runs (and disconnect the user if their time runs out), set this
option to Yes.
*/
							k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Monitor Time Left",opt);
							if(!k && !(cfg.xtrn[i]->misc&XTRN_CHKTIME)) {
								cfg.xtrn[i]->misc|=XTRN_CHKTIME;
								uifc.changes=TRUE; }
							else if(k==1 && cfg.xtrn[i]->misc&XTRN_CHKTIME) {
								cfg.xtrn[i]->misc&=~XTRN_CHKTIME;
								uifc.changes=TRUE; }
							break;
						} 
					}
					break;

				} } }
}

void xedit_cfg()
{
	static int dflt,dfltopt,bar;
	char str[81],code[81],done=0,*p;
	int j,k;
	uint i;
	static xedit_t savxedit;

while(1) {
	for(i=0;i<cfg.total_xedits && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-8.8s    %.40s",cfg.xedit[i]->code,cfg.xedit[i]->rcmd);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_RHT;
	uifc.savnum=0;
	if(cfg.total_xedits)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_xedits<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savxedit.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
External Editors:

This is a list of the configured external editors.

To add an editor, select the desired location and hit  INS .

To delete an editor, select it and hit  DEL .

To configure an editor, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&dflt,&bar,"External Editors",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
External Editor Name:

This is the name or description of the external editor.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"External Editor Name",str,40
			,0)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
External Editor Internal Code:

This is the internal code for the external editor.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"External Editor Internal Code",code,8
			,K_UPPER|K_EDIT)<1)
            continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }

		if((cfg.xedit=(xedit_t **)REALLOC(cfg.xedit
            ,sizeof(xedit_t *)*(cfg.total_xedits+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_xedits+1);
			cfg.total_xedits=0;
			bail(1);
            continue; }
		if(cfg.total_xedits)
			for(j=cfg.total_xedits;j>i;j--)
				cfg.xedit[j]=cfg.xedit[j-1];
		if((cfg.xedit[i]=(xedit_t *)MALLOC(sizeof(xedit_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xedit_t));
			continue; }
		memset((xedit_t *)cfg.xedit[i],0,sizeof(xedit_t));
		strcpy(cfg.xedit[i]->name,str);
		strcpy(cfg.xedit[i]->code,code);
		cfg.total_xedits++;
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.xedit[i]);
		cfg.total_xedits--;
		for(j=i;j<cfg.total_xedits;j++)
			cfg.xedit[j]=cfg.xedit[j+1];
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxedit=*cfg.xedit[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.xedit[i]=savxedit;
		uifc.changes=TRUE;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-32.32s%s","Name",cfg.xedit[i]->name);
		sprintf(opt[k++],"%-32.32s%s","Internal Code",cfg.xedit[i]->code);
		sprintf(opt[k++],"%-32.32s%.40s","Remote Command Line",cfg.xedit[i]->rcmd);
		sprintf(opt[k++],"%-32.32s%.40s","Access Requirements",cfg.xedit[i]->arstr);
		sprintf(opt[k++],"%-32.32s%s%s","Intercept Standard I/O"
			,cfg.xedit[i]->misc&IO_INTS ? "Yes":"No"
			,cfg.xedit[i]->misc&WWIVCOLOR ? ", WWIV" : nulstr);
        sprintf(opt[k++],"%-32.32s%s","Native (32-bit) Executable"
			,cfg.xedit[i]->misc&XTRN_NATIVE ? "Yes" : "No");
		sprintf(opt[k++],"%-32.32s%s","Use Shell to Execute"
			,cfg.xedit[i]->misc&XTRN_SH ? "Yes" : "No");
		sprintf(opt[k++],"%-32.32s%s","Quoted Text"
			,cfg.xedit[i]->misc&QUOTEALL ? "All":cfg.xedit[i]->misc&QUOTENONE
				? "None" : "Prompt User");
		sprintf(opt[k++],"%-32.32s%s","QuickBBS Style (MSGTMP)"
			,cfg.xedit[i]->misc&QUICKBBS ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Expand Line Feeds to CRLF"
			,cfg.xedit[i]->misc&EXPANDLF ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Strip FidoNet Kludge Lines"
			,cfg.xedit[i]->misc&STRIPKLUDGE ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","BBS Drop File Type"
			,dropfile(cfg.xedit[i]->type,cfg.xedit[i]->misc));
        opt[k][0]=0;
		SETHELP(WHERE);
/*
External Editor Configuration:

This menu allows you to change the settings for the selected external
message editor. External message editors are very common on BBSs. Some
popular editors include SyncEdit, WWIVedit, FEdit, GEdit, IceEdit,
and many others.
*/

		uifc.savnum=1;
		sprintf(str,"%s Editor",cfg.xedit[i]->name);
		switch(uifc.list(WIN_SAV|WIN_ACT|WIN_L2R|WIN_BOT,0,0,70,&dfltopt,0
			,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
External Editor Name:

This is the name or description of the external editor.
*/
				strcpy(str,cfg.xedit[i]->name);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"External Editor Name"
					,cfg.xedit[i]->name,40,K_EDIT))
					strcpy(cfg.xedit[i]->name,str);
				break;
			case 1:
				strcpy(str,cfg.xedit[i]->code);
				SETHELP(WHERE);
/*
External Editor Internal Code:

Every external editor must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(cfg.xedit[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
					uifc.helpbuf=0; }
                break;
		   case 2:
				SETHELP(WHERE);
/*
External Editor Remote Command Line:

This is the command line to execute when using this editor remotely.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Remote"
					,cfg.xedit[i]->rcmd,50,K_EDIT);
				break;
			case 3:
				uifc.savnum=2;
				sprintf(str,"%s External Editor",cfg.xedit[i]->name);
				getar(str,cfg.xedit[i]->arstr);
				break;
			case 4:
				k=cfg.xedit[i]->misc&IO_INTS ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Intercept Standard I/O:

If this external editor uses a FOSSIL driver or SOCKET communications,
set this option to No.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept Standard I/O"
					,opt);
				if(!k && !(cfg.xedit[i]->misc&IO_INTS)) {
					cfg.xedit[i]->misc|=IO_INTS;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&IO_INTS) {
					cfg.xedit[i]->misc&=~(IO_INTS|WWIVCOLOR);
					uifc.changes=TRUE; }
				if(!(cfg.xedit[i]->misc&IO_INTS))
					break;
				k=cfg.xedit[i]->misc&WWIVCOLOR ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Editor Uses WWIV Color Codes:

If this editor was written for use exclusively under WWIV, set this
option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Editor Uses WWIV Color Codes",opt);
				if(!k && !(cfg.xedit[i]->misc&WWIVCOLOR)) {
					cfg.xedit[i]->misc|=WWIVCOLOR;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&WWIVCOLOR) {
					cfg.xedit[i]->misc&=~WWIVCOLOR;
                    uifc.changes=TRUE; }
                break;
			case 5:
				k=cfg.xedit[i]->misc&XTRN_NATIVE ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Native (32-bit) Executable:

If this editor is a native 32-bit executable, set this option to Yes.
*/
				uifc.savnum=2;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Native (32-bit)",opt);
				if(!k && !(cfg.xedit[i]->misc&XTRN_NATIVE)) {
					cfg.xedit[i]->misc|=XTRN_NATIVE;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&XTRN_NATIVE) {
					cfg.xedit[i]->misc&=~XTRN_NATIVE;
					uifc.changes=TRUE; }
				break;
			case 6:
				k=cfg.xedit[i]->misc&XTRN_SH ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
`Use Shell to Execute Command:`

If this command-line requires the system command shell to execute, (Unix 
shell script or DOS batch file), set this option to ~Yes~.
*/
				uifc.savnum=2;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Use Shell",opt);
				if(!k && !(cfg.xedit[i]->misc&XTRN_SH)) {
					cfg.xedit[i]->misc|=XTRN_SH;
					uifc.changes=TRUE; 
				} else if(k==1 && cfg.xedit[i]->misc&XTRN_SH) {
					cfg.xedit[i]->misc&=~XTRN_SH;
					uifc.changes=TRUE; 
				}
				break;
			case 7:
				k=3;
				strcpy(opt[0],"All");
				strcpy(opt[1],"None");
				strcpy(opt[2],"Prompt User");
				opt[3][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Quoted Text:

If you want all the message text to be automatically entered into the
message edit/quote file (INPUT.MSG or MSGTMP), select All.

If you want the user to be prompted for which lines to quote before
running the editor, select Prompt User.

If you want none of the lines to be automatically quoted, select None.
This option is mainly for use with editors that support the QUOTES.TXT
drop file (like SyncEdit v2.x).
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Quoted Text"
					,opt);
				if(!k && !(cfg.xedit[i]->misc&QUOTEALL)) {
					cfg.xedit[i]->misc|=QUOTEALL;
					cfg.xedit[i]->misc&=~QUOTENONE;
					uifc.changes=TRUE; }
				else if(k==1 && !(cfg.xedit[i]->misc&QUOTENONE)) {
					cfg.xedit[i]->misc|=QUOTENONE;
					cfg.xedit[i]->misc&=~QUOTEALL;
                    uifc.changes=TRUE; }
				else if(k==2 && cfg.xedit[i]->misc&(QUOTENONE|QUOTEALL)) {
					cfg.xedit[i]->misc&=~(QUOTENONE|QUOTEALL);
					uifc.changes=TRUE; }
                break;
			case 8:
				k=cfg.xedit[i]->misc&QUICKBBS ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
QuickBBS Style (MSGTMP):

If this external editor uses the QuickBBS style MSGTMP interface, set
this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"QuickBBS Style (MSGTMP)"
					,opt);
				if(!k && !(cfg.xedit[i]->misc&QUICKBBS)) {
					cfg.xedit[i]->misc|=QUICKBBS;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&QUICKBBS) {
					cfg.xedit[i]->misc&=~QUICKBBS;
                    uifc.changes=TRUE; }
				break;
			case 9:
				k=cfg.xedit[i]->misc&EXPANDLF ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Expand Line Feeds to Carriage Return/Line Feed Pairs:

If this external editor saves new lines as a single line feed character
instead of a carriage return/line feed pair, set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Expand LF to CRLF"
					,opt);
				if(!k && !(cfg.xedit[i]->misc&EXPANDLF)) {
					cfg.xedit[i]->misc|=EXPANDLF;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&EXPANDLF) {
					cfg.xedit[i]->misc&=~EXPANDLF;
                    uifc.changes=TRUE; }
				break;
			case 10:
				k=cfg.xedit[i]->misc&STRIPKLUDGE ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Strip FidoNet Kludge Lines From Messages:

If this external editor adds FidoNet Kludge lines to the message text,
set this option to Yes to strip those lines from the message.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
                	,"Strip FidoNet Kludge Lines"
					,opt);
				if(!k && !(cfg.xedit[i]->misc&STRIPKLUDGE)) {
					cfg.xedit[i]->misc|=STRIPKLUDGE;
					uifc.changes=TRUE; }
				else if(k==1 && cfg.xedit[i]->misc&STRIPKLUDGE) {
					cfg.xedit[i]->misc&=~STRIPKLUDGE;
                    uifc.changes=TRUE; }
				break;
			case 11:
				k=0;
				strcpy(opt[k++],"None");
				sprintf(opt[k++],"%-15s %s","Synchronet","XTRN.DAT");
				sprintf(opt[k++],"%-15s %s","WWIV","CHAIN.TXT");
				sprintf(opt[k++],"%-15s %s","GAP","DOOR.SYS");
				sprintf(opt[k++],"%-15s %s","RBBS/QuickBBS","DORINFO#.DEF");
				sprintf(opt[k++],"%-15s %s","Wildcat","CALLINFO.BBS");
				sprintf(opt[k++],"%-15s %s","PCBoard","PCBOARD.SYS");
				sprintf(opt[k++],"%-15s %s","SpitFire","SFDOORS.DAT");
				sprintf(opt[k++],"%-15s %s","MegaMail","UTIDOOR.TXT");
				sprintf(opt[k++],"%-15s %s","Solar Realms","DOORFILE.SR");
				sprintf(opt[k++],"%-15s %s","RBBS/QuickBBS","DORINFO1.DEF");
				sprintf(opt[k++],"%-15s %s","TriBBS","TRIBBS.SYS");
				sprintf(opt[k++],"%-15s %s","Mystic","DOOR32.SYS");
				opt[k][0]=0;
				k=cfg.xedit[i]->type;
				SETHELP(WHERE);
/*
External Program BBS Drop File Type:

If this external editor requires a specific BBS data (drop) file
format, select the file format from the list.
*/
				uifc.savnum=2;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"BBS Drop File Type",opt);
				if(k==-1)
					break;
				if(cfg.xedit[i]->type!=k) {
					cfg.xedit[i]->type=k;
                    if(cfg.xedit[i]->type==XTRN_DOOR32)
                        cfg.xedit[i]->misc|=XTRN_NATIVE;
					uifc.changes=TRUE; 
				}
				if(cfg.xedit[i]->type) {
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					k=cfg.xedit[i]->misc&XTRN_LWRCASE ? 0:1;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Lowercase Filename",opt);
					if(k==0 && !(cfg.xedit[i]->misc&XTRN_LWRCASE)) {
						cfg.xedit[i]->misc|=XTRN_LWRCASE;
						uifc.changes=TRUE; 
					}
					else if(k==1 && cfg.xedit[i]->misc&XTRN_LWRCASE) {
						cfg.xedit[i]->misc&=~XTRN_LWRCASE;
						uifc.changes=TRUE; 
					} 
				}
				break;

				} } }
}

int natvpgm_cfg()
{
	static int dflt,bar;
	char str[81];
	int j;
	uint i;

while(1) {
	for(i=0;i<MAX_OPTS && i<cfg.total_natvpgms;i++)
		sprintf(opt[i],"%-12s %-12s",cfg.natvpgm[i]->name
			,cfg.natvpgm[i]->misc&OS2_POPEN ? "(port open)":nulstr);
	opt[i][0]=0;
	j=WIN_ACT|WIN_CHE|WIN_L2R|WIN_SAV;
	uifc.savnum=0;
	if(cfg.total_natvpgms)
		j|=WIN_DEL;
	if(cfg.total_natvpgms<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	SETHELP(WHERE);
/*
Native (32-bit) Program List:

This is a list of all native (32-bit) external program (executable file)
names that you may execute under Synchronet. This list is not
used in Synchronet for DOS. Any programs not listed here will be assumed
to be DOS programs and executed accordingly.

Use  INS  and  DELETE  to add and remove native program names.

To change the filename of a program, hit  ENTER .
*/
	i=uifc.list(j,0,0,30,&dflt,&bar,"Native (32-bit) Program List",opt);
	if((signed)i==-1)
		break;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Native (32-bit) Program Name:

This is the executable filename of the native external program.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Native Program Name",str,12
			,0)<1)
            continue;
		if((cfg.natvpgm=(natvpgm_t **)REALLOC(cfg.natvpgm
			,sizeof(natvpgm_t *)*(cfg.total_natvpgms+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_natvpgms+1);
			cfg.total_natvpgms=0;
			bail(1);
            continue; }
		if(cfg.total_natvpgms)
			for(j=cfg.total_natvpgms;j>i;j--)
				cfg.natvpgm[j]=cfg.natvpgm[j-1];
		if((cfg.natvpgm[i]=(natvpgm_t *)MALLOC(sizeof(natvpgm_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(natvpgm_t));
			continue; }
		memset((natvpgm_t *)cfg.natvpgm[i],0,sizeof(natvpgm_t));
		strcpy(cfg.natvpgm[i]->name,str);
		cfg.total_natvpgms++;
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.natvpgm[i]);
		cfg.total_natvpgms--;
		for(j=i;j<cfg.total_natvpgms;j++)
			cfg.natvpgm[j]=cfg.natvpgm[j+1];
		uifc.changes=TRUE;
		continue; }
	SETHELP(WHERE);
/*
Native Program Name:

This is the executable filename of the Native external program.
*/
	strcpy(str,cfg.natvpgm[i]->name);
	if(uifc.input(WIN_MID|WIN_SAV,0,5,"Native (32-bit) Program Name",str,12
		,K_EDIT)>0)
		strcpy(cfg.natvpgm[i]->name,str);
	}
return(0);
}


void xtrnsec_cfg()
{
	static int xtrnsec_dflt,xtrnsec_opt;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static xtrnsec_t savxtrnsec;

while(1) {
	for(i=0;i<cfg.total_xtrnsecs && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.xtrnsec[i]->name);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_BOT;
	uifc.savnum=0;
	if(cfg.total_xtrnsecs)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_xtrnsecs<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savxtrnsec.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Online Program Sections:

This is a list of Online Program Sections configured for your system.

To add an online program section, select the desired location with the
arrow keys and hit  INS .

To delete an online program section, select it and hit  DEL .

To configure an online program section, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&xtrnsec_dflt,0,"Online Program Sections",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Online Program Section Name:

This is the name of this section.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Online Program Section Name",str,40
			,0)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Online Program Section Internal Code:

Every online program section must have its own unique internal code
for Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Online Program Section Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.xtrnsec=(xtrnsec_t **)REALLOC(cfg.xtrnsec
			,sizeof(xtrnsec_t *)*(cfg.total_xtrnsecs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_xtrnsecs+1);
			cfg.total_xtrnsecs=0;
			bail(1);
			continue; }
		if(cfg.total_xtrnsecs) {
			for(j=cfg.total_xtrnsecs;j>i;j--)
				cfg.xtrnsec[j]=cfg.xtrnsec[j-1];
			for(j=0;j<cfg.total_xtrns;j++)
				if(cfg.xtrn[j]->sec>=i)
					cfg.xtrn[j]->sec++; }


		if((cfg.xtrnsec[i]=(xtrnsec_t *)MALLOC(sizeof(xtrnsec_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xtrnsec_t));
			continue; }
		memset((xtrnsec_t *)cfg.xtrnsec[i],0,sizeof(xtrnsec_t));
		strcpy(cfg.xtrnsec[i]->name,str);
		strcpy(cfg.xtrnsec[i]->code,code);
		cfg.total_xtrnsecs++;
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.xtrnsec[i]);
		for(j=0;j<cfg.total_xtrns;) {
			if(cfg.xtrn[j]->sec==i) {	 /* delete xtrns of this group */
				FREE(cfg.xtrn[j]);
				cfg.total_xtrns--;
				k=j;
				while(k<cfg.total_xtrns) {	 /* move all xtrns down */
					cfg.xtrn[k]=cfg.xtrn[k+1];
					k++; } }
			else j++; }
		for(j=0;j<cfg.total_xtrns;j++)	 /* move xtrn group numbers down */
			if(cfg.xtrn[j]->sec>i)
				cfg.xtrn[j]->sec--;
		cfg.total_xtrnsecs--;
		while(i<cfg.total_xtrnsecs) {
			cfg.xtrnsec[i]=cfg.xtrnsec[i+1];
            i++; }
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxtrnsec=*cfg.xtrnsec[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.xtrnsec[i]=savxtrnsec;
		uifc.changes=TRUE;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",cfg.xtrnsec[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.xtrnsec[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements"
			,cfg.xtrnsec[i]->arstr);
		sprintf(opt[k++],"%s","Available Online Programs...");
		opt[k][0]=0;
		uifc.savnum=1;
		sprintf(str,"%s Program Section",cfg.xtrnsec[i]->name);
		switch(uifc.list(WIN_SAV|WIN_ACT|WIN_MID,0,0,60,&xtrnsec_opt,0,str
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Online Program Section Name:

This is the name of this section.
*/
				strcpy(str,cfg.xtrnsec[i]->name);	 /* save */
				if(!uifc.input(WIN_MID|WIN_SAV,0,10
					,"Online Program Section Name"
					,cfg.xtrnsec[i]->name,40,K_EDIT))
					strcpy(cfg.xtrnsec[i]->name,str);
				break;
			case 1:
				strcpy(str,cfg.xtrnsec[i]->code);
				SETHELP(WHERE);
/*
Online Program Section Internal Code:

Every online program section must have its own unique internal code
for Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(cfg.xtrnsec[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
					uifc.helpbuf=0; }
				break;
			case 2:
				getar(cfg.xtrnsec[i]->name,cfg.xtrnsec[i]->arstr);
                break;
			case 3:
				xtrn_cfg(i);
				break; } } }
}

void hotkey_cfg(void)
{
	static int dflt,dfltopt,bar;
	char str[81],done=0;
	int j,k;
	uint i;
	static hotkey_t savhotkey;

while(1) {
	for(i=0;i<cfg.total_hotkeys && i<MAX_OPTS;i++)
		sprintf(opt[i],"Ctrl-%c    %.40s"
            ,cfg.hotkey[i]->key+'@'
            ,cfg.hotkey[i]->cmd);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_RHT;
	uifc.savnum=0;
	if(cfg.total_hotkeys)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_hotkeys<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savhotkey.cmd[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Global Hot Key Events:

This is a list of programs or loadable modules that can be executed by
anyone on the BBS at any time (while the BBS has control of user input).

To add a hot key event, select the desired location and hit  INS .

To delete a hot key event, select it and hit  DEL .

To configure a hot key event, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&dflt,&bar,"Global Hot Key Events",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Global Hot Key:

This is the control key used to trigger the hot key event. Example, A
indicates a Ctrl-A hot key event.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Control Key",str,1
			,K_UPPER)<1)
            continue;

		if((cfg.hotkey=(hotkey_t **)REALLOC(cfg.hotkey
            ,sizeof(hotkey_t *)*(cfg.total_hotkeys+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_hotkeys+1);
			cfg.total_hotkeys=0;
			bail(1);
            continue; }
		if(cfg.total_hotkeys)
			for(j=cfg.total_hotkeys;j>i;j--)
				cfg.hotkey[j]=cfg.hotkey[j-1];
		if((cfg.hotkey[i]=(hotkey_t *)MALLOC(sizeof(hotkey_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(hotkey_t));
			continue; }
		memset((hotkey_t *)cfg.hotkey[i],0,sizeof(hotkey_t));
		cfg.hotkey[i]->key=str[0]-'@';
		cfg.total_hotkeys++;
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.hotkey[i]);
		cfg.total_hotkeys--;
		for(j=i;j<cfg.total_hotkeys;j++)
			cfg.hotkey[j]=cfg.hotkey[j+1];
		uifc.changes=TRUE;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savhotkey=*cfg.hotkey[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.hotkey[i]=savhotkey;
		uifc.changes=TRUE;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27sCtrl-%c","Global Hot Key"
            ,cfg.hotkey[i]->key+'@');
		sprintf(opt[k++],"%-27.27s%.40s","Command Line",cfg.hotkey[i]->cmd);
        opt[k][0]=0;
		SETHELP(WHERE);
/*
Global Hot Key Event:

This menu allows you to change the settings for the selected global
hot key event. Hot key events are control characters that are used to
execute an external program or module anywhere in the BBS.
*/
		uifc.savnum=1;
		sprintf(str,"Ctrl-%C Hot Key Event",cfg.hotkey[i]->key+'@');
		switch(uifc.list(WIN_SAV|WIN_ACT|WIN_L2R|WIN_BOT,0,0,60,&dfltopt,0
			,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Global Hot-Ctrl Key:

This is the global control key used to execute this event.
*/
				sprintf(str,"%c",cfg.hotkey[i]->key+'@');
				if(uifc.input(WIN_MID|WIN_SAV,0,10,"Global Hot Ctrl-Key"
					,str,1,K_EDIT|K_UPPER)>0)
					cfg.hotkey[i]->key=str[0]-'@';
				break;
		   case 1:
				SETHELP(WHERE);
/*
Hot Key Event Command Line:

This is the command line to execute when this hot key is pressed.
*/
				uifc.input(WIN_MID|WIN_SAV,0,10,"Command Line"
					,cfg.hotkey[i]->cmd,50,K_EDIT);
				break;
				} } }

}
