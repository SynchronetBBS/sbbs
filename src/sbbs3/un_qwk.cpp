/* un_qwk.cpp */

/* Synchronet QWK unpacking routine */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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
/* Unpacks .QWK packet, hubnum is the number of the QWK net hub 			*/
/****************************************************************************/
bool sbbs_t::unpack_qwk(char *packet,uint hubnum)
{
	char	str[MAX_PATH+1],fname[MAX_PATH+1];
	char 	tmp[512];
	char	from[26];
	char	to[26];
	char	inbox[MAX_PATH+1];
	uchar	block[QWK_BLOCK_LEN];
	int 	k,file;
	uint	i,j,n,lastsub=INVALID_SUB;
	uint	blocks;
	long	l,size,misc;
	ulong	t;
	ulong	msgs=0;
	ulong	tmsgs=0;
	time_t	start;
	time_t	startsub;
	DIR*	dir;
	DIRENT*	dirent;
	FILE*	qwk;
	BOOL	twit_list;

	sprintf(fname,"%stwitlist.cfg",cfg.ctrl_dir);
	twit_list=fexist(fname);

	start=time(NULL);
	if((l=flength(packet))<1) {
		errormsg(WHERE,ERR_LEN,packet,l);
		return(false);
	}
	i=external(cmdstr(cfg.qhub[hubnum]->unpack,packet,ALLFILES,NULL),EX_OFFLINE);
	if(i) {
		errormsg(WHERE,ERR_EXEC,cmdstr(cfg.qhub[hubnum]->unpack,packet,ALLFILES,NULL),i);
		return(false); 
	}
	sprintf(str,"%sMESSAGES.DAT",cfg.temp_dir);
	if(!fexistcase(str)) {
		SAFEPRINTF2(tmp,"%s doesn't contain MESSAGES.DAT (%s)",packet,str);
		errorlog(tmp);
		return(false); 
	}
	if((qwk=fnopen(&file,str,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return(false); 
	}
	size=filelength(file);
	/********************/
	/* Process messages */
	/********************/
	eprintf(LOG_INFO,"Importing QWK Network Packet: %s",packet);

	for(l=QWK_BLOCK_LEN;l<size;l+=blocks*QWK_BLOCK_LEN) {
		if(terminated) {
			eprintf(LOG_NOTICE,"!Terminated");
			break;
		}
		fseek(qwk,l,SEEK_SET);
		fread(block,QWK_BLOCK_LEN,1,qwk);
		if(block[0]<' ' || block[0]&0x80) {
			eprintf(LOG_NOTICE,"!Invalid message status (%02X) at offset %lu in %s"
				,block[0], l, packet);
			blocks=1;
			continue;
		}
		sprintf(tmp,"%.6s",block+116);
		blocks=atoi(tmp);  /* i = number of blocks */
		if(blocks<2) {
			eprintf(LOG_NOTICE,"!Invalid number of blocks (%d) at offset %lu in %s"
				,blocks, l+116, packet);
			blocks=1;
			continue; 
		}
		/*********************************/
		/* public message on a sub-board */
		/*********************************/
		n=(uint)block[123]|(((uint)block[124])<<8);  /* conference number */

		if(!n) {		/* NETMAIL */
			sprintf(str,"%25.25s",block+21);
			truncsp(str);
			eprintf(LOG_INFO,"NetMail from %s to %s", cfg.qhub[hubnum]->id, str);
			if(!stricmp(str,"NETMAIL")) {  /* QWK to FidoNet NetMail */
				qwktonetmail(qwk,(char *)block,NULL,hubnum+1);
				continue; 
			}
			if(strchr(str,'@')) {
				qwktonetmail(qwk,(char *)block,str,hubnum+1);
				continue; 
			}
			j=atoi(str);
			if(j && j>lastuser(&cfg))
				j=0;
			if(!j)
				j=matchuser(&cfg,str,TRUE /* sysop_alias */);
			if(!j) {
				eprintf(LOG_NOTICE,"!NetMail from %s to UNKNOWN USER: %s", cfg.qhub[hubnum]->id, str);
				continue; 
			}

			getuserrec(&cfg,j,U_MISC,8,str);
			misc=ahtoul(str);
			if(misc&NETMAIL && cfg.sys_misc&SM_FWDTONET) {
				getuserrec(&cfg,j,U_NETMAIL,LEN_NETMAIL,str);
				qwktonetmail(qwk,(char*)block,str,hubnum+1);
				continue; 
			}

			smb_stack(&smb,SMB_STACK_PUSH);
			sprintf(smb.file,"%smail",cfg.data_dir);
			smb.retry_time=cfg.smb_retry_time;
			smb.subnum=INVALID_SUB;
			if((k=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
				smb_stack(&smb,SMB_STACK_POP);
				continue; 
			}
			if(!filelength(fileno(smb.shd_fp))) {
				smb.status.max_crcs=cfg.mail_maxcrcs;
				smb.status.max_msgs=0;
				smb.status.max_age=cfg.mail_maxage;
				smb.status.attr=SMB_EMAIL;
				if((k=smb_create(&smb))!=0) {
					smb_close(&smb);
					errormsg(WHERE,ERR_CREATE,smb.file,k,smb.last_error);
					smb_stack(&smb,SMB_STACK_POP);
					continue; 
				} 
			}
			if((k=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_LOCK,smb.file,k,smb.last_error);
				smb_stack(&smb,SMB_STACK_POP);
				continue; 
			}
			if((k=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_READ,smb.file,k,smb.last_error);
				smb_stack(&smb,SMB_STACK_POP);
				continue; 
			}
			smb_unlocksmbhdr(&smb);
			if(qwktomsg(qwk,(char *)block,hubnum+1,INVALID_SUB,j)) {
				sprintf(tmp,"%-25.25s",block+46);
				truncsp(tmp);
				sprintf(str,text[UserSentYouMail],tmp);
				putsmsg(&cfg,j,str);
				tmsgs++;
			}
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			continue;
		}

		for(j=0;j<cfg.qhub[hubnum]->subs;j++)
			if(cfg.qhub[hubnum]->conf[j]==n)
				break;
		if(j>=cfg.qhub[hubnum]->subs) {	/* ignore messages for subs not in config */
			eprintf(LOG_NOTICE,"!Message from %s on UNKNOWN CONFERENCE NUMBER: %u"
				,cfg.qhub[hubnum]->id, n);
			continue;
		}

		j=cfg.qhub[hubnum]->sub[j];

		/* TWIT FILTER */
		if(twit_list) {
			sprintf(fname,"%stwitlist.cfg",cfg.ctrl_dir);

			sprintf(from,"%25.25s",block+46);  /* From user */
			truncsp(from);
			sprintf(to,"%25.25s",block+21);		/* To user */
			truncsp(to);

			if(findstr(from,fname) || findstr(to,fname)) {
				eprintf(LOG_NOTICE,"!Filtering post from %s to %s on %s %s"
					,from
					,to
					,cfg.grp[cfg.sub[j]->grp]->sname,cfg.sub[j]->lname); 
				continue; 
			}
		}

		if(j!=lastsub) {

			if(msgs) {
				t=time(NULL)-startsub;
				if(t<1)
					t=1;
				eprintf(LOG_INFO,"Imported %lu msgs in %lu seconds (%lu msgs/sec)", msgs,t,msgs/t);
			}
			msgs=0;
			startsub=time(NULL);

			eprintf(LOG_INFO,"Importing messages from %s into %s %s"
				,cfg.qhub[hubnum]->id, cfg.grp[cfg.sub[j]->grp]->sname,cfg.sub[j]->lname);

			if(lastsub!=INVALID_SUB)
				smb_close(&smb);
			lastsub=INVALID_SUB;
			sprintf(smb.file,"%s%s",cfg.sub[j]->data_dir,cfg.sub[j]->code);
			smb.retry_time=cfg.smb_retry_time;
			smb.subnum=j;
			if((k=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
				continue; 
			}
			if(!filelength(fileno(smb.shd_fp))) {
				smb.status.max_crcs=cfg.sub[j]->maxcrcs;
				smb.status.max_msgs=cfg.sub[j]->maxmsgs;
				smb.status.max_age=cfg.sub[j]->maxage;
				smb.status.attr=cfg.sub[j]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
				if((k=smb_create(&smb))!=0) {
					smb_close(&smb);
					errormsg(WHERE,ERR_CREATE,smb.file,k,smb.last_error);
					continue; 
				} 
			}
			if((k=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_LOCK,smb.file,k,smb.last_error);
				continue; 
			}
			if((k=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_READ,smb.file,k,smb.last_error);
				continue; 
			}
			smb_unlocksmbhdr(&smb);
			lastsub=j; 
		}

		if(!qwktomsg(qwk,(char *)block,hubnum+1,j,0)) {
//			eprintf("!QWKTOMSG failed");
			continue;
		}

		signal_sub_sem(&cfg,j);
/*
		eprintf("Message from %s Posted on %s %s"
			,cfg.qhub[hubnum]->id,cfg.grp[cfg.sub[j]->grp]->sname,cfg.sub[j]->lname); 
*/
		msgs++;
		tmsgs++;
	}

	update_qwkroute(NULL);		/* Write ROUTE.DAT */

	fclose(qwk);
	if(lastsub!=INVALID_SUB)
		smb_close(&smb);

	delfiles(cfg.temp_dir,"*.NDX");
	sprintf(str,"%sMESSAGES.DAT",cfg.temp_dir);
	remove(str);
	sprintf(str,"%sDOOR.ID",cfg.temp_dir);
	remove(str);
	sprintf(str,"%sCONTROL.DAT",cfg.temp_dir);
	remove(str);
	sprintf(str,"%sNETFLAGS.DAT",cfg.temp_dir);
	remove(str);

	dir=opendir(cfg.temp_dir);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		sprintf(str,"%s%s",cfg.temp_dir,dirent->d_name);
		if(isdir(str))	/* sub-dir */
			continue;

		// Create directory if necessary
		sprintf(inbox,"%sqnet/%s.in",cfg.data_dir,cfg.qhub[hubnum]->id);
		MKDIR(inbox);

		// Copy files
		sprintf(fname,"%s/%s",inbox,dirent->d_name);
		mv(str,fname,1 /* overwrite */);
		sprintf(str,text[ReceivedFileViaQWK],dirent->d_name,cfg.qhub[hubnum]->id);
		putsmsg(&cfg,1,str);
		eprintf(LOG_INFO,"Received %s from %s", dirent->d_name, cfg.qhub[hubnum]->id);
	}
	if(dir!=NULL)
		closedir(dir);

	t=time(NULL)-start;
	if(tmsgs) {
		if(t<1)
			t=1;
		eprintf(LOG_INFO,"Finished Importing QWK Network Packet from %s: "
			"(%lu msgs) in %lu seconds (%lu msgs/sec)"
			,cfg.qhub[hubnum]->id, tmsgs, t, tmsgs/t);
		/* trigger timed event with internal code of 'qnet-qwk' to run */
		sprintf(str,"%sqnet-qwk.now",cfg.data_dir);
		ftouch(str);
	}
	delfiles(cfg.temp_dir,ALLFILES);
	return(true);
}
