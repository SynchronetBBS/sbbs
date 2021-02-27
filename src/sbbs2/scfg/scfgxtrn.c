#line 2 "SCFGXTRN.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

extern char *wday[];

char *daystr(char days);


/****************************************************************************/
/* Synchronet configuration utility 										*/
/****************************************************************************/

#include "scfg.h"

void xprogs_cfg()
{
	static int xprogs_dflt;
	int 	i;
	char	str[81];

while(1) {
	i=0;
	strcpy(opt[i++],"Fixed Events");
	strcpy(opt[i++],"Timed Events");
	strcpy(opt[i++],"Global Swap List");
	strcpy(opt[i++],"OS/2 Program List");
	strcpy(opt[i++],"External Editors");
	strcpy(opt[i++],"Online Programs");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
Online External Programs:

From this menu, you can configure external events, external editors, or
online external programs (doors).
*/
	switch(ulist(WIN_ORG|WIN_CHE|WIN_ACT,0,0,0,&xprogs_dflt,0
		,"External Programs",opt)) {
		case -1:
			i=save_changes(WIN_MID);
            if(i==-1)
                break;
			if(!i) {
                write_xtrn_cfg();
				write_main_cfg(); }
			return;
		case 0:
			fevents_cfg();
			break;
		case 1:
			tevents_cfg();
			break;
		case 2:
			swap_cfg();
			break;
		case 3:
			os2pgm_cfg();
			break;
		case 4:
			xedit_cfg();
			break;
		case 5:
			xtrnsec_cfg();
			break; } }
}

void fevents_cfg()
{
	static int event_dflt;
	char str[81],*p;
	int i;

while(1) {
	i=0;
	sprintf(opt[i++],"%-32.32s%.40s","Logon Event",sys_logon);
	sprintf(opt[i++],"%-32.32s%.40s","Logout Event",sys_logout);
	sprintf(opt[i++],"%-32.32s%.40s","Daily Event",sys_daily);
	opt[i][0]=0;
	savnum=0;
	SETHELP(WHERE);
/*
External Events:

From this menu, you can configure the logon and logout events, and the
system daily event.
*/
	switch(ulist(WIN_ACT|WIN_SAV|WIN_CHE|WIN_BOT|WIN_RHT,0,0,60,&event_dflt,0
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
			uinput(WIN_MID|WIN_SAV,0,0,"Logon Event"
				,sys_logon,50,K_EDIT);
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
			uinput(WIN_MID|WIN_SAV,0,0,"Logout Event"
				,sys_logout,50,K_EDIT);
			break;
		case 2:
			SETHELP(WHERE);
/*
Daily Event:

This is the command line for a program that will run after the first
user that logs on after midnight, logs off (regardless of what node).
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Daily Event"
				,sys_daily,50,K_EDIT);

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
	for(i=0;i<total_events && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-8.8s      %.50s",event[i]->code,event[i]->cmd);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_RHT;
	savnum=0;
	if(total_events)
		j|=WIN_DEL|WIN_GET;
	if(total_events<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT;
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
	i=ulist(j,0,0,45,&dflt,&bar,"Timed Events",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i=total_events;
		SETHELP(WHERE);
/*
Timed Event Internal Code:

This is the internal code for the timed event.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Event Internal Code",str,8
			,K_UPPER)<1)
            continue;
		if((event=(event_t **)REALLOC(event
			,sizeof(event_t *)*(total_events+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_events+1);
			total_events=0;
			bail(1);
            continue; }
		if((event[i]=(event_t *)MALLOC(sizeof(event_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(event_t));
			continue; }
		memset((event_t *)event[i],0,sizeof(event_t));
		strcpy(event[i]->code,str);
		event[i]->node=1;
		event[i]->days=0xff;
		total_events++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(event[i]);
		total_events--;
		for(j=i;j<total_events;j++)
			event[j]=event[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savevent=*event[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*event[i]=savevent;
		changes=1;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-32.32s%s","Internal Code",event[i]->code);
		sprintf(opt[k++],"%-32.32s%.40s","Start-up Directory",event[i]->dir);
		sprintf(opt[k++],"%-32.32s%.40s","Command Line",event[i]->cmd);
		sprintf(opt[k++],"%-32.32s%u","Execution Node",event[i]->node);
		sprintf(opt[k++],"%-32.32s%s","Execution Days",daystr(event[i]->days));
		sprintf(opt[k++],"%-32.32s%02u:%02u","Execution Time"
			,event[i]->time/60,event[i]->time%60);
		sprintf(opt[k++],"%-32.32s%s","Requires Exclusive Execution"
			,event[i]->misc&EVENT_EXCL ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Force Users Off-line For Event"
			,event[i]->misc&EVENT_FORCE ? "Yes":"No");
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
		savnum=1;
		sprintf(str,"%s Timed Event",event[i]->code);
		switch(ulist(WIN_SAV|WIN_ACT|WIN_L2R|WIN_BOT,0,0,70,&dfltopt,0
			,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				strcpy(str,event[i]->code);
				SETHELP(WHERE);
/*
Timed Event Internal Code:

Every timed event must have its own unique internal code for Synchronet
to reference it by. It is helpful if this code is an abreviation of the
command line.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(event[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
					helpbuf=0; }
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
				uinput(WIN_MID|WIN_SAV,0,10,"Directory"
					,event[i]->dir,50,K_EDIT|K_UPPER);
                break;
			case 2:
				SETHELP(WHERE);
/*
Timed Event Command Line:

This is the command line to execute upon this timed event.
*/
				uinput(WIN_MID|WIN_SAV,0,10,"Command"
					,event[i]->cmd,50,K_EDIT);
				break;
			case 3:
				SETHELP(WHERE);
/*
Timed Event Node:

This is the node number to execute the timed event.
*/
				sprintf(str,"%u",event[i]->node);
				uinput(WIN_MID|WIN_SAV,0,0,"Node Number"
					,str,3,K_EDIT|K_NUMBER);
				event[i]->node=atoi(str);
				break;
			case 4:
				j=0;
				while(1) {
					for(k=0;k<7;k++)
						sprintf(opt[k],"%s        %s"
							,wday[k],(event[i]->days&(1<<k)) ? "Yes":"No");
					opt[k][0]=0;
					savnum=2;
					SETHELP(WHERE);
/*
Days to Execute Event:

These are the days of the week that this event will be executed.
*/
					k=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Days to Execute Event",opt);
					if(k==-1)
						break;
					event[i]->days^=(1<<k);
					changes=1; }
				break;
			case 5:
				sprintf(str,"%2.2d:%2.2d",event[i]->time/60
					,event[i]->time%60);
				SETHELP(WHERE);
/*
Time to Execute Event:

This is the time (in 24 hour HH:MM format) to execute the event.
*/
				if(uinput(WIN_MID|WIN_SAV,0,0
					,"Time to Execute Event (HH:MM)"
					,str,5,K_UPPER|K_EDIT)>0) {
					event[i]->time=atoi(str)*60;
					if((p=strchr(str,':'))!=NULL)
						event[i]->time+=atoi(p+1); }
				break;
			case 6:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Exclusive Event Execution:

If this event must be run exclusively (all nodes inactive), set this
option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Exclusive Execution"
					,opt);
				if(!k && !(event[i]->misc&EVENT_EXCL)) {
					event[i]->misc|=EVENT_EXCL;
					changes=1; }
				else if(k==1 && event[i]->misc&EVENT_EXCL) {
					event[i]->misc&=~EVENT_EXCL;
					changes=1; }
                break;
			case 7:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Force Users Off-line for Event:

If you want to have your users' on-line time reduced so the event can
execute precisely on time, set this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Force Users Off-line for Event",opt);
				if(!k && !(event[i]->misc&EVENT_FORCE)) {
					event[i]->misc|=EVENT_FORCE;
					changes=1; }
				else if(k==1 && event[i]->misc&EVENT_FORCE) {
					event[i]->misc&=~EVENT_FORCE;
                    changes=1; }
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
	for(i=0,j=0;i<total_xtrns && j<MAX_OPTS;i++)
		if(xtrn[i]->sec==section) {
			sprintf(opt[j],"%-25s",xtrn[i]->name);
			xtrnnum[j++]=i; }
	xtrnnum[j]=total_xtrns;
	opt[j][0]=0;
	savnum=2;
	i=WIN_ACT|WIN_CHE|WIN_SAV|WIN_RHT;
	if(j)
		i|=WIN_DEL|WIN_GET;
	if(total_xtrns<MAX_OPTS)
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
	sprintf(str,"%s Online Programs",xtrnsec[section]->name);
	i=ulist(i,0,0,45,&ext_dflt,&ext_bar,str,opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Online Program Name:

This is the name or description of the online program (door).
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Online Program Name",str,25
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
		if(uinput(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((xtrn=(xtrn_t **)REALLOC(xtrn,sizeof(xtrn_t *)*(total_xtrns+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_xtrns+1);
			total_xtrns=0;
			bail(1);
            continue; }
		if(j)
			for(n=total_xtrns;n>xtrnnum[i];n--)
				xtrn[n]=xtrn[n-1];
		if((xtrn[xtrnnum[i]]=(xtrn_t *)MALLOC(sizeof(xtrn_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xtrn_t));
			continue; }
		memset((xtrn_t *)xtrn[xtrnnum[i]],0,sizeof(xtrn_t));
		strcpy(xtrn[xtrnnum[i]]->name,str);
		strcpy(xtrn[xtrnnum[i]]->code,code);
		xtrn[xtrnnum[i]]->sec=section;
		total_xtrns++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(xtrn[xtrnnum[i]]);
		total_xtrns--;
		for(j=xtrnnum[i];j<total_xtrns;j++)
			xtrn[j]=xtrn[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxtrn=*xtrn[xtrnnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*xtrn[xtrnnum[i]]=savxtrn;
		xtrn[xtrnnum[i]]->sec=section;
		changes=1;
        continue; }
	done=0;
	i=xtrnnum[i];
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",xtrn[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",xtrn[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Start-up Directory",xtrn[i]->path);
		sprintf(opt[k++],"%-27.27s%.40s","Command Line",xtrn[i]->cmd);
		sprintf(opt[k++],"%-27.27s%.40s","Clean-up Command Line",xtrn[i]->clean);
		if(xtrn[i]->cost)
			sprintf(str,"%lu credits",xtrn[i]->cost);
		else
			strcpy(str,"None");
		sprintf(opt[k++],"%-27.27s%s","Execution Cost",str);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",xtrn[i]->ar);
		sprintf(opt[k++],"%-27.27s%.40s","Execution Requirements"
			,xtrn[i]->run_ar);
		sprintf(opt[k++],"%-27.27s%s","Multiple Concurrent Users"
			,xtrn[i]->misc&MULTIUSER ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s%s","Intercept I/O Interrupts"
			,xtrn[i]->misc&IO_INTS ? "Yes" : "No"
			,xtrn[i]->misc&WWIVCOLOR ? ", WWIV" : nulstr);
		sprintf(opt[k++],"%-27.27s%s","Swap BBS out of Memory"
			,xtrn[i]->misc&SWAP ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Modify User Data"
            ,xtrn[i]->misc&MODUSERDAT ? "Yes" : "No");
		switch(xtrn[i]->event) {
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
		if(xtrn[i]->misc&EVENTONLY && xtrn[i]->event)
			strcat(str,", Only");
		sprintf(opt[k++],"%-27.27s%s","Execute on Event",str);
		switch(xtrn[i]->type) {
			case XTRN_SBBS:
				sprintf(str,"%-15s %s","Synchronet","XTRN.DAT");
				break;
			case XTRN_WWIV:
				sprintf(str,"%-15s %s","WWIV","CHAIN.TXT");
				break;
			case XTRN_GAP:
				sprintf(str,"%-15s %s","GAP","DOOR.SYS");
				break;
			case XTRN_RBBS:
				sprintf(str,"%-15s %s","RBBS/QuickBBS","DORINFO#.DEF");
				break;
			case XTRN_RBBS1:
				sprintf(str,"%-15s %s","RBBS/QuickBBS","DORINFO1.DEF");
                break;
			case XTRN_WILDCAT:
				sprintf(str,"%-15s %s","Wildcat","CALLINFO.BBS");
				break;
			case XTRN_PCBOARD:
				sprintf(str,"%-15s %s","PCBoard","PCBOARD.SYS");
				break;
			case XTRN_SPITFIRE:
				sprintf(str,"%-15s %s","SpitFire","SFDOORS.DAT");
				break;
			case XTRN_UTI:
				sprintf(str,"%-15s %s","MegaMail","UTIDOOR.TXT");
				break;
			case XTRN_SR:
				sprintf(str,"%-15s %s","Solar Realms","DOORFILE.SR");
				break;
			case XTRN_TRIBBS:
				sprintf(str,"%-15s %s","TriBBS","TRIBBS.SYS");
				break;
			default:
				strcpy(str,"None");
				break; }
		sprintf(opt[k++],"%-23.23s%-4s%s","BBS Drop File Type"
			,xtrn[i]->misc&REALNAME ? "(R)":nulstr,str);
		sprintf(opt[k++],"%-27.27s%s","Place Drop File In"
			,xtrn[i]->misc&STARTUPDIR ? "Start-Up Directory":"Node Directory");
		sprintf(opt[k++],"Time Options...");
		opt[k][0]=0;
		savnum=3;
		SETHELP(WHERE);
/*
Online Program Configuration:

This menu is for configuring the selected online program.
*/
		switch(ulist(WIN_SAV|WIN_ACT|WIN_MID,0,0,60,&opt_dflt,0,xtrn[i]->name
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
				strcpy(str,xtrn[i]->name);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"Online Program Name"
					,xtrn[i]->name,25,K_EDIT))
					strcpy(xtrn[i]->name,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Online Program Internal Code:

Every online program must have its own unique code for Synchronet to
refer to it internally. This code is usually an abreviation of the
online program name.
*/
				strcpy(str,xtrn[i]->code);
				uinput(WIN_MID|WIN_SAV,0,10,"Internal Code"
					,str,8,K_UPPER|K_EDIT);
				if(code_ok(str))
					strcpy(xtrn[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
                    helpbuf=0; }
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
				uinput(WIN_MID|WIN_SAV,0,10,"Directory"
					,xtrn[i]->path,50,K_EDIT|K_UPPER);
                break;
			case 3:
				SETHELP(WHERE);
/*
Online Program Command Line:

This is the command line to execute to run the online program.
*/
				uinput(WIN_MID|WIN_SAV,0,10,"Command"
					,xtrn[i]->cmd,50,K_EDIT);
				break;
			case 4:
				SETHELP(WHERE);
/*
Online Program Clean-up Command:

This is the command line to execute after the main command line. This
option is usually only used for multiuser online programs.
*/
				uinput(WIN_MID|WIN_SAV,0,10,"Clean-up"
					,xtrn[i]->clean,50,K_EDIT);
                break;
			case 5:
                ultoa(xtrn[i]->cost,str,10);
                SETHELP(WHERE);
/*
Online Program Cost to Run:

If you want users to be charged credits to run this online program,
set this value to the number of credits to charge. If you want this
online program to be free, set this value to 0.
*/
                uinput(WIN_MID|WIN_SAV,0,0,"Cost to Run (in Credits)"
                    ,str,10,K_EDIT|K_NUMBER);
                xtrn[i]->cost=atol(str);
                break;
			case 6:
				savnum=4;
				sprintf(str,"%s Access",xtrn[i]->name);
				getar(str,xtrn[i]->ar);
                break;
			case 7:
				savnum=4;
				sprintf(str,"%s Execution",xtrn[i]->name);
				getar(str,xtrn[i]->run_ar);
                break;
			case 8:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Supports Multiple Users:

If this online program supports multiple simultaneous users (nodes),
set this option to Yes.
*/
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Supports Multiple Users"
					,opt);
				if(!k && !(xtrn[i]->misc&MULTIUSER)) {
					xtrn[i]->misc|=MULTIUSER;
					changes=1; }
				else if(k==1 && xtrn[i]->misc&MULTIUSER) {
					xtrn[i]->misc&=~MULTIUSER;
					changes=1; }
                break;
			case 9:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Intercept I/O Interrupts:

If this online program has its own serial communication abilities, set
this option to No.
*/
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept I/O Interrupts"
					,opt);
				if(!k && !(xtrn[i]->misc&IO_INTS)) {
					xtrn[i]->misc|=IO_INTS;
					changes=1; }
				else if(k==1 && xtrn[i]->misc&IO_INTS) {
					xtrn[i]->misc&=~(IO_INTS|WWIVCOLOR);
					changes=1; }
				if(!(xtrn[i]->misc&IO_INTS))
					break;
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Program Uses WWIV Color Codes:

If this program was written for use exclusively under WWIV, set this
option to Yes.
*/
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Program Uses WWIV Color Codes"
					,opt);
				if(!k && !(xtrn[i]->misc&WWIVCOLOR)) {
					xtrn[i]->misc|=WWIVCOLOR;
					changes=1; }
				else if(k==1 && xtrn[i]->misc&WWIVCOLOR) {
					xtrn[i]->misc&=~WWIVCOLOR;
                    changes=1; }
                break;
			case 10:
				k=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Swap BBS out of Memory to Run Executable:

If this online programs requires a large amount of free memory,
set this option to Yes to have the BBS swapped out of memory.
*/
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Swap BBS out of Memory",opt);
				if(!k && !(xtrn[i]->misc&SWAP)) {
					xtrn[i]->misc|=SWAP;
					changes=1; }
				else if(k==1 && xtrn[i]->misc&SWAP) {
					xtrn[i]->misc&=~SWAP;
					changes=1; }
				break;
			case 11:
				k=1;
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
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Program Can Modify User Data",opt);
				if(!k && !(xtrn[i]->misc&MODUSERDAT)) {
					xtrn[i]->misc|=MODUSERDAT;
					changes=1; }
				else if(k==1 && xtrn[i]->misc&MODUSERDAT) {
					xtrn[i]->misc&=~MODUSERDAT;
					changes=1; }
                break;
			case 12:
				k=0;
				strcpy(opt[k++],"No");
				strcpy(opt[k++],"Logon");
				strcpy(opt[k++],"Logoff");
				strcpy(opt[k++],"New User");
				strcpy(opt[k++],"Birthday");
				opt[k][0]=0;
				switch(xtrn[i]->event) {
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
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Execute on Event",opt);
				if(k==-1)
					break;
				if(xtrn[i]->event!=k) {
					xtrn[i]->event=k;
					changes=1; }
				if(!xtrn[i]->event)
					break;
				k=1;
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
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k
					,0,"Execute as Event Only"
                    ,opt);
                if(!k && !(xtrn[i]->misc&EVENTONLY)) {
                    xtrn[i]->misc|=EVENTONLY;
                    changes=1; }
                else if(k==1 && xtrn[i]->misc&EVENTONLY) {
                    xtrn[i]->misc&=~EVENTONLY;
                    changes=1; }
                break;
			case 13:
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
				opt[k][0]=0;
				k=xtrn[i]->type;
				SETHELP(WHERE);
/*
Online Program BBS Drop File Type:

If this online program requires a specific BBS data (drop) file
format, select the file format from the list.
*/
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"BBS Drop File Type",opt);
				if(k==-1)
					break;
				if(xtrn[i]->type!=k) {
					xtrn[i]->type=k;
					changes=1; }
				if(xtrn[i]->type && uq&UQ_ALIASES) {
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					k=1;
					k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Use Real Names",opt);
					if(k==0 && !(xtrn[i]->misc&REALNAME)) {
						xtrn[i]->misc|=REALNAME;
						changes=1; }
					else if(k==1 && xtrn[i]->misc&REALNAME) {
						xtrn[i]->misc&=~REALNAME;
						changes=1; } }
				break;
			case 14:
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
				savnum=4;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Create Drop File In"
                    ,opt);
				if(!k && xtrn[i]->misc&STARTUPDIR) {
					xtrn[i]->misc&=~STARTUPDIR;
                    changes=1; }
				else if(k==1 && !(xtrn[i]->misc&STARTUPDIR)) {
					xtrn[i]->misc|=STARTUPDIR;
                    changes=1; }
				break;
			case 15:
				while(1) {
					k=0;
					if(xtrn[i]->textra)
						sprintf(str,"%u minutes",xtrn[i]->textra);
					else
						strcpy(str,"None");
					sprintf(opt[k++],"%-25.25s%s","Extra Time",str);
					if(xtrn[i]->maxtime)
						sprintf(str,"%u minutes",xtrn[i]->maxtime);
					else
						strcpy(str,"None");
					sprintf(opt[k++],"%-25.25s%s","Maximum Time",str);
					sprintf(opt[k++],"%-25.25s%s","Suspended (Free) Time"
						,xtrn[i]->misc&FREETIME ? "Yes" : "No");
					opt[k][0]=0;
					SETHELP(WHERE);
/*
Online Program Time Options:

This sub-menu allows you to define specific preferences regarding the
time users spend running this program.
*/
					savnum=4;
					k=ulist(WIN_SAV|WIN_ACT|WIN_RHT|WIN_BOT,0,0,40
						,&time_dflt,0
						,"Online Program Time Options",opt);
					if(k==-1)
						break;
					savnum=5;
					switch(k) {
						case 0:
							itoa(xtrn[i]->textra,str,10);
							SETHELP(WHERE);
/*
Extra Time to Give User in Program:

If you want to give users extra time while in this online program,
set this value to the number of minutes to add to their current time
left online.
*/
							uinput(WIN_MID|WIN_SAV,0,0
								,"Extra Time to Give User (in minutes)"
								,str,2,K_EDIT|K_NUMBER);
							xtrn[i]->textra=atoi(str);
							break;
						case 1:
							itoa(xtrn[i]->maxtime,str,10);
							SETHELP(WHERE);
/*
Maximum Time Allowed in Program:

If this program supports a drop file that contains the number of minutes
left online for the current user, this option allows the sysop to set
the maximum number of minutes that will be allowed in the drop file.

Setting this option to 0, disables this feature.
*/
							uinput(WIN_MID|WIN_SAV,0,0
								,"Maximum Time (in minutes, 0=disabled)"
								,str,2,K_EDIT|K_NUMBER);
							xtrn[i]->maxtime=atoi(str);
							break;
						case 2:
							k=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Suspended (Free) Time:

If you want the user's time online to be suspended while running this
online program (e.g. Free Time), set this option to Yes.
*/
							k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Suspended (Free) Time",opt);
							if(!k && !(xtrn[i]->misc&FREETIME)) {
								xtrn[i]->misc|=FREETIME;
								changes=1; }
							else if(k==1 && xtrn[i]->misc&FREETIME) {
								xtrn[i]->misc&=~FREETIME;
								changes=1; }
							break; } }
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
	for(i=0;i<total_xedits && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-8.8s    %.40s",xedit[i]->code,xedit[i]->lcmd);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_RHT;
	savnum=0;
	if(total_xedits)
		j|=WIN_DEL|WIN_GET;
	if(total_xedits<MAX_OPTS)
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
	i=ulist(j,0,0,45,&dflt,&bar,"External Editors",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
External Editor Name:

This is the name or description of the external editor.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"External Editor Name",str,40
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
		if(uinput(WIN_MID|WIN_SAV,0,0,"External Editor Internal Code",code,8
			,K_UPPER|K_EDIT)<1)
            continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }

		if((xedit=(xedit_t **)REALLOC(xedit
            ,sizeof(xedit_t *)*(total_xedits+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,total_xedits+1);
			total_xedits=0;
			bail(1);
            continue; }
		if(total_xedits)
			for(j=total_xedits;j>i;j--)
				xedit[j]=xedit[j-1];
		if((xedit[i]=(xedit_t *)MALLOC(sizeof(xedit_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xedit_t));
			continue; }
		memset((xedit_t *)xedit[i],0,sizeof(xedit_t));
		strcpy(xedit[i]->name,str);
		strcpy(xedit[i]->code,code);
		total_xedits++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(xedit[i]);
		total_xedits--;
		for(j=i;j<total_xedits;j++)
			xedit[j]=xedit[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxedit=*xedit[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*xedit[i]=savxedit;
		changes=1;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-32.32s%s","Name",xedit[i]->name);
		sprintf(opt[k++],"%-32.32s%s","Internal Code",xedit[i]->code);
		sprintf(opt[k++],"%-32.32s%.40s","Local Command Line",xedit[i]->lcmd);
		sprintf(opt[k++],"%-32.32s%.40s","Remote Command Line",xedit[i]->rcmd);
		sprintf(opt[k++],"%-32.32s%.40s","Access Requirements",xedit[i]->ar);
		sprintf(opt[k++],"%-32.32s%s%s","Intercept I/O Interrupts"
			,xedit[i]->misc&IO_INTS ? "Yes":"No"
			,xedit[i]->misc&WWIVCOLOR ? ", WWIV" : nulstr);
		sprintf(opt[k++],"%-32.32s%s","Quoted Text"
			,xedit[i]->misc&QUOTEALL ? "All":xedit[i]->misc&QUOTENONE
				? "None" : "Prompt User");
		sprintf(opt[k++],"%-32.32s%s","QuickBBS Style (MSGTMP)"
			,xedit[i]->misc&QUICKBBS ? "Yes":"No");
		sprintf(opt[k++],"%-32.32s%s","Expand Line Feeds to CRLF"
			,xedit[i]->misc&EXPANDLF ? "Yes":"No");
		switch(xedit[i]->type) {
			case XTRN_SBBS:
				sprintf(str,"%-15s %s","Synchronet","XTRN.DAT");
				break;
			case XTRN_WWIV:
				sprintf(str,"%-15s %s","WWIV","CHAIN.TXT");
				break;
			case XTRN_GAP:
				sprintf(str,"%-15s %s","GAP","DOOR.SYS");
				break;
			case XTRN_RBBS:
				sprintf(str,"%-15s %s","RBBS/QuickBBS","DORINFO#.DEF");
				break;
			case XTRN_RBBS1:
				sprintf(str,"%-15s %s","RBBS/QuickBBS","DORINFO1.DEF");
                break;
			case XTRN_WILDCAT:
				sprintf(str,"%-15s %s","Wildcat","CALLINFO.BBS");
				break;
			case XTRN_PCBOARD:
				sprintf(str,"%-15s %s","PCBoard","PCBOARD.SYS");
				break;
			case XTRN_SPITFIRE:
				sprintf(str,"%-15s %s","SpitFire","SFDOORS.DAT");
				break;
			case XTRN_UTI:
				sprintf(str,"%-15s %s","MegaMail","UTIDOOR.TXT");
				break;
			case XTRN_SR:
				sprintf(str,"%-15s %s","Solar Realms","DOORFILE.SR");
				break;
			case XTRN_TRIBBS:
				sprintf(str,"%-15s %s","TriBBS","TRIBBS.SYS");
                break;
			default:
				strcpy(str,"None");
                break; }
		sprintf(opt[k++],"%-32.32s%s","BBS Drop File Type",str);
        opt[k][0]=0;
		SETHELP(WHERE);
/*
External Editor Configuration:

This menu allows you to change the settings for the selected external
message editor. External message editors are very common on BBSs. Some
popular editors include SyncEdit, WWIVedit, FEdit, GEdit, IceEdit,
and many others.
*/

		savnum=1;
		sprintf(str,"%s Editor",xedit[i]->name);
		switch(ulist(WIN_SAV|WIN_ACT|WIN_L2R|WIN_BOT,0,0,70,&dfltopt,0
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
				strcpy(str,xedit[i]->name);
				if(!uinput(WIN_MID|WIN_SAV,0,10,"External Editor Name"
					,xedit[i]->name,40,K_EDIT))
					strcpy(xedit[i]->name,str);
				break;
			case 1:
				strcpy(str,xedit[i]->code);
				SETHELP(WHERE);
/*
External Editor Internal Code:

Every external editor must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(xedit[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
					helpbuf=0; }
                break;
			case 2:
				SETHELP(WHERE);
/*
External Editor Local Command Line:

This is the command line to execute when using this editor locally.
*/
				uinput(WIN_MID|WIN_SAV,0,10,"Local"
					,xedit[i]->lcmd,50,K_EDIT);
				break;

		   case 3:
				SETHELP(WHERE);
/*
External Editor Remote Command Line:

This is the command line to execute when using this editor remotely.
*/
				uinput(WIN_MID|WIN_SAV,0,10,"Remote"
					,xedit[i]->rcmd,50,K_EDIT);
				break;
			case 4:
				savnum=2;
				sprintf(str,"%s External Editor",xedit[i]->name);
				getar(str,xedit[i]->ar);
				break;
			case 5:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Intercept I/O Interrupts:

If this external editor has its own serial communication abilities or
requires a FOSSIL driver, set this option to No.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept I/O Interrupts"
					,opt);
				if(!k && !(xedit[i]->misc&IO_INTS)) {
					xedit[i]->misc|=IO_INTS;
					changes=1; }
				else if(k==1 && xedit[i]->misc&IO_INTS) {
					xedit[i]->misc&=~(IO_INTS|WWIVCOLOR);
					changes=1; }
				if(!(xedit[i]->misc&IO_INTS))
					break;
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Editor Uses WWIV Color Codes:

If this editor was written for use exclusively under WWIV, set this
option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Editor Uses WWIV Color Codes",opt);
				if(!k && !(xedit[i]->misc&WWIVCOLOR)) {
					xedit[i]->misc|=WWIVCOLOR;
					changes=1; }
				else if(k==1 && xedit[i]->misc&WWIVCOLOR) {
					xedit[i]->misc&=~WWIVCOLOR;
                    changes=1; }
                break;
			case 6:
				k=3;
				strcpy(opt[0],"All");
				strcpy(opt[1],"None");
				strcpy(opt[2],"Prompt User");
				opt[3][0]=0;
				savnum=2;
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
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Quoted Text"
					,opt);
				if(!k && !(xedit[i]->misc&QUOTEALL)) {
					xedit[i]->misc|=QUOTEALL;
					xedit[i]->misc&=~QUOTENONE;
					changes=1; }
				else if(k==1 && !(xedit[i]->misc&QUOTENONE)) {
					xedit[i]->misc|=QUOTENONE;
					xedit[i]->misc&=~QUOTEALL;
                    changes=1; }
				else if(k==2 && xedit[i]->misc&(QUOTENONE|QUOTEALL)) {
					xedit[i]->misc&=~(QUOTENONE|QUOTEALL);
					changes=1; }
                break;
			case 7:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
QuickBBS Style (MSGTMP):

If this external editor uses the QuickBBS style MSGTMP interface, set
this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"QuickBBS Style (MSGTMP)"
					,opt);
				if(!k && !(xedit[i]->misc&QUICKBBS)) {
					xedit[i]->misc|=QUICKBBS;
					changes=1; }
				else if(k==1 && xedit[i]->misc&QUICKBBS) {
					xedit[i]->misc&=~QUICKBBS;
                    changes=1; }
				break;
			case 8:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				savnum=2;
				SETHELP(WHERE);
/*
Expand Line Feeds to Carriage Return/Line Feed Pairs:

If this external editor saves new lines as a single line feed character
instead of a carriage return/line feed pair, set this option to Yes.
*/
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0,"Expand LF to CRLF"
					,opt);
				if(!k && !(xedit[i]->misc&EXPANDLF)) {
					xedit[i]->misc|=EXPANDLF;
					changes=1; }
				else if(k==1 && xedit[i]->misc&EXPANDLF) {
					xedit[i]->misc&=~EXPANDLF;
                    changes=1; }
				break;
			case 9:
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
				opt[k][0]=0;
				k=xedit[i]->type;
				SETHELP(WHERE);
/*
External Program BBS Drop File Type:

If this external editor requires a specific BBS data (drop) file
format, select the file format from the list.
*/
				savnum=2;
				k=ulist(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"BBS Drop File Type",opt);
				if(k==-1)
					break;
				if(xedit[i]->type!=k) {
					xedit[i]->type=k;
					changes=1; }
				break;

				} } }
}


void swap_cfg()
{
	static int dflt,bar;
	char str[81];
	int j,k;
	uint i;

while(1) {
	for(i=0;i<MAX_OPTS && i<total_swaps;i++)
		sprintf(opt[i],"%-25s",swap[i]->cmd);
	opt[i][0]=0;
	j=WIN_ACT|WIN_CHE|WIN_L2R|WIN_SAV;
	savnum=0;
	if(total_swaps)
		j|=WIN_DEL;
	if(total_swaps<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	SETHELP(WHERE);
/*
External Program Global Swap List:

This is a list of the external program (executable file) names to swap
the BBS out of memory for. They are termed Global because if you add
a program name to this list, the BBS will swap to run it regardless of
when, where, or why the program is run from the BBS.

Use  INS  and  DELETE  to add and remove swappable programs.

To change the filename of a program, hit  ENTER .

To swap whenever PKZIP is run, add PKZIP to the list.

To swap for all DOS Shells, add COMMAND.COM to the list.
*/
	i=ulist(j,0,0,17,&dflt,&bar,"Global Swap List",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Swappable Program Name:

This is the executable filename of the external program.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Program Name",str,12
			,K_UPPER)<1)
            continue;
		if((swap=(swap_t **)REALLOC(swap
			,sizeof(swap_t *)*(total_swaps+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_swaps+1);
			total_swaps=0;
			bail(1);
            continue; }
		if(total_swaps)
			for(j=total_swaps;j>i;j--)
				swap[j]=swap[j-1];
		if((swap[i]=(swap_t *)MALLOC(sizeof(swap_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(swap_t));
			continue; }
		memset((swap_t *)swap[i],0,sizeof(swap_t));
		strcpy(swap[i]->cmd,str);
		total_swaps++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(swap[i]);
		total_swaps--;
		for(j=i;j<total_swaps;j++)
			swap[j]=swap[j+1];
		changes=1;
		continue; }
	SETHELP(WHERE);
/*
Swappable Program Name:

This is the executable filename of the external program.
*/
	strcpy(str,swap[i]->cmd);
	if(uinput(WIN_MID|WIN_SAV,0,5,"Program Name",str,12
		,K_UPPER|K_EDIT)>0)
		strcpy(swap[i]->cmd,str); }
}

int os2pgm_cfg()
{
	static int dflt,bar;
	char str[81];
	int j,k;
	uint i;

while(1) {
	for(i=0;i<MAX_OPTS && i<total_os2pgms;i++)
		sprintf(opt[i],"%-12s %-12s",os2pgm[i]->name
			,os2pgm[i]->misc&OS2_POPEN ? "(port open)":nulstr);
	opt[i][0]=0;
	j=WIN_ACT|WIN_CHE|WIN_L2R|WIN_SAV;
	savnum=0;
	if(total_os2pgms)
		j|=WIN_DEL;
	if(total_os2pgms<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	SETHELP(WHERE);
/*
OS/2 Program List:

This is a list of all native OS/2 external program (executable file)
names that you may execute under Synchronet for OS/2. This list is not
used in Synchronet for DOS. Any programs not listed here will be assumed
to be DOS programs and executed accordingly.

Use  INS  and  DELETE  to add and remove OS/2 programs.

To change the filename of a program, hit  ENTER .
*/
	i=ulist(j,0,0,30,&dflt,&bar,"OS/2 Program List",opt);
	if((signed)i==-1)
		break;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
OS/2 Program Name:

This is the executable filename of the OS/2 external program.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"OS/2 Program Name",str,12
			,K_UPPER)<1)
            continue;
		if((os2pgm=(os2pgm_t **)REALLOC(os2pgm
			,sizeof(os2pgm_t *)*(total_os2pgms+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_os2pgms+1);
			total_os2pgms=0;
			bail(1);
            continue; }
		if(total_os2pgms)
			for(j=total_os2pgms;j>i;j--)
				os2pgm[j]=os2pgm[j-1];
		if((os2pgm[i]=(os2pgm_t *)MALLOC(sizeof(os2pgm_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(os2pgm_t));
			continue; }
		memset((os2pgm_t *)os2pgm[i],0,sizeof(os2pgm_t));
		strcpy(os2pgm[i]->name,str);

		SETHELP(WHERE);
/*
Leave COM Port Open During Execution:

If this program accesses the COM port via handle (currently open COM
port), set this option to Yes, otherwise set it to No.
*/
		strcpy(opt[0],"Yes (This Program Uses Port Handle)");
		strcpy(opt[1],"No  (This Program Uses Port Number)");
		opt[2][0]=0;
		j=1;
		if(ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
			,"Leave COM Port Open During Execution?",opt)==0)
			os2pgm[i]->misc|=OS2_POPEN;
		total_os2pgms++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(os2pgm[i]);
		total_os2pgms--;
		for(j=i;j<total_os2pgms;j++)
			os2pgm[j]=os2pgm[j+1];
		changes=1;
		continue; }
	SETHELP(WHERE);
/*
OS/2 Program Name:

This is the executable filename of the OS/2 external program.
*/
	strcpy(str,os2pgm[i]->name);
	if(uinput(WIN_MID|WIN_SAV,0,5,"OS/2 Program Name",str,12
		,K_UPPER|K_EDIT)>0)
		strcpy(os2pgm[i]->name,str);

	SETHELP(WHERE);
/*
Leave COM Port Open During Execution:

If this program accesses the COM port via handle (currently open COM
port), set this option to Yes, otherwise set it to No.
*/
	strcpy(opt[0],"Yes (This Program Uses Port Handle)");
	strcpy(opt[1],"No  (This Program Uses Port Number)");
	opt[2][0]=0;
	j=1;
	savnum=1;
	j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
		,"Leave COM Port Open During Execution",opt);
	if(j==0 && !(os2pgm[i]->misc&OS2_POPEN)) {
		os2pgm[i]->misc|=OS2_POPEN;
		changes=1; }
	else if(j==1 && os2pgm[i]->misc&OS2_POPEN) {
		os2pgm[i]->misc&=~OS2_POPEN;
		changes=1; }
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
	for(i=0;i<total_xtrnsecs && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",xtrnsec[i]->name);
	opt[i][0]=0;
	j=WIN_SAV|WIN_ACT|WIN_CHE|WIN_BOT;
	savnum=0;
	if(total_xtrnsecs)
		j|=WIN_DEL|WIN_GET;
	if(total_xtrnsecs<MAX_OPTS)
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
	i=ulist(j,0,0,45,&xtrnsec_dflt,0,"Online Program Sections",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Online Program Section Name:

This is the name of this section.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Online Program Section Name",str,40
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
		if(uinput(WIN_MID|WIN_SAV,0,0,"Online Program Section Internal Code"
			,code,8,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((xtrnsec=(xtrnsec_t **)REALLOC(xtrnsec
			,sizeof(xtrnsec_t *)*(total_xtrnsecs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_xtrnsecs+1);
			total_xtrnsecs=0;
			bail(1);
			continue; }
		if(total_xtrnsecs) {
			for(j=total_xtrnsecs;j>i;j--)
				xtrnsec[j]=xtrnsec[j-1];
			for(j=0;j<total_xtrns;j++)
				if(xtrn[j]->sec>=i)
					xtrn[j]->sec++; }


		if((xtrnsec[i]=(xtrnsec_t *)MALLOC(sizeof(xtrnsec_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(xtrnsec_t));
			continue; }
		memset((xtrnsec_t *)xtrnsec[i],0,sizeof(xtrnsec_t));
		strcpy(xtrnsec[i]->name,str);
		strcpy(xtrnsec[i]->code,code);
		total_xtrnsecs++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(xtrnsec[i]);
		for(j=0;j<total_xtrns;) {
			if(xtrn[j]->sec==i) {	 /* delete xtrns of this group */
				FREE(xtrn[j]);
				total_xtrns--;
				k=j;
				while(k<total_xtrns) {	 /* move all xtrns down */
					xtrn[k]=xtrn[k+1];
					k++; } }
			else j++; }
		for(j=0;j<total_xtrns;j++)	 /* move xtrn group numbers down */
			if(xtrn[j]->sec>i)
				xtrn[j]->sec--;
		total_xtrnsecs--;
		while(i<total_xtrnsecs) {
			xtrnsec[i]=xtrnsec[i+1];
            i++; }
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savxtrnsec=*xtrnsec[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*xtrnsec[i]=savxtrnsec;
		changes=1;
        continue; }
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",xtrnsec[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",xtrnsec[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements"
			,xtrnsec[i]->ar);
		sprintf(opt[k++],"%s","Available Online Programs...");
		opt[k][0]=0;
		savnum=1;
		sprintf(str,"%s Program Section",xtrnsec[i]->name);
		switch(ulist(WIN_SAV|WIN_ACT|WIN_MID,0,0,60,&xtrnsec_opt,0,str
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
				strcpy(str,xtrnsec[i]->name);	 /* save */
				if(!uinput(WIN_MID|WIN_SAV,0,10
					,"Online Program Section Name"
					,xtrnsec[i]->name,40,K_EDIT))
					strcpy(xtrnsec[i]->name,str);
				break;
			case 1:
				strcpy(str,xtrnsec[i]->code);
				SETHELP(WHERE);
/*
Online Program Section Internal Code:

Every online program section must have its own unique internal code
for Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(xtrnsec[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
					helpbuf=0; }
				break;
			case 2:
				getar(xtrnsec[i]->name,xtrnsec[i]->ar);
                break;
			case 3:
				xtrn_cfg(i);
				break; } } }
}
