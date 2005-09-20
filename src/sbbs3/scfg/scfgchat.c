/* scfgchat.c */

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

void page_cfg()
{
	static int dflt,bar;
	char str[81],done=0;
	int j,k;
	uint i;
	static page_t savpage;

while(1) {
	for(i=0;i<cfg.total_pages && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-40.40s %-.20s",cfg.page[i]->cmd,cfg.page[i]->arstr);
	opt[i][0]=0;
	uifc.savnum=0;
	j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
	if(cfg.total_pages)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_pages<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savpage.cmd[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
External Sysop Chat Pagers:

This is a list of the configured external sysop chat pagers.

To add a pager, select the desired location and hit  INS .

To delete a pager, select it and hit  DEL .

To configure a pager, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&dflt,&bar,"External Sysop Chat Pagers",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		sprintf(str,"%%!tone +chatpage.ton");
		SETHELP(WHERE);
/*
External Chat Pager Command Line:

This is the command line to execute for this external chat pager.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Command Line",str,50
			,K_EDIT)<1)
            continue;
		if((cfg.page=(page_t **)realloc(cfg.page,sizeof(page_t *)*(cfg.total_pages+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_pages+1);
			cfg.total_pages=0;
			bail(1);
            continue; }
		if(cfg.total_pages)
			for(j=cfg.total_pages;j>i;j--)
				cfg.page[j]=cfg.page[j-1];
		if((cfg.page[i]=(page_t *)malloc(sizeof(page_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(page_t));
			continue; }
		memset((page_t *)cfg.page[i],0,sizeof(page_t));
		strcpy(cfg.page[i]->cmd,str);
		cfg.total_pages++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		free(cfg.page[i]);
		cfg.total_pages--;
		for(j=i;j<cfg.total_pages;j++)
			cfg.page[j]=cfg.page[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savpage=*cfg.page[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.page[i]=savpage;
		uifc.changes=1;
        continue; }
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%.40s","Command Line",cfg.page[i]->cmd);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",cfg.page[i]->arstr);
		sprintf(opt[k++],"%-27.27s%s","Intercept I/O Interrupts"
			,cfg.page[i]->misc&IO_INTS ? "Yes":"No");
		opt[k][0]=0;
		sprintf(str,"Sysop Chat Pager #%d",i+1);
		uifc.savnum=1;
		switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&j,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
External Chat Pager Command Line:

This is the command line to execute for this external chat pager.
*/
				strcpy(str,cfg.page[i]->cmd);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Command Line"
					,cfg.page[i]->cmd,sizeof(cfg.page[i]->cmd)-1,K_EDIT))
					strcpy(cfg.page[i]->cmd,str);
				break;
			case 1:
				uifc.savnum=2;
				getar(str,cfg.page[i]->arstr);
				break;
			case 2:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Intercept I/O Interrupts:

If you wish the DOS screen output and keyboard input to be intercepted
when running this chat pager, set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Intercept I/O Interrupts"
					,opt);
				if(!k && !(cfg.page[i]->misc&IO_INTS)) {
					cfg.page[i]->misc|=IO_INTS;
					uifc.changes=1; }
				else if(k==1 && cfg.page[i]->misc&IO_INTS) {
					cfg.page[i]->misc&=~IO_INTS;
					uifc.changes=1; }
                break;

				} } }
}

void chan_cfg()
{
	static int chan_dflt,chan_bar,opt_dflt;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static chan_t savchan;

while(1) {
	for(i=0;i<cfg.total_chans && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.chan[i]->name);
	opt[i][0]=0;
	j=WIN_ACT|WIN_SAV|WIN_BOT|WIN_RHT;
	uifc.savnum=0;
	if(cfg.total_chans)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_chans<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savchan.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Multinode Chat Channels:

This is a list of the configured multinode chat channels.

To add a channel, select the desired location with the arrow keys and
hit  INS .

To delete a channel, select it with the arrow keys and hit  DEL .

To configure a channel, select it with the arrow keys and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&chan_dflt,&chan_bar,"Multinode Chat Channels",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Open");
		SETHELP(WHERE);
/*
Channel Name:

This is the name or description of the chat channel.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Chat Channel Name",str,25
			,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,' ');
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Chat Channel Internal Code:

Every chat channel must have its own unique code for Synchronet to refer
to it internally. This code is usually an abreviation of the chat
channel name.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,LEN_CODE,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.chan=(chan_t **)realloc(cfg.chan,sizeof(chan_t *)*(cfg.total_chans+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_chans+1);
			cfg.total_chans=0;
			bail(1);
            continue; }
		if(cfg.total_chans)
			for(j=cfg.total_chans;j>i;j--)
				cfg.chan[j]=cfg.chan[j-1];
		if((cfg.chan[i]=(chan_t *)malloc(sizeof(chan_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chan_t));
			continue; }
		memset((chan_t *)cfg.chan[i],0,sizeof(chan_t));
		strcpy(cfg.chan[i]->name,str);
		strcpy(cfg.chan[i]->code,code);
		cfg.total_chans++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		free(cfg.chan[i]);
		cfg.total_chans--;
		for(j=i;j<cfg.total_chans;j++)
			cfg.chan[j]=cfg.chan[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savchan=*cfg.chan[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.chan[i]=savchan;
		uifc.changes=1;
        continue; }
    j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",cfg.chan[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.chan[i]->code);
		sprintf(opt[k++],"%-27.27s%lu","Cost in Credits",cfg.chan[i]->cost);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements"
			,cfg.chan[i]->arstr);
		sprintf(opt[k++],"%-27.27s%s","Password Protection"
			,cfg.chan[i]->misc&CHAN_PW ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Guru Joins When Empty"
			,cfg.chan[i]->misc&CHAN_GURU ? "Yes" : "No");
		sprintf(opt[k++],"%-27.27s%s","Channel Guru"
			,cfg.chan[i]->guru<cfg.total_gurus ? cfg.guru[cfg.chan[i]->guru]->name : "");
        sprintf(opt[k++],"%-27.27s%s","Channel Action Set"
            ,cfg.actset[cfg.chan[i]->actset]->name);
		opt[k][0]=0;
		SETHELP(WHERE);
/*
Chat Channel Configuration:

This menu is for configuring the selected chat channel.
*/
		uifc.savnum=1;
		sprintf(str,"%s Chat Channel",cfg.chan[i]->name);
		switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Chat Channel Name:

This is the name or description of the chat channel.
*/
				strcpy(str,cfg.chan[i]->name);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Chat Channel Name"
					,cfg.chan[i]->name,sizeof(cfg.chan[i]->name)-1,K_EDIT))
					strcpy(cfg.chan[i]->name,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Chat Channel Internal Code:

Every chat channel must have its own unique code for Synchronet to refer
to it internally. This code is usually an abreviation of the chat
channel name.
*/
				strcpy(str,cfg.chan[i]->code);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Internal Code"
					,str,LEN_CODE,K_UPPER|K_EDIT))
					break;
				if(code_ok(str))
					strcpy(cfg.chan[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
                    uifc.helpbuf=0; }
                break;
			case 2:
				ultoa(cfg.chan[i]->cost,str,10);
                SETHELP(WHERE);
/*
Chat Channel Cost to Join:

If you want users to be charged credits to join this chat channel, set
this value to the number of credits to charge. If you want this channel
to be free, set this value to 0.
*/
				uifc.input(WIN_MID|WIN_SAV,0,0,"Cost to Join (in Credits)"
                    ,str,10,K_EDIT|K_NUMBER);
				cfg.chan[i]->cost=atol(str);
                break;
			case 3:
				uifc.savnum=2;
				sprintf(str,"%s Chat Channel",cfg.chan[i]->name);
				getar(str,cfg.chan[i]->arstr);
				break;
			case 4:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Allow Channel to be Password Protected:

If you want to allow the first user to join this channel to password
protect it, set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Allow Channel to be Password Protected"
					,opt);
				if(!k && !(cfg.chan[i]->misc&CHAN_PW)) {
					cfg.chan[i]->misc|=CHAN_PW;
					uifc.changes=1; }
				else if(k==1 && cfg.chan[i]->misc&CHAN_PW) {
					cfg.chan[i]->misc&=~CHAN_PW;
					uifc.changes=1; }
				break;
			case 5:
				k=1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Guru Joins This Channel When Empty:

If you want the system guru to join this chat channel when there is
only one user, set this option to Yes.
*/
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Guru Joins This Channel When Empty"
					,opt);
				if(!k && !(cfg.chan[i]->misc&CHAN_GURU)) {
					cfg.chan[i]->misc|=CHAN_GURU;
					uifc.changes=1; }
				else if(k==1 && cfg.chan[i]->misc&CHAN_GURU) {
					cfg.chan[i]->misc&=~CHAN_GURU;
					uifc.changes=1; }
				break;
			case 6:
SETHELP(WHERE);
/*
Channel Guru:

This is a list of available chat Gurus.  Select the one that you wish
to have available in this channel.
*/
				k=0;
				for(j=0;j<cfg.total_gurus && j<MAX_OPTS;j++)
					sprintf(opt[j],"%-25s",cfg.guru[j]->name);
				opt[j][0]=0;
				uifc.savnum=2;
				k=uifc.list(WIN_SAV|WIN_RHT,0,0,25,&j,0
					,"Available Chat Gurus",opt);
				if(k==-1)
					break;
				cfg.chan[i]->guru=k;
				break;
			case 7:
SETHELP(WHERE);
/*
Channel Action Set:

This is a list of available chat action sets.  Select the one that you
wish to have available in this channel.
*/
				k=0;
				for(j=0;j<cfg.total_actsets && j<MAX_OPTS;j++)
					sprintf(opt[j],"%-25s",cfg.actset[j]->name);
				opt[j][0]=0;
				uifc.savnum=2;
				k=uifc.list(WIN_SAV|WIN_RHT,0,0,25,&j,0
					,"Available Chat Action Sets",opt);
				if(k==-1)
					break;
				uifc.changes=1;
				cfg.chan[i]->actset=k;
				break; } } }
}

void chatact_cfg(uint setnum)
{
	static int chatact_dflt,chatact_bar;
	char str[128],cmd[128],out[128];
	int j;
	uint i,n,chatnum[MAX_OPTS+1];
	static chatact_t savchatact;

while(1) {
	for(i=0,j=0;i<cfg.total_chatacts && j<MAX_OPTS;i++)
		if(cfg.chatact[i]->actset==setnum) {
			sprintf(opt[j],"%-*.*s %s",LEN_CHATACTCMD,LEN_CHATACTCMD
				,cfg.chatact[i]->cmd,cfg.chatact[i]->out);
			chatnum[j++]=i; }
	chatnum[j]=cfg.total_chatacts;
	opt[j][0]=0;
	uifc.savnum=2;
	i=WIN_ACT|WIN_SAV;
	if(j)
		i|=WIN_DEL|WIN_GET;
	if(j<MAX_OPTS)
		i|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savchatact.cmd[0])
		i|=WIN_PUT;
	SETHELP(WHERE);
/*
Multinode Chat Actions:

This is a list of the configured multinode chat actions.  The users can
use these actions in multinode chat by turning on action commands with
the /A command in multinode chat.  Then if a line is typed which
begins with a valid action command and has a user name, chat handle,
or node number following, the output string will be displayed replacing
the %s symbols with the sending user's name and the receiving user's
name (in that order).

To add an action, select the desired location with the arrow keys and
hit  INS .

To delete an action, select it with the arrow keys and hit  DEL .

To configure an action, select it with the arrow keys and hit  ENTER .
*/
	sprintf(str,"%s Chat Actions",cfg.actset[setnum]->name);
	i=uifc.list(i,0,0,70,&chatact_dflt,&chatact_bar,str,opt);
	uifc.savnum=3;
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Chat Action Command:

This is the command word (normally a verb) to trigger the action output.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Action Command",cmd,LEN_CHATACTCMD
			,K_UPPER)<1)
            continue;
		SETHELP(WHERE);
/*
Chat Action Output String:

This is the output string displayed with this action output.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"",out,LEN_CHATACTOUT
			,K_MSG)<1)
            continue;
		if((cfg.chatact=(chatact_t **)realloc(cfg.chatact
            ,sizeof(chatact_t *)*(cfg.total_chatacts+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_chatacts+1);
			cfg.total_chatacts=0;
			bail(1);
            continue; }
		if(j)
			for(n=cfg.total_chatacts;n>chatnum[i];n--)
				cfg.chatact[n]=cfg.chatact[n-1];
		if((cfg.chatact[chatnum[i]]=(chatact_t *)malloc(sizeof(chatact_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(chatact_t));
			continue; }
		memset((chatact_t *)cfg.chatact[chatnum[i]],0,sizeof(chatact_t));
		strcpy(cfg.chatact[chatnum[i]]->cmd,cmd);
		strcpy(cfg.chatact[chatnum[i]]->out,out);
		cfg.chatact[chatnum[i]]->actset=setnum;
		cfg.total_chatacts++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		free(cfg.chatact[chatnum[i]]);
		cfg.total_chatacts--;
		for(j=chatnum[i];j<cfg.total_chatacts && j<MAX_OPTS;j++)
			cfg.chatact[j]=cfg.chatact[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savchatact=*cfg.chatact[chatnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.chatact[chatnum[i]]=savchatact;
		cfg.chatact[chatnum[i]]->actset=setnum;
		uifc.changes=1;
        continue; }
	SETHELP(WHERE);
/*
Chat Action Command:

This is the command that triggers this chat action.
*/
	strcpy(str,cfg.chatact[chatnum[i]]->cmd);
	if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Chat Action Command"
		,cfg.chatact[chatnum[i]]->cmd,LEN_CHATACTCMD,K_EDIT|K_UPPER)) {
		strcpy(cfg.chatact[chatnum[i]]->cmd,str);
		continue; }
	SETHELP(WHERE);
/*
Chat Action Output String:

This is the output string that results from this chat action.
*/
	strcpy(str,cfg.chatact[chatnum[i]]->out);
	if(!uifc.input(WIN_MID|WIN_SAV,0,10,""
		,cfg.chatact[chatnum[i]]->out,LEN_CHATACTOUT,K_EDIT|K_MSG))
		strcpy(cfg.chatact[chatnum[i]]->out,str); }
}

void guru_cfg()
{
	static int guru_dflt,guru_bar,opt_dflt;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static guru_t savguru;

while(1) {
	for(i=0;i<cfg.total_gurus && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.guru[i]->name);
	opt[i][0]=0;
	uifc.savnum=0;
	j=WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT;
	if(cfg.total_gurus)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_gurus<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savguru.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Gurus:

This is a list of the configured Gurus.

To add a Guru, select the desired location with the arrow keys and
hit  INS .

To delete a Guru, select it with the arrow keys and hit  DEL .

To configure a Guru, select it with the arrow keys and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&guru_dflt,&guru_bar,"Artificial Gurus",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Guru Name:

This is the name of the selected Guru.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Guru Name",str,25
			,0)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,' ');
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Guru Internal Code:

Every Guru must have its own unique code for Synchronet to refer to
it internally. This code is usually an abreviation of the Guru name.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code"
			,code,LEN_CODE,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.guru=(guru_t **)realloc(cfg.guru,sizeof(guru_t *)*(cfg.total_gurus+1)))
            ==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_gurus+1);
			cfg.total_gurus=0;
			bail(1);
            continue; }
		if(cfg.total_gurus)
			for(j=cfg.total_gurus;j>i;j--)
				cfg.guru[j]=cfg.guru[j-1];
		if((cfg.guru[i]=(guru_t *)malloc(sizeof(guru_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(guru_t));
			continue; }
		memset((guru_t *)cfg.guru[i],0,sizeof(guru_t));
		strcpy(cfg.guru[i]->name,str);
		strcpy(cfg.guru[i]->code,code);
		cfg.total_gurus++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		free(cfg.guru[i]);
		cfg.total_gurus--;
		for(j=i;j<cfg.total_gurus;j++)
			cfg.guru[j]=cfg.guru[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savguru=*cfg.guru[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.guru[i]=savguru;
		uifc.changes=1;
        continue; }
    j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Guru Name",cfg.guru[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Guru Internal Code",cfg.guru[i]->code);
		sprintf(opt[k++],"%-27.27s%.40s","Access Requirements",cfg.guru[i]->arstr);
		opt[k][0]=0;
		uifc.savnum=1;
		SETHELP(WHERE);
/*
Guru Configuration:

This menu is for configuring the selected Guru.
*/
		switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,cfg.guru[i]->name
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Guru Name:

This is the name of the selected Guru.
*/
				strcpy(str,cfg.guru[i]->name);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Guru Name"
					,cfg.guru[i]->name,sizeof(cfg.guru[i]->name)-1,K_EDIT))
					strcpy(cfg.guru[i]->name,str);
				break;
			case 1:
SETHELP(WHERE);
/*
Guru Internal Code:

Every Guru must have its own unique code for Synchronet to refer to
it internally. This code is usually an abreviation of the Guru name.
*/
				strcpy(str,cfg.guru[i]->code);
				if(!uifc.input(WIN_MID|WIN_SAV,0,0,"Guru Internal Code"
					,str,LEN_CODE,K_EDIT|K_UPPER))
					break;
				if(code_ok(str))
					strcpy(cfg.guru[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
                    uifc.helpbuf=0; }
				break;
			case 2:
				uifc.savnum=2;
				getar(cfg.guru[i]->name,cfg.guru[i]->arstr);
				break; } } }
}

void actsets_cfg()
{
    static int actset_dflt,actset_bar,opt_dflt;
    char str[81];
    int j,k,done;
    uint i;
    static actset_t savactset;

while(1) {
	for(i=0;i<cfg.total_actsets && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.actset[i]->name);
	opt[i][0]=0;
	j=WIN_ACT|WIN_RHT|WIN_BOT|WIN_SAV;
	uifc.savnum=0;
    if(cfg.total_actsets)
        j|=WIN_DEL|WIN_GET;
	if(cfg.total_actsets<MAX_OPTS)
        j|=WIN_INS|WIN_INSACT|WIN_XTR;
    if(savactset.name[0])
        j|=WIN_PUT;
    SETHELP(WHERE);
/*
Chat Action Sets:

This is a list of the configured action sets.

To add an action set, select the desired location with the arrow keys and
hit  INS .

To delete an action set, select it with the arrow keys and hit  DEL .

To configure an action set, select it with the arrow keys and hit
 ENTER .
*/
	i=uifc.list(j,0,0,45,&actset_dflt,&actset_bar,"Chat Action Sets",opt);
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
        SETHELP(WHERE);
/*
Chat Action Set Name:

This is the name of the selected chat action set.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Chat Action Set Name",str,25
			,0)<1)
            continue;
        if((cfg.actset=(actset_t **)realloc(cfg.actset,sizeof(actset_t *)*(cfg.total_actsets+1)))
            ==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_actsets+1);
			cfg.total_actsets=0;
			bail(1);
            continue; }
        if(cfg.total_actsets)
            for(j=cfg.total_actsets;j>i;j--)
                cfg.actset[j]=cfg.actset[j-1];
        if((cfg.actset[i]=(actset_t *)malloc(sizeof(actset_t)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(actset_t));
            continue; }
		memset((actset_t *)cfg.actset[i],0,sizeof(actset_t));
        strcpy(cfg.actset[i]->name,str);
        cfg.total_actsets++;
        uifc.changes=1;
        continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
        free(cfg.actset[i]);
        cfg.total_actsets--;
        for(j=i;j<cfg.total_actsets;j++)
            cfg.actset[j]=cfg.actset[j+1];
        uifc.changes=1;
        continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
        savactset=*cfg.actset[i];
        continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
        *cfg.actset[i]=savactset;
        uifc.changes=1;
        continue; }
    j=0;
    done=0;
    while(!done) {
        k=0;
        sprintf(opt[k++],"%-27.27s%s","Action Set Name",cfg.actset[i]->name);
        sprintf(opt[k++],"%-27.27s","Configure Chat Actions...");
		opt[k][0]=0;
        SETHELP(WHERE);
/*
Chat Action Set Configuration:

This menu is for configuring the selected chat action set.
*/
		sprintf(str,"%s Chat Action Set",cfg.actset[i]->name);
		uifc.savnum=1;
		switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&opt_dflt,0,str
            ,opt)) {
            case -1:
                done=1;
                break;
            case 0:
                SETHELP(WHERE);
/*
Chat Action Set Name:

This is the name of the selected action set.
*/
                strcpy(str,cfg.actset[i]->name);
				if(!uifc.input(WIN_MID|WIN_SAV,0,10,"Action Set Name"
                    ,cfg.actset[i]->name,sizeof(cfg.actset[i]->name)-1,K_EDIT))
                    strcpy(cfg.actset[i]->name,str);
                break;
            case 1:
                chatact_cfg(i);
                break; } } }
}

