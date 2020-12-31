/* Upgrade Synchronet files from v3.18 to v3.19 */

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

#include <stdbool.h>
#include "sbbs.h"
#include "sbbs4defs.h"
#include "ini_file.h"
#include "dat_file.h"
#include "datewrap.h"

scfg_t scfg;
BOOL overwrite_existing_files=TRUE;
ini_style_t style = { 25, NULL, NULL, " = ", NULL };

BOOL overwrite(const char* path)
{
	char	str[128];

	if(!overwrite_existing_files && fexist(path)) {
		printf("\n%s already exists, overwrite? ",path);
		fgets(str,sizeof(str),stdin);
		if(toupper(*str)!='Y')
			return(FALSE);
	}

	return(TRUE);
}

int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(puts(sbuf));
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
long DLLCALL dstrtodate(scfg_t* cfg, char *instr)
{
	char*	p;
	char*	day;
	char	str[16];
	struct tm tm;

	if(!instr[0] || !strncmp(instr,"00/00/00",8))
		return(0);

	if(isdigit(instr[0]) && isdigit(instr[1])
		&& isdigit(instr[3]) && isdigit(instr[4])
		&& isdigit(instr[6]) && isdigit(instr[7]))
		p=instr;	/* correctly formatted */
	else {
		p=instr;	/* incorrectly formatted */
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		day=p;
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		sprintf(str,"%02u/%02u/%02u"
			,atoi(instr)%100,atoi(day)%100,atoi(p)%100);
		p=str;
	}

	memset(&tm,0,sizeof(tm));
	tm.tm_year=((p[6]&0xf)*10)+(p[7]&0xf);
	if(cfg->sys_misc&SM_EURODATE) {
		tm.tm_mon=((p[3]&0xf)*10)+(p[4]&0xf);
		tm.tm_mday=((p[0]&0xf)*10)+(p[1]&0xf); }
	else {
		tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
		tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); }

	return(((tm.tm_year+1900)*10000)+(tm.tm_mon*100)+tm.tm_mday);
}

int file_uldate_compare(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return f1->dateuled - f2->dateuled;
}

bool upgrade_file_bases(void)
{
	int result;
	ulong total_files = 0;
	time_t start = time(NULL);

	printf("Upgrading File Bases...\n");

	for(int i = 0; i < scfg.total_dirs; i++) {
		smb_t smb;

		SAFEPRINTF2(smb.file, "%s%s", scfg.dir[i]->data_dir, scfg.dir[i]->code);
		if((result = smb_open(&smb)) != SMB_SUCCESS) {
			fprintf(stderr, "Error %d (%s) opening %s\n", result, smb.last_error, smb.file);
			return false;
		}
		smb.status.attr = SMB_FILE_DIRECTORY|SMB_NOHASH;
		smb.status.max_age = scfg.dir[i]->maxage;
		smb.status.max_msgs = scfg.dir[i]->maxfiles;
		if((result = smb_create(&smb)) != SMB_SUCCESS)
			return false;

		char str[MAX_PATH+1];
		int file;
		int extfile = openextdesc(&scfg, i);

		sprintf(str,"%s%s.ixb",scfg.dir[i]->data_dir,scfg.dir[i]->code);
		if((file=open(str,O_RDONLY|O_BINARY))==-1)
			continue;
		long l=filelength(file);
		if(!l) {
			close(file);
			continue;
		}
		uchar* ixbbuf;
		if((ixbbuf=(uchar *)malloc(l))==NULL) {
			close(file);
			printf("\7ERR_ALLOC %s %lu\n",str,l);
			continue;
		}
		if(read(file,ixbbuf,l)!=(int)l) {
			close(file);
			printf("\7ERR_READ %s %lu\n",str,l);
			free(ixbbuf);
			continue;
		}
		close(file);
		size_t file_count = l / F_IXBSIZE;
		file_t* filelist = malloc(sizeof(file_t) * file_count);
		if(filelist == NULL) {
			printf("malloc failure");
			return false;
		}
		memset(filelist, 0, sizeof(file_t) * file_count);
		file_t* f = filelist;
		long m=0L;
		while(m + F_IXBSIZE <= l) {
			int j;
			f->dir = i;
			for(j=0;j<12 && m<l;j++)
				if(j==8)
					f->name[j]=ixbbuf[m]>' ' ? '.' : ' ';
				else
					f->name[j]=ixbbuf[m++]; /* Turns FILENAMEEXT into FILENAME.EXT */
			f->name[j]=0;
			f->datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
			f->dateuled=(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)|((long)ixbbuf[m+5]<<16)
				|((long)ixbbuf[m+6]<<24));
			f->datedled =(ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)|((long)ixbbuf[m+9]<<16)
				|((long)ixbbuf[m+10]<<24));
			m+=11;
			f++;
		};

		/* SMB index is sorted by import (upload) time */
		qsort(filelist, file_count, sizeof(*filelist), file_uldate_compare);

		for(size_t fi = 0; fi < file_count; fi++) {
			f = &filelist[fi];
			if(!getfiledat(&scfg, f)) {
				fprintf(stderr, "Error getting file data for %s %s\n", scfg.dir[i]->code, f->name);
				continue;
			}
			char fpath[MAX_PATH+1];
			getfilepath(&scfg, f, fpath);
			smbfile_t file;
			memset(&file, 0, sizeof(file));
			file.hdr.when_written.time = (time32_t)fdate(fpath);
			file.hdr.when_imported.time = f->dateuled;
			file.hdr.last_downloaded = f->datedled;
			file.hdr.times_downloaded = f->timesdled;
			file.hdr.altpath = f->altpath;
			smb_hfield_str(&file, SMB_FILENAME, getfname(fpath));
			smb_hfield_str(&file, SMB_FILEDESC, f->desc);
			smb_hfield_str(&file, SENDER, f->uler);
			smb_hfield_bin(&file, SMB_COST, f->cdt);
			if(f->misc&FM_ANON)
				file.hdr.attr |= MSG_ANONYMOUS;
			{
				const char* body = NULL;
				char extdesc[F_EXBSIZE+1] = {0};
				if(f->misc&FM_EXTDESC && extfile > 0) {
					fgetextdesc(&scfg, i, f->datoffset, extdesc, extfile);
					truncsp(extdesc);
					body = extdesc;
				}
				result = smb_addfile(&smb, &file, SMB_FASTALLOC, body);
			}
			if(result != SMB_SUCCESS) {
				fprintf(stderr, "Error %d (%s) adding file to %s\n", result, smb.last_error, smb.file);
			} else {
				total_files++;
				time_t diff = time(NULL) - start;
				printf("\r%-16s (%-5u bases remain) %lu files imported (%lu files/second)"
					, scfg.dir[i]->code, scfg.total_dirs - (i + 1), total_files, (ulong)(diff ? total_files / diff : total_files));
			}
		}
		free(filelist);
		smb_close(&smb);
		closeextdesc(extfile);
		free(ixbbuf);
	}
	printf("\r%lu files imported in %u directories%40s\n", total_files, scfg.total_dirs,"");

	return true;
}

char *usage="\nusage: upgrade [ctrl_dir]\n";

int main(int argc, char** argv)
{
	char	error[512];

	fprintf(stderr,"\nupgrade - Upgrade Synchronet BBS to %s\n"
		,VERSION
		);

	memset(&scfg, 0, sizeof(scfg));
	scfg.size = sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */true));

	if(chdir(scfg.ctrl_dir) != 0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		return EXIT_FAILURE + __COUNTER__;
	}

	if(!upgrade_file_bases())
		return EXIT_FAILURE + __COUNTER__;

	printf("Upgrade successful.\n");
    return EXIT_SUCCESS;
}
