/* un_rep.cpp */

/* Synchronet QWK replay (REP) packet unpacking routine */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "sbbs.h"
#include "qwk.h"

/****************************************************************************/
/* Unpacks .REP packet, 'repname' is the path and filename of the packet    */
/****************************************************************************/
bool sbbs_t::unpack_rep(char* repfile)
{
	char	str[MAX_PATH+1],fname[MAX_PATH+1]
			,*AttemptedToUploadREPpacket="Attempted to upload REP packet";
	char 	tmp[512];
	char	inbox[MAX_PATH+1];
	char	block[QWK_BLOCK_LEN];
	int 	file;
	uint	i,j,k,lastsub=INVALID_SUB;
	long	l,size,misc;
	ulong	n;
	ulong	ex;
	node_t	node;
	FILE*	rep;
	DIR*	dir;
	DIRENT*	dirent;
	BOOL	twit_list;

	sprintf(fname,"%stwitlist.cfg",cfg.ctrl_dir);
	twit_list=fexist(fname);

	if(repfile!=NULL)
		strcpy(str,repfile);
	else
		sprintf(str,"%s%s.rep",cfg.temp_dir,cfg.sys_id);
	if(!fexistcase(str)) {
		bputs(text[QWKReplyNotReceived]);
		logline("U!",AttemptedToUploadREPpacket);
		logline(nulstr,"REP file not received");
		return(false); 
	}
	for(k=0;k<cfg.total_fextrs;k++)
		if(!stricmp(cfg.fextr[k]->ext,useron.tmpext) && chk_ar(cfg.fextr[k]->ar,&useron))
			break;
	if(k>=cfg.total_fextrs)
		k=0;
	ex=EX_OUTL|EX_OUTR;
	if(online!=ON_REMOTE)
		ex|=EX_OFFLINE;
	i=external(cmdstr(cfg.fextr[k]->cmd,str,ALLFILES,NULL),ex);
	if(i) {
		bputs(text[QWKExtractionFailed]);
		logline("U!",AttemptedToUploadREPpacket);
		logline(nulstr,"Extraction failed");
		return(false); 
	}
	sprintf(str,"%s%s.msg",cfg.temp_dir,cfg.sys_id);
	if(!fexistcase(str)) {
		bputs(text[QWKReplyNotReceived]);
		logline("U!",AttemptedToUploadREPpacket);
		logline(nulstr,"MSG file not received");
		return(false); 
	}
	if((rep=fnopen(&file,str,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return(false); 
	}
	size=filelength(file);
	fread(block,QWK_BLOCK_LEN,1,rep);
	if(strnicmp((char *)block,cfg.sys_id,strlen(cfg.sys_id))) {
		fclose(rep);
		bputs(text[QWKReplyNotReceived]);
		logline("U!",AttemptedToUploadREPpacket);
		logline(nulstr,"Incorrect BBSID");
		return(false); 
	}
	logline("U+","Uploaded REP packet");
	/********************/
	/* Process messages */
	/********************/
	bputs(text[QWKUnpacking]);

	for(l=QWK_BLOCK_LEN;l<size;l+=i*QWK_BLOCK_LEN) {
		lncntr=0;					/* defeat pause */
		if(fseek(rep,l,SEEK_SET)!=0) {
			sprintf(str,"%s.msg", cfg.sys_id);
			errormsg(WHERE,ERR_SEEK,str,l);
			break;
		}
		if(fread(block,1,QWK_BLOCK_LEN,rep)!=QWK_BLOCK_LEN) {
			sprintf(str,"%s.msg", cfg.sys_id);
			errormsg(WHERE,ERR_READ,str,ftell(rep));
			break;
		}
		sprintf(tmp,"%.6s",block+116);
		i=atoi(tmp);  /* i = number of blocks */
		if(i<2) {
			sprintf(str,"%s.msg blocks (read '%s' at offset %ld)", cfg.sys_id, tmp, l);
			errormsg(WHERE,ERR_CHK,str,i);
			i=1;
			continue; }
		if(atoi(block+1)==0) {					/**********/
			if(useron.rest&FLAG('E')) {         /* E-mail */
				bputs(text[R_Email]);			/**********/
				continue; }

			sprintf(str,"%25.25s",block+21);
			truncsp(str);
			if(!stricmp(str,"NETMAIL")) {  /* QWK to FidoNet NetMail */
				qwktonetmail(rep,block,NULL,0);
				continue; }
			if(strchr(str,'@')) {
				qwktonetmail(rep,block,str,0);
				continue; }
			if(!stricmp(str,"SBBS")) {    /* to SBBS, config stuff */
				qwkcfgline(block+71,INVALID_SUB);
				continue; }

			if(useron.etoday>=cfg.level_emailperday[useron.level]
				&& !(useron.rest&FLAG('Q'))) {
				bputs(text[TooManyEmailsToday]);
				continue; }
			j=atoi(str);
			if(j && j>lastuser(&cfg))
				j=0;
			if(!j)
				j=matchuser(&cfg,str,TRUE /* sysop_alias */);
			if(!j) {
				bputs(text[UnknownUser]);
				continue; }
			if(j==1 && useron.rest&FLAG('S')) {
				bprintf(text[R_Feedback],cfg.sys_op);
				continue; }

			getuserrec(&cfg,j,U_MISC,8,str);
			misc=ahtoul(str);
			if(misc&NETMAIL && cfg.sys_misc&SM_FWDTONET) {
				getuserrec(&cfg,j,U_NETMAIL,LEN_NETMAIL,str);
				qwktonetmail(rep,block,str,0);
				continue; }

			sprintf(smb.file,"%smail",cfg.data_dir);
			smb.retry_time=cfg.smb_retry_time;

			if(lastsub!=INVALID_SUB) {
				smb_close(&smb);
				lastsub=INVALID_SUB; }

			smb.subnum=INVALID_SUB;
			if((k=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
				continue; }

			if(!filelength(fileno(smb.shd_fp))) {
				smb.status.max_crcs=cfg.mail_maxcrcs;
				smb.status.max_msgs=MAX_SYSMAIL;
				smb.status.max_age=cfg.mail_maxage;
				smb.status.attr=SMB_EMAIL;
				if((k=smb_create(&smb))!=0) {
					smb_close(&smb);
					errormsg(WHERE,ERR_CREATE,smb.file,k);
					continue; } }

			if((k=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_LOCK,smb.file,k);
				continue; }

			if((k=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_READ,smb.file,k);
				continue; }

			smb_unlocksmbhdr(&smb);

			if(!qwktomsg(rep,block,0,INVALID_SUB,j)) {
				smb_close(&smb);
				continue; }
			smb_close(&smb);

			if(j==1) {
				useron.fbacks++;
				logon_fbacks++;
				putuserrec(&cfg,useron.number,U_FBACKS,5
					,ultoa(useron.fbacks,tmp,10)); }
			else {
				useron.emails++;
				logon_emails++;
				putuserrec(&cfg,useron.number,U_EMAILS,5
					,ultoa(useron.emails,tmp,10)); }
			useron.etoday++;
			putuserrec(&cfg,useron.number,U_ETODAY,5
				,ultoa(useron.etoday,tmp,10));
			bprintf(text[Emailed],username(&cfg,j,tmp),j);
			sprintf(str,"%s sent e-mail to %s #%d"
				,useron.alias,username(&cfg,j,tmp),j);
			logline("E+",str);
			if(useron.rest&FLAG('Q')) {
				sprintf(tmp,"%-25.25s",block+46);
				truncsp(tmp); }
			else
				strcpy(tmp,useron.alias);
			for(k=1;k<=cfg.sys_nodes;k++) { /* Tell user, if online */
				getnodedat(k,&node,0);
				if(node.useron==j && !(node.misc&NODE_POFF)
					&& (node.status==NODE_INUSE
					|| node.status==NODE_QUIET)) {
					sprintf(str,text[EmailNodeMsg]
						,cfg.node_num,tmp);
					putnmsg(&cfg,k,str);
					break; } }
			if(k>cfg.sys_nodes) {
				sprintf(str,text[UserSentYouMail],tmp);
				putsmsg(&cfg,j,str); } }    /* end of email */

				/**************************/
		else {	/* message on a sub-board */
				/**************************/
			n=atol((char *)block+1); /* conference number */
			for(j=0;j<usrgrps;j++) {
				for(k=0;k<usrsubs[j];k++)
					if(cfg.sub[usrsub[j][k]]->qwkconf==n)
						break;
				if(k<usrsubs[j])
					break; }

			if(j>=usrgrps) {
				if(n<1000) {			 /* version 1 method, start at 101 */
					j=n/100;
					k=n-(j*100); }
				else {					 /* version 2 method, start at 1001 */
					j=n/1000;
					k=n-(j*1000); }
				j--;	/* j is group */
				k--;	/* k is sub */
				if(j>=usrgrps || k>=usrsubs[j] || cfg.sub[usrsub[j][k]]->qwkconf) {
					bprintf(text[QWKInvalidConferenceN],n);
					sprintf(str,"%s: Invalid conference number %lu",useron.alias,n);
					logline("P!",str);
					continue; } }

			n=usrsub[j][k];

			/* if posting, add to new-scan config for QWKnet nodes automatically */
			if(useron.rest&FLAG('Q'))
				subscan[n].cfg|=SUB_CFG_NSCAN;

			sprintf(str,"%-25.25s","SBBS");
			if(!strnicmp((char *)block+21,str,25)) {	/* to SBBS, config stuff */
				qwkcfgline((char *)block+71,n);
				continue; }

			if(!SYSOP && cfg.sub[n]->misc&SUB_QNET) {	/* QWK Netted */
				sprintf(str,"%-25.25s","DROP");         /* Drop from new-scan? */
				if(!strnicmp((char *)block+71,str,25))	/* don't allow post */
					continue;
				sprintf(str,"%-25.25s","ADD");          /* Add to new-scan? */
				if(!strnicmp((char *)block+71,str,25))	/* don't allow post */
					continue; }

			if(useron.rest&FLAG('Q') && !(cfg.sub[n]->misc&SUB_QNET)) {
				bputs(text[CantPostOnSub]);
				logline("P!","Attempted to post on non-QWKnet sub");
				continue; }

			if(useron.rest&FLAG('P')) {
				bputs(text[R_Post]);
				logline("P!","Post attempted");
				continue; }

			if(useron.ptoday>=cfg.level_postsperday[useron.level]
				&& !(useron.rest&FLAG('Q'))) {
				bputs(text[TooManyPostsToday]);
				continue; }

			if(useron.rest&FLAG('N')
				&& cfg.sub[n]->misc&(SUB_FIDO|SUB_PNET|SUB_QNET|SUB_INET)) {
				bputs(text[CantPostOnSub]);
				logline("P!","Networked post attempted");
				continue; }

			if(!chk_ar(cfg.sub[n]->post_ar,&useron)) {
				bputs(text[CantPostOnSub]);
				logline("P!","Post attempted");
				continue; }

			if((block[0]=='*' || block[0]=='+')
				&& !(cfg.sub[n]->misc&SUB_PRIV)) {
				bputs(text[PrivatePostsNotAllowed]);
				logline("P!","Private post attempt");
				continue; }

			if(block[0]=='*' || block[0]=='+'           /* Private post */
				|| cfg.sub[n]->misc&SUB_PONLY) {
				sprintf(str,"%-25.25s",nulstr);
				sprintf(tmp,"%-25.25s","ALL");
				if(!strnicmp((char *)block+21,str,25)
					|| !strnicmp((char *)block+21,tmp,25)) {	/* to blank */
					bputs(text[NoToUser]);						/* or all */
					continue; } }

			if(!SYSOP && !(useron.rest&FLAG('Q'))) {
				sprintf(str,"%-25.25s","SYSOP");
				if(!strnicmp((char *)block+21,str,25)) {
					sprintf(str,"%-25.25s",username(&cfg,1,tmp));
					memcpy((char *)block+21,str,25); } }	/* change from sysop */
															/* to user name */

			/* TWIT FILTER */
			if(twit_list) {
				sprintf(fname,"%stwitlist.cfg",cfg.ctrl_dir);
				sprintf(str,"%25.25s",block+46);  /* From user */
				truncsp(str);

				if(findstr(str,fname)) {
					sprintf(str,"Filtering post from twit (%s) on %s %s"
						,str,cfg.grp[cfg.sub[n]->grp]->sname,cfg.sub[n]->lname);
					logline("P!",str);
					continue; 
				}
			}

			if(n!=lastsub) {
				if(lastsub!=INVALID_SUB)
					smb_close(&smb);
				lastsub=INVALID_SUB;
				sprintf(smb.file,"%s%s",cfg.sub[n]->data_dir,cfg.sub[n]->code);
				smb.retry_time=cfg.smb_retry_time;
				smb.subnum=n;
				if((j=smb_open(&smb))!=0) {
					errormsg(WHERE,ERR_OPEN,smb.file,j,smb.last_error);
					continue; }

				if(!filelength(fileno(smb.shd_fp))) {
					smb.status.max_crcs=cfg.sub[n]->maxcrcs;
					smb.status.max_msgs=cfg.sub[n]->maxmsgs;
					smb.status.max_age=cfg.sub[n]->maxage;
					smb.status.attr=cfg.sub[n]->misc&SUB_HYPER ? SMB_HYPERALLOC:0;
					if((j=smb_create(&smb))!=0) {
						smb_close(&smb);
						lastsub=INVALID_SUB;
						errormsg(WHERE,ERR_CREATE,smb.file,j);
						continue; } }

				if((j=smb_locksmbhdr(&smb))!=0) {
					smb_close(&smb);
					lastsub=INVALID_SUB;
					errormsg(WHERE,ERR_LOCK,smb.file,j);
					continue; }
				if((j=smb_getstatus(&smb))!=0) {
					smb_close(&smb);
					lastsub=INVALID_SUB;
					errormsg(WHERE,ERR_READ,smb.file,j);
					continue; }
				smb_unlocksmbhdr(&smb);
				lastsub=n; }

			if(!qwktomsg(rep,block,0,n,0))
				continue;

			useron.ptoday++;
			useron.posts++;
			logon_posts++;
			putuserrec(&cfg,useron.number,U_POSTS,5,ultoa(useron.posts,str,10));
			putuserrec(&cfg,useron.number,U_PTODAY,5,ultoa(useron.ptoday,str,10));
			bprintf(text[Posted],cfg.grp[cfg.sub[n]->grp]->sname
				,cfg.sub[n]->lname);
			sprintf(str,"%s posted on %s %s"
				,useron.alias,cfg.grp[cfg.sub[n]->grp]->sname,cfg.sub[n]->lname);
			if(cfg.sub[n]->misc&SUB_FIDO && cfg.sub[n]->echomail_sem[0])  /* semaphore */
				if((file=nopen(cmdstr(cfg.sub[n]->echomail_sem,nulstr,nulstr,NULL)
					,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
					close(file);
			logline("P+",str); } }         /* end of public message */

	update_qwkroute(NULL);			/* Write ROUTE.DAT */

	if(lastsub!=INVALID_SUB)
		smb_close(&smb);
	fclose(rep);

	if(useron.rest&FLAG('Q')) {             /* QWK Net Node */
		sprintf(str,"%s%s.msg",cfg.temp_dir,cfg.sys_id);
		if(fexistcase(str))
			remove(str);
		sprintf(str,"%s%s.rep",cfg.temp_dir,cfg.sys_id);
		if(fexistcase(str))
			remove(str);

		dir=opendir(cfg.temp_dir);
		while(dir!=NULL && (dirent=readdir(dir))!=NULL) {				/* Extra files */
			// Move files
			sprintf(str,"%s%s",cfg.temp_dir,dirent->d_name);
			if(isdir(str))
				continue;

			// Create directory if necessary
			sprintf(inbox,"%sqnet/%s.in",cfg.data_dir,useron.alias);
			MKDIR(inbox); 

			sprintf(fname,"%s/%s",inbox,dirent->d_name);
			mv(str,fname,1);
			sprintf(str,text[ReceivedFileViaQWK],dirent->d_name,useron.alias);
			putsmsg(&cfg,1,str);
		} 
		if(dir!=NULL)
			closedir(dir);
		sprintf(str,"%sqnet-rep.now",cfg.data_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
			close(file);
	}

	bputs(text[QWKUnpacked]);
	CRLF;
	/**********************************************/
	/* Hang-up now if that's what the user wanted */
	/**********************************************/
	autohangup();

	return(true);
}
