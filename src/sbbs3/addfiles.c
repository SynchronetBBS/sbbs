/* addfiles.c */

/* Program to add files to a Synchronet file database */

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

#define ADDFILES_VER "3.00"

char *crlf="\r\n";

scfg_t scfg;

int cur_altpath=0;

long files=0,removed=0,mode=0;

#define DEL_LIST	(1L<<0)
#define NO_EXTEND	(1L<<1)
#define FILE_DATE	(1L<<2)
#define TODAYS_DATE (1L<<3)
#define FILE_ID 	(1L<<4)
#define NO_UPDATE	(1L<<5)
#define NO_NEWDATE	(1L<<6)
#define ASCII_ONLY	(1L<<7)
#define UL_STATS	(1L<<8)
#define ULDATE_ONLY (1L<<9)
#define KEEP_DESC	(1L<<10)
#define AUTO_ADD	(1L<<11)
#define SEARCH_DIR	(1L<<12)
#define SYNC_LIST	(1L<<13)
#define KEEP_SPACE	(1L<<14)

long lputs(char FAR16 *str)
{
    char tmp[512];
    int i,j,k;

	j=strlen(str);
	for(i=k=0;i<j;i++)      /* remove CRs */
		if(str[i]==CR && str[i+1]==LF)
			continue;
		else
			tmp[k++]=str[i];
	tmp[k]=0;
	return(fputs(tmp,stdout));
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[512];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	lputs(sbuf);
	return(chcount);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *cmdstr(char *instr, char *fpath, char *fspec, char *outstr)
{
    static char cmd[MAX_PATH+1];
    char str[MAX_PATH+1];
    int i,j,len;
#ifdef _WIN32
	char sfpath[MAX_PATH+1];
#endif

	len=strlen(instr);
	for(i=j=0;i<len && j<128;i++) {
		if(instr[i]=='%') {
			i++;
			cmd[j]=0;
			switch(toupper(instr[i])) {
				case 'F':   /* File path */
					strcat(cmd,fpath);
					break;
				case '~':	/* DOS-compatible (8.3) filename */
#ifdef _WIN32
					SAFECOPY(sfpath,fpath);
					GetShortPathName(fpath,sfpath,sizeof(sfpath));
					strcat(cmd,sfpath);
#else
                    strcat(cmd,fpath);
#endif			
					break;
				case 'G':   /* Temp directory */
					strcat(cmd,scfg.temp_dir);
					break;
                case 'J':
                    strcat(cmd,scfg.data_dir);
                    break;
                case 'K':
                    strcat(cmd,scfg.ctrl_dir);
                    break;
				case 'N':   /* Node Directory (same as SBBSNODE environment var) */
					strcat(cmd,scfg.node_dir);
					break;
				case 'S':   /* File Spec */
					strcat(cmd,fspec);
					break;
                case 'Z':
                    strcat(cmd,scfg.text_dir);
                    break;
				case '!':   /* EXEC Directory */
					strcat(cmd,scfg.exec_dir);
					break;
				case '#':   /* Node number (same as SBBSNNUM environment var) */
					sprintf(str,"%d",scfg.node_num);
					strcat(cmd,str);
					break;
				case '%':   /* %% for percent sign */
					strcat(cmd,"%");
					break;
				default:    /* unknown specification */
					break; 
			}
			j=strlen(cmd); 
		}
		else
			cmd[j++]=instr[i]; 
	}
	cmd[j]=0;

	return(cmd);
}

/****************************************************************************/
/* Updates dstst.dab file													*/
/****************************************************************************/
void updatestats(ulong size)
{
    char	str[MAX_PATH+1];
    int		file;
	ulong	l;

	sprintf(str,"%sdsts.dab",scfg.ctrl_dir);
	if((file=nopen(str,O_RDWR))==-1) {
		printf("ERR_OPEN %s\n",str);
		return; 
	}
	lseek(file,20L,SEEK_SET);	/* Skip timestamp, logons and logons today */
	read(file,&l,4);			/* Uploads today		 */
	l++;
	lseek(file,-4L,SEEK_CUR);
	write(file,&l,4);
	read(file,&l,4);			/* Upload bytes today	 */
	l+=size;
	lseek(file,-4L,SEEK_CUR);
	write(file,&l,4);
	close(file);
}

void addlist(char *inpath, file_t f, uint dskip, uint sskip)
{
	char str[MAX_PATH+1];
	char tmp[MAX_PATH+1];
	char fname[MAX_PATH+1];
	char listpath[MAX_PATH+1];
	char filepath[MAX_PATH+1];
	char curline[256],nextline[256];
	char *p;
	uchar ext[1024],tmpext[513];
	int i,file;
	long l;
	BOOL exist;
	FILE *stream;
	DIR*	dir;
	DIRENT*	dirent;

	if(mode&SEARCH_DIR) {
		strcpy(str,cur_altpath ? scfg.altpath[cur_altpath-1] : scfg.dir[f.dir]->path);
		printf("Searching %s\n\n",str);
		dir=opendir(str);

		while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
			sprintf(filepath,"%s%s",str,dirent->d_name);
			f.misc=0;
			f.desc[0]=0;
			f.cdt=flength(filepath);
			padfname(dirent->d_name,f.name);
			printf("%s  %10lu  %s\n"
				,f.name,f.cdt,unixtodstr(&scfg,fdate(filepath),str));
			exist=findfile(&scfg,f.dir,f.name);
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				getfileixb(&scfg,&f);
				if(mode&ULDATE_ONLY) {
					f.dateuled=time(NULL);
					update_uldate(&scfg, &f);
					continue; 
				} 
			}

			if(mode&FILE_DATE) {		/* get the file date and put into desc */
				unixtodstr(&scfg,fdate(filepath),f.desc);
				strcat(f.desc,"  "); 
			}

			if(mode&TODAYS_DATE) {		/* put today's date in desc */
				unixtodstr(&scfg,time(NULL),f.desc);
				strcat(f.desc,"  "); 
			}

			if(mode&FILE_ID) {
				for(i=0;i<scfg.total_fextrs;i++)
					if(!stricmp(scfg.fextr[i]->ext,f.name+9))
						break;
				if(i<scfg.total_fextrs) {
					sprintf(tmp,"%sFILE_ID.DIZ",scfg.temp_dir);
					remove(tmp);
					system(cmdstr(scfg.fextr[i]->cmd,filepath,"FILE_ID.DIZ",NULL));
					if(!fexistcase(tmp)) {
						sprintf(tmp,"%sDESC.SDI",scfg.temp_dir);
						remove(tmp);
						system(cmdstr(scfg.fextr[i]->cmd,filepath,"DESC.SDI",NULL)); 
						fexistcase(tmp);
					}
					if((file=nopen(tmp,O_RDONLY))!=-1) {
						memset(ext,0,513);
						read(file,ext,512);
						for(i=512;i;i--)
							if(ext[i-1]>SP)
								break;
						ext[i]=0;
						if(mode&ASCII_ONLY)
							strip_exascii(ext);
						if(!(mode&KEEP_DESC)) {
							sprintf(tmpext,"%.256s",ext);
							strip_ctrl(tmpext);
							for(i=0;tmpext[i];i++)
								if(isalpha(tmpext[i]))
									break;
							sprintf(f.desc,"%.*s",LEN_FDESC,tmpext+i);
							for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
								;
							f.desc[i]=0; }
						close(file);
						f.misc|=FM_EXTDESC; 
					} 
				} 
			}

			f.dateuled=time(NULL);
			f.altpath=cur_altpath;
			strip_ctrl(f.desc);
			if(mode&ASCII_ONLY)
				strip_exascii(f.desc);
			if(exist) {
				putfiledat(&scfg,&f);
				if(!(mode&NO_NEWDATE))
					update_uldate(&scfg, &f); }
			else
				addfiledat(&scfg,&f);
			if(f.misc&FM_EXTDESC) {
				truncsp(ext);
				putextdesc(&scfg,f.dir,f.datoffset,ext); }

			if(mode&UL_STATS)
				updatestats(f.cdt);
			files++; 
		}
		if(dir!=NULL)
			closedir(dir);
		return; 
	}


	strcpy(listpath,inpath);
	fexistcase(listpath);
	if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
		sprintf(listpath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[f.dir]->path,inpath);
		if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
			printf("Can't open: %s\n"
				   "        or: %s\n",inpath,listpath);
			return; } }

	printf("Adding %s to %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[f.dir]->lib]->sname,scfg.dir[f.dir]->sname);

	fgets(nextline,255,stream);
	while(!feof(stream) && !ferror(stream)) {
		f.misc=0;
		f.desc[0]=0;
		strcpy(curline,nextline);
		nextline[0]=0;
		fgets(nextline,255,stream);
		truncsp(curline);
		if(curline[0]<=SP || (uchar)curline[0]>=0x7e)
			continue;
		printf("%s\n",curline);
		strcpy(fname,curline);

		p=strchr(fname,'.');
		if(!p || p==fname || p>fname+8)    /* no dot or invalid dot location */
			continue;
		p=strchr(p,SP);
		if(p) *p=0;
		else				   /* no space after filename? */
			continue;
		strupr(fname);
		strcpy(fname,unpadfname(fname,tmp));

		padfname(fname,f.name);
		if(strcspn(f.name,"\\/|<>+[]:=\";,")!=strlen(f.name))
			continue;

		for(i=0;i<12;i++)
			if(f.name[i]<SP || (uchar)f.name[i]>0x7e)
				break;

		if(i<12)					/* Ctrl chars or EX-ASCII in filename? */
			continue;
		exist=findfile(&scfg,f.dir,f.name);
		if(exist) {
			if(mode&NO_UPDATE)
				continue;
			getfileixb(&scfg,&f);
			if(mode&ULDATE_ONLY) {
				f.dateuled=time(NULL);
				update_uldate(&scfg, &f);
				continue; } }

		sprintf(filepath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
			: scfg.dir[f.dir]->path,fname);

		if(mode&FILE_DATE) {		/* get the file date and put into desc */
			l=fdate(filepath);
			unixtodstr(&scfg,l,f.desc);
			strcat(f.desc,"  "); }

		if(mode&TODAYS_DATE) {		/* put today's date in desc */
			l=time(NULL);
			unixtodstr(&scfg,l,f.desc);
			strcat(f.desc,"  "); }

		if(dskip && strlen(curline)>=dskip) p=curline+dskip;
		else {
			p++;
			while(*p==SP) p++; }
		sprintf(tmp,"%.*s",(int)(LEN_FDESC-strlen(f.desc)),p);
		strcat(f.desc,tmp);

		if(nextline[0]==SP || strlen(p)>LEN_FDESC) {	/* ext desc */
			if(!(mode&NO_EXTEND)) {
				memset(ext,0,513);
				f.misc|=FM_EXTDESC;
				sprintf(ext,"%s\r\n",p); }

			if(nextline[0]==SP) {
				strcpy(str,nextline);				   /* tack on to end of desc */
				p=str+dskip;
				i=LEN_FDESC-strlen(f.desc);
				if(i>1) {
					p[i-1]=0;
					truncsp(p);
					if(p[0]) {
						strcat(f.desc," ");
						strcat(f.desc,p); } } }

			while(!feof(stream) && !ferror(stream) && strlen(ext)<512) {
				if(nextline[0]!=SP)
					break;
				truncsp(nextline);
				printf("%s\n",nextline);
				if(!(mode&NO_EXTEND)) {
					f.misc|=FM_EXTDESC;
					p=nextline+dskip;
					while(*p==SP) p++;
					strcat(ext,p);
					strcat(ext,"\r\n"); }
				nextline[0]=0;
				fgets(nextline,255,stream); } }


		if(sskip) l=atol(fname+sskip);
		else {
			l=flength(filepath);
			if(l<1L) {
				printf("%s not found.\n",filepath);
				continue; } }

		if(mode&FILE_ID) {
			for(i=0;i<scfg.total_fextrs;i++)
				if(!stricmp(scfg.fextr[i]->ext,f.name+9))
					break;
			if(i<scfg.total_fextrs) {
				sprintf(tmp,"%sFILE_ID.DIZ",scfg.temp_dir);
				remove(tmp);
				system(cmdstr(scfg.fextr[i]->cmd,filepath,"FILE_ID.DIZ",NULL));
				if(!fexistcase(tmp)) {
					sprintf(tmp,"%sDESC.SDI",scfg.temp_dir);
					remove(tmp);
					system(cmdstr(scfg.fextr[i]->cmd,filepath,"DESC.SDI",NULL)); 
					fexistcase(tmp);
				}
				if((file=nopen(tmp,O_RDONLY))!=-1) {
					memset(ext,0,513);
					read(file,ext,512);
					for(i=512;i;i--)
						if(ext[i-1]>SP)
							break;
					ext[i]=0;
					if(mode&ASCII_ONLY)
						strip_exascii(ext);
					if(!(mode&KEEP_DESC)) {
						sprintf(tmpext,"%.256s",ext);
						strip_ctrl(tmpext);
						for(i=0;tmpext[i];i++)
							if(isalpha(tmpext[i]))
								break;
						sprintf(f.desc,"%.*s",LEN_FDESC,tmpext+i);
						for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
							;
						f.desc[i]=0; }
					close(file);
					f.misc|=FM_EXTDESC; 
				} 
			} 
		}

		f.cdt=l;
		f.dateuled=time(NULL);
		f.altpath=cur_altpath;
		strip_ctrl(f.desc);
		if(mode&ASCII_ONLY)
			strip_exascii(f.desc);
		if(exist) {
			putfiledat(&scfg,&f);
			if(!(mode&NO_NEWDATE))
				update_uldate(&scfg, &f); }
		else
			addfiledat(&scfg,&f);
		if(f.misc&FM_EXTDESC) {
			truncsp(ext);
			putextdesc(&scfg,f.dir,f.datoffset,ext); }

		if(mode&UL_STATS)
			updatestats(l);
		files++; }
	fclose(stream);
	if(mode&DEL_LIST && !(mode&SYNC_LIST)) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath); }

}

void synclist(char *inpath, int dirnum)
{
	uchar	str[1024];
	char	fname[MAX_PATH+1];
	char	listpath[MAX_PATH+1];
	uchar*	ixbbuf;
	uchar*	p;
	int		i,file,found;
	long	l,m,length;
	FILE*	stream;
	file_t	f;

	sprintf(str,"%s%s.ixb",scfg.dir[dirnum]->data_dir,scfg.dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))==-1) {
		printf("ERR_OPEN %s\n",str);
		return; }
	length=filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		printf("ERR_LEN (%ld) of %s\n",length,str);
		return; }
	if((ixbbuf=(uchar HUGE16 *)MALLOC(length))==NULL) {
		close(file);
		printf("ERR_ALLOC %s\n",str);
		return; }
	if(lread(file,ixbbuf,length)!=length) {
		close(file);
		FREE((char *)ixbbuf);
		printf("ERR_READ %s\n",str);
		return; }
	close(file);

	strcpy(listpath,inpath);
	if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
		sprintf(listpath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[dirnum]->path,inpath);
		if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
			printf("Can't open: %s\n"
				   "        or: %s\n",inpath,listpath);
			return; } }

	printf("\nSynchronizing %s with %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[dirnum]->lib]->sname,scfg.dir[dirnum]->sname);

	for(l=0;l<length;l+=F_IXBSIZE) {
		m=l;
		for(i=0;i<12 && l<length;i++)
			if(i==8)
				str[i]='.';
			else
				str[i]=ixbbuf[m++]; 	/* Turns FILENAMEEXT into FILENAME.EXT */
		str[i]=0;
		unpadfname(str,fname);
		rewind(stream);
		found=0;
		while(!found) {
			if(!fgets(str,1000,stream))
				break;
			truncsp(str);
			p=strchr(str,SP);
			if(p) *p=0;
			if(!stricmp(str,fname))
				found=1; }
		if(found)
			continue;
		padfname(fname,f.name);
		printf("%s not found in list - ",f.name);
		f.dir=dirnum;
		f.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
		getfiledat(&scfg,&f);
		if(f.opencount) {
			printf("currently OPEN by %u users\n",f.opencount);
			continue; }
		removefiledat(&scfg,&f);
		sprintf(str,"%s%s"
			,f.altpath>0 && f.altpath<=scfg.altpaths ? scfg.altpath[f.altpath-1]
			: scfg.dir[f.dir]->path,fname);
		if(remove(str))
			printf("Error removing %s\n",str);
		removed++;
		printf("Removed from database\n"); }

	if(mode&DEL_LIST) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath); }
}

char *usage="\nusage: addfiles code [.alt_path] [/opts] [\"*user\"] +list "
			 "[desc_off] [size_off]"
	"\n   or: addfiles code [.alt_path] [/opts] [\"*user\"]  file "
		"\"description\"\n"
	"\navailable opts:"
	"\n       a    import ASCII only (no extended ASCII)"
	"\n       b    synchronize database with file list (use with caution)"
	"\n       c    do not remove extra spaces from file description"
	"\n       d    delete list after import"
	"\n       e    do not import extended descriptions"
	"\n       f    include file date in descriptions"
	"\n       t    include today's date in descriptions"
	"\n       i    include added files in upload statistics"
	"\n       n    do not update information for existing files"
	"\n       o    update upload date only for existing files"
	"\n       u    do not update upload date for existing files"
	"\n       z    check for and import FILE_ID.DIZ and DESC.SDI"
	"\n       k    keep original short description (not DIZ)"
	"\n       s    search directory for files (no file list)"
	"\n"
	"\nAuto-ADD:   use * in place of code for Auto-ADD of FILES.BBS"
	"\n            use *filename to Auto-ADD a different filename"
	"\n"
	;

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	char error[512];
	char revision[16];
	char str[MAX_PATH+1];
	char tmp[MAX_PATH+1];
	uchar *p,exist,listgiven=0,namegiven=0,ext[513]
		,auto_name[MAX_PATH+1]="FILES.BBS";
	int i,j,file;
	long l;
	file_t	f;

	sscanf("$Revision$" + 11, "%s", revision);

	fprintf(stderr,"\nADDFILES v%s-%s (rev %s) - Adds Files to Synchronet "
		"Filebase\n"
		,ADDFILES_VER
		,PLATFORM_DESC
		,revision
		);

	if(argc<2) {
		printf(usage);
		return(1); }

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\nSBBSCTRL environment variable not set.\n");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); 
	}

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir,p);
	printf("Reading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}
	printf("\n\n");

	if(!(scfg.sys_misc&SM_LOCAL_TZ))
		putenv("TZ=UTC0");

	if(argv[1][0]=='*') {
		if(argv[1][1])
			SAFECOPY(auto_name,argv[1]+1);
		mode|=AUTO_ADD;
		i=0; }
	else {
		for(i=0;i<scfg.total_dirs;i++)
			if(!stricmp(scfg.dir[i]->code,argv[1]))
				break;

		if(i>=scfg.total_dirs) {
			printf("Directory code '%s' not found.\n",argv[1]);
			exit(1); } }

	memset(&f,0,sizeof(file_t));
	f.dir=i;
	strcpy(f.uler,"-> ADDFILES <-");

	for(j=2;j<argc;j++) {
		if(argv[j][0]=='*')     /* set the uploader name */
			sprintf(f.uler,"%.25s",argv[j]+1);
		else if(argv[j][0]=='/') {      /* options */
			for(i=1;argv[j][i];i++)
				switch(toupper(argv[j][i])) {
					case 'A':
						mode|=ASCII_ONLY;
						break;
					case 'B':
						mode|=(SYNC_LIST|NO_UPDATE);
						break;
					case 'C':
						mode|=KEEP_SPACE;
						break;
					case 'D':
						mode|=DEL_LIST;
						break;
					case 'E':
						mode|=NO_EXTEND;
						break;
					case 'I':
						mode|=UL_STATS;
						break;
					case 'Z':
						mode|=FILE_ID;
						break;
					case 'K':
						mode|=KEEP_DESC;	 /* Don't use DIZ for short desc */
						break;
					case 'N':
						mode|=NO_UPDATE;
						break;
					case 'O':
						mode|=ULDATE_ONLY;
						break;
					case 'U':
						mode|=NO_NEWDATE;
						break;
					case 'F':
						mode|=FILE_DATE;
						break;
					case 'T':
						mode|=TODAYS_DATE;
						break;
					case 'S':
						mode|=SEARCH_DIR;
						break;
					default:
						printf(usage);
						return(1); } }
		else if(argv[j][0]=='+') {      /* filelist - FILES.BBS */
			listgiven=1;
			if(isdigit(argv[j+1][0])) { /* skip x characters before description */
				if(isdigit(argv[j+2][0])) { /* skip x characters before size */
					addlist(argv[j]+1,f,atoi(argv[j+1]),atoi(argv[j+2]));
					j+=2; }
				else {
					addlist(argv[j]+1,f,atoi(argv[j+1]),0);
					j++; } }
			else
				addlist(argv[j]+1,f,0,0);
			if(mode&SYNC_LIST)
				synclist(argv[j]+1,f.dir); }
		else if(argv[j][0]=='.') {      /* alternate file path */
			cur_altpath=atoi(argv[j]+1);
			if(cur_altpath>scfg.altpaths) {
				printf("Invalid alternate path.\n");
				exit(1); } }
		else {
			namegiven=1;
			padfname(argv[j],f.name);
			f.desc[0]=0;
			strupr(f.name);
			if(j+1==argc) {
				printf("%s no description given.\n",f.name);
				continue; }
			sprintf(str,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[f.dir]->path,argv[j]);
			if(mode&FILE_DATE)
				sprintf(f.desc,"%s  ",unixtodstr(&scfg,fdate(str),tmp));
			if(mode&TODAYS_DATE)
				sprintf(f.desc,"%s  ",unixtodstr(&scfg,time(NULL),tmp));
			sprintf(tmp,"%.*s",(int)(LEN_FDESC-strlen(f.desc)),argv[++j]);
			strcpy(f.desc,tmp);
			l=flength(str);
			if(l==-1) {
				printf("%s not found.\n",str);
				continue; }
			exist=findfile(&scfg,f.dir,f.name);
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				getfileixb(&scfg,&f);
				if(mode&ULDATE_ONLY) {
					f.dateuled=time(NULL);
					update_uldate(&scfg, &f);
					continue; } }
			f.cdt=l;
			f.dateuled=time(NULL);
			f.altpath=cur_altpath;
			strip_ctrl(f.desc);
			if(mode&ASCII_ONLY)
				strip_exascii(f.desc);
			printf("%s %7lu %s\n",f.name,f.cdt,f.desc);
			if(mode&FILE_ID) {
				for(i=0;i<scfg.total_fextrs;i++)
					if(!stricmp(scfg.fextr[i]->ext,f.name+9))
						break;
				if(i<scfg.total_fextrs) {
					sprintf(tmp,"%sFILE_ID.DIZ",scfg.temp_dir);
					remove(tmp);
					system(cmdstr(scfg.fextr[i]->cmd,str,"FILE_ID.DIZ",NULL));
					if(!fexistcase(tmp)) {
						sprintf(tmp,"%sDESC.SDI",scfg.temp_dir);
						remove(tmp);
						system(cmdstr(scfg.fextr[i]->cmd,str,"DESC.SDI",NULL)); 
						fexistcase(tmp);
					}
					if((file=nopen(tmp,O_RDONLY))!=-1) {
						memset(ext,0,513);
						read(file,ext,512);
						if(!(mode&KEEP_DESC)) {
							sprintf(f.desc,"%.*s",LEN_FDESC,ext);
							for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
								;
							f.desc[i]=0; }
						close(file);
						f.misc|=FM_EXTDESC; } } }
			if(exist) {
				putfiledat(&scfg,&f);
				if(!(mode&NO_NEWDATE))
					update_uldate(&scfg, &f); }
			else
				addfiledat(&scfg,&f);

			if(f.misc&FM_EXTDESC)
				putextdesc(&scfg,f.dir,f.datoffset,ext);

			if(mode&UL_STATS)
				updatestats(l);
			files++; } }

	if(mode&AUTO_ADD) {
		for(i=0;i<scfg.total_dirs;i++) {
			if(scfg.dir[i]->misc&DIR_NOAUTO)
				continue;
			f.dir=i;
			if(mode&SEARCH_DIR) {
				addlist("",f,0,0);
				continue; }
			sprintf(str,"%s.lst",scfg.dir[f.dir]->code);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str,f,0,0);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue; }
			sprintf(str,"%s%s",scfg.dir[f.dir]->path,auto_name);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str,f,0,0);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue; } } }

	else {
		if(!listgiven && !namegiven) {
			sprintf(str,"%s.lst",scfg.dir[f.dir]->code);
			if(!fexistcase(str) || flength(str)<=0L)
				strcpy(str,"FILES.BBS");
			addlist(str,f,0,0);
			if(mode&SYNC_LIST)
				synclist(str,f.dir); } }

	printf("\n%lu file(s) added.",files);
	if(removed)
		printf("\n%lu files(s) removed.",removed);
	printf("\n");
	return(0);
}

