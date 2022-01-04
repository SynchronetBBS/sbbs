/* Synchronet QWK unpacking routine */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "qwk.h"
#include "filedat.h"

static void log_qwk_import_stats(sbbs_t* sbbs, ulong msgs, time_t start)
{
	if(msgs) {
		time_t t = time(NULL) - start;
		if(t < 1)
			t = 1;
		sbbs->lprintf(LOG_INFO,"Imported %lu QWK messages in %lu seconds (%lu msgs/sec)", msgs, (ulong)t, (ulong)(msgs/t));
	}
}

/****************************************************************************/
/* Unpacks .QWK packet, hubnum is the number of the QWK net hub 			*/
/****************************************************************************/
bool sbbs_t::unpack_qwk(char *packet,uint hubnum)
{
	char	str[MAX_PATH+1],fname[MAX_PATH+1];
	char 	tmp[512];
	char	error[256] = "";
	char	inbox[MAX_PATH+1];
	uchar	block[QWK_BLOCK_LEN];
	int 	k,file;
	uint	i,j,n,lastsub=INVALID_SUB;
	uint	usernum;
	uint	blocks;
	long	l,size,misc;
	ulong	t;
	ulong	msgs=0;
	ulong	tmsgs=0;
	ulong	dupes=0;
	ulong	errors=0;
	time_t	start;
	time_t	startsub;
	DIR*	dir;
	DIRENT*	dirent;
	FILE*	qwk;
	FILE*	fp;
	smbmsg_t	msg;
	str_list_t	headers=NULL;
	str_list_t	voting=NULL;
	str_list_t	ip_can=NULL;
	str_list_t	host_can=NULL;
	str_list_t	subject_can=NULL;
	str_list_t	twit_list=NULL;
	link_list_t user_list={0};

	memset(&msg,0,sizeof(msg));

	start=time(NULL);
	if((l=(long)flength(packet))<1) {
		errormsg(WHERE,ERR_LEN,packet,l);
		return(false);
	}
	delfiles(cfg.temp_dir,ALLFILES);
	long file_count = extract_files_from_archive(packet
		,/* outdir: */cfg.temp_dir
		,/* allowed_filename_chars: */NULL /* any */
		,/* with_path: */false
		,/* overwrite: */true
		,/* max_files: */0 /* unlimited */
		,/* file_list: */NULL /* all files */
		,error, sizeof(error));
	if(file_count >= 0) {
		lprintf(LOG_DEBUG, "libarchive extracted %ld files from %s", file_count, packet);
	} else {
		lprintf(LOG_ERR, "libarchive error %ld (%s) extracting %s", file_count, error, packet);
		if(*cfg.qhub[hubnum]->unpack == '\0')
			return false;
		i=external(cmdstr(cfg.qhub[hubnum]->unpack,packet,ALLFILES,NULL),EX_OFFLINE);
		if(i) {
			errormsg(WHERE,ERR_EXEC,cmdstr(cfg.qhub[hubnum]->unpack,packet,ALLFILES,NULL),i);
			return(false); 
		}
	}
	SAFEPRINTF(str,"%sMESSAGES.DAT",cfg.temp_dir);
	if(!fexistcase(str)) {
		lprintf(LOG_WARNING,"%s doesn't contain MESSAGES.DAT (%s)",packet,str);
		return(false); 
	}
	if((qwk=fnopen(&file,str,O_RDONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return(false); 
	}
	size=(long)filelength(file);

	SAFEPRINTF(fname,"%sHEADERS.DAT",cfg.temp_dir);
	if(fexistcase(fname)) {
		lprintf(LOG_DEBUG, "Reading %s", fname);
		if((fp=fopen(fname,"r")) == NULL)
			errormsg(WHERE,ERR_OPEN,fname,O_RDONLY);
		else {
			headers=iniReadFile(fp);
			fclose(fp);
		}
		remove(fname);
	}
	SAFEPRINTF(fname, "%sVOTING.DAT", cfg.temp_dir);
	if(fexistcase(fname)) {
		lprintf(LOG_DEBUG, "Reading %s", fname);
		if((fp=fopen(fname,"r")) == NULL)
			errormsg(WHERE,ERR_OPEN,fname,O_RDONLY);
		else {
			voting=iniReadFile(fp);
			fclose(fp);
		}
		remove(fname);
	}

	/********************/
	/* Process messages */
	/********************/
	lprintf(LOG_INFO,"Importing QWK Network Packet: %s",packet);

	ip_can=trashcan_list(&cfg,"ip");
	host_can=trashcan_list(&cfg,"host");
	subject_can=trashcan_list(&cfg,"subject");

	SAFEPRINTF(fname,"%stwitlist.cfg",cfg.ctrl_dir);
	twit_list = findstr_list(fname);

	for(l=QWK_BLOCK_LEN;l<size;l+=blocks*QWK_BLOCK_LEN) {
		if(terminated) {
			lprintf(LOG_NOTICE,"!Terminated");
			break;
		}
		fseek(qwk,l,SEEK_SET);
		if(fread(block,QWK_BLOCK_LEN,1,qwk) != 1) {
			errormsg(WHERE, ERR_READ, packet, QWK_BLOCK_LEN);
			blocks=1;
			errors++;
			continue;
		}
		if(block[0]<' ' || block[0]&0x80) {
			lprintf(LOG_NOTICE,"!Invalid QWK message status (%02X) at offset %lu in %s"
				,block[0], l, packet);
			blocks=1;
			errors++;
			continue;
		}
		sprintf(tmp,"%.6s",block+116);
		blocks=atoi(tmp);  /* i = number of blocks */
		n=(uint)block[123]|(((uint)block[124])<<8);  /* conference number */
		if(blocks<2) {
			if(block[0] == 'V' && blocks == 1 && voting != NULL) {	/* VOTING DATA */
				if(qwk_msg_filtered(&msg, ip_can, host_can, subject_can, twit_list))
					continue;
				if(!qwk_voting(&voting, l, NET_QWK, cfg.qhub[hubnum]->id, n, hubnum)) {
					lprintf(LOG_WARNING, "QWK vote failure, offset %lu in %s", l, packet);
					errors++;
				}
				continue;
			}
			lprintf(LOG_NOTICE,"!Invalid number of QWK blocks (%d) at offset %lu in %s"
				,blocks, l+116, packet);
			errors++;
			blocks=1;
			continue; 
		}

		if(!qwk_new_msg(n, &msg,(char*)block,/* offset: */l,headers,/* parse_sender_hfields: */true)) {
			errors++;
			continue;
		}

		if(qwk_msg_filtered(&msg, ip_can, host_can, subject_can))
			continue;

		if(!n) {		/* NETMAIL */
			lprintf(LOG_INFO,"QWK NetMail from %s to %s", cfg.qhub[hubnum]->id, msg.to);
			if(!stricmp(msg.to,"NETMAIL")) {  /* QWK to FidoNet NetMail */
				qwktonetmail(qwk,(char *)block,NULL,hubnum+1);
				continue; 
			}
			if(strchr(msg.to,'@')) {
				qwktonetmail(qwk,(char *)block,msg.to,hubnum+1);
				continue; 
			}
			usernum=atoi(msg.to);
			if(usernum && usernum>lastuser(&cfg))
				usernum=0;
			if(!usernum)
				usernum=matchuser(&cfg,msg.to,TRUE /* sysop_alias */);
			if(!usernum) {
				lprintf(LOG_NOTICE,"!QWK NetMail from %s to UNKNOWN USER: %s", cfg.qhub[hubnum]->id, msg.to);
				continue; 
			}

			getuserrec(&cfg,usernum,U_MISC,8,str);
			misc=ahtoul(str);
			if(misc&NETMAIL && cfg.sys_misc&SM_FWDTONET) {
				getuserrec(&cfg,usernum,U_NETMAIL,LEN_NETMAIL,str);
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
				errors++;
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
					errors++;
					continue; 
				} 
			}
			if((k=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_LOCK,smb.file,k,smb.last_error);
				smb_stack(&smb,SMB_STACK_POP);
				errors++;
				continue; 
			}
			if((k=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_READ,smb.file,k,smb.last_error);
				smb_stack(&smb,SMB_STACK_POP);
				errors++;
				continue; 
			}
			smb_unlocksmbhdr(&smb);
			bool dupe=false;
			if(qwk_import_msg(qwk, (char *)block, blocks, hubnum+1, &smb, usernum, &msg, &dupe)) {
				lprintf(LOG_INFO,"Imported QWK mail message from %s to %s #%u", msg.from, msg.to, usernum);
				SAFEPRINTF(str,text[UserSentYouMail],msg.from);
				putsmsg(&cfg,usernum,str);
				tmsgs++;
			} else {
				if(dupe)
					dupes++;
				else
					errors++;
			}
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			continue;
		}
		/*********************************/
		/* public message on a sub-board */
		/*********************************/

		if((j = resolve_qwkconf(n, hubnum)) == INVALID_SUB) {	/* ignore messages for subs not in config */
			lprintf(LOG_NOTICE,"!Message from %s on UNKNOWN QWK CONFERENCE NUMBER: %u"
				,cfg.qhub[hubnum]->id, n);
			errors++;
			continue;
		}

		/* TWIT FILTER */
		if(qwk_msg_filtered(&msg, /* ip_can: */NULL, /* host_can: */NULL, /* subject_can: */NULL, twit_list))
			continue; 

		if(j!=lastsub) {

			if(lastsub != INVALID_SUB)
				log_qwk_import_stats(this, msgs, startsub);
			msgs=0;
			startsub=time(NULL);

			lprintf(LOG_INFO,"Importing QWK messages from %s into %s %s"
				,cfg.qhub[hubnum]->id, cfg.grp[cfg.sub[j]->grp]->sname,cfg.sub[j]->lname);

			if(lastsub!=INVALID_SUB)
				smb_close(&smb);
			lastsub=INVALID_SUB;
			sprintf(smb.file,"%s%s",cfg.sub[j]->data_dir,cfg.sub[j]->code);
			smb.retry_time=cfg.smb_retry_time;
			smb.subnum=j;
			if((k=smb_open(&smb))!=0) {
				errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
				errors++;
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
					errors++;
					continue; 
				} 
			}
			if((k=smb_locksmbhdr(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_LOCK,smb.file,k,smb.last_error);
				errors++;
				continue; 
			}
			if((k=smb_getstatus(&smb))!=0) {
				smb_close(&smb);
				errormsg(WHERE,ERR_READ,smb.file,k,smb.last_error);
				errors++;
				continue; 
			}
			smb_unlocksmbhdr(&smb);
			lastsub=j; 
		}

		bool dupe = false;
		if(qwk_import_msg(qwk, (char *)block, blocks, hubnum+1, &smb, /*touser: */0, &msg, &dupe)) {
			signal_sub_sem(&cfg,j);
			msgs++;
			tmsgs++;
			int destuser = lookup_user(&cfg, &user_list, msg.to);
			if(destuser > 0) {
				SAFEPRINTF4(str, text[MsgPostedToYouVia]
					,msg.from
					,cfg.qhub[hubnum]->id
					,cfg.grp[cfg.sub[j]->grp]->sname, cfg.sub[j]->lname);
				putsmsg(&cfg, destuser, str);
			}
		} else {
			if(dupe)
				dupes++;
			else
				errors++;
		}
	}
	if(lastsub != INVALID_SUB) {
		log_qwk_import_stats(this, msgs, startsub);
		smb_close(&smb);
	}

	qwk_handle_remaining_votes(&voting, NET_QWK, cfg.qhub[hubnum]->id, hubnum);

	update_qwkroute(NULL);		/* Write ROUTE.DAT */

	smb_freemsgmem(&msg);
	fclose(qwk);

	iniFreeStringList(headers);
	iniFreeStringList(voting);

	strListFree(&ip_can);
	strListFree(&host_can);
	strListFree(&subject_can);
	strListFree(&twit_list);
	listFree(&user_list);

	delfiles(cfg.temp_dir,"*.NDX");
	SAFEPRINTF(str,"%sMESSAGES.DAT",cfg.temp_dir);
	removecase(str);
	SAFEPRINTF(str,"%sDOOR.ID",cfg.temp_dir);
	removecase(str);
	SAFEPRINTF(str,"%sCONTROL.DAT",cfg.temp_dir);
	removecase(str);
	SAFEPRINTF(str,"%sNETFLAGS.DAT",cfg.temp_dir);
	removecase(str);
	SAFEPRINTF(str,"%sTOREADER.EXT",cfg.temp_dir);
	removecase(str);

	dir=opendir(cfg.temp_dir);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		sprintf(str,"%s%s",cfg.temp_dir,dirent->d_name);
		if(isdir(str))	/* sub-dir */
			continue;

		if(::trashcan(&cfg, dirent->d_name, "file")) {
			lprintf(LOG_NOTICE,"Ignored blocked filename from %s: %s", cfg.qhub[hubnum]->id, dirent->d_name);
			continue;
		}

		// Create directory if necessary
		SAFEPRINTF2(inbox,"%sqnet/%s.in",cfg.data_dir,cfg.qhub[hubnum]->id);
		if(MKDIR(inbox) != 0)
			errormsg(WHERE, ERR_CREATE, inbox, 0);

		// Copy files
		SAFEPRINTF2(fname,"%s/%s",inbox,dirent->d_name);
		mv(str,fname,1 /* overwrite */);
		sprintf(str,text[ReceivedFileViaQWK],dirent->d_name,cfg.qhub[hubnum]->id);
		putsmsg(&cfg,1,str);
		lprintf(LOG_INFO,"Received file from %s: %s", cfg.qhub[hubnum]->id, dirent->d_name);
	}
	if(dir!=NULL)
		closedir(dir);

	t=(ulong)(time(NULL)-start);
	if(tmsgs || errors || dupes) {
		if(t<1)
			t=1;
		lprintf(LOG_INFO,"Finished Importing QWK Network Packet from %s: "
			"(%lu msgs) in %lu seconds (%lu msgs/sec), %lu errors, %lu dupes"
			,cfg.qhub[hubnum]->id, tmsgs, t, tmsgs/t, errors, dupes);
		/* trigger timed event with internal code of 'qnet-qwk' to run */
		sprintf(str,"%sqnet-qwk.now",cfg.data_dir);
		ftouch(str);
	}
	delfiles(cfg.temp_dir,ALLFILES);
	return errors == 0;
}
