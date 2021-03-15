/* Synchronet temp directory file transfer routines */

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

/*****************************************************************************/
/* Temp directory section. Files must be extracted here and both temp_uler   */
/* and temp_uler fields should be filled before entrance.                    */
/*****************************************************************************/
void sbbs_t::temp_xfer()
{
#if 0	// TODO
    char	str[256],tmp2[256],done=0,ch;
	char 	tmp[512];
	int		error;
    uint	i,dirnum=cfg.total_dirs,files;
    ulong	bytes;
	ulong	space;
    time_t	start,end,t;
    file_t	f;
	glob_t	g;
	struct	tm tm;

	if(!usrlibs)
		return;
	if(useron.rest&FLAG('D')) {
		bputs(text[R_Download]);
		return; 
	}
	/*************************************/
	/* Create TEMP directory information */
	/*************************************/
	if((cfg.dir[dirnum]=(dir_t *)malloc(sizeof(dir_t)))==0) {
		errormsg(WHERE,ERR_ALLOC,"temp_dir",sizeof(dir_t));
		return; 
	}
	memset(cfg.dir[dirnum],0,sizeof(dir_t));
	SAFECOPY(cfg.dir[dirnum]->lname,"Temporary");
	SAFECOPY(cfg.dir[dirnum]->sname,"Temp");
	SAFECOPY(cfg.dir[dirnum]->code,"TEMP");
	SAFECOPY(cfg.dir[dirnum]->path,cfg.temp_dir);
	SAFECOPY(cfg.dir[dirnum]->data_dir,cfg.dir[0]->data_dir);
	cfg.dir[dirnum]->maxfiles=MAX_FILES;
	temp_dirnum=curdirnum=usrdir[curlib][curdir[curlib]];
	cfg.total_dirs++;

	/****************************/
	/* Fill filedat information */
	/****************************/
	memset(&f,0,sizeof(f));
	SAFEPRINTF2(f.name,"temp_%3.3d.%s",cfg.node_num,useron.tmpext);
	SAFECOPY(f.desc,"Temp File");
	f.dir=dirnum;

	if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT))
		menu("tempxfer");
	lncntr=0;
	while(online && !done) {
		if(!(useron.misc&(EXPERT|RIP|WIP|HTML))) {
			sys_status&=~SS_ABORT;
			if(lncntr) {
				SYNC;
				CRLF;
				if(lncntr)          /* CRLF or SYNC can cause pause */
					pause(); 
			}
			menu("tempxfer"); 
		}
		ASYNC;
		bputs(text[TempDirPrompt]);
		SAFECOPY(f.uler,temp_uler);
		ch=(char)getkeys("ADEFNILQRVX?\r",0);
		if(ch>' ')
			logch(ch,0);
		switch(ch) {
			case 'A':   /* add to temp file */
				if(!isdir(cfg.temp_dir)) {
					bprintf(text[DirectoryDoesNotExist], cfg.temp_dir);
					lprintf(LOG_ERR,"Temp directory does not exist: %s", cfg.temp_dir);
					break;
				}
				/* free disk space */
				space=getfreediskspace(cfg.temp_dir,1024);
				if(space<(ulong)cfg.min_dspace) {
					bputs(text[LowDiskSpace]);
					lprintf(LOG_ERR,"Diskspace is low: %s (%lu kilobytes)"
						,cfg.temp_dir,space);
					if(!dir_op(dirnum))
						break; 
				}
				bprintf(text[DiskNBytesFree],ultoac(space,tmp));
				if(!getfilespec(str))
					break;
				if(!checkfname(str))
					break;
				SAFEPRINTF2(tmp2,"added %s to %s"
					,str,f.name);
				logline(nulstr,tmp2);
				SAFEPRINTF2(tmp2,"%s%s",cfg.temp_dir,str);
				SAFEPRINTF2(str,"%s%s",cfg.temp_dir,f.name);
				external(cmdstr(temp_cmd(),str,tmp2,NULL),EX_WILDCARD|EX_STDOUT);
				break;
			case 'D':   /* download from temp dir */
				SAFEPRINTF2(str,"%s%s",cfg.temp_dir,f.name);
				if(!fexist(str)) {
					bprintf(text[TempFileNotCreatedYet],f.name);
					break; 
				}
				f.size=f.cdt=(long)flength(str);
				f.opencount=0;
				if(temp_cdt)    /* if file was not free */
					f.cdt=f.size;
				else
					f.cdt=0;
				if(!(useron.exempt&FLAG('D'))
					&& f.cdt>useron.cdt+useron.freecdt) {
					bprintf(text[YouOnlyHaveNCredits]
						,ultoac(useron.cdt+useron.freecdt,tmp));
					break;     /* f.cdt must equal size here */
				}
				if(!(useron.exempt&FLAG('T')) && !dir_op(dirnum)
					&& !(cfg.dir[temp_dirnum]->misc&DIR_TFREE) && cur_cps
					&& f.size/(ulong)cur_cps>timeleft) {
					bputs(text[NotEnoughTimeToDl]);
					break; 
				}
				if(!chk_ar(cfg.dir[temp_dirnum]->dl_ar,&useron,&client)) {
					bputs(text[CantDownloadFromDir]);
					break; 
				}
				addfiledat(&cfg,&f);
				xfer_prot_menu(XFER_DOWNLOAD);
				SYNC;
				mnemonics(text[ProtocolOrQuit]);
				SAFECOPY(tmp2,"Q");
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
						sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
						SAFECAT(tmp2,tmp); 
					}
				ungetkey(useron.prot);
				ch=(char)getkeys(tmp2,0);
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron,&client))
						break;
				if(i<cfg.total_prots) {
					getnodedat(cfg.node_num,&thisnode,1);
					action=NODE_DLNG;
					t=now;
					if(cur_cps) 
						t+=(f.size/(ulong)cur_cps);
					if(localtime_r(&t,&tm)==NULL)
						break;
					thisnode.aux=(tm.tm_hour*60)+tm.tm_min;

					putnodedat(cfg.node_num,&thisnode); /* calculate ETA */
					start=time(NULL);
					error=protocol(cfg.prot[i],XFER_DOWNLOAD,str,nulstr,false);
					end=time(NULL);
					if(cfg.dir[temp_dirnum]->misc&DIR_TFREE)
						starttime+=end-start;
					if(checkprotresult(cfg.prot[i],error,&f))
						downloadedfile(&f);
					else
						notdownloaded(f.size,start,end);
					autohangup(); 
				}
				removefiledat(&cfg,&f);
				break;
			case 'E':
				extract(usrdir[curlib][curdir[curlib]]);
				sys_status&=~SS_ABORT;
				break;
			case 'F':   /* Create a file list */
				delfiles(cfg.temp_dir,ALLFILES);
				create_filelist("FILELIST.TXT",0);
				if(!(sys_status&SS_ABORT))
					logline(nulstr,"Created list of all files");
				CRLF;
				sys_status&=~SS_ABORT;
				break;
			case 'I':   /* information on what's here */
				bprintf(text[TempFileInfo],f.uler,temp_file);
				break;
			case 'L':   /* list files in dir */
				if(!getfilespec(str))
					break;
				if(!checkfname(str))
					break;
				bytes=files=0L;
				CRLF;
				SAFEPRINTF2(tmp2,"%s%s",cfg.temp_dir,str);
				glob(tmp2,0,NULL,&g);
				for(i=0;i<(uint)g.gl_pathc && !msgabort();i++) {
					if(isdir(g.gl_pathv[i]))
						continue;
					t=fdate(g.gl_pathv[i]);
					bprintf("%-25s %15s   %s\r\n",getfname(g.gl_pathv[i])
						,ultoac((long)flength(g.gl_pathv[i]),tmp)
						,timestr(t));
					files++;
					bytes+=(long)flength(g.gl_pathv[i]);
				}
				globfree(&g);
				if(!files)
					bputs(text[EmptyDir]);
				else if(files>1)
					bprintf(text[TempDirTotal],ultoac(bytes,tmp),files);
				break;
			case 'N':   /* Create a list of new files */
				delfiles(cfg.temp_dir,ALLFILES);
				create_filelist("NEWFILES.TXT",FL_ULTIME);
				if(!(sys_status&SS_ABORT))
					logline(nulstr,"Created list of new files");
				CRLF;
				sys_status&=~SS_ABORT;
				break;
			case 'R':   /* Remove files from dir */
				if(!getfilespec(str) || !checkfname(str))
					break;
				bprintf(text[NFilesRemoved],delfiles(cfg.temp_dir,str));
				break;
			case 'V':   /* view files in dir */
				bputs(text[FileSpec]);
				if(!getstr(str,64,K_NONE) || !checkfname(str))
					break;
				viewfiles(dirnum,str);
				break;
			case CR:
			case 'Q':   /* quit */
				done=1;
				break;
			case 'X':   /* extract from archive in temp dir */
				extract(dirnum);
				sys_status&=~SS_ABORT;
				break;
			case '?':   /* menu */
				if(useron.misc&(EXPERT|RIP|WIP|HTML))
					menu("tempxfer");
				break; 
		}
		if(sys_status&SS_ABORT)
			break; 
	}
	free(cfg.dir[dirnum]);
	cfg.total_dirs--;
#endif
}

/*****************************************************************************/
/* Handles extraction from a normal transfer file to the temp directory      */
/*****************************************************************************/
void sbbs_t::extract(uint dirnum)
{
#if 0 // NFB-TODO
    char	fname[13],str[256],excmd[256],path[256],done
				,tmp[256],intmp=0;
    uint	i,j;
	ulong	space;
    file_t	f;
	DIR*	dir;
	DIRENT*	dirent;

	temp_dirnum=curdirnum=dirnum;
	if(!strcmp(cfg.dir[dirnum]->code,"TEMP"))
		intmp=1;

	if(!isdir(cfg.temp_dir)) {
		bprintf(text[DirectoryDoesNotExist], cfg.temp_dir);
		lprintf(LOG_ERR,"Temp directory does not exist: %s", cfg.temp_dir);
		return;
	}

	/* get free disk space */
	space=getfreediskspace(cfg.temp_dir,1024);
	if(space<(ulong)cfg.min_dspace) {
		bputs(text[LowDiskSpace]);
		lprintf(LOG_ERR,"Diskspace is low: %s (%lu kilobytes)",cfg.temp_dir,space);
		if(!dir_op(dirnum))
			return; 
	}
	else if(!intmp) {   /* not in temp dir */
		CRLF; 
	}
	bprintf(text[DiskNBytesFree],ultoac(space,tmp));

	if(!intmp) {    /* not extracting FROM temp directory */
		SAFEPRINTF2(str,"%s%s",cfg.temp_dir,ALLFILES);
		if(fexist(str)) {
			bputs(text[RemovingTempFiles]);
			dir=opendir(cfg.temp_dir);
			while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
				SAFEPRINTF2(str,"%s%s",cfg.temp_dir,dirent->d_name);
        		if(!isdir(str))
					remove(str);
			}
			if(dir!=NULL)
				closedir(dir);
			CRLF; 
		} 
	}
	bputs(text[ExtractFrom]);
	if(!getstr(fname,sizeof(fname)-1,K_NONE) || !checkfname(fname) || strchr(fname,'*')
		|| strchr(fname,'?'))
		return;
	padfname(fname,f.name);
	SAFECOPY(str,f.name);
	truncsp(str);
	for(i=0;i<cfg.total_fextrs;i++)
		if(!stricmp(str+9,cfg.fextr[i]->ext) && chk_ar(cfg.fextr[i]->ar,&useron,&client)) {
			SAFECOPY(excmd,cfg.fextr[i]->cmd);
			break; 
		}
	if(i==cfg.total_fextrs) {
		bputs(text[UnextractableFile]);
		return; 
	}
	if(!intmp && !findfile(&cfg, dirnum, f.name, NULL)) {    /* not temp dir */
		bputs(text[SearchingAllDirs]);
		for(i=0;i<usrdirs[curlib] && !msgabort();i++) {
			if(i==dirnum) continue;
			if(findfile(&cfg, usrdir[curlib][i], f.name, NULL))
				break; 
		}
		if(i==usrdirs[curlib]) { /* not found in cur lib */
			bputs(text[SearchingAllLibs]);
			for(i=j=0;i<usrlibs;i++) {
				if(i==curlib) continue;
				for(j=0;j<usrdirs[i] && !msgabort();j++)
					if(findfile(&cfg, usrdir[i][j], f.name, NULL))
						break;
				if(j<usrdirs[i])
					break; 
			}
			if(i==usrlibs) {
				bputs(text[FileNotFound]);  /* not in database */
				return; 
			}
			dirnum=usrdir[i][j]; 
		}
		else
			dirnum=usrdir[curlib][i]; 
	}
	if(sys_status&SS_ABORT)
		return;
	SAFEPRINTF2(path,"%s%s",cfg.dir[dirnum]->path,fname);
	if(!intmp) {    /* not temp dir, so get temp_file info */
		f.datoffset=f.dateuled=f.datedled=0L;
		f.dir=dirnum;
		getfileixb(&cfg,&f);
		if(!f.datoffset && !f.dateuled && !f.datedled)  /* error reading ixb */
			return;
		f.size=0;
		getfiledat(&cfg,&f);
		fileinfo(&f);
		if(f.altpath>0 && f.altpath<=cfg.altpaths)
			SAFEPRINTF2(path,"%s%s",cfg.altpath[f.altpath-1],fname);
		temp_dirnum=dirnum;
		if(cfg.dir[f.dir]->misc&DIR_FREE)
			temp_cdt=0L;
		else
			temp_cdt=f.cdt;
		SAFECOPY(temp_uler,f.uler);
		SAFECOPY(temp_file,f.name);      /* padded filename */
	}
	if(!fexistcase(path)) {
		bprintf(text[FileDoesNotExist],path);  /* not on disk */
		return; 
	}
	done=0;
	while(online && !done) {
		mnemonics(text[ExtractFilesPrompt]);
		switch(getkeys("EVQ",0)) {
			case 'E':
				if(!getfilespec(str))
					break;
				if(!checkfname(str))
					break;
				if((i=external(cmdstr(excmd,path,str,NULL),EX_STDIO))!=0) {
					errormsg(WHERE,ERR_EXEC,cmdstr(excmd,path,str,NULL),i);
					return; 
				}
				SAFEPRINTF2(tmp,"extracted %s from %s", str,path);
				logline(nulstr,tmp);
				CRLF;
				break;
			case 'V':
				viewfiles(dirnum,fname);
				break;
			default:
				done=1;
				break; 
		} 
	}
#endif
}

/****************************************************************************/
/* Creates a text file named NEWFILES.DAT in the temp directory that        */
/* all new files since p-date. Returns number of files in list.             */
/****************************************************************************/
ulong sbbs_t::create_filelist(const char *name, long mode)
{
    char	str[256];
	FILE*	fp;
	uint	i,j,d;
	ulong	l,k;

	if(online == ON_REMOTE)
		bprintf(text[CreatingFileList],name);
	SAFEPRINTF2(str,"%s%s",cfg.temp_dir,name);
	if((fp = fopen(str,"ab")) == NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY|O_APPEND);
		return(0);
	}
	k=0;
	if(mode&FL_ULTIME) {
		fprintf(fp, "New files since: %s\r\n", timestr(ns_time));
	}
	for(i=j=d=0;i<usrlibs;i++) {
		for(j=0;j<usrdirs[i];j++,d++) {
			outchar('.');
			if(d && !(d%5))
				bputs("\b\b\b\b\b     \b\b\b\b\b");
			if(mode&FL_ULTIME /* New-scan */
				&& (cfg.lib[usrlib[i]]->offline_dir==usrdir[i][j]
				|| cfg.dir[usrdir[i][j]]->misc&DIR_NOSCAN))
				continue;
			l=listfiles(usrdir[i][j], nulstr, fp, mode);
			if((long)l==-1)
				break;
			k+=l;
		}
		if(j<usrdirs[i])
			break;
	}
	if(k>1) {
		fprintf(fp,"\r\n%ld Files Listed.\r\n",k);
	}
	fclose(fp);
	if(k)
		bprintf(text[CreatedFileList],name);
	else {
		if(online == ON_REMOTE)
			bputs(text[NoFiles]);
		SAFEPRINTF2(str,"%s%s",cfg.temp_dir,name);
		remove(str);
	}
	SAFECOPY(temp_file,name);
	SAFECOPY(temp_uler,"File List");
	return(k);
}

/****************************************************************************/
/* This function returns the command line for the temp file extension for	*/
/* current user online. 													*/
/****************************************************************************/
const char* sbbs_t::temp_cmd(void)
{
	int i;

	if(!cfg.total_fcomps) {
		errormsg(WHERE,ERR_CHK,"compressible file types",0);
		return(nulstr); 
	}
	for(i=0;i<cfg.total_fcomps;i++)
		if(!stricmp(useron.tmpext,cfg.fcomp[i]->ext)
			&& chk_ar(cfg.fcomp[i]->ar,&useron,&client))
			return(cfg.fcomp[i]->cmd);
	return(cfg.fcomp[0]->cmd);
}
