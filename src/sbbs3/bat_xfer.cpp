/* bat_xfer.cpp */

/* Synchronet batch file transfer functions */

/* $Id: bat_xfer.cpp,v 1.41 2020/05/13 23:56:08 rswindell Exp $ */

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

#include "sbbs.h"

/****************************************************************************/
/* This is the batch menu section                                           */
/****************************************************************************/
void sbbs_t::batchmenu()
{
    char	str[129],tmp2[250],done=0,ch;
	char 	tmp[512];
	char	keys[32];
	uint	i,n,xfrprot,xfrdir;
    int64_t	totalcdt,totalsize,totaltime;
    time_t	start,end;
    file_t	f;

	if((batchdn_list_count(&cfg, useron.number) < 1) && (batchup_list_count(&cfg, useron.number) < 1) && cfg.upload_dir==INVALID_DIR) {
		bputs(text[NoFilesInBatchQueue]);
		return; 
	}
	if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT))
		menu("batchxfer");
	lncntr=0;
	while(online && !done && (cfg.upload_dir!=INVALID_DIR || batchdn_list_count(&cfg, useron.number) || batchup_list_count(&cfg, useron.number))) {
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
		SAFEPRINTF(keys,"CDLRU?\r%c", text[YNQP][2]);
		ch=(char)getkeys(keys,0);
		if(ch>' ')
			logch(ch,0);
		if(ch==text[YNQP][2] || ch=='\r') {	/* Quit */
			lncntr=0;
			done=1;
			break;
		}
		switch(ch) {
			case '?':
				if(useron.misc&(EXPERT|RIP|WIP|HTML))
					menu("batchxfr");
				break;
			case 'C':
				if(batchup_list_count(&cfg, useron.number) < 1) {
					bputs(text[UploadQueueIsEmpty]);
				} else {
					if(text[ClearUploadQueueQ][0]==0 || !noyes(text[ClearUploadQueueQ])) {
						if(clearbatul())
							bputs(text[UploadQueueCleared]);
					} 
				}
				if(batchdn_list_count(&cfg, useron.number) <1 ) {
					bputs(text[DownloadQueueIsEmpty]);
				} else {
					if(text[ClearDownloadQueueQ][0]==0 || !noyes(text[ClearDownloadQueueQ])) {
						if(clearbatdl())
							bputs(text[DownloadQueueCleared]); 
					} 
				}
				break;
			case 'D':
				start_batch_download();
				break;
			case 'L':
			{
				if(batup_total) {
					bputs(text[UploadQueueLstHdr]);
					for(i=0;i<batup_total;i++)
						bprintf(text[UploadQueueLstFmt],i+1,batup_name[i]
							,batup_desc[i]); 
				}
				totalsize = 0;
				totalcdt = 0;
				str_list_t ini = batchdn_list_read(&cfg, useron.number);
				str_list_t filenames = iniGetSectionList(ini, NULL);
				if(strListCount(filenames)) {
					bputs(text[DownloadQueueLstHdr]);
					for(size_t i = 0; filenames[i]; ++i) {
						const char* filename = filenames[i];
						int64_t size = iniGetBytes(ini, filename, "size", 1, 0);
						int64_t cost = iniGetBytes(ini, filename, "cost", 1, 0);
						bprintf(text[DownloadQueueLstFmt],i+1
							,filename
							,ultoac((ulong)cost, tmp)
							,ultoac((ulong)size, tmp)
							,cur_cps
								? sectostr((uint)(size/(ulong)cur_cps),tmp2)
								: "??:??:??");
						totalsize += size;
						totalcdt += cost; 
					}
					bprintf(text[DownloadQueueTotals]
						,ultoac((ulong)totalcdt,tmp),ultoac((ulong)totalsize,str),cur_cps
						? sectostr((ulong)totalsize/(ulong)cur_cps,tmp2)
						: "??:??:??"); 
				}
				iniFreeStringList(filenames);
				iniFreeStringList(ini);
				break;  /* Questionable line ^^^, see note above function */
			}
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
							n++; 
						}
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
						SAFECOPY(f.name,batdn_name[n]);
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
				if(batchup_list_count(&cfg, useron.number) < 1 && cfg.upload_dir==INVALID_DIR) {
					bputs(text[UploadQueueIsEmpty]);
					break; 
				}
				xfer_prot_menu(XFER_BATCH_UPLOAD);
				if(!create_batchup_lst())
					break;
				ASYNC;
				mnemonics(text[ProtocolOrQuit]);
				SAFEPRINTF(str,"%c",text[YNQP][2]);
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->batulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
						sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
						SAFECAT(str,tmp); 
					}
				ch=(char)getkeys(str,0);
				if(ch==text[YNQP][2])
					break;
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->batulcmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron,&client))
						break;
				if(i<cfg.total_prots) {
					SAFEPRINTF(str,"%sBATCHUP.LST",cfg.node_dir);
					xfrprot=i;
					if(batup_total)
						xfrdir=batup_dir[0]; // TODO: fix
					else
						xfrdir=cfg.upload_dir;
					action=NODE_ULNG;
					SYNC;
					if(online==ON_REMOTE) {
						delfiles(cfg.temp_dir,ALLFILES);
						start=time(NULL);
						protocol(cfg.prot[xfrprot],XFER_BATCH_UPLOAD,str,nulstr,true);
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
	char	list[1024] = "";
	int		error;
    int		j;
    uint	i,xfrprot;
    time_t	start,end,t;
	struct	tm tm;

	if(useron.rest&FLAG('D')) {     /* Download restriction */
		bputs(text[R_Download]);
		return(FALSE); 
	}

	str_list_t ini = batchdn_list_read(&cfg, useron.number);

	if(!iniGetSectionCount(ini, NULL)) {
		bputs(text[DownloadQueueIsEmpty]);
		iniFreeStringList(ini);
		return(FALSE);
	}
	str_list_t filenames = iniGetSectionList(ini, NULL);

	int64_t totalcdt = 0;
	for(size_t i=0; filenames[i] != NULL; ++i) {
		totalcdt += iniGetBytes(ini, filenames[i], "cost", 1, 0);
	}
	if(totalcdt > useron.cdt+useron.freecdt) {
		bprintf(text[YouOnlyHaveNCredits]
			,ultoac(useron.cdt+useron.freecdt,tmp));
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
		return(FALSE); 
	}

	int64_t totalsize = 0;
	int64_t totaltime = 0;
	for(size_t i=0; filenames[i] != NULL; ++i) {
		char value[INI_MAX_VALUE_LEN + 1];
		int dirnum = getdirnum(&cfg, iniGetString(ini, filenames[i], "dir", NULL, value));
		if(!(cfg.dir[dirnum]->misc&DIR_TFREE) && cur_cps)
			totaltime += iniGetBytes(ini, filenames[i], "size", 1, 0)/(ulong)cur_cps;
		smbfile_t f;
		if(loadfile(&cfg, dirnum, filenames[i], &f)) {
			SAFECAT(list, getfullfilepath(&cfg, &f, path));
			SAFECAT(list, " ");
		}
	}

	if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime > timeleft) {
		bputs(text[NotEnoughTimeToDl]);
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
		return(FALSE); 
	}
	xfer_prot_menu(XFER_BATCH_DOWNLOAD);
	ASYNC;
	mnemonics(text[ProtocolOrQuit]);
	SAFEPRINTF(str,"%c",text[YNQP][2]);
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->batdlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
			sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
			SAFECAT(str,tmp); 
		}
	ungetkey(useron.prot);
	ch=(char)getkeys(str,0);
	if(ch==text[YNQP][2] || sys_status&SS_ABORT)
		return(FALSE);
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->batdlcmd[0] && cfg.prot[i]->mnemonic==ch
			&& chk_ar(cfg.prot[i]->ar,&useron,&client))
			break;
	if(i>=cfg.total_prots || !create_batchdn_lst((cfg.prot[i]->misc&PROT_NATIVE) ? true:false)) {
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
		return(FALSE);
	}
	xfrprot=i;
#if 0 // Download events
	list=NULL;
	for(i=0;i<batdn_total;i++) {
		curdirnum=batdn_dir[i]; 		/* for ARS */
		unpadfname(batdn_name[i],fname);
		if(cfg.dir[batdn_dir[i]]->seqdev) {
			lncntr=0;
			SAFEPRINTF2(path,"%s%s",cfg.temp_dir,fname);
			if(!fexistcase(path)) {
				seqwait(cfg.dir[batdn_dir[i]]->seqdev);
				bprintf(text[RetrievingFile],fname);
				SAFEPRINTF2(str,"%s%s"
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
			SAFEPRINTF2(path,"%s%s"
				,batdn_alt[i]>0 && batdn_alt[i]<=cfg.altpaths
				? cfg.altpath[batdn_alt[i]-1]
				: cfg.dir[batdn_dir[i]]->path
				,fname);
		if(list==NULL)
			list_len=0;
		else
			list_len=strlen(list)+1;	/* add one for ' ' */
		char* np;
		if((np=(char*)realloc(list,list_len+strlen(path)+1	/* add one for '\0'*/))==NULL) {
			free(list);
			errormsg(WHERE,ERR_ALLOC,"list",list_len+strlen(path));
			return(FALSE);
		}
		list = np;
		if(!list_len)
			strcpy(list,path);
		else {
			strcat(list," ");
			strcat(list,path);
		}
		for(j=0;j<cfg.total_dlevents;j++) {
			if(stricmp(cfg.dlevent[j]->ext,batdn_name[i]+9))
				continue;
			if(!chk_ar(cfg.dlevent[j]->ar,&useron,&client))
				continue;
			bputs(cfg.dlevent[j]->workstr);
			external(cmdstr(cfg.dlevent[j]->cmd,path,nulstr,NULL),EX_OUTL);
			CRLF; 
		}
	}
#endif

	SAFEPRINTF(str,"%sBATCHDN.LST",cfg.node_dir);
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
	error=protocol(cfg.prot[xfrprot],XFER_BATCH_DOWNLOAD,str,list,false);
	end=time(NULL);
	if(cfg.prot[xfrprot]->misc&PROT_DSZLOG || !error)
		batch_download(xfrprot);
	if(batchdn_list_count(&cfg, useron.number))
		notdownloaded(totalsize,start,end);
	autohangup();

	return TRUE;
}

/****************************************************************************/
/* Creates the file BATCHDN.LST in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/****************************************************************************/
bool sbbs_t::create_batchdn_lst(bool native)
{
	char	path[MAX_PATH+1];

	SAFEPRINTF(path, "%sBATCHDN.LST", cfg.node_dir);
	FILE* fp = fopen(path, "wb");
	str_list_t ini = batchdn_list_read(&cfg, useron.number);
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */NULL);
	for(size_t i = 0; filenames[i] != NULL; ++i) {
		const char* filename = filenames[i];
		smbfile_t f;
		char value[INI_MAX_VALUE_LEN + 1];
		f.dir = getdirnum(&cfg, iniGetString(ini, filename, "dir", NULL, value));
		if(!loadfile(&cfg, f.dir, filename, &f)) {
			errormsg(WHERE, "loading file", filename, i);
			continue;
		}
		getfullfilepath(&cfg, &f, path);
#ifdef _WIN32
		if(!native) {
			char tmp[MAX_PATH + 1];
			GetShortPathName(path, tmp, sizeof(tmp));
			SAFECOPY(path, tmp);
		}
#endif
		fprintf(fp, "%s\r\n", path);
		freefile(&f);
	}
	fclose(fp);
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	return true;
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

	SAFEPRINTF(str,"%sBATCHUP.LST",cfg.node_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return(false); 
	}
	for(i=0;i<batup_total;i++) {
		if(batup_dir[i]>=cfg.total_dirs)
			SAFECOPY(str,cfg.temp_dir);
		else
			SAFECOPY(str,batup_alt[i]>0 && batup_alt[i]<=cfg.altpaths
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
/* Processes files that were supposed to be received in the batch queue     */
/****************************************************************************/
void sbbs_t::batch_upload()
{
#if 0
    char	str1[MAX_PATH+1],str2[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char 	tmp[MAX_PATH+1];
	uint	i,j,x,y;
    file_t	f;
	DIR*	dir;
	DIRENT*	dirent;

	for(i=0;i<batup_total;) {
		curdirnum=batup_dir[i]; 			/* for ARS */
		lncntr=0;                               /* defeat pause */
		unpadfname(batup_name[i],tmp);
		SAFEPRINTF2(str1,"%s%s",cfg.temp_dir,tmp);
		SAFEPRINTF2(str2,"%s%s",cfg.dir[batup_dir[i]]->path,tmp);
		if(fexistcase(str1) && fexistcase(str2)) { /* file's in two places */
			bprintf(text[FileAlreadyThere],batup_name[i]);
			remove(str1);    /* this is the one received */
			i++;
			continue; 
		}
		if(fexist(str1))
			mv(str1,str2,0);
		SAFECOPY(f.name,batup_name[i]);
		SAFECOPY(f.desc,batup_desc[i]);
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
		SAFEPRINTF2(tmp,"%s%s",cfg.temp_dir,dirent->d_name);
		if(isdir(tmp))
			continue;
		memset(&f,0,sizeof(file_t));
		f.dir=cfg.upload_dir;
		SAFEPRINTF2(path,"%s%s",cfg.dir[f.dir]->path,dirent->d_name);
		if(fexistcase(path)) {
			bprintf(text[FileAlreadyOnline], dirent->d_name);
			continue;
		}
		if(mv(tmp, path, /* copy: */false))
			continue;

#ifdef _WIN32
		GetShortPathName(path, tmp, sizeof(tmp));
#endif
		padfname(getfname(tmp),f.name);

		for(x=0;x<usrlibs;x++) {
			for(y=0;y<usrdirs[x];y++)
				if(cfg.dir[usrdir[x][y]]->misc&DIR_DUPES
					&& findfile(&cfg,usrdir[x][y],f.name))
					break;
			if(y<usrdirs[x])
				break; 
		}
		if(x<usrlibs) {
			bprintf(text[FileAlreadyOnline],f.name);
		} else {
			uploadfile(&f); 
		}
	}
	if(dir!=NULL)
		closedir(dir);
#endif
}

/****************************************************************************/
/* Processes files that were supposed to be sent from the batch queue       */
/* xfrprot is -1 if downloading files from within QWK (no DSZLOG)           */
/****************************************************************************/
void sbbs_t::batch_download(int xfrprot)
{
	char path[MAX_PATH + 1];
	FILE* fp = iniOpenFile(batchdn_list_name(&cfg, useron.number, path, sizeof(path)), /* create: */FALSE);
	str_list_t ini = iniReadFile(fp);
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */NULL);

	for(size_t i = 0; filenames[i] != NULL; ++i) {
		char* filename = filenames[i];
		lncntr=0;                               /* defeat pause */
		if(xfrprot==-1 || checkprotresult(cfg.prot[xfrprot], 0, filename)) {
		    smbfile_t	f;
			char value[INI_MAX_VALUE_LEN + 1];
			f.dir = getdirnum(&cfg, iniGetString(ini, filename, "dir", NULL, value));
			iniRemoveSection(&ini, filename);
			if(!loadfile(&cfg, f.dir, filename, &f)) {
				errormsg(WHERE, "loading file", filename, i);
				continue;
			}
			if(cfg.dir[f.dir]->misc&DIR_TFREE && cur_cps)
				starttime+=f.size/(ulong)cur_cps;
			downloadedfile(&f);
			freefile(&f);
		}
	}
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
}

/****************************************************************************/
/* Adds a list of files to the batch download queue 						*/
/****************************************************************************/
void sbbs_t::batch_add_list(char *list)
{
#if 0 // No use?
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
			truncnl(str);
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
#endif
}

/**************************************************************************/
/* Add file 'f' to batch download queue. Return 1 if successful, 0 if not */
/**************************************************************************/
bool sbbs_t::addtobatdl(smbfile_t* f)
{
    char	str[256],tmp2[256];
	char 	tmp[512];
    uint	i;
	uint64_t	totalcost, totalsize;
	uint64_t	totaltime;

	if(useron.rest&FLAG('D')) {
		bputs(text[R_Download]);
		return false;
	}
	if(!chk_ar(cfg.dir[f->dir]->dl_ar,&useron,&client)) {
		bprintf(text[CantAddToQueue],f->filename);
		bputs(text[CantDownloadFromDir]);
		return false;
	}
	if(getfilesize(&cfg, f) < 1) {
		bprintf(text[CantAddToQueue], f->filename);
		bprintf(text[FileIsNotOnline], f->filename);
		return false;
	}

	char path[MAX_PATH + 1];
	FILE* fp = iniOpenFile((batchdn_list_name(&cfg, useron.number, path, sizeof(path))), /* create: */TRUE);
	str_list_t ini = iniReadFile(fp);

	if(iniSectionExists(ini, f->filename)) {
		bprintf(text[FileAlreadyInQueue], f->filename);
		iniFreeStringList(ini);
		iniCloseFile(fp);
		return false;
	}

	bool result = false;
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */NULL);
	if(strListCount(filenames) >= cfg.max_batdn) {
		bprintf(text[CantAddToQueue] ,f->filename);
		bputs(text[BatchDlQueueIsFull]);
	} else {
		totalcost = 0;
		totaltime = 0;
		totalsize = 0;
		for(i=0; filenames[i] != NULL; ++i) {
			const char* filename = filenames[i];
			char value[INI_MAX_VALUE_LEN + 1];
			uint dirnum = getdirnum(&cfg, iniGetString(ini, filename, "dir", NULL, value));
			totalcost += iniGetBytes(ini, filename, "cost", 1, 0);
			int64_t fsize = iniGetBytes(ini, filename, "size", 1, 0);
			totalsize += fsize;
			if(!(cfg.dir[dirnum]->misc&DIR_TFREE) && cur_cps)
				totaltime += fsize/(ulong)cur_cps; 
		}
		if(cfg.dir[f->dir]->misc&DIR_FREE)
			f->cost=0L;
		if(!is_download_free(&cfg,f->dir,&useron,&client))
			totalcost += f->cost;
		if(totalcost > useron.cdt+useron.freecdt) {
			bprintf(text[CantAddToQueue],f->filename);
			bprintf(text[YouOnlyHaveNCredits],ultoac(useron.cdt+useron.freecdt,tmp));
		} else {
			totalsize += f->size;
			if(!(cfg.dir[f->dir]->misc&DIR_TFREE) && cur_cps)
				totaltime += f->size/(ulong)cur_cps;
			if(!(useron.exempt&FLAG('T')) && totaltime > timeleft) {
				bprintf(text[CantAddToQueue],f->filename);
				bputs(text[NotEnoughTimeToDl]);
			} else {
				iniSetString(&ini, f->filename, "dir", cfg.dir[f->dir]->code, /* style: */NULL);
				iniSetBytes(&ini, f->filename, "cost", /* unit: */1, f->cost, /* style: */NULL);
				iniSetBytes(&ini, f->filename, "size", /* unit: */1, getfilesize(&cfg, f), /* style: */NULL);
				bprintf(text[FileAddedToBatDlQueue]
					,f->filename, strListCount(filenames) + 1, cfg.max_batdn, ultoac((ulong)totalcost,tmp)
					,ultoac((ulong)totalsize,tmp2)
					,sectostr((ulong)totalsize/(ulong)cur_cps,str));
				result = iniWriteFile(fp, ini);
			}
		}
	}
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	iniCloseFile(fp);
	return result;
}

bool sbbs_t::clearbatdl(void)
{
	char path[MAX_PATH + 1];
	batchdn_list_name(&cfg, useron.number, path, sizeof(path));
	return remove(path) == 0;
}

bool sbbs_t::clearbatul(void)
{
	char path[MAX_PATH + 1];
	batchup_list_name(&cfg, useron.number, path, sizeof(path));
	return remove(path) == 0;
}
