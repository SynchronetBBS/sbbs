/* scfgsub.c */

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

void sub_cfg(uint grpnum)
{
	static int dflt,tog_dflt,opt_dflt,net_dflt,adv_dflt,bar;
	char str[81],str2[81],str3[11],done=0,code[9],*p;
	int j,m,n,ptridx,q,s;
	uint i,subnum[MAX_OPTS+1];
	static sub_t savsub;

while(1) {
	for(i=0,j=0;i<cfg.total_subs && j<MAX_OPTS;i++)
        if(cfg.sub[i]->grp==grpnum) {
			subnum[j]=i;
			if(cfg.sub[subnum[0]]->qwkconf)
				sprintf(opt[j],"%-5u %s"
					,cfg.sub[i]->qwkconf,cfg.sub[i]->lname);
			else
				sprintf(opt[j],"%s"
					,cfg.sub[i]->lname);
			j++; }
	subnum[j]=cfg.total_subs;
	opt[j][0]=0;
	sprintf(str,"%s Sub-boards",cfg.grp[grpnum]->sname);
	uifc.savnum=0;
	i=WIN_SAV|WIN_ACT;
	if(j)
		i|=WIN_DEL|WIN_GET|WIN_DELACT;
	if(j<MAX_OPTS)
		i|=WIN_INS|WIN_XTR|WIN_INSACT;
	if(savsub.sname[0])
		i|=WIN_PUT;
	SETHELP(WHERE);
/*
Message Sub-boards:

This is a list of message sub-boards that have been configured for the
selected message group.

To add a sub-board, select the desired position with the arrow keys and
hit  INS .

To delete a sub-board, select it with the arrow keys and hit  DEL .

To configure a sub-board, select it with the arrow keys and hit  ENTER .
*/
	i=uifc.list(i,24,1,LEN_SLNAME+5,&dflt,&bar,str,opt);
	uifc.savnum=1;
	if((signed)i==-1)
		return;
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"General");
		SETHELP(WHERE);
/*
Sub-board Long Name:

This is a description of the message sub-board which is displayed in all
sub-board listings.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Long Name",str,LEN_SLNAME
			,K_EDIT)<1)
			continue;
		sprintf(str2,"%.*s",LEN_SSNAME,str);
		SETHELP(WHERE);
/*
Sub-board Short Name:

This is a short description of the message sub-board which is displayed
at the main and reading messages prompts.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Short Name",str2,LEN_SSNAME
			,K_EDIT)<1)
			continue;
		sprintf(str3,"%.10s",str2);
		SETHELP(WHERE);
/*
Sub-board QWK Name:

This is the name of the sub-board used for QWK off-line readers.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board QWK Name",str3,10
            ,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str3);
		p=strchr(code,SP);
		if(p) *p=0;
		strupr(code);
		SETHELP(WHERE);
/*
Sub-board Internal Code:

Every sub-board must have its own unique code for Synchronet to refer to
it internally. This code should be descriptive of the sub-board's topic,
usually an abreviation of the sub-board's name.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Internal Code",code,8
			,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.savnum=0;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
			continue; }

		if((cfg.sub=(sub_t **)REALLOC(cfg.sub,sizeof(sub_t *)*(cfg.total_subs+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_subs+1);
			cfg.total_subs=0;
			bail(1);
            continue; }

		for(ptridx=0;ptridx>-1;ptridx++) { /* Search for unused pointer indx */
            for(n=0;n<cfg.total_subs;n++)
				if(cfg.sub[n]->ptridx==ptridx)
                    break;
            if(n==cfg.total_subs)
                break; }

		if(j) {
			for(n=cfg.total_subs;n>subnum[i];n--)
                cfg.sub[n]=cfg.sub[n-1];
			for(q=0;q<cfg.total_qhubs;q++)
				for(s=0;s<cfg.qhub[q]->subs;s++)
					if(cfg.qhub[q]->sub[s]>=subnum[i])
						cfg.qhub[q]->sub[s]++; }

		if((cfg.sub[subnum[i]]=(sub_t *)MALLOC(sizeof(sub_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(sub_t));
			continue; }
		memset((sub_t *)cfg.sub[subnum[i]],0,sizeof(sub_t));
		cfg.sub[subnum[i]]->grp=grpnum;
		if(cfg.total_faddrs)
			cfg.sub[subnum[i]]->faddr=cfg.faddr[0];
		else
			memset(&cfg.sub[subnum[i]]->faddr,0,sizeof(faddr_t));
		cfg.sub[subnum[i]]->maxmsgs=500;
		strcpy(cfg.sub[subnum[i]]->code_suffix,code);
		strcpy(cfg.sub[subnum[i]]->lname,str);
		strcpy(cfg.sub[subnum[i]]->sname,str2);
		strcpy(cfg.sub[subnum[i]]->qwkname,str3);
		cfg.sub[subnum[i]]->misc=(SUB_NSDEF|SUB_SSDEF|SUB_QUOTE|SUB_TOUSER
			|SUB_HDRMOD|SUB_FAST);
		cfg.sub[subnum[i]]->ptridx=ptridx;
		cfg.total_subs++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Delete Data in Sub-board:

If you want to delete all the messages for this sub-board, select Yes.
*/
		j=1;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
			,"Delete Data in Sub-board",opt);
		if(j==-1)
			continue;
		if(j==0) {
				sprintf(str,"%s%s.s*"
					,cfg.grp[cfg.sub[i]->grp]->code_prefix
					,cfg.sub[i]->code_suffix);
				strlwr(str);
				if(!cfg.sub[subnum[i]]->data_dir[0])
					sprintf(tmp,"%ssubs/",cfg.data_dir);
				else
					strcpy(tmp,cfg.sub[subnum[i]]->data_dir);
				delfiles(tmp,str);
				clearptrs(subnum[i]); 
		}
		FREE(cfg.sub[subnum[i]]);
		cfg.total_subs--;
		for(j=subnum[i];j<cfg.total_subs;j++)
			cfg.sub[j]=cfg.sub[j+1];
		for(q=0;q<cfg.total_qhubs;q++)
			for(s=0;s<cfg.qhub[q]->subs;s++) {
				if(cfg.qhub[q]->sub[s]==subnum[i])
					cfg.qhub[q]->sub[s]=INVALID_SUB;
				else if(cfg.qhub[q]->sub[s]>subnum[i])
					cfg.qhub[q]->sub[s]--; }
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savsub=*cfg.sub[subnum[i]];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		ptridx=cfg.sub[subnum[i]]->ptridx;
		*cfg.sub[subnum[i]]=savsub;
		cfg.sub[subnum[i]]->ptridx=ptridx;
		cfg.sub[subnum[i]]->grp=grpnum;
		uifc.changes=1;
        continue; }
	i=subnum[i];
	j=0;
	done=0;
	while(!done) {
		n=0;
		sprintf(opt[n++],"%-27.27s%s","Long Name",cfg.sub[i]->lname);
		sprintf(opt[n++],"%-27.27s%s","Short Name",cfg.sub[i]->sname);
		sprintf(opt[n++],"%-27.27s%s","QWK Name",cfg.sub[i]->qwkname);
		sprintf(opt[n++],"%-27.27s%s","Internal Code",cfg.sub[i]->code_suffix);
		sprintf(opt[n++],"%-27.27s%s","Newsgroup Name",cfg.sub[i]->newsgroup);
		sprintf(opt[n++],"%-27.27s%.40s","Access Requirements"
			,cfg.sub[i]->arstr);
		sprintf(opt[n++],"%-27.27s%.40s","Reading Requirements"
            ,cfg.sub[i]->read_arstr);
		sprintf(opt[n++],"%-27.27s%.40s","Posting Requirements"
			,cfg.sub[i]->post_arstr);
		sprintf(opt[n++],"%-27.27s%.40s","Operator Requirements"
			,cfg.sub[i]->op_arstr);
		sprintf(opt[n++],"%-27.27s%.40s","Moderated Posting User"
			,cfg.sub[i]->mod_arstr);
		sprintf(opt[n++],"%-27.27s%lu","Maximum Messages"
            ,cfg.sub[i]->maxmsgs);
		if(cfg.sub[i]->maxage)
            sprintf(str,"Enabled (%u days old)",cfg.sub[i]->maxage);
        else
            strcpy(str,"Disabled");
		sprintf(opt[n++],"%-27.27s%s","Purge by Age",str);
		if(cfg.sub[i]->maxcrcs)
			sprintf(str,"Enabled (%lu message CRCs)",cfg.sub[i]->maxcrcs);
		else
			strcpy(str,"Disabled");
		sprintf(opt[n++],"%-27.27s%s","Duplicate Checking",str);

		strcpy(opt[n++],"Toggle Options...");
		strcpy(opt[n++],"Network Options...");
		strcpy(opt[n++],"Advanced Options...");
		opt[n][0]=0;
		sprintf(str,"%s Sub-board",cfg.sub[i]->sname);
		uifc.savnum=1;
		SETHELP(WHERE);
/*
Sub-board Configuration:

This menu allows you to configure the individual selected sub-board.
Options with a trailing ... provide a sub-menu of more options.
*/
		switch(uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT
			,0,0,60,&opt_dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Sub-board Long Name:

This is a description of the message sub-board which is displayed in all
sub-board listings.
*/
				strcpy(str,cfg.sub[i]->lname);	/* save */
				if(!uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Listings"
					,cfg.sub[i]->lname,LEN_SLNAME,K_EDIT))
					strcpy(cfg.sub[i]->lname,str);	/* restore */
				break;
			case 1:
				SETHELP(WHERE);
/*
Sub-board Short Name:

This is a short description of the message sub-board which is displayed
at the main and reading messages prompts.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Prompts"
					,cfg.sub[i]->sname,LEN_SSNAME,K_EDIT);
				break;
			case 2:
				SETHELP(WHERE);
/*
Sub-board QWK Name:

This is the name of the sub-board used for QWK off-line readers.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for QWK Packets"
					,cfg.sub[i]->qwkname,10,K_EDIT);
                break;
			case 3:
                SETHELP(WHERE);
/*
Sub-board Internal Code:

Every sub-board must have its own unique code for Synchronet to refer
to it internally. This code should be descriptive of the sub-board's
topic, usually an abreviation of the sub-board's name.
*/
                strcpy(str,cfg.sub[i]->code_suffix);
                uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
                    ,str,8,K_EDIT|K_UPPER);
                if(code_ok(str))
                    strcpy(cfg.sub[i]->code_suffix,str);
                else {
                    uifc.helpbuf=invalid_code;
                    uifc.msg("Invalid Code");
                    uifc.helpbuf=0; }
                break;
			case 4:
				SETHELP(WHERE);
/*
Newsgroup Name:

This is the name of the sub-board used for newsgroup readers. If no name
is configured here, a name will be automatically generated from the
sub-board's name and group name.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Newsgroup"
					,cfg.sub[i]->newsgroup,50,K_EDIT);
                break;
			case 5:
				uifc.savnum=2;
				sprintf(str,"%s Access",cfg.sub[i]->sname);
				getar(str,cfg.sub[i]->arstr);
				break;
			case 6:
				uifc.savnum=2;
				sprintf(str,"%s Reading",cfg.sub[i]->sname);
				getar(str,cfg.sub[i]->read_arstr);
                break;
			case 7:
				uifc.savnum=2;
				sprintf(str,"%s Posting",cfg.sub[i]->sname);
				getar(str,cfg.sub[i]->post_arstr);
                break;
			case 8:
				uifc.savnum=2;
				sprintf(str,"%s Operator",cfg.sub[i]->sname);
				getar(str,cfg.sub[i]->op_arstr);
                break;
			case 9:
				uifc.savnum=2;
				sprintf(str,"%s Moderated Posting User",cfg.sub[i]->sname);
				getar(str,cfg.sub[i]->mod_arstr);
                break;
			case 10:
				sprintf(str,"%lu",cfg.sub[i]->maxmsgs);
                SETHELP(WHERE);
/*
Maximum Number of Messages:

This value is the maximum number of messages that will be kept in the
sub-board. Once this maximum number of messages is reached, the oldest
messages will be automatically purged. Usually, 100 messages is a
sufficient maximum.
*/
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of Messages"
                    ,str,5,K_EDIT|K_NUMBER);
                cfg.sub[i]->maxmsgs=atoi(str);
                cfg.sub[i]->misc|=SUB_HDRMOD;
				break;
			case 11:
				sprintf(str,"%u",cfg.sub[i]->maxage);
                SETHELP(WHERE);
/*
Maximum Age of Messages:

This value is the maximum number of days that messages will be kept in
the sub-board.
*/
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Age of Messages (in days)"
                    ,str,5,K_EDIT|K_NUMBER);
                cfg.sub[i]->maxage=atoi(str);
                cfg.sub[i]->misc|=SUB_HDRMOD;
				break;
			case 12:
				sprintf(str,"%lu",cfg.sub[i]->maxcrcs);
				SETHELP(WHERE);
/*
Maximum Number of CRCs:

This value is the maximum number of CRCs that will be kept in the
sub-board for duplicate message checking. Once this maximum number of
CRCs is reached, the oldest CRCs will be automatically purged.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of CRCs"
					,str,5,K_EDIT|K_NUMBER);
				cfg.sub[i]->maxcrcs=atol(str);
				cfg.sub[i]->misc|=SUB_HDRMOD;
                break;
			case 13:
				while(1) {
					n=0;
					sprintf(opt[n++],"%-27.27s%s","Allow Private Posts"
						,cfg.sub[i]->misc&SUB_PRIV ? cfg.sub[i]->misc&SUB_PONLY
						? "Only":"Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Allow Anonymous Posts"
						,cfg.sub[i]->misc&SUB_ANON ? cfg.sub[i]->misc&SUB_AONLY
						? "Only":"Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Post Using Real Names"
						,cfg.sub[i]->misc&SUB_NAME ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Users Can Edit Posts"
						,cfg.sub[i]->misc&SUB_EDIT ? cfg.sub[i]->misc&SUB_EDITLAST 
						? "Last" : "Yes" : "No");
					sprintf(opt[n++],"%-27.27s%s","Users Can Delete Posts"
						,cfg.sub[i]->misc&SUB_DEL ? cfg.sub[i]->misc&SUB_DELLAST
						? "Last" : "Yes" : "No");
					sprintf(opt[n++],"%-27.27s%s","Default On for New Scan"
						,cfg.sub[i]->misc&SUB_NSDEF ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Forced On for New Scan"
						,cfg.sub[i]->misc&SUB_FORCED ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Default On for Your Scan"
						,cfg.sub[i]->misc&SUB_SSDEF ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Public 'To' User"
						,cfg.sub[i]->misc&SUB_TOUSER ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Allow Message Quoting"
						,cfg.sub[i]->misc&SUB_QUOTE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Suppress User Signatures"
						,cfg.sub[i]->misc&SUB_NOUSERSIG ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Permanent Operator Msgs"
						,cfg.sub[i]->misc&SUB_SYSPERM ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Kill Read Messages"
						,cfg.sub[i]->misc&SUB_KILL ? "Yes"
						: (cfg.sub[i]->misc&SUB_KILLP ? "Pvt" : "No"));
					sprintf(opt[n++],"%-27.27s%s","Compress Messages (LZH)"
						,cfg.sub[i]->misc&SUB_LZH ? "Yes" : "No");

					opt[n][0]=0;
					uifc.savnum=2;
					SETHELP(WHERE);
/*
Sub-board Toggle Options:

This menu allows you to toggle certain options for the selected
sub-board between two or more settings, such as Yes and No.
*/
					n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,36,&tog_dflt,0
						,"Toggle Options",opt);
					if(n==-1)
						break;
					uifc.savnum=3;
					switch(n) {
						case 0:
							if(cfg.sub[i]->misc&SUB_PONLY)
								n=2;
							else 
								n=(cfg.sub[i]->misc&SUB_PRIV) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Only");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Allow Private Posts on Sub-board:

If you want users to be able to post private messages to other users
on this sub-board, set this value to Yes. Usually, E-mail is the
preferred method of private communication. If you want users to be able
to post private messages only on this sub-board, select Only.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Allow Private Posts",opt);
							if(n==-1)
								break;
							if(!n && (cfg.sub[i]->misc&(SUB_PRIV|SUB_PONLY))
								!=SUB_PRIV) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_PONLY;
								cfg.sub[i]->misc|=SUB_PRIV;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_PRIV) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_PRIV;
								break; }
							if(n==2 && (cfg.sub[i]->misc&(SUB_PRIV|SUB_PONLY))
								!=(SUB_PRIV|SUB_PONLY)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=(SUB_PRIV|SUB_PONLY); }
							break;
						case 1:
							if(cfg.sub[i]->misc&SUB_AONLY)
								n=2;
							else 
								n=(cfg.sub[i]->misc&SUB_ANON) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Only");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Allow Anonymous Posts on Sub-board:

If you want users with the A exemption to be able to post anonymously on
this sub-board, select Yes. If you want all posts on this sub-board to be
forced anonymous, select Only. If you do not want anonymous posts allowed
on this sub-board at all, select No.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Allow Anonymous Posts",opt);
							if(n==-1)
								break;
							if(!n && (cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY))
								!=SUB_ANON) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_AONLY;
								cfg.sub[i]->misc|=SUB_ANON;
								break; }
							if(n==1 && cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY)) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~(SUB_ANON|SUB_AONLY);
								break; }
							if(n==2 && (cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY))
								!=(SUB_ANON|SUB_AONLY)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=(SUB_ANON|SUB_AONLY); }
                            break;
						case 2:
							n=(cfg.sub[i]->misc&SUB_NAME) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
User Real Names in Posts on Sub-board:

If you allow aliases on your system, you can have messages on this
sub-board automatically use the real name of the posting user by setting
this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Use Real Names in Posts",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_NAME)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_NAME;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_NAME) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_NAME; }
							break;
						case 3:
							if(cfg.sub[i]->misc&SUB_EDITLAST)
								n=2;
							else
								n=(cfg.sub[i]->misc&SUB_EDIT) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Last Post Only");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Users Can Edit Posts on Sub-board:

If you wish to allow users to edit their messages after they have been
posted, this option to Yes. If you wish to allow users to edit only the
last message on a message base, set this option to Last Post Only.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Users Can Edit Messages",opt);
							if(n==-1)
                                break;
							if(n==0 /* yes */
								&& (cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST))
								!=SUB_EDIT
								) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_EDIT;
								cfg.sub[i]->misc&=~SUB_EDITLAST;
								break; 
							}
							if(n==1 /* no */
								&& cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST)
								) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~(SUB_EDIT|SUB_EDITLAST);
								break;
							}
							if(n==2 /* last only */
								&& (cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST))
								!=(SUB_EDIT|SUB_EDITLAST)
								) {
								uifc.changes=1;
								cfg.sub[i]->misc|=(SUB_EDIT|SUB_EDITLAST);
								break;
							}
                            break;
						case 4:
							if(cfg.sub[i]->misc&SUB_DELLAST)
								n=2;
							else 
								n=(cfg.sub[i]->misc&SUB_DEL) ? 0:1;

							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Last Post Only");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Users Can Delete Posts on Sub-board:

If you want users to be able to delete any of their own posts on this
sub-board, set this option to Yes. If you want to allow users the
ability to delete their message only if it is the last message on the
sub-board, select Last Post Only. If you want to disallow users from
deleting any of their posts, set this option to No.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Users Can Delete Posts",opt);
							if(n==-1)
								break;
							if(!n && (cfg.sub[i]->misc&(SUB_DEL|SUB_DELLAST))
								!=SUB_DEL) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_DELLAST;
								cfg.sub[i]->misc|=SUB_DEL;
								break; 
							}
							if(n==1 && cfg.sub[i]->misc&SUB_DEL) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_DEL;
								break; 
							}
							if(n==2 && (cfg.sub[i]->misc&(SUB_DEL|SUB_DELLAST))
								!=(SUB_DEL|SUB_DELLAST)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=(SUB_DEL|SUB_DELLAST); 
							}
                            break;
						case 5:
							n=(cfg.sub[i]->misc&SUB_NSDEF) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Default On for New Scan:

If you want this sub-board to be included in all user new message scans
by default, set this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Default On for New Scan",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_NSDEF)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_NSDEF;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_NSDEF) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_NSDEF; }
                            break;
						case 6:
							n=(cfg.sub[i]->misc&SUB_FORCED) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Forced On for New Scan:

If you want this sub-board to be included in all user new message scans
even if the user has removed it from their new scan configuration, set
this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Forced New Scan",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_FORCED)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_FORCED;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_FORCED) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_FORCED; }
                            break;
						case 7:
							n=(cfg.sub[i]->misc&SUB_SSDEF) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Default On for Your Scan:

If you want this sub-board to be included in all user personal message
scans by default, set this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Default On for Your Scan",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_SSDEF)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_SSDEF;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_SSDEF) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_SSDEF; }
                            break;
						case 8:
							n=(cfg.sub[i]->misc&SUB_TOUSER) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Prompt for 'To' User on Public Posts:

If you want all posts on this sub-board to be prompted for a 'To' user,
set this option to Yes. This is a useful option for sub-boards that
are on a network that does not allow private posts.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Prompt for 'To' User on Public Posts",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_TOUSER)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_TOUSER;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_TOUSER) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_TOUSER; }
							break;
						case 9:
							n=(cfg.sub[i]->misc&SUB_QUOTE) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Allow Message Quoting:

If you want users to be allowed to quote messages on this sub-board, set
this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Allow Message Quoting",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_QUOTE)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_QUOTE;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_QUOTE) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_QUOTE; }
                            break;
						case 10:
							n=(cfg.sub[i]->misc&SUB_NOUSERSIG) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Suppress User Signatures:

If you do not wish to have user signatures automatically appended to
messages posted in this sub-board, set this option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Suppress User Signatures",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_NOUSERSIG)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_NOUSERSIG;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_NOUSERSIG) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_NOUSERSIG; }
                            break;
						case 11:
							n=(cfg.sub[i]->misc&SUB_SYSPERM) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Operator Messages Automatically Permanent:

If you want messages posted by System and Sub-board Operators to be
automatically permanent (non-purgable) for this sub-board, set this
option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Permanent Operator Messages",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_SYSPERM)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_SYSPERM;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_SYSPERM) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_SYSPERM; }
                            break;
						case 12:
							if(cfg.sub[i]->misc&SUB_KILLP)
								n=2;
							else
								n=(cfg.sub[i]->misc&SUB_KILL) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							strcpy(opt[2],"Private");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Kill Read Messages Automatically:

If you want messages that have been read by the intended recipient to
be automatically deleted by SMBUTIL, set this option to Yes or
Private if you want only private messages to be automatically deleted.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Kill Read Messages",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_KILL)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_KILL;
								cfg.sub[i]->misc&=~SUB_KILLP;
								break; }
							if(n==1 && cfg.sub[i]->misc&(SUB_KILL|SUB_KILLP)) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~(SUB_KILL|SUB_KILLP); }
							if(n==2 && !(cfg.sub[i]->misc&SUB_KILLP)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_KILLP;
								cfg.sub[i]->misc&=~SUB_KILL;
                                break; }
                            break;
						case 13:
							n=(cfg.sub[i]->misc&SUB_LZH) ? 0:1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Compress Messages with LZH Encoding:

If you want all messages in this sub-board to be automatically
compressed via LZH (Lempel/Ziv/Huffman algorithm used in LHarc, LHA,
and other popular compression and archive programs), this option to Yes.

Compression will slow down the reading and writing of messages slightly,
but the storage space saved can be as much as 50 percent.

Before setting this option to Yes, make sure that all of the SMB
compatible mail programs you use support the LZH translation.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Compress Messages (LZH)",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_LZH)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_LZH;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_LZH) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_LZH; }
                            break;

							} }
				break;
			case 14:
				while(1) {
					n=0;
					sprintf(opt[n++],"%-27.27s%s","Append Tag/Origin Line"
						,cfg.sub[i]->misc&SUB_NOTAG ? "No":"Yes");
					sprintf(opt[n++],"%-27.27s%s","Export ASCII Only"
						,cfg.sub[i]->misc&SUB_ASCII ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","Gate Between Net Types"
						,cfg.sub[i]->misc&SUB_GATE ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","QWK Networked"
						,cfg.sub[i]->misc&SUB_QNET ? "Yes":"No");
					sprintf(opt[n++],"QWK Tagline");
					sprintf(opt[n++],"%-27.27s%s","Internet (UUCP/NNTP)"
						,cfg.sub[i]->misc&SUB_INET ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","PostLink or PCRelay"
                        ,cfg.sub[i]->misc&SUB_PNET ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","FidoNet EchoMail"
						,cfg.sub[i]->misc&SUB_FIDO ? "Yes":"No");
					sprintf(opt[n++],"%-27.27s%s","FidoNet Address"
                        ,faddrtoa(&cfg.sub[i]->faddr,tmp));
					sprintf(opt[n++],"EchoMail Origin Line");
					opt[n][0]=0;
					uifc.savnum=2;
					SETHELP(WHERE);
/*
Sub-board Network Options:

This menu contains options for the selected sub-board that pertain
specifically to message networking.
*/
					n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,60,&net_dflt,0
						,"Network Options",opt);
					if(n==-1)
						break;
					uifc.savnum=3;
                    switch(n) {
						case 0:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Append Tag/Origin Line to Posts:

If you want to disable the automatic addition of a network tagline or
origin line to the bottom of outgoing networked posts from this
sub-board, set this option to No.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Append Tag/Origin Line to Posts",opt);
							if(n==-1)
                                break;
							if(!n && cfg.sub[i]->misc&SUB_NOTAG) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_NOTAG;
								break; }
							if(n==1 && !(cfg.sub[i]->misc&SUB_NOTAG)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_NOTAG; }
                            break;
						case 1:
							n=0;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Export ASCII Characters Only:

If the network that this sub-board is echoed on does not allow extended
ASCII (>127) or control codes (<20, not including CR), set this option
to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Export ASCII Characters Only",opt);
							if(n==-1)
                                break;
							if(n && cfg.sub[i]->misc&SUB_ASCII) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_ASCII;
								break; }
							if(!n && !(cfg.sub[i]->misc&SUB_ASCII)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_ASCII; }
                            break;
						case 2:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Gate Between Net Types:

If this sub-board is networked using more than one network technology,
and you want messages to be gated between the networks, set this
option to Yes.

If this option is set to No, messages imported from one network type
will not be exported to another network type. This is the default and
should be used unless you have specific permission from both networks
to gate this sub-board. Incorrectly gated sub-boards can cause duplicate
messages loops and cross-posted messages.

This option does not affect the exporting of messages created on your
BBS.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Gate Between Net Types",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_GATE)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_GATE;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_GATE) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_GATE; }
                            break;
						case 3:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Sub-board Networked via QWK Packets:

If this sub-board is networked with other BBSs via QWK packets, this
option should be set to Yes. With this option set to Yes, titles of
posts on this sub-board will be limited to the QWK packet limitation of
25 characters. It also allows the Network restriction to function
properly.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Networked via QWK Packets",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_QNET)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_QNET;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_QNET) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_QNET; }
                            break;
						case 4:
							SETHELP(WHERE);
/*
Sub-board QWK Network Tagline:

If you want to use a different QWK tagline than the configured default
tagline in the Networks configuration, you should enter that tagline
here. If this option is left blank, the default tagline is used.
*/
							uifc.input(WIN_MID|WIN_SAV,0,0,nulstr,cfg.sub[i]->tagline
								,63,K_MSG|K_EDIT);
							break;
						case 5:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Sub-board Networked via Internet:

If this sub-board is networked to the Internet via UUCP or NNTP, this
option should be set to Yes.

It will allow the Network user restriction to function properly.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Networked via Internet",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_INET)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_INET;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_INET) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_INET; }
                            break;
						case 6:
                            n=1;
                            strcpy(opt[0],"Yes");
                            strcpy(opt[1],"No");
                            opt[2][0]=0;
                            SETHELP(WHERE);
/*
Sub-board Networked via PostLink or PCRelay:

If this sub-board is networked with other BBSs via PostLink or PCRelay,
this option should be set to Yes. With this option set to Yes,
titles of posts on this sub-board will be limited to the UTI
specification limitation of 25 characters. It also allows the Network
restriction to function properly.
*/
                            n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
                                ,"Networked via PostLink or PCRelay",opt);
                            if(n==-1)
                                break;
                            if(!n && !(cfg.sub[i]->misc&SUB_PNET)) {
                                uifc.changes=1;
                                cfg.sub[i]->misc|=SUB_PNET;
                                break; }
                            if(n==1 && cfg.sub[i]->misc&SUB_PNET) {
                                uifc.changes=1;
                                cfg.sub[i]->misc&=~SUB_PNET; }
                            break;
						case 7:
							n=1;
							strcpy(opt[0],"Yes");
							strcpy(opt[1],"No");
							opt[2][0]=0;
							SETHELP(WHERE);
/*
Sub-board Networked via FidoNet EchoMail:

If this sub-board is part of a FidoNet EchoMail conference, set this
option to Yes.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Networked via FidoNet EchoMail",opt);
							if(n==-1)
                                break;
							if(!n && !(cfg.sub[i]->misc&SUB_FIDO)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_FIDO;
								break; }
							if(n==1 && cfg.sub[i]->misc&SUB_FIDO) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~SUB_FIDO; }
                            break;
						case 8:
							faddrtoa(&cfg.sub[i]->faddr,str);
							SETHELP(WHERE);
/*
Sub-board FidoNet Address:

If this sub-board is part of a FidoNet EchoMail conference, this is
the address used for this sub-board. Format: Zone:Net/Node[.Point]
*/
							uifc.input(WIN_MID|WIN_SAV,0,0,"FidoNet Address"
								,str,20,K_EDIT);
							cfg.sub[i]->faddr=atofaddr(str);
							break;
						case 9:
							SETHELP(WHERE);
/*
Sub-board FidoNet Origin Line:

If this sub-board is part of a FidoNet EchoMail conference and you
want to use an origin line other than the default origin line in the
Networks configuration, set this value to the desired origin line.

If this option is blank, the default origin line is used.
*/
							uifc.input(WIN_MID|WIN_SAV,0,0,nulstr,cfg.sub[i]->origline
								,50,K_EDIT);
                            break;
					} 
				}
				break;
			case 15:
				while(1) {
					n=0;
					if(cfg.sub[i]->qwkconf)
						sprintf(str,"Static (%u)",cfg.sub[i]->qwkconf);
					else
						strcpy(str,"Dynamic");
					sprintf(opt[n++],"%-27.27s%s","QWK Conference Number"
						,str);
					sprintf(opt[n++],"%-27.27s%s","Storage Method"
						,cfg.sub[i]->misc&SUB_HYPER ? "Hyper Allocation"
						: cfg.sub[i]->misc&SUB_FAST ? "Fast Allocation"
						: "Self-packing");
					if(!cfg.sub[i]->data_dir[0])
						sprintf(str,"%ssubs/",cfg.data_dir);
					else
						strcpy(str,cfg.sub[i]->data_dir);
					sprintf(opt[n++],"%-27.27s%.40s","Storage Directory",str);
					sprintf(opt[n++],"%-27.27s%.40s","Semaphore File",cfg.sub[i]->post_sem);
					opt[n][0]=0;
					uifc.savnum=2;
					SETHELP(WHERE);
/*
Sub-board Advanced Options:

This menu contains options for the selected sub-board that are advanced
in nature.
*/
					n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,60,&adv_dflt,0
						,"Advanced Options",opt);
					if(n==-1)
						break;
					uifc.savnum=3;
                    switch(n) {
                        case 0:
							SETHELP(WHERE);
/*
Sub-board QWK Conference Number:

If you wish to have the QWK conference number for this sub-board
automatically generated by Synchronet (based on the group number
and sub-board number for the user), set this option to Dynamic.

If you wish to have the same QWK conference number for this sub-board
regardless of which user access it, set this option to Static
by entering the conference number you want to use.
*/
							if(cfg.sub[i]->qwkconf)
								sprintf(str,"%u",cfg.sub[i]->qwkconf);
							else
								str[0]=0;
							if(uifc.input(WIN_MID|WIN_SAV,0,17
								,"QWK Conference Number (0=Dynamic)"
								,str,5,K_EDIT|K_NUMBER)>=0)
								cfg.sub[i]->qwkconf=atoi(str);
							break;
						case 1:
							n=0;
							strcpy(opt[0],"Hyper Allocation");
							strcpy(opt[1],"Fast Allocation");
							strcpy(opt[2],"Self-packing");
							opt[3][0]=0;
							SETHELP(WHERE);
/*
Self-Packing is the slowest storage method because it conserves disk
  space as it imports messages by using deleted message header and data
  blocks for new messages automatically. If you use this storage method,
  you will not need to run SMBUTIL P on this message base unless you
  accumilate a large number of deleted message blocks and wish to free
  that disk space. You can switch from self-packing to fast allocation
  storage method and back again as you wish.
Fast Allocation is faster than self-packing because it does not search
  for deleted message blocks for new messages. It automatically places
  all new message blocks at the end of the header and data files. If you
  use this storage method, you will need to run SMBUTIL P on this
  message base periodically or it will continually use up disk space.
Hyper Allocation is the fastest storage method because it does not
  maintain allocation files at all. Once a message base is setup to use
  this storage method, it should not be changed without first deleting
  the message base data files in your DATA\DIRS\SUBS directory for this
  sub-board. You must use SMBUTIL P as with the fast allocation method.
*/
							n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
								,"Storage Method",opt);
							if(n==-1)
								break;
							if(!n && !(cfg.sub[i]->misc&SUB_HYPER)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_HYPER;
								cfg.sub[i]->misc&=~SUB_FAST;
								cfg.sub[i]->misc|=SUB_HDRMOD;
								break; }
							if(!n)
								break;
							if(cfg.sub[i]->misc&SUB_HYPER) {	/* Switching from hyper */
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No, I want to use Hyper Allocation");
								opt[2][0]=0;
								m=0;
								if(uifc.list(WIN_SAV|WIN_MID,0,0,0,&m,0
									,"Delete all messages in this sub-board?",opt)!=0)
									break;
								if(cfg.sub[i]->data_dir[0])
									sprintf(str,"%s",cfg.sub[i]->data_dir);
								else
									sprintf(str,"%ssubs/",cfg.data_dir);
								sprintf(str2,"%s%s.s*"
									,cfg.grp[cfg.sub[i]->grp]->code_prefix
									,cfg.sub[i]->code_suffix);
								strlwr(str2);
								delfiles(str,str2); 
							}

							if(cfg.sub[i]->misc&SUB_HYPER)
								cfg.sub[i]->misc|=SUB_HDRMOD;
							if(n==1 && !(cfg.sub[i]->misc&SUB_FAST)) {
								uifc.changes=1;
								cfg.sub[i]->misc|=SUB_FAST;
								cfg.sub[i]->misc&=~SUB_HYPER;
								break; 
							}
							if(n==2 && cfg.sub[i]->misc&(SUB_FAST|SUB_HYPER)) {
								uifc.changes=1;
								cfg.sub[i]->misc&=~(SUB_FAST|SUB_HYPER);
								break; 
							}
							break;
						case 2:
							SETHELP(WHERE);
/*
Sub-board Storage Directory:

Use this if you wish to place the data directory for this sub-board on
another drive or in another directory besides the default setting.
*/
							uifc.input(WIN_MID|WIN_SAV,0,17,"Directory"
								,cfg.sub[i]->data_dir,50,K_EDIT);
							break; 
						case 3:
SETHELP(WHERE);
/*
`Sub-board Semaphore File:`

This is a filename that will be created as a semaphore (signal) to an
external program or event whenever a message is posted in this sub-board.
*/
							uifc.input(WIN_MID|WIN_SAV,0,17,"Semaphore File"
								,cfg.sub[i]->post_sem,50,K_EDIT);
							break; 
					} 
				}
				break;
			} 
		} 
	}
}
