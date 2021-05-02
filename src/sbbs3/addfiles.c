/* Add files to a Synchronet file database(s) */

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

#include "nopen.h"
#include "str_util.h"
#include "datewrap.h"
#include "date_str.h"
#include "userdat.h"
#include "filedat.h"
#include "load_cfg.h"
#include "smblib.h"
#include "git_branch.h"
#include "git_hash.h"

#include <stdbool.h>
#include <stdarg.h>

#define ADDFILES_VER "3.19"

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

bool reupload(smb_t* smb, file_t* f)
{
	char path[MAX_PATH + 1];
	if(smb_renewfile(smb, f, SMB_SELFPACK, getfilepath(&scfg, f, path)) != SMB_SUCCESS) {
		fprintf(stderr, "!Error renewing: %s\n", f->name);
		return false;
	}
	return true;
}


bool get_file_diz(file_t* f, char* ext, size_t maxlen)
{
	char path[MAX_PATH + 1];
	printf("Extracting DIZ from: %s\n", getfilepath(&scfg, f, path));
	char diz_fpath[MAX_PATH + 1];
	if(!extract_diz(&scfg, f, /* diz_fnames */NULL, diz_fpath, sizeof(diz_fpath))) {
		printf("DIZ does not exist in: %s\n", getfilepath(&scfg, f, path));
		return false;
	}
	printf("Parsing DIZ: %s\n", diz_fpath);
	char* lines = read_diz(diz_fpath, NULL);
	format_diz(lines, ext, maxlen, 0, false);
	free(lines);
	remove(diz_fpath);

	if(mode&ASCII_ONLY)
		strip_exascii(ext, ext);

	if(!(mode&KEEP_DESC)) {
		char* fdesc = strdup(ext);
		smb_new_hfield_str(f, SMB_FILEDESC, prep_file_desc(fdesc, fdesc));
		free(fdesc);
	}
	return true;
}

void addlist(char *inpath, uint dirnum, const char* uploader, uint dskip, uint sskip)
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
	off_t l;
	BOOL exist;
	FILE *stream;
	DIR*	dir;
	DIRENT*	dirent;
	smb_t smb;
	file_t f;

	int result = smb_open_dir(&scfg, &smb, dirnum);
	if(result != SMB_SUCCESS) {
		fprintf(stderr, "!Error %d (%s) opening %s\n", result, smb.last_error, smb.file);
		return;
	}
	str_list_t fname_list = loadfilenames(&smb, ALLFILES, /* time: */0, FILE_SORT_NATURAL, /* count: */NULL);
	if(mode&SEARCH_DIR) {
		SAFECOPY(str,cur_altpath ? scfg.altpath[cur_altpath-1] : scfg.dir[dirnum]->path);
		printf("Searching %s\n\n",str);
		dir=opendir(str);

		while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
			sprintf(filepath, "%s%s"
				,cur_altpath ? scfg.altpath[cur_altpath-1] : scfg.dir[dirnum]->path
				,dirent->d_name);
			if(isdir(filepath))
				continue;
			const char* fname = getfname(filepath);
			printf("%s  ", fname);
			if(strListFind(fname_list, fname, /* case-sensitive: */FALSE) && (mode&NO_UPDATE)) {
				printf("already added.\n");
				continue;
			}
			memset(ext, 0, sizeof(ext));
			memset(&f, 0, sizeof(f));
			char fdesc[LEN_FDESC + 1] = {0};
			uint32_t cdt = (uint32_t)flength(filepath);
			time_t file_timestamp = fdate(filepath);
			printf("%10"PRIu32"  %s\n"
				,cdt, unixtodstr(&scfg,(time32_t)file_timestamp,str));
			exist = smb_findfile(&smb, fname, &f) == SMB_SUCCESS;
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				if((mode&CHECK_DATE) && file_timestamp <= f.idx.time)
					continue;
				if(mode&ULDATE_ONLY) {
					reupload(&smb, &f);
					continue; 
				} 
			}

			if(mode&TODAYS_DATE) {		/* put today's date in desc */
				time_t now = time(NULL);
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&now, &tm);
					strftime(fdesc, sizeof(fdesc), datefmt, &tm);
				} else
					unixtodstr(&scfg, (time32_t)now, fdesc);
				SAFECAT(fdesc,"  ");
			}
			else if(mode&FILE_DATE) {		/* get the file date and put into desc */
				if(datefmt) {
					struct tm tm = {0};
					localtime_r(&file_timestamp, &tm);
					strftime(fdesc, sizeof(fdesc), datefmt, &tm);
				} else
					unixtodstr(&scfg,(time32_t)file_timestamp,fdesc);
				SAFECAT(fdesc,"  ");
			}

			char* ext_desc = NULL;
			if(mode&FILE_ID) {
				if(get_file_diz(&f, ext, sizeof(ext))) {
					ext_desc = ext;
				}
			}

			prep_file_desc(fdesc, fdesc);
			if(mode&ASCII_ONLY)
				strip_exascii(fdesc, fdesc);

			smb_hfield_str(&f, SMB_FILENAME, fname);
			smb_hfield_str(&f, SMB_FILEDESC, fdesc);
			smb_hfield_str(&f, SENDER, uploader);
			smb_hfield_bin(&f, SMB_COST, cdt);

			truncsp(ext_desc);
			if(exist) {
				result = smb_updatemsg(&smb, &f);
				if(!(mode&NO_NEWDATE))
					reupload(&smb, &f);
			}
			else
				result = smb_addfile(&smb, &f, SMB_SELFPACK, ext_desc, NULL, filepath);
			smb_freefilemem(&f);
			if(result != SMB_SUCCESS)
				fprintf(stderr, "!Error %d (%s) adding file to %s", result, smb.last_error, smb.file);
			if(mode&UL_STATS)
				updatestats(cdt);
			files++; 
		}
		if(dir!=NULL)
			closedir(dir);
		smb_close(&smb);
		strListFree(&fname_list);
		return; 
	}


	SAFECOPY(listpath,inpath);
	fexistcase(listpath);
	if((stream=fopen(listpath,"r"))==NULL) {
		fprintf(stderr,"Error %d (%s) opening %s\n"
			,errno,strerror(errno),listpath);
		sprintf(listpath,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[dirnum]->path,inpath);
		fexistcase(listpath);
		if((stream=fopen(listpath,"r"))==NULL) {
			printf("Can't open: %s\n"
				   "        or: %s\n",inpath,listpath);
			smb_close(&smb);
			strListFree(&fname_list);
			return; 
		} 
	}

	printf("Adding %s to %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[dirnum]->lib]->sname,scfg.dir[dirnum]->sname);

	fgets(nextline,255,stream);
	do {
		char fdesc[LEN_FDESC + 1] = {0};
		memset(ext, 0, sizeof(ext));
		SAFECOPY(curline,nextline);
		nextline[0]=0;
		fgets(nextline,255,stream);
		truncsp(curline);
		if(curline[0]<=' ' || (mode&ASCII_ONLY && (uchar)curline[0]>=0x7e))
			continue;
		printf("%s\n",curline);
		SAFECOPY(fname,curline);

		p=strchr(fname,' ');
		if(p) *p=0;
#if 0 // allow import of bare filename list
		else				   /* no space after filename? */
			continue;
#endif
		if(!IS_ALPHANUMERIC(*fname)) {	// filename doesn't begin with an alpha-numeric char?
			continue;
		}

		sprintf(filepath, "%s%s", cur_altpath ? scfg.altpath[cur_altpath-1]
			: scfg.dir[dirnum]->path,fname);

		if(strcspn(fname,"\\/|<>+[]:=\";,")!=strlen(fname)) {
			fprintf(stderr, "!Illegal filename: %s\n", fname);
			continue;
		}

		time_t file_timestamp = fdate(filepath);
		memset(&f, 0, sizeof(f));
		exist = smb_findfile(&smb, fname, &f) == SMB_SUCCESS;
		if(exist) {
			if(mode&NO_UPDATE)
				continue;
			if((mode&CHECK_DATE) && file_timestamp <= f.idx.time)
				continue;
			if(mode&ULDATE_ONLY) {
				reupload(&smb, &f);
				continue; 
			} 
		}

		if(mode&FILE_DATE) {		/* get the file date and put into desc */
			unixtodstr(&scfg,(time32_t)file_timestamp, fdesc);
			strcat(fdesc, "  "); 
		}

		if(mode&TODAYS_DATE) {		/* put today's date in desc */
			l=time32(NULL);
			unixtodstr(&scfg,(time32_t)l,fdesc);
			strcat(fdesc, "  "); 
		}

		if(dskip && strlen(curline)>=dskip) p=curline+dskip;
		else {
			p = curline;
			FIND_WHITESPACE(p);
			SKIP_WHITESPACE(p);
		}
		SAFECOPY(tmp,p);
		prep_file_desc(tmp, tmp);
		sprintf(fdesc + strlen(fdesc), "%.*s", (int)(LEN_FDESC-strlen(fdesc)), tmp);

		char* ext_desc = NULL;
		if(nextline[0]==' ' || strlen(p)>LEN_FDESC) {	/* ext desc */
			if(!(mode&NO_EXTEND)) {
				memset(ext, 0, sizeof(ext));
				sprintf(ext,"%s\r\n",p); 
				ext_desc = ext;
			}

			if(nextline[0]==' ') {
				SAFECOPY(str,nextline);				   /* tack on to end of desc */
				p=str+dskip;
				while(*p>0 && *p<=' ') p++;
				i=LEN_FDESC-strlen(fdesc);
				if(i>1) {
					p[i-1]=0;
					truncsp(p);
					if(p[0]) {
						SAFECAT(fdesc," ");
						SAFECAT(fdesc,p);
					}
				}
			}

			while(!feof(stream) && !ferror(stream) && strlen(ext)<512) {
				if(nextline[0]!=' ')
					break;
				truncsp(nextline);
				printf("%s\n",nextline);
				if(!(mode&NO_EXTEND)) {
					p=nextline+dskip;
					ext_desc = ext;
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
			get_file_diz(&f, ext, sizeof(ext));

		prep_file_desc(fdesc, fdesc);
		if(mode&ASCII_ONLY)
			strip_exascii(fdesc, fdesc);
		uint32_t cdt = (uint32_t)l;
		smb_hfield_bin(&f, SMB_COST, cdt);
		smb_hfield_str(&f, SMB_FILENAME, fname);
		smb_hfield_str(&f, SMB_FILEDESC, fdesc);
		smb_hfield_str(&f, SENDER, uploader);
		if(exist) {
			result = smb_updatemsg(&smb, &f);
			if(!(mode&NO_NEWDATE))
				reupload(&smb, &f);
		}
		else
			result = smb_addfile(&smb, &f, SMB_SELFPACK, ext_desc, NULL, filepath);
		smb_freefilemem(&f);
		if(result != SMB_SUCCESS)
			fprintf(stderr, "!ERROR %d (%s) writing to %s\n"
				, result, smb.last_error, smb.file);

		if(mode&UL_STATS)
			updatestats((ulong)l);
		files++;
	} while(!feof(stream) && !ferror(stream));
	fclose(stream);
	if(mode&DEL_LIST && !(mode&SYNC_LIST)) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath);
	}
	smb_close(&smb);
	strListFree(&fname_list);
}

void synclist(char *inpath, int dirnum)
{
	char	str[1024];
	char	listpath[MAX_PATH+1];
	char*	p;
	int		found;
	long	l;
	FILE*	stream;
	file_t*	f;
	file_t*	file_list;
	smb_t	smb;

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

	int result = smb_open_dir(&scfg, &smb, dirnum);
	if(result != SMB_SUCCESS) {
		fprintf(stderr, "!Error %d (%s) opening %s\n", result, smb.last_error, smb.file);
		fclose(stream);
		return;
	}
	size_t file_count;
	file_list = loadfiles(&smb, NULL, 0, /* extdesc: */FALSE, FILE_SORT_NATURAL, &file_count);

	printf("\nSynchronizing %s with %s %s\n\n"
		,listpath,scfg.lib[scfg.dir[dirnum]->lib]->sname,scfg.dir[dirnum]->sname);

	for(l = 0; l < (long)file_count; l++) {
		f = &file_list[l];
		rewind(stream);
		found=0;
		while(!found) {
			if(!fgets(str,1000,stream))
				break;
			truncsp(str);
			p=strchr(str,' ');
			if(p) *p=0;
			if(!stricmp(str, f->name))
				found=1; 
		}
		if(found)
			continue;
		printf("%s not found in list - ", f->name);
		if(smb_removefile(&smb, f) != SMB_SUCCESS)
			fprintf(stderr, "!ERROR (%s) removing file from database\n", smb.last_error);
		else {
			if(remove(getfilepath(&scfg, f, str)))
				printf("Error removing %s\n",str);
			removed++;
			printf("Removed from database\n");
		}
	}
	freefiles(file_list, file_count);

	if(mode&DEL_LIST) {
		printf("\nDeleting %s\n",listpath);
		remove(listpath);
	}
	smb_close(&smb);
	fclose(stream);
}

char *usage="\nusage: addfiles code [.alt_path] [-opts] +list "
			 "[desc_off] [size_off]"
	"\n   or: addfiles code [.alt_path] [-opts] file "
		"[\"description\"]\n"
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
	char str[MAX_PATH+1];
	char tmp[MAX_PATH+1];
	char *p;
	char exist,listgiven=0,namegiven=0,ext[LEN_EXTDESC + 1]
		,auto_name[MAX_PATH+1]="FILES.BBS";
	int i,j;
	uint desc_offset=0, size_offset=0;
	long l;
	smb_t	smb;
	file_t	f;
	uint dirnum = INVALID_DIR;

	fprintf(stderr,"\nADDFILES v%s-%s %s/%s - Adds Files to Synchronet "
		"Filebase\n"
		,ADDFILES_VER
		,PLATFORM_DESC
		,GIT_BRANCH
		,GIT_HASH
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
	if(!load_cfg(&scfg, /* text: */NULL, /* prep: */TRUE, /* node: */FALSE, error, sizeof(error))) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}
	SAFECOPY(scfg.temp_dir,"../temp");
	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
	memset(&smb, 0, sizeof(smb));

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
		if(!IS_ALPHANUMERIC(argv[1][0]) && argc==2) {
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
		dirnum = i;
	}

	memset(&f,0,sizeof(f));
	const char* uploader = "-> ADDFILES <-";

	for(j=2;j<argc;j++) {
		if(argv[j][0]=='*')     /* set the uploader name (legacy) */
			uploader = argv[j]+1;
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
						uploader = argv[j];
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
		else if(IS_DIGIT(argv[j][0])) {
			if(desc_offset==0)
				desc_offset=atoi(argv[j]);
			else
				size_offset=atoi(argv[j]);
			continue;
		}
		else if(argv[j][0]=='+') {      /* filelist - FILES.BBS */
			listgiven=1;
			if(argc > j+1
				&& IS_DIGIT(argv[j+1][0])) { /* skip x characters before description */
				if(argc > j+2
					&& IS_DIGIT(argv[j+2][0])) { /* skip x characters before size */
					addlist(argv[j]+1, dirnum, uploader, atoi(argv[j+1]), atoi(argv[j+2]));
					j+=2;
				}
				else {
					addlist(argv[j]+1, dirnum, uploader, atoi(argv[j+1]), 0);
					j++; 
				} 
			}
			else
				addlist(argv[j]+1, dirnum, uploader, 0, 0);
			if(mode&SYNC_LIST)
				synclist(argv[j]+1, dirnum); 
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
			const char* fname = argv[j];
			char fdesc[LEN_FDESC + 1] = {0};

			if(j+1==argc) {
				printf("%s no description given.\n",fname);
				SAFECOPY(fdesc, "no description given");
			}

			sprintf(str,"%s%s",cur_altpath ? scfg.altpath[cur_altpath-1]
				: scfg.dir[dirnum]->path, fname);
			if(mode&FILE_DATE)
				sprintf(fdesc, "%s  ", unixtodstr(&scfg,(time32_t)fdate(str),tmp));
			if(mode&TODAYS_DATE)
				sprintf(fdesc, "%s  ", unixtodstr(&scfg,time32(NULL),tmp));
			sprintf(tmp, "%.*s", (int)(LEN_FDESC-strlen(fdesc)), argv[++j]);
			SAFECOPY(fdesc, tmp);
			l=(long)flength(str);
			if(l==-1) {
				printf("%s not found.\n",str);
				continue;
			}
			if(SMB_IS_OPEN(&smb))
				smb_close(&smb);

			int result = smb_open_dir(&scfg, &smb, dirnum);
			if(result != SMB_SUCCESS) {
				fprintf(stderr, "!ERROR %d (%s) opening %s\n", result, smb.last_error, smb.file);
				exit(EXIT_FAILURE);
			}

			exist = smb_findfile(&smb, fname, &f) == SMB_SUCCESS;
			if(exist) {
				if(mode&NO_UPDATE)
					continue;
				if((mode&CHECK_DATE) && fdate(str) <= f.idx.time)
					continue;
				if(mode&ULDATE_ONLY) {
					reupload(&smb, &f);
					continue; 
				} 
			}
			memset(&f, 0, sizeof(f));
			prep_file_desc(fdesc, fdesc);
			if(mode&ASCII_ONLY)
				strip_exascii(fdesc, fdesc);
			smb_hfield_str(&f, SMB_FILENAME, fname);
			smb_hfield_str(&f, SMB_FILEDESC, fdesc);
			smb_hfield_str(&f, SENDER, uploader);
			uint32_t cdt = (uint32_t)l;
			smb_hfield_bin(&f, SMB_COST, cdt);

			printf("%s %7"PRIu64" %s\n",fname, (int64_t)l, fdesc);
			char* ext_desc = NULL;
			if(get_file_diz(&f, ext, sizeof(ext)))
				ext_desc = ext;
			if(exist) {
				if(!(mode&NO_NEWDATE))
					reupload(&smb, &f);
				else
					result = smb_updatemsg(&smb, &f);
			}
			else
				result = smb_addfile(&smb, &f, SMB_SELFPACK, ext_desc, NULL, str);
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
			dirnum = i;
			if(mode&SEARCH_DIR) {
				addlist("", dirnum, uploader, desc_offset, size_offset);
				continue; 
			}
			sprintf(str,"%s%s.lst",scfg.dir[dirnum]->path,scfg.dir[dirnum]->code);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str, dirnum, uploader, desc_offset, size_offset);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue;
			}
			SAFEPRINTF2(str,"%s%s",scfg.dir[dirnum]->path,auto_name);
			if(fexistcase(str) && flength(str)>0L) {
				printf("Auto-adding %s\n",str);
				addlist(str, dirnum, uploader, desc_offset, size_offset);
				if(mode&SYNC_LIST)
					synclist(str,i);
				continue;
			}
		}
	}

	else {
		if(!listgiven && !namegiven) {
			sprintf(str,"%s%s.lst",scfg.dir[dirnum]->path, scfg.dir[dirnum]->code);
			if(!fexistcase(str) || flength(str)<=0L)
				SAFEPRINTF2(str,"%s%s",scfg.dir[dirnum]->path, auto_name);
			addlist(str, dirnum, uploader, desc_offset, size_offset);
			if(mode&SYNC_LIST)
				synclist(str, dirnum);
		}
	}

	printf("\n%lu file(s) added.",files);
	if(removed)
		printf("\n%lu files(s) removed.",removed);
	printf("\n");
	return(0);
}

