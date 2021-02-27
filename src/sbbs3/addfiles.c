/* Program to add files to a Synchronet file database */

/* $Id: addfiles.c,v 1.63 2020/08/17 00:48:27 rswindell Exp $ */

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
#include <stdbool.h>

#define ADDFILES_VER "3.04"

scfg_t scfg;

int cur_altpath=0;

long files=0,removed=0,mode=0;

char lib[LEN_GSNAME+1];
const char* datefmt = NULL;

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
#define CHECK_DATE	(1L<<15)

/****************************************************************************/
/* This is needed by load_cfg.c												*/
/****************************************************************************/
int lprintf(int level, const char *fmat, ...)
{
	va_list argptr;
	char sbuf[512];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	truncsp(sbuf);
	printf("%s\n",sbuf);
	return(chcount);
}

void prep_desc(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i] && j < sizeof(tmp)-1;i++) {
		if(j && str[i]==' ' && tmp[j-1]==' ' && (mode&KEEP_SPACE))
			tmp[j++]=str[i];
		else if(j && str[i]<=' ' && str[i] > 0&& tmp[j-1]==' ')
			continue;
		else if(i && !isalnum((uchar)str[i]) && str[i]==str[i-1])
			continue;
		else if(str[i]>=' ' || str[i]<0)
			tmp[j++]=str[i];
		else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
			tmp[j++]=' ';
	}
	tmp[j]=0;
	strcpy(str,tmp);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *mycmdstr(const char *instr, const char *fpath, const char *fspec, char *outstr)
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
					SAFECAT(cmd,fpath);
					break;
				case '~':	/* DOS-compatible (8.3) filename */
#ifdef _WIN32
					SAFECOPY(sfpath,fpath);
					GetShortPathName(fpath,sfpath,sizeof(sfpath));
					SAFECAT(cmd,sfpath);
#else
                    SAFECAT(cmd,fpath);
#endif
					break;
				case 'G':   /* Temp directory */
					SAFECAT(cmd,scfg.temp_dir);
					break;
                case 'J':
                    SAFECAT(cmd,scfg.data_dir);
                    break;
                case 'K':
                    SAFECAT(cmd,scfg.ctrl_dir);
                    break;
				case 'N':   /* Node Directory (same as SBBSNODE environment var) */
					SAFECAT(cmd,scfg.node_dir);
					break;
				case 'S':   /* File Spec */
					SAFECAT(cmd,fspec);
					break;
                case 'Z':
                    SAFECAT(cmd,scfg.text_dir);
                    break;
				case '!':   /* EXEC Directory */
					SAFECAT(cmd,scfg.exec_dir);
					break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    SAFECAT(cmd,scfg.exec_dir);
#endif
                    break;
				case '#':   /* Node number (same as SBBSNNUM environment var) */
					sprintf(str,"%d",scfg.node_num);
					SAFECAT(cmd,str);
					break;
				case '%':   /* %% for percent sign */
					SAFECAT(cmd,"%");
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
	uint32_t	l;

	sprintf(str,"%sdsts.dab",scfg.ctrl_dir);
	if((file=nopen(str,O_RDWR|O_BINARY))==-1) {
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

bool get_file_diz(file_t* f, const char* filepath, char* ext)
{
	int i,file;
	char tmp[MAX_PATH+1];
	char tmpext[513];

	for(i=0;i<scfg.total_fextrs;i++)
		if(!stricmp(scfg.fextr[i]->ext,f->name+9)
			&& chk_ar(&scfg,scfg.fextr[i]->ar,/* user: */NULL, /* client: */NULL))
			break;
	// If we could not find an extractor which matches our requirements, use any
	if(i >= scfg.total_fextrs) {
		for(i=0;i<scfg.total_fextrs;i++)
			if(!stricmp(scfg.fextr[i]->ext,f->name+9))
				break;
	}
	if(i >= scfg.total_fextrs)
		return false;

	SAFEPRINTF(tmp,"%sFILE_ID.DIZ",scfg.temp_dir);
	removecase(tmp);
	system(mycmdstr(scfg.fextr[i]->cmd,filepath,"FILE_ID.DIZ",NULL));
	if(!fexistcase(tmp)) {
		SAFEPRINTF(tmp,"%sDESC.SDI",scfg.temp_dir);
		removecase(tmp);
		system(mycmdstr(scfg.fextr[i]->cmd,filepath,"DESC.SDI",NULL));
		fexistcase(tmp);
	}

	if((file=nopen(tmp,O_RDONLY|O_BINARY)) == -1)
		return false;

	memset(ext,0,513);
	read(file,ext,512);
	for(i=512;i;i--)
		if(ext[i-1]>' ' || ext[i-1]<0)
			break;
	ext[i]=0;
	if(mode&ASCII_ONLY)
		strip_exascii(ext, ext);
	if(!(mode&KEEP_DESC)) {
		sprintf(tmpext,"%.256s",ext);
		prep_desc(tmpext);
		for(i=0;tmpext[i];i++)
			if(isalpha((uchar)tmpext[i]))
				break;
		sprintf(f->desc,"%.*s",LEN_FDESC,tmpext+i);
		for(i=0;(f->desc[i]>=' ' || f->desc[i]<0) && i<LEN_FDESC;i++)
			;
		f->desc[i]=0; }
	close(file);
	f->misc|=FM_EXTDESC;
	return true;
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
	char ext[1024];
	int i;
	long l;
	BOOL exist;
	FILE *stream;
	DIR*	dir;
	DIRENT*	dirent;

	if(mode&SEARCH_DIR) {
		SAFECOPY(str,cur_altpath ? scfg.altpath[cur_altpath-1] : scfg.dir[f.dir]->path);
		printf("Searching %s\n\n",str);
		dir=opendir(str);

		while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
			sprintf(tmp,"%s%s"
				,cur_altpath ? scfg.altpath[cur_altpath-1] : scfg.dir[f.dir]->path
				,dirent->d_name);
			if(isdir(tmp))
				continue;
#ifdef _WIN32
			GetShortPathName(tmp, filepath, sizeof(filepath));
#else
			SAFECOPY(filepath,tmp);
#endif
			f.misc=0;
			f.desc[0]=0;
			memset(ext, 0, sizeof(ext));
			f.cdt=flength(filepath);
			time_t file_timestamp = fdate(filepath);
			padfname(getfname(filepath),f.name);
			printf("%s  %10"PRIu32"  %s\n"
				,f.name,f.cdt,unixtodstr(&scfg,(time32_t)file_timestamp,str));
			exist=findfile(&scfg,f.dir,f.name);
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				if(!getfileixb(&scfg,&f)) {
					fprintf(stderr, "!ERROR reading index of directory %u\n", f.dir);
					continue;
				}
				if((mode&CHECK_DATE) && file_timestamp <= f.dateuled)
					continue;
				if(mode&ULDATE_ONLY) {
					f.dateuled=time32(NULL);
					update_uldate(&scfg, &f);
					continue;
				}
				if(f.misc & FM_EXTDESC)
					getextdesc(&scfg, f.dir, f.datoffset, ext);
			}

			if(mode&TODAYS_DATE) {		/* put today's date in desc */
				time_t now = time(NULL);
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&now, &tm);
					strftime(f.desc, sizeof(f.desc), datefmt, &tm);
				} else
					unixtodstr(&scfg, (time32_t)now, f.desc);
				SAFECAT(f.desc,"  ");
			}
			else if(mode&FILE_DATE) {		/* get the file date and put into desc */
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&file_timestamp, &tm);
					strftime(f.desc, sizeof(f.desc), datefmt, &tm);
				} else
					unixtodstr(&scfg,(time32_t)file_timestamp,f.desc);
				SAFECAT(f.desc,"  ");
			}

			if(mode&FILE_ID)
				get_file_diz(&f, filepath, ext);

			f.dateuled=time32(NULL);
			f.altpath=cur_altpath;
			prep_desc(f.desc);
			if(mode&ASCII_ONLY)
				strip_exascii(f.desc, f.desc);
			if(exist) {
				putfiledat(&scfg,&f);
				if(!(mode&NO_NEWDATE))
					update_uldate(&scfg, &f);
			}
			else
				addfiledat(&scfg,&f);
			if(f.misc&FM_EXTDESC) {
				truncsp(ext);
				putextdesc(&scfg,f.dir,f.datoffset,ext);
			}
			if(mode&UL_STATS)
				updatestats(f.cdt);
			files++;
		}
		if(dir!=NULL)
			closedir(dir);
		return;
	}


	SAFECOPY(listpath,inpath);
	fexistcase(listpath);
	if((stream=fopen(listpath,"r"))==NULL) {
		fprintf(stderr,"Error %d (%s) opening %s\n"
			,errno,strerror(errno),listpath);
		sprintf(listpath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[f.dir]->path,inpath);
		fexistcase(listpath);
		if((stream=fopen(listpath,"r"))==NULL) {
			printf("Can't open: %s\n"
				   "        or: %s\n",inpath,listpath);
			return;
		}
	}

	printf("Adding %s to %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[f.dir]->lib]->sname,scfg.dir[f.dir]->sname);

	fgets(nextline,255,stream);
	do {
		f.misc=0;
		f.desc[0]=0;
		memset(ext, 0, sizeof(ext));
		SAFECOPY(curline,nextline);
		nextline[0]=0;
		fgets(nextline,255,stream);
		truncsp(curline);
		if(curline[0]<=' ' || (mode&ASCII_ONLY && (uchar)curline[0]>=0x7e))
			continue;
		printf("%s\n",curline);
		SAFECOPY(fname,curline);

#if 0	/* Files without dots are valid on modern systems */
		p=strchr(fname,'.');
		if(!p || p==fname || p>fname+8)    /* no dot or invalid dot location */
			continue;
#endif
		p=strchr(fname,' ');
		if(p) *p=0;
#if 0 // allow import of bare filename list
		else				   /* no space after filename? */
			continue;
#endif
		if(!isalnum(*fname)) {	// filename doesn't begin with an alpha-numeric char?
			continue;
		}
		SAFEPRINTF2(filepath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
			: scfg.dir[f.dir]->path,fname);

#ifdef _WIN32
		{
			char shortpath[MAX_PATH+1];
			GetShortPathName(filepath, shortpath, sizeof(shortpath));
			SAFECOPY(fname, getfname(shortpath));
		}
#else
		fexistcase(filepath);
		SAFECOPY(fname, getfname(filepath));
#endif

		padfname(fname,f.name);
		if(strcspn(f.name,"\\/|<>+[]:=\";,")!=strlen(f.name))
			continue;

		for(i=0;i<12;i++)
			if(f.name[i]<' ' || (mode&ASCII_ONLY && (uchar)f.name[i]>0x7e))
				break;

		if(i<12)					/* Ctrl chars or EX-ASCII in filename? */
			continue;
		time_t file_timestamp = fdate(filepath);
		exist=findfile(&scfg,f.dir,f.name);
		if(exist) {
			if(mode&NO_UPDATE)
				continue;
			if(!getfileixb(&scfg,&f)) {
				fprintf(stderr, "!ERROR reading index of directory %u\n", f.dir);
				continue;
			}
			if((mode&CHECK_DATE) && file_timestamp <= f.dateuled)
				continue;
			if(mode&ULDATE_ONLY) {
				f.dateuled=time32(NULL);
				update_uldate(&scfg, &f);
				continue;
			}
			if (f.misc & FM_EXTDESC)
				getextdesc(&scfg, f.dir, f.datoffset, ext);
		}

		if(mode&TODAYS_DATE) {		/* put today's date in desc */
			time_t now = time(NULL);
			if(datefmt) {
				struct tm tm = {0};
				localtime_r(&now, &tm);
				strftime(f.desc, sizeof(f.desc), datefmt, &tm);
			} else
				unixtodstr(&scfg, (time32_t)now, f.desc);
			SAFECAT(f.desc,"  ");
		}
		else if(mode&FILE_DATE) {		/* get the file date and put into desc */
			if(datefmt) {
				struct tm tm = {0};
				localtime_r(&file_timestamp, &tm);
				strftime(f.desc, sizeof(f.desc), datefmt, &tm);
			} else
				unixtodstr(&scfg,(time32_t)file_timestamp,f.desc);
			SAFECAT(f.desc,"  ");
		}

		if(dskip && strlen(curline)>=dskip) p=curline+dskip;
		else {
			p = curline;
			FIND_WHITESPACE(p);
			SKIP_WHITESPACE(p);
		}
		SAFECOPY(tmp,p);
		prep_desc(tmp);
		sprintf(f.desc+strlen(f.desc),"%.*s",(int)(LEN_FDESC-strlen(f.desc)),tmp);

		if(nextline[0]==' ' || strlen(p)>LEN_FDESC) {	/* ext desc */
			if(!(mode&NO_EXTEND)) {
				memset(ext, 0, sizeof(ext));
				f.misc|=FM_EXTDESC;
				sprintf(ext,"%s\r\n",p);
			}

			if(nextline[0]==' ') {
				SAFECOPY(str,nextline);				   /* tack on to end of desc */
				p=str+dskip;
				while(*p>0 && *p<=' ') p++;
				i=LEN_FDESC-strlen(f.desc);
				if(i>1) {
					p[i-1]=0;
					truncsp(p);
					if(p[0]) {
						SAFECAT(f.desc," ");
						SAFECAT(f.desc,p);
					}
				}
			}

			while(!feof(stream) && !ferror(stream) && strlen(ext)<512) {
				if(nextline[0]!=' ')
					break;
				truncsp(nextline);
				printf("%s\n",nextline);
				if(!(mode&NO_EXTEND)) {
					f.misc|=FM_EXTDESC;
					p=nextline+dskip;
					while(*p==' ') p++;
					SAFECAT(ext,p);
					SAFECAT(ext,"\r\n");
				}
				nextline[0]=0;
				fgets(nextline,255,stream);
			}
		}


		l=flength(filepath);
		if(l<0L) {
			printf("%s not found.\n",filepath);
			continue;
		}
		if(l == 0L) {
			printf("%s is a zero length file.\n",filepath);
			continue;
		}
		if(sskip) l=atol(fname+sskip);

		if(mode&FILE_ID)
			get_file_diz(&f, filepath, ext);

		f.cdt=l;
		f.dateuled=time32(NULL);
		f.altpath=cur_altpath;
		prep_desc(f.desc);
		if(mode&ASCII_ONLY)
			strip_exascii(f.desc, f.desc);
		if(exist) {
			putfiledat(&scfg,&f);
			if(!(mode&NO_NEWDATE))
				update_uldate(&scfg, &f);
		}
		else
			addfiledat(&scfg,&f);
		if(f.misc&FM_EXTDESC) {
			truncsp(ext);
			putextdesc(&scfg,f.dir,f.datoffset,ext);
		}

		if(mode&UL_STATS)
			updatestats(l);
		files++;
	} while(!feof(stream) && !ferror(stream));
	fclose(stream);
	if(mode&DEL_LIST && !(mode&SYNC_LIST)) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath);
	}

}

void synclist(char *inpath, int dirnum)
{
	char	str[1024];
	char	fname[MAX_PATH+1];
	char	listpath[MAX_PATH+1];
	uchar*	ixbbuf;
	char*	p;
	int		i,file,found;
	long	l,m,length;
	FILE*	stream;
	file_t	f;

	sprintf(str,"%s%s.ixb",scfg.dir[dirnum]->data_dir,scfg.dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY|O_BINARY))==-1) {
		printf("ERR_OPEN %s\n",str);
		return;
	}
	length=filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		printf("ERR_LEN (%ld) of %s\n",length,str);
		return;
	}
	if((ixbbuf=(uchar *)malloc(length))==NULL) {
		close(file);
		printf("ERR_ALLOC %s\n",str);
		return;
	}
	if(lread(file,ixbbuf,length)!=length) {
		close(file);
		free((char *)ixbbuf);
		printf("ERR_READ %s\n",str);
		return;
	}
	close(file);

	SAFECOPY(listpath,inpath);
	if((stream=fopen(listpath,"r"))==NULL) {
		sprintf(listpath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[dirnum]->path,inpath);
		if((stream=fopen(listpath,"r"))==NULL) {
			printf("Can't open: %s\n"
				   "        or: %s\n",inpath,listpath);
			return;
		}
	}

	printf("\nSynchronizing %s with %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[dirnum]->lib]->sname,scfg.dir[dirnum]->sname);

	for(l=0;l<length;l+=F_IXBSIZE) {
		m=l;
		for(i=0;i<12 && l<length;i++)
			if(i==8)
				str[i]=ixbbuf[m]>' ' ? '.' : ' ';
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
			p=strchr(str,' ');
			if(p) *p=0;
			if(!stricmp(str,fname))
				found=1;
		}
		if(found)
			continue;
		padfname(fname,f.name);
		printf("%s not found in list - ",f.name);
		f.dir=dirnum;
		f.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
		if(!getfiledat(&scfg,&f)) {
			fprintf(stderr, "!ERROR reading index of directory %u\n", f.dir);
			continue;
		}
		if(f.opencount) {
			printf("currently OPEN by %u users\n",f.opencount);
			continue;
		}
		removefiledat(&scfg,&f);
		if(remove(getfilepath(&scfg,&f,str)))
			printf("Error removing %s\n",str);
		removed++;
		printf("Removed from database\n");
	}

	if(mode&DEL_LIST) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath);
	}
}

char *usage="\nusage: addfiles code [.alt_path] [-opts] +list "
			 "[desc_off] [size_off]"
	"\n   or: addfiles code [.alt_path] [-opts] file "
		"\"description\"\n"
	"\navailable opts:"
	"\n      -a         import ASCII only (no extended ASCII)"
	"\n      -b         synchronize database with file list (use with caution)"
	"\n      -c         do not remove extra spaces from file description"
	"\n      -d         delete list after import"
	"\n      -e         do not import extended descriptions"
	"\n      -f         include file date in descriptions"
	"\n      -F <fmt>   include file date in descriptions, using strftime format"
	"\n      -t         include today's date in descriptions"
	"\n      -i         include added files in upload statistics"
	"\n      -n         do not update information for existing files"
	"\n      -o         update upload date only for existing files"
	"\n      -p         compare file date with upload date for existing files"
	"\n      -u         do not update upload date for existing files"
	"\n      -z         check for and import FILE_ID.DIZ and DESC.SDI"
	"\n      -k         keep original short description (not DIZ)"
	"\n      -s         search directory for files (no file list)"
	"\n      -l <lib>   specify library (short name) to Auto-ADD"
	"\n      -x <name>  specify uploader's user name (may require quotes)"
	"\n"
	"\nAuto-ADD:   use - in place of code for Auto-ADD of FILES.BBS"
	"\n            use -filename to Auto-ADD a different filename"
	"\n            use -l \"libname\" to only Auto-ADD files to a specific library"
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
	char *p;
	char exist,listgiven=0,namegiven=0,ext[513]
		,auto_name[MAX_PATH+1]="FILES.BBS";
	int i,j;
	uint desc_offset=0, size_offset=0;
	long l;
	file_t	f;

	sscanf("$Revision: 1.63 $", "%*s %s", revision);

	fprintf(stderr,"\nADDFILES v%s-%s (rev %s) - Adds Files to Synchronet "
		"Filebase\n"
		,ADDFILES_VER
		,PLATFORM_DESC
		,revision
		);

	if(argc<2) {
		puts(usage);
		return(1);
	}

	p = get_ctrl_dir(/* warn: */true);

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir,p);

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}
	SAFECOPY(scfg.temp_dir,"../temp");
	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));

	if(argv[1][0]=='*' || argv[1][0]=='-') {
		if(argv[1][1]=='?') {
			puts(usage);
			exit(0);
		}
		if(argv[1][1])
			SAFECOPY(auto_name,argv[1]+1);
		mode|=AUTO_ADD;
		i=0;
	} else {
		if(!isalnum((uchar)argv[1][0]) && argc==2) {
			puts(usage);
			return(1);
		}

		for(i=0;i<scfg.total_dirs;i++)
			if(!stricmp(scfg.dir[i]->code,argv[1])) /* use matchmatchi() instead? */
				break;

		if(i>=scfg.total_dirs) {
			printf("Directory code '%s' not found.\n",argv[1]);
			exit(1);
		}
	}

	memset(&f,0,sizeof(file_t));
	f.dir=i;
	SAFECOPY(f.uler,"-> ADDFILES <-");

	for(j=2;j<argc;j++) {
		if(argv[j][0]=='*')     /* set the uploader name (legacy) */
			SAFECOPY(f.uler,argv[j]+1);
		else
			if(argv[j][0]=='-'
			|| argv[j][0]=='/'
			) {      /* options */
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
					case 'L':
						j++;
						if(argv[j]==NULL) {
							puts(usage);
							return(-1);
						}
						SAFECOPY(lib,argv[j]);
						i=strlen(argv[j])-1;
						break;
					case 'X':
						j++;
						if(argv[j]==NULL) {
							puts(usage);
							return(-1);
						}
						SAFECOPY(f.uler,argv[j]);
						i=strlen(argv[j])-1;
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
					case 'P':
						mode|=CHECK_DATE;
						break;
					case 'U':
						mode|=NO_NEWDATE;
						break;
					case 'F':
						mode|=FILE_DATE;
						if(argv[j][i] == 'F') {
							j++;
							if(argv[j]==NULL) {
								puts(usage);
								return(-1);
							}
							datefmt = argv[j];
							i=strlen(argv[j]) - 1;
							break;
						}
						break;
					case 'T':
						mode|=TODAYS_DATE;
						break;
					case 'S':
						mode|=SEARCH_DIR;
						break;
					default:
						puts(usage);
						return(1);
			}
		}
		else if(isdigit((uchar)argv[j][0])) {
			if(desc_offset==0)
				desc_offset=atoi(argv[j]);
			else
				size_offset=atoi(argv[j]);
			continue;
		}
		else if(argv[j][0]=='+') {      /* filelist - FILES.BBS */
			listgiven=1;
			if(argc > j+1
				&& isdigit((uchar)argv[j+1][0])) { /* skip x characters before description */
				if(argc > j+2
					&& isdigit((uchar)argv[j+2][0])) { /* skip x characters before size */
					addlist(argv[j]+1,f,atoi(argv[j+1]),atoi(argv[j+2]));
					j+=2;
				}
				else {
					addlist(argv[j]+1,f,atoi(argv[j+1]),0);
					j++;
				}
			}
			else
				addlist(argv[j]+1,f,0,0);
			if(mode&SYNC_LIST)
				synclist(argv[j]+1,f.dir);
		}
		else if(argv[j][0]=='.') {      /* alternate file path */
			cur_altpath=atoi(argv[j]+1);
			if(cur_altpath>scfg.altpaths) {
				printf("Invalid alternate path.\n");
				exit(1);
			}
		}
		else {
			namegiven=1;
			padfname(argv[j],f.name);
			f.desc[0]=0;
			memset(ext, 0, sizeof(ext));
#if 0
			strupr(f.name);
#endif
			if(j+1==argc) {
				printf("%s no description given.\n",f.name);
				continue;
			}
			sprintf(str,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[f.dir]->path,argv[j]);
			if(mode&TODAYS_DATE) {
				time_t now = time(NULL);
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&now, &tm);
					strftime(f.desc, sizeof(f.desc), datefmt, &tm);
				} else
					SAFECOPY(f.desc, unixtodstr(&scfg, (time32_t)now, tmp));
				SAFECAT(f.desc, "  ");
			}
			else if(mode&FILE_DATE) {
				time_t file_timestamp = fdate(str);
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&file_timestamp, &tm);
					strftime(f.desc, sizeof(f.desc), datefmt, &tm);
				} else
					SAFECOPY(f.desc, unixtodstr(&scfg,(time32_t)file_timestamp,tmp));
				SAFECAT(f.desc, "  ");
			}
			j++;
			SAFECAT(f.desc, argv[j]);
			l=flength(str);
			if(l==-1) {
				printf("%s not found.\n",str);
				continue;
			}
			exist=findfile(&scfg,f.dir,f.name);
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				if(!getfileixb(&scfg,&f)) {
					fprintf(stderr, "!ERROR reading index of directory %u\n", f.dir);
					continue;
				}
				if((mode&CHECK_DATE) && fdate(str) <= f.dateuled)
					continue;
				if(mode&ULDATE_ONLY) {
					f.dateuled=time32(NULL);
					update_uldate(&scfg, &f);
					continue;
				}
				if (f.misc & FM_EXTDESC)
					getextdesc(&scfg, f.dir, f.datoffset, ext);
			}
			f.cdt=l;
			f.dateuled=time32(NULL);
			f.altpath=cur_altpath;
			prep_desc(f.desc);
			if(mode&ASCII_ONLY)
				strip_exascii(f.desc, f.desc);
			printf("%s %7"PRIu32" %s\n",f.name,f.cdt,f.desc);
			if(mode&FILE_ID)
				get_file_diz(&f, str, ext);

			if(exist) {
				putfiledat(&scfg,&f);
				if(!(mode&NO_NEWDATE))
					update_uldate(&scfg, &f);
			}
			else
				addfiledat(&scfg,&f);

			if(f.misc&FM_EXTDESC)
				putextdesc(&scfg,f.dir,f.datoffset,ext);

			if(mode&UL_STATS)
				updatestats(l);
			files++;
		}
	}

	if(mode&AUTO_ADD) {
		for(i=0;i<scfg.total_dirs;i++) {
			if(lib[0] && stricmp(scfg.lib[scfg.dir[i]->lib]->sname,lib))
				continue;
			if(scfg.dir[i]->misc&DIR_NOAUTO)
				continue;
			f.dir=i;
			if(mode&SEARCH_DIR) {
				addlist("",f,desc_offset,size_offset);
				continue;
			}
			sprintf(str,"%s%s.lst",scfg.dir[f.dir]->path,scfg.dir[f.dir]->code);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str,f,desc_offset,size_offset);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue;
			}
			sprintf(str,"%s%s",scfg.dir[f.dir]->path,auto_name);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str,f,desc_offset,size_offset);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue;
			}
		}
	}

	else {
		if(!listgiven && !namegiven) {
			sprintf(str,"%s%s.lst",scfg.dir[f.dir]->path, scfg.dir[f.dir]->code);
			if(!fexistcase(str) || flength(str)<=0L)
				sprintf(str,"%s%s",scfg.dir[f.dir]->path, auto_name);
			addlist(str,f,desc_offset,size_offset);
			if(mode&SYNC_LIST)
				synclist(str,f.dir);
		}
	}

	printf("\n%lu file(s) added.",files);
	if(removed)
		printf("\n%lu files(s) removed.",removed);
	printf("\n");
	return(0);
}

