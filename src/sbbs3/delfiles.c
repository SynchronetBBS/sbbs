/* Program to delete expired files from a Synchronet file database */

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

#include "scfgdefs.h"
#include "load_cfg.h"
#include "filedat.h"
#include "nopen.h"
#include "smblib.h"
#include "str_util.h"
#include <stdarg.h>
#include <stdbool.h>
#include "git_branch.h"
#include "git_hash.h"

#define DELFILES_VER "3.19"

char tmp[256];
char *crlf="\r\n";

#define MAX_NOTS	25

#define ALL 	(1L<<0)
#define OFFLINE (1L<<1)
#define NO_LINK (1L<<2)
#define REPORT	(1L<<3)

void bail(int code)
{
	exit(code);
}

long lputs(char *str)
{
    char tmp[256];
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
int lprintf(const char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	lputs(sbuf);
	return(chcount);
}

bool delfile(const char *filename)
{
	if(remove(filename) != 0) {
		fprintf(stderr, "ERROR %u (%s) removing file %s\n"
			,errno, strerror(errno), filename);
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	char str[256],not[MAX_NOTS][LEN_EXTCODE + 1],nots=0,*p;
	char fpath[MAX_PATH+1];
	int i,j,dirnum,libnum;
	size_t fi;
	long misc=0;
	time_t now = time(NULL);
	scfg_t cfg;
	glob_t gl;

	setvbuf(stdout,NULL,_IONBF,0);

	fprintf(stderr,"\nDELFILES Version %s-%s %s/%s - Removes files from Synchronet "
		"Filebase\n" ,DELFILES_VER, PLATFORM_DESC, GIT_BRANCH, GIT_HASH);

	if(argc<2) {
		printf("\n   usage: %s <dir_code or * for ALL> [switches]\n", argv[0]);
		printf("\nswitches: -lib name All directories of specified library\n");
		printf("          -all      Search all directories\n");
		printf("          -not code Exclude specific directory\n");
		printf("          -off      Remove files that are offline "
			"(don't exist on disk)\n");
		printf("          -nol      Remove files with no link "
			"(don't exist in database)\n");
		printf("          -rpt      Report findings only "
			"(don't delete any files)\n");
		return(0); 
	}

	p = get_ctrl_dir(/* warn: */TRUE);

	memset(&cfg, 0, sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir, p);
	backslash(cfg.ctrl_dir);

	load_cfg(&cfg, /* text: */NULL, /* prep: */TRUE, /* node: */FALSE, str, sizeof(str));
	(void)chdir(cfg.ctrl_dir);

	dirnum=libnum=-1;
	if(argv[1][0]=='*')
		misc|=ALL;
	else if(argv[1][0]!='/' && argv[1][0]!='-') {
		strupr(argv[1]);
		for(i=0;i<cfg.total_dirs;i++)
			if(!stricmp(argv[1],cfg.dir[i]->code))
				break;
		if(i>=cfg.total_dirs) {
			printf("\nDirectory code '%s' not found.\n",argv[1]);
			return(1); 
		}
		dirnum=i; 
	}
	for(i=1;i<argc;i++) {
		if(!stricmp(argv[i]+1,"LIB")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and /LIB parameters were used.\n");
				return(1); 
			}
			i++;
			if(i>=argc) {
				printf("\nLibrary short name must follow /LIB parameter.\n");
				return(1); 
			}
			strupr(argv[i]);
			for(j=0;j<cfg.total_libs;j++)
				if(!stricmp(cfg.lib[j]->sname,argv[i]))
					break;
			if(j>=cfg.total_libs) {
				printf("\nLibrary short name '%s' not found.\n",argv[i]);
				return(1); 
			}
			libnum=j; 
		}
		else if(!stricmp(argv[i]+1,"NOT")) {
			if(nots>=MAX_NOTS) {
				printf("\nMaximum number of /NOT options (%u) exceeded.\n"
					,MAX_NOTS);
				return(1); 
			}
			i++;
			if(i>=argc) {
				printf("\nDirectory internal code must follow /NOT parameter.\n");
				return(1); 
			}
			SAFECOPY(not[nots], argv[i]); 
			nots++;
		}
		else if(!stricmp(argv[i]+1,"OFF"))
			misc|=OFFLINE;
		else if(!stricmp(argv[i]+1,"NOL"))
			misc|=NO_LINK;
		else if(!stricmp(argv[i]+1,"RPT"))
			misc|=REPORT;
		else if(!stricmp(argv[i]+1,"ALL")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and /ALL parameters were used.\n");
				return(1); 
			}
			if(libnum!=-1) {
				printf("\nBoth library name and /ALL parameters were used.\n");
				return(1); 
			}
			misc|=ALL; 
		} 
	}

	for(i=0;i<cfg.total_dirs;i++) {
		if(!(misc&ALL) && i!=dirnum && cfg.dir[i]->lib!=libnum)
			continue;
		for(j=0;j<nots;j++)
			if(!stricmp(not[j], cfg.dir[i]->code))
				break;
		if(j<nots)
			continue;

		smb_t smb;
		int result = smb_open_dir(&cfg, &smb, i);
		if(result != SMB_SUCCESS) {
			fprintf(stderr, "!ERROR %d (%s) opening %s\n", result, smb.last_error, smb.file);
			continue;
		}

		if(misc&NO_LINK && cfg.dir[i]->misc&DIR_FCHK) {
			printf("\nSearching %s for unlinked files\n", cfg.dir[i]->path);
			sprintf(str, "%s%s", cfg.dir[i]->path, ALLFILES);
			if(glob(str, GLOB_MARK, NULL, &gl) == 0) {
				for(j=0; j<(int)gl.gl_pathc; j++) {
					const char* fname = gl.gl_pathv[j];
					/* emulate _A_NORMAL */
					if(isdir(fname))
						continue;
					if(access(fname, R_OK|W_OK))
						continue;
					if(!smb_findfile(&smb, getfname(fname), /* file: */NULL)) {
						printf("Not in database: %s\n", fname);
						if(!(misc&REPORT))
							delfile(fname);
					}
				}
			}
			globfree(&gl);
		}

		if(!cfg.dir[i]->maxage && !(misc&OFFLINE)) {
			smb_close(&smb);
			continue;
		}

		printf("\nScanning %s %s\n", cfg.lib[cfg.dir[i]->lib]->sname, cfg.dir[i]->lname);

		size_t file_count;
		file_t* file_list = loadfiles(&smb, NULL, /* since: */0, file_detail_normal, FILE_SORT_NATURAL, &file_count);

		for(fi = 0; fi < file_count; fi++) {
			file_t* f = &file_list[fi];
			getfilepath(&cfg, f, fpath);
			if(cfg.dir[i]->maxage && cfg.dir[i]->misc&DIR_SINCEDL && f->hdr.last_downloaded
				&& (now - f->hdr.last_downloaded)/86400L > cfg.dir[i]->maxage) {
					printf("Deleting %s (%ld days since last download)\n"
						,f->name
						,(long)(now - f->hdr.last_downloaded)/86400L);
					if(!(misc&REPORT)) {
						if(smb_removefile(&smb, f) == SMB_SUCCESS)
							delfile(fpath);
						else
							printf("ERROR (%s) removing file from database\n", smb.last_error);
					} 
			}
			else if(cfg.dir[i]->maxage
				&& !(f->hdr.last_downloaded && cfg.dir[i]->misc&DIR_SINCEDL)
				&& (now - f->hdr.when_imported.time)/86400L > cfg.dir[i]->maxage) {
					printf("Deleting %s (uploaded %ld days ago)\n"
						,f->name
						,(long)(now - f->hdr.when_imported.time)/86400L);
					if(!(misc&REPORT)) {
						if(smb_removefile(&smb, f) == SMB_SUCCESS)
							delfile(fpath);
						else
							printf("ERROR (%s) removing file from database\n", smb.last_error);
					} 
			}
			else if(misc&OFFLINE && cfg.dir[i]->misc&DIR_FCHK && !fexist(fpath)) {
					printf("Removing %s (doesn't exist)\n", f->name);
					if(!(misc&REPORT)) {
						if(smb_removefile(&smb, f) != SMB_SUCCESS)
							printf("ERROR (%s) removing file from database\n", smb.last_error);
					}
			} 
		}
		freefiles(file_list, file_count);

		smb_close(&smb);
	}

	return(0);
}
