/* bat_xfer.cpp */

/* Synchronet batch file transfer functions */

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

/****************************************************************************/
/* This is the batch menu section                                           */
/****************************************************************************/
void sbbs_t::batchmenu()
{
    char	str[129],tmp2[250],done=0,ch;
	char 	tmp[512];
	uint	i,n,xfrprot,xfrdir;
    ulong	totalcdt,totalsize,totaltime;
    time_t	start,end;
    file_t	f;

	if(!batdn_total && !batup_total && cfg.upload_dir==INVALID_DIR) {
		bputs(text[NoFilesInBatchQueue]);
		return; 
	}
	if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT))
		menu("batchxfer");
	lncntr=0;
	while(online && !done && (batdn_total || batup_total
		|| cfg.upload_dir!=INVALID_DIR)) {
		if(!(useron.misc&(EXPERT|RIP|WIP|HTML))) {
			sys_status&=~SS_ABORT;
			if(lncntr) {
				SYNC;
				CRLF;
				if(lncntr)          /* CRLF or SYNC can cause pause */
					pause(); 
			}
			menu("batchxfr"); 
		}
		ASYNC;
		bputs(text[BatchMenuPrompt]);
		ch=(char)getkeys("BCDLQRU?\r",0);
		if(ch>SP)
			logch(ch,0);
		switch(ch) {
			case '?':
				if(useron.misc&(EXPERT|RIP|WIP|HTML))
					menu("batchxfr");
				break;
			case CR:
			case 'Q':
				lncntr=0;
				done=1;
				break;
			case 'B':   /* Bi-directional transfers */
				if(useron.rest&FLAG('D')) {
					bputs(text[R_Download]);
					break; 
				}
				if(useron.rest&FLAG('U')) {
					bputs(text[R_Upload]);
					break; 
				}
				if(!batdn_total) {
					bputs(text[DownloadQueueIsEmpty]);
					break; 
				}
				if(!batup_total && cfg.upload_dir==INVALID_DIR) {
					bputs(text[UploadQueueIsEmpty]);
					break; 
				}
				for(i=0,totalcdt=0;i<batdn_total;i++)
					if(!is_download_free(&cfg,batdn_dir[i],&useron))
						totalcdt+=batdn_cdt[i];
				if(totalcdt>useron.cdt+useron.freecdt) {
					bprintf(text[YouOnlyHaveNCredits]
						,ultoac(useron.cdt+useron.freecdt,tmp));
					break; 
				}
				for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
					totalsize+=batdn_size[i];
					if(!(cfg.dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
						totaltime+=batdn_size[i]/(ulong)cur_cps; 
				}
				if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime>timeleft) {
					bputs(text[NotEnoughTimeToDl]);
					break; 
				}
				menu("biprot");
				if(!create_batchdn_lst())
					break;
				if(!create_batchup_lst())
					break;
				if(!create_bimodem_pth())
					break;
				SYNC;
				mnemonics(text[ProtocolOrQuit]);
				strcpy(tmp2,"Q");
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->bicmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
						sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
						strcat(tmp2,tmp); }
				ungetkey(useron.prot);
				ch=(char)getkeys(tmp2,0);
				if(ch=='Q')
					break;
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->bicmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron))
						break;
				if(i<cfg.total_prots) {
					xfrprot=i;
					action=NODE_BXFR;
					SYNC;
					for(i=0;i<batdn_total;i++)
						if(cfg.dir[batdn_dir[i]]->seqdev) {
							lncntr=0;
							unpadfname(batdn_name[i],tmp);
							sprintf(tmp2,"%s%s",cfg.temp_dir,tmp);
							if(!fexistcase(tmp2)) {
								seqwait(cfg.dir[batdn_dir[i]]->seqdev);
								bprintf(text[RetrievingFile],tmp);
								sprintf(str,"%s%s"
									,batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
									? cfg.altpath[batdn_alt[i]-1]
									: cfg.dir[batdn_dir[i]]->path
									,tmp);
								mv(str,tmp2,1); /* copy the file to temp dir */
								if(getnodedat(cfg.node_num,&thisnode,true)==0) {
									thisnode.aux=0xff;
									putnodedat(cfg.node_num,&thisnode);
								}
								CRLF; 
							} 
						}
					sprintf(str,"%sBATCHDN.LST",cfg.node_dir);
					sprintf(tmp2,"%sBATCHUP.LST",cfg.node_dir);
					start=time(NULL);
					protocol(cmdstr(cfg.prot[xfrprot]->bicmd,str,tmp2,NULL),true);
					end=time(NULL);
					for(i=0;i<batdn_total;i++)
						if(cfg.dir[batdn_dir[i]]->seqdev) {
							unpadfname(batdn_name[i],tmp);
							sprintf(tmp2,"%s%s",cfg.temp_dir,tmp);
							remove(tmp2); }
					batch_upload();
					batch_download(xfrprot);
					if(batdn_total)     /* files still in queue, not xfered */
						notdownloaded(totalsize,start,end);
					autohangup(); 
				}
				break;
			case 'C':
				if(batup_total) {
					if(!noyes(text[ClearUploadQueueQ])) {
						batup_total=0;
						bputs(text[UploadQueueCleared]); 
					} 
				}
				if(batdn_total) {
					if(!noyes(text[ClearDownloadQueueQ])) {
						for(i=0;i<batdn_total;i++) {
							f.dir=batdn_dir[i];
							f.datoffset=batdn_offset[i];
							f.size=batdn_size[i];
							strcpy(f.name,batdn_name[i]);
							closefile(&f); }
						batdn_total=0;
						bputs(text[DownloadQueueCleared]); 
					} 
				}
				break;
			case 'D':
				start_batch_download();
				break;
			case 'L':
				if(batup_total) {
					bputs(text[UploadQueueLstHdr]);
					for(i=0;i<batup_total;i++)
						bprintf(text[UploadQueueLstFmt],i+1,batup_name[i]
							,batup_desc[i]); }
				if(batdn_total) {
					bputs(text[DownloadQueueLstHdr]);
					for(i=0,totalcdt=0,totalsize=0;i<batdn_total;i++) {
						bprintf(text[DownloadQueueLstFmt],i+1
							,batdn_name[i],ultoac(batdn_cdt[i],tmp)
							,ultoac(batdn_size[i],str)
							,cur_cps
							? sectostr(batdn_size[i]/(ulong)cur_cps,tmp2)
							: "??:??:??");
						totalsize+=batdn_size[i];
						totalcdt+=batdn_cdt[i]; }
					bprintf(text[DownloadQueueTotals]
						,ultoac(totalcdt,tmp),ultoac(totalsize,str),cur_cps
						? sectostr(totalsize/(ulong)cur_cps,tmp2)
						: "??:??:??"); }
				break;  /* Questionable line ^^^, see note above function */
			case 'R':
				if(batup_total) {
					bprintf(text[RemoveWhichFromUlQueue],batup_total);
					n=getnum(batup_total);
					if((int)n>=1) {
						n--;
						batup_total--;
						while(n<batup_total) {
							batup_dir[n]=batup_dir[n+1];
							batup_misc[n]=batup_misc[n+1];
							batup_alt[n]=batup_alt[n+1];
							strcpy(batup_name[n],batup_name[n+1]);
							strcpy(batup_desc[n],batup_desc[n+1]);
							n++; }
						if(!batup_total)
							bputs(text[UploadQueueCleared]); 
					} 
				}
				if(batdn_total) {
					bprintf(text[RemoveWhichFromDlQueue],batdn_total);
					n=getnum(batdn_total);
					if((int)n>=1) {
						n--;
						f.dir=batdn_dir[n];
						strcpy(f.name,batdn_name[n]);
						f.datoffset=batdn_offset[n];
						f.size=batdn_size[n];
						closefile(&f);
						batdn_total--;
						while(n<batdn_total) {
							strcpy(batdn_name[n],batdn_name[n+1]);
							batdn_dir[n]=batdn_dir[n+1];
							batdn_cdt[n]=batdn_cdt[n+1];
							batdn_alt[n]=batdn_alt[n+1];
							batdn_size[n]=batdn_size[n+1];
							batdn_offset[n]=batdn_offset[n+1];
							n++; 
						}
						if(!batdn_total)
							bputs(text[DownloadQueueCleared]); 
					} 
				}
				break;
		   case 'U':
				if(useron.rest&FLAG('U')) {
					bputs(text[R_Upload]);
					break; 
				}
				if(!batup_total && cfg.upload_dir==INVALID_DIR) {
					bputs(text[UploadQueueIsEmpty]);
					break; 
				}
				menu("batuprot");
				if(!create_batchup_lst())
					break;
				if(!create_bimodem_pth())
					break;
				ASYNC;
				mnemonics(text[ProtocolOrQuit]);
				strcpy(str,"Q");
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->batulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
						sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
						strcat(str,tmp); 
					}
				ch=(char)getkeys(str,0);
				if(ch=='Q')
					break;
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->batulcmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron))
						break;
				if(i<cfg.total_prots) {
					sprintf(str,"%sBATCHUP.LST",cfg.node_dir);
					xfrprot=i;
					if(batup_total)
						xfrdir=batup_dir[0];
					else
						xfrdir=cfg.upload_dir;
					action=NODE_ULNG;
					SYNC;
					if(online==ON_REMOTE) {
						delfiles(cfg.temp_dir,ALLFILES);
						start=time(NULL);
						protocol(cmdstr(cfg.prot[xfrprot]->batulcmd,str,nulstr,NULL),true);
						end=time(NULL);
						if(!(cfg.dir[xfrdir]->misc&DIR_ULTIME))
							starttime+=end-start; 
					}
					batch_upload();
					delfiles(cfg.temp_dir,ALLFILES);
					autohangup(); 
				}
				break; 
		} 
	}
	delfiles(cfg.temp_dir,ALLFILES);
}

/****************************************************************************/
/* Download files from batch queue                                          */
/****************************************************************************/
BOOL sbbs_t::start_batch_download()
{
	char	ch;
	char	tmp[32];
	char	fname[64];
	char	str[MAX_PATH+1];
	char 	path[MAX_PATH+1];
	char*	list;
	size_t	list_len;
	int		error;
    int		j;
    uint	i,xfrprot;
    ulong	totalcdt,totalsize,totaltime;
    time_t	start,end,t;
	struct	tm tm;

	if(batdn_total==0) {
		bputs(text[DownloadQueueIsEmpty]);
		return(FALSE);
	}
	if(useron.rest&FLAG('D')) {     /* Download restriction */
		bputs(text[R_Download]);
		return(FALSE); 
	}
	for(i=0,totalcdt=0;i<batdn_total;i++)
		if(is_download_free(&cfg,batdn_dir[i],&useron))
			totalcdt+=batdn_cdt[i];
	if(totalcdt>useron.cdt+useron.freecdt) {
		bprintf(text[YouOnlyHaveNCredits]
			,ultoac(useron.cdt+useron.freecdt,tmp));
		return(FALSE); 
	}

	for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
		totalsize+=batdn_size[i];
		if(!(cfg.dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
			totaltime+=batdn_size[i]/(ulong)cur_cps; 
	}
	if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime>timeleft) {
		bputs(text[NotEnoughTimeToDl]);
		return(FALSE); 
	}
	menu("batdprot");
	if(!create_batchdn_lst())
		return(FALSE);
	if(!create_bimodem_pth())
		return(FALSE);
	ASYNC;
	mnemonics(text[ProtocolOrQuit]);
	strcpy(str,"Q");
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->batdlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron)) {
			sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
			strcat(str,tmp); 
		}
	ungetkey(useron.prot);
	ch=(char)getkeys(str,0);
	if(ch=='Q' || sys_status&SS_ABORT)
		return(FALSE);
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->batdlcmd[0] && cfg.prot[i]->mnemonic==ch
			&& chk_ar(cfg.prot[i]->ar,&useron))
			break;
	if(i>=cfg.total_prots)
		return(FALSE);	/* no protocol selected */

	xfrprot=i;
	list=NULL;
	for(i=0;i<batdn_total;i++) {
		curdirnum=batdn_dir[i]; 		/* for ARS */
		unpadfname(batdn_name[i],fname);
		if(cfg.dir[batdn_dir[i]]->seqdev) {
			lncntr=0;
			sprintf(path,"%s%s",cfg.temp_dir,fname);
			if(!fexistcase(path)) {
				seqwait(cfg.dir[batdn_dir[i]]->seqdev);
				bprintf(text[RetrievingFile],fname);
				sprintf(str,"%s%s"
					,batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
					? cfg.altpath[batdn_alt[i]-1]
					: cfg.dir[batdn_dir[i]]->path
					,fname);
				mv(str,path,1); /* copy the file to temp dir */
				if(getnodedat(cfg.node_num,&thisnode,true)==0) {
					thisnode.aux=40; /* clear the seq dev # */
					putnodedat(cfg.node_num,&thisnode);
				}
				CRLF; 
			} 
		}
		else
			sprintf(path,"%s%s"
				,batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
				? cfg.altpath[batdn_alt[i]-1]
				: cfg.dir[batdn_dir[i]]->path
				,fname);
		if(list==NULL)
			list_len=0;
		else
			list_len=strlen(list)+1;	/* add one for ' ' */
		if((list=(char*)realloc(list,list_len+strlen(path)+1	/* add one for '\0'*/))==NULL) {
			errormsg(WHERE,ERR_ALLOC,"list",list_len+strlen(path));
			return(FALSE);
		}
		if(!list_len)
			strcpy(list,path);
		else {
			strcat(list," ");
			strcat(list,path);
		}
		for(j=0;j<cfg.total_dlevents;j++) {
			if(stricmp(cfg.dlevent[j]->ext,batdn_name[i]+9))
				continue;
			if(!chk_ar(cfg.dlevent[j]->ar,&useron))
				continue;
			bputs(cfg.dlevent[j]->workstr);
			external(cmdstr(cfg.dlevent[j]->cmd,path,nulstr,NULL),EX_OUTL);
			CRLF; 
		}
	}

	sprintf(str,"%sBATCHDN.LST",cfg.node_dir);
	action=NODE_DLNG;
	t=now;
	if(cur_cps) 
		t+=(totalsize/(ulong)cur_cps);
	localtime_r(&t,&tm);
	if(getnodedat(cfg.node_num,&thisnode,true)==0) {
		thisnode.aux=(tm.tm_hour*60)+tm.tm_min;
		thisnode.action=action;
		putnodedat(cfg.node_num,&thisnode); /* calculate ETA */
	}
	start=time(NULL);
	error=protocol(cmdstr(cfg.prot[xfrprot]->batdlcmd,str,list,NULL),false);
	end=time(NULL);
	if(cfg.prot[xfrprot]->misc&PROT_DSZLOG || !error)
		batch_download(xfrprot);
	if(batdn_total)
		notdownloaded(totalsize,start,end);
	autohangup(); 
	if(list!=NULL)
		free(list);

	return(TRUE);
}

/****************************************************************************/
/* Creates the file BATCHDN.LST in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/****************************************************************************/
bool sbbs_t::create_batchdn_lst()
{
	char	str[256];
	int		file;
	uint	i;

	sprintf(str,"%sBATCHDN.LST",cfg.node_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(false); 
	}
	for(i=0;i<batdn_total;i++) {
		if(batdn_dir[i]>=cfg.total_dirs || cfg.dir[batdn_dir[i]]->seqdev)
			strcpy(str,cfg.temp_dir);
		else
			strcpy(str,batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
				? cfg.altpath[batdn_alt[i]-1] : cfg.dir[batdn_dir[i]]->path);
		write(file,str,strlen(str));
		unpadfname(batdn_name[i],str);
		strcat(str,crlf);
		write(file,str,strlen(str)); 
	}
	close(file);
	return(true);
}

/****************************************************************************/
/* Creates the file BATCHUP.LST in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/* This list is not used by any protocols to date.                          */
/****************************************************************************/
bool sbbs_t::create_batchup_lst()
{
    char	str[256];
    int		file;
	uint	i;

	sprintf(str,"%sBATCHUP.LST",cfg.node_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(false); 
	}
	for(i=0;i<batup_total;i++) {
		if(batup_dir[i]>=cfg.total_dirs)
			strcpy(str,cfg.temp_dir);
		else
			strcpy(str,batup_alt[i]>0 && batup_alt[i]<=cfg.altpaths
				? cfg.altpath[batup_alt[i]-1] : cfg.dir[batup_dir[i]]->path);
		write(file,str,strlen(str));
		unpadfname(batup_name[i],str);
		strcat(str,crlf);
		write(file,str,strlen(str)); 
	}
	close(file);
	return(true);
}

/****************************************************************************/
/* Creates the file BIMODEM.PTH in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/****************************************************************************/
bool sbbs_t::create_bimodem_pth()
{
    char	str[256],tmp2[512];
	char 	tmp[512];
    int		file;
	uint	i;

	sprintf(str,"%sBIMODEM.PTH",cfg.node_dir);  /* Create bimodem file */
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(false); 
	}
	for(i=0;i<batup_total;i++) {
		sprintf(str,"%s%s",batup_dir[i]>=cfg.total_dirs ? cfg.temp_dir
			: batup_alt[i]>0 && batup_alt[i]<=cfg.altpaths
			? cfg.altpath[batup_alt[i]-1] : cfg.dir[batup_dir[i]]->path
			,unpadfname(batup_name[i],tmp));
		sprintf(tmp2,"D       %-80.80s%-160.160s"
			,unpadfname(batup_name[i],tmp),str);
		write(file,tmp2,248); 
	}
	for(i=0;i<batdn_total;i++) {
		sprintf(str,"%s%s"
			,(batdn_dir[i]>=cfg.total_dirs || cfg.dir[batdn_dir[i]]->seqdev)
			? cfg.temp_dir : batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
				? cfg.altpath[batdn_alt[i]-1] : cfg.dir[batdn_dir[i]]->path
				,unpadfname(batdn_name[i],tmp));
		sprintf(tmp2,"U       %-240.240s",str);
		write(file,tmp2,248); 
	}
	close(file);
	return(true);
}

/****************************************************************************/
/* Processes files that were supposed to be received in the batch queue     */
/****************************************************************************/
void sbbs_t::batch_upload()
{
    char	str1[256],str2[256];
	char 	tmp[512];
	uint	i,j,x,y;
    file_t	f;
	DIR*	dir;
	DIRENT*	dirent;

	for(i=0;i<batup_total;) {
		curdirnum=batup_dir[i]; 			/* for ARS */
		lncntr=0;                               /* defeat pause */
		unpadfname(batup_name[i],tmp);
		sprintf(str1,"%s%s",cfg.temp_dir,tmp);
		sprintf(str2,"%s%s",cfg.dir[batup_dir[i]]->path,tmp);
		if(fexistcase(str1) && fexistcase(str2)) { /* file's in two places */
			bprintf(text[FileAlreadyThere],batup_name[i]);
			remove(str1);    /* this is the one received */
			i++;
			continue; 
		}
		if(fexist(str1))
			mv(str1,str2,0);
		strcpy(f.name,batup_name[i]);
		strcpy(f.desc,batup_desc[i]);
		f.dir=batup_dir[i];
		f.misc=batup_misc[i];
		f.altpath=batup_alt[i];
		if(uploadfile(&f)) {
			batup_total--;
			for(j=i;j<batup_total;j++) {
				batup_dir[j]=batup_dir[j+1];
				batup_alt[j]=batup_alt[j+1];
				batup_misc[j]=batup_misc[j+1];
				strcpy(batup_name[j],batup_name[j+1]);
				strcpy(batup_desc[j],batup_desc[j+1]); 
			} 
		}
		else i++; 
	}
	if(cfg.upload_dir==INVALID_DIR)
		return;
	dir=opendir(cfg.temp_dir);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		sprintf(str1,"%s%s",cfg.temp_dir,dirent->d_name);
		if(isdir(str1))
			continue;
		memset(&f,0,sizeof(file_t));
		f.dir=cfg.upload_dir;
		padfname(dirent->d_name,f.name);
		for(x=0;x<usrlibs;x++) {
			for(y=0;y<usrdirs[x];y++)
				if(cfg.dir[usrdir[x][y]]->misc&DIR_DUPES
					&& findfile(&cfg,usrdir[x][y],f.name))
					break;
			if(y<usrdirs[x])
				break; 
		}
		sprintf(str2,"%s%s",cfg.dir[f.dir]->path,dirent->d_name);
		if(x<usrlibs || fexistcase(str2)) {
			bprintf(text[FileAlreadyOnline],f.name);
			remove(str1); 
		} else {
			mv(str1,str2,0);
			uploadfile(&f); 
		}
	}
	if(dir!=NULL)
		closedir(dir);
}

/****************************************************************************/
/* Processes files that were supposed to be sent from the batch queue       */
/* xfrprot is -1 if downloading files from within QWK (no DSZLOG)           */
/****************************************************************************/
void sbbs_t::batch_download(int xfrprot)
{
    uint	i,j;
    file_t	f;

	for(i=0;i<batdn_total;) {
		lncntr=0;                               /* defeat pause */
		f.dir=curdirnum=batdn_dir[i];
		strcpy(f.name,batdn_name[i]);
		f.datoffset=batdn_offset[i];
		f.size=batdn_size[i];
		f.altpath=batdn_alt[i];
		if(xfrprot==-1 || checkprotresult(cfg.prot[xfrprot],0,&f)) {
			if(cfg.dir[f.dir]->misc&DIR_TFREE && cur_cps)
				starttime+=f.size/(ulong)cur_cps;
			downloadfile(&f);
			closefile(&f);
			batdn_total--;
			for(j=i;j<batdn_total;j++) {
				strcpy(batdn_name[j],batdn_name[j+1]);
				batdn_dir[j]=batdn_dir[j+1];
				batdn_cdt[j]=batdn_cdt[j+1];
				batdn_alt[j]=batdn_alt[j+1];
				batdn_size[j]=batdn_size[j+1];
				batdn_offset[j]=batdn_offset[j+1]; 
			} 
		}
		else i++; 
	}
}

/****************************************************************************/
/* Adds a list of files to the batch download queue 						*/
/****************************************************************************/
void sbbs_t::batch_add_list(char *list)
{
    char	str[128];
	int		file;
	uint	i,j,k;
    FILE *	stream;
	file_t	f;

	if((stream=fnopen(&file,list,O_RDONLY))!=NULL) {
		bputs(text[SearchingAllLibs]);
		while(!feof(stream)) {
			checkline();
			if(!online)
				break;
			if(!fgets(str,127,stream))
				break;
			truncsp(str);
			sprintf(f.name,"%.12s",str);
			lncntr=0;
			for(i=j=k=0;i<usrlibs;i++) {
				for(j=0;j<usrdirs[i];j++,k++) {
					outchar('.');
					if(k && !(k%5))
						bputs("\b\b\b\b\b     \b\b\b\b\b");
					if(findfile(&cfg,usrdir[i][j],f.name))
						break; 
				}
				if(j<usrdirs[i])
					break; 
			}
			if(i<usrlibs) {
				f.dir=usrdir[i][j];
				getfileixb(&cfg,&f);
				f.size=0;
				getfiledat(&cfg,&f);
				if(f.size==-1L)
					bprintf(text[FileIsNotOnline],f.name);
				else
					addtobatdl(&f); 
			} 
		}
		fclose(stream);
		remove(list);
		CRLF; 
	}
}
/****************************************************************************/
void sbbs_t::batch_create_list()
{
	char	str[MAX_PATH+1];
	int		i;
	FILE*	stream;

	if(batdn_total) {
		sprintf(str,"%sfile/%04u.dwn",cfg.data_dir,useron.number);
		if((stream=fnopen(NULL,str,O_WRONLY|O_TRUNC|O_CREAT))!=NULL) {
			for(i=0;i<(int)batdn_total;i++)
				fprintf(stream,"%s\r\n",batdn_name[i]);
			fclose(stream); 
		} 
	}
}

/**************************************************************************/
/* Add file 'f' to batch download queue. Return 1 if successful, 0 if not */
/**************************************************************************/
bool sbbs_t::addtobatdl(file_t* f)
{
    char	str[256],tmp2[256];
	char 	tmp[512];
    uint	i;
	ulong	totalcdt, totalsize, totaltime;

	if(useron.rest&FLAG('D')) {
		bputs(text[R_Download]);
		return(false); 
	}
	/***
	sprintf(str,"%s%s",f->altpath>0 && f->altpath<=altpaths ? altpath[f->altpath-1]
		: cfg.dir[f->dir]->path,unpadfname(f->name,tmp));
	***/
	for(i=0;i<batdn_total;i++) {
		if(!strcmp(batdn_name[i],f->name) && f->dir==batdn_dir[i]) {
			bprintf(text[FileAlreadyInQueue],f->name);
			return(false); 
		} 
	}
	if(f->size<=0 /* !fexist(str) */) {
		bprintf(text[CantAddToQueue],f->name);
		bputs(text[FileIsNotOnline]);
		return(false); 
	}
	if(batdn_total>=cfg.max_batdn) {
		bprintf(text[CantAddToQueue],f->name);
		bputs(text[BatchDlQueueIsFull]);
		return(false); 
	}
	for(i=0,totalcdt=0;i<batdn_total;i++)
		if(!is_download_free(&cfg,batdn_dir[i],&useron))
			totalcdt+=batdn_cdt[i];
	if(cfg.dir[f->dir]->misc&DIR_FREE) f->cdt=0L;
	if(!is_download_free(&cfg,f->dir,&useron))
		totalcdt+=f->cdt;
	if(totalcdt>useron.cdt+useron.freecdt) {
		bprintf(text[CantAddToQueue],f->name);
		bprintf(text[YouOnlyHaveNCredits],ultoac(useron.cdt+useron.freecdt,tmp));
		return(false); 
	}
	if(!chk_ar(cfg.dir[f->dir]->dl_ar,&useron)) {
		bprintf(text[CantAddToQueue],f->name);
		bputs(text[CantDownloadFromDir]);
		return(false); 
	}
	for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
		totalsize+=batdn_size[i];
		if(!(cfg.dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
			totaltime+=batdn_size[i]/(ulong)cur_cps; 
	}
	totalsize+=f->size;
	if(!(cfg.dir[f->dir]->misc&DIR_TFREE) && cur_cps)
		totaltime+=f->size/(ulong)cur_cps;
	if(!(useron.exempt&FLAG('T')) && totaltime>timeleft) {
		bprintf(text[CantAddToQueue],f->name);
		bputs(text[NotEnoughTimeToDl]);
		return(false); 
	}
	strcpy(batdn_name[batdn_total],f->name);
	batdn_dir[batdn_total]=f->dir;
	batdn_cdt[batdn_total]=f->cdt;
	batdn_offset[batdn_total]=f->datoffset;
	batdn_size[batdn_total]=f->size;
	batdn_alt[batdn_total]=f->altpath;
	batdn_total++;
	openfile(f);
	bprintf(text[FileAddedToBatDlQueue]
		,f->name,batdn_total,cfg.max_batdn,ultoac(totalcdt,tmp)
		,ultoac(totalsize,tmp2)
		,sectostr(totalsize/(ulong)cur_cps,str));
	return(true);
}
