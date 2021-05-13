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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdbool.h>
#include "sbbs.h"
#include "sbbs4defs.h"
#include "ini_file.h"
#include "dat_file.h"
#include "datewrap.h"
#include "filedat.h"

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
long dstrtodate(scfg_t* cfg, char *instr)
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

/*****************************/
/* LEGACY FILEBASE CONSTANTS */
/*****************************/
#define LEN_FCDT		 9	/* 9 digits for file credit values				*/
/****************************************************************************/
/* Offsets into DIR .DAT file for different fields for each file 			*/
/****************************************************************************/
#define F_CDT		0				/* Offset in DIR#.DAT file for cdts		*/
#define F_DESC		(F_CDT+LEN_FCDT)/* Description							*/
#define F_ULER		(F_DESC+LEN_FDESC+2)   /* Uploader						*/
#define F_TIMESDLED (F_ULER+30+2) 	/* Number of times downloaded 			*/
#define F_OPENCOUNT	(F_TIMESDLED+5+2)
#define F_MISC		(F_OPENCOUNT+3+2)
#define F_ALTPATH	(F_MISC+1)		/* Two hex digit alternate path */
#define F_LEN		(F_ALTPATH+2+2) /* Total length of all fdat in file		*/

#define F_IXBSIZE	22				/* Length of each index entry			*/

#define F_EXBSIZE	512				/* Length of each ext-desc entry		*/

/***********************/
/* LEGACY FILEBASE API */
/***********************/
typedef struct {						/* File (transfers) Data */
	char    name[13],					/* Name of file FILENAME.EXT */
			desc[LEN_FDESC+1],			/* Uploader's Description */
			uler[LEN_ALIAS+1];			/* User who uploaded */
	uchar	opencount;					/* Times record is currently open */
	time32_t  date,						/* File date/time */
			dateuled,					/* Date/Time (Unix) Uploaded */
			datedled;					/* Date/Time (Unix) Last downloaded */
	uint16_t	dir,						/* Directory file is in */
			altpath,
			timesdled,					/* Total times downloaded */
			timetodl;					/* How long transfer time */
	int32_t	datoffset,					/* Offset into .DAT file */
			size,						/* Size of file */
			misc;						/* Miscellaneous bits */
	uint32_t	cdt;						/* Credit value for this file */

} oldfile_t;

                                    /* Bit values for file_t.misc */
#define FM_EXTDESC  (1<<0)          /* Extended description exists */
#define FM_ANON 	(1<<1)			/* Anonymous upload */

/****************************************************************************/
/* Turns FILE.EXT into FILE    .EXT                                         */
/****************************************************************************/
char* padfname(const char *filename, char *str)
{
    int c,d;

	for(c=0;c<8;c++)
		if(filename[c]=='.' || !filename[c]) break;
		else str[c]=filename[c];
	d=c;
	if(filename[c]=='.') c++;
	while(d<8)
		str[d++]=' ';
	if(filename[c]>' ')	/* Change "FILE" to "FILE        " */
		str[d++]='.';	/* (don't add a dot if there's no extension) */
	else
		str[d++]=' ';
	while(d<12)
		if(!filename[c]) break;
		else str[d++]=filename[c++];
	while(d<12)
		str[d++]=' ';
	str[d]=0;
	return(str);
}

/****************************************************************************/
/* Turns FILE    .EXT into FILE.EXT                                         */
/****************************************************************************/
char* unpadfname(const char *filename, char *str)
{
    int c,d;

	for(c=0,d=0;filename[c];c++)
		if(filename[c]!=' ') str[d++]=filename[c];
	str[d]=0;
	return(str);
}

/****************************************************************************/
/* Returns full (case-corrected) path to specified file						*/
/****************************************************************************/
char* getoldfilepath(scfg_t* cfg, oldfile_t* f, char* path)
{
	char	fname[MAX_PATH+1];

	unpadfname(f->name,fname);
	if(f->dir>=cfg->total_dirs)
		safe_snprintf(path,MAX_PATH,"%s%s",cfg->temp_dir,fname);
	else
		safe_snprintf(path,MAX_PATH,"%s%s",f->altpath>0 && f->altpath<=cfg->altpaths 
			? cfg->altpath[f->altpath-1] : cfg->dir[f->dir]->path
			,fname);
	if(!fexistcase(path)) {
		char tmp[MAX_PATH + 1];
		safe_snprintf(tmp,MAX_PATH,"%s%s",f->altpath>0 && f->altpath<=cfg->altpaths 
			? cfg->altpath[f->altpath-1] : cfg->dir[f->dir]->path
			,f->desc);
		if(fexistcase(tmp))
			strcpy(path, tmp);
	}
	return(path);
}

int file_uldate_compare(const void* v1, const void* v2)
{
	oldfile_t* f1 = (oldfile_t*)v1;
	oldfile_t* f2 = (oldfile_t*)v2;

	return f1->dateuled - f2->dateuled;
}

/****************************************************************************/
/* Gets filedata from dircode.DAT file										*/
/* Need fields .name ,.dir and .offset to get other info    				*/
/* Does not fill .dateuled or .datedled fields.                             */
/****************************************************************************/
BOOL getfiledat(scfg_t* cfg, oldfile_t* f)
{
	char buf[F_LEN+1],str[MAX_PATH+1];
	int file;
	long length;

	SAFEPRINTF2(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=(long)filelength(file);
	if(f->datoffset>length) {
		close(file);
		return(FALSE); 
	}
	if(length%F_LEN) {
		close(file);
		return(FALSE); 
	}
	lseek(file,f->datoffset,SEEK_SET);
	if(read(file,buf,F_LEN)!=F_LEN) {
		close(file);
		return(FALSE); 
	}
	close(file);
	getrec(buf,F_ALTPATH,2,str);
	f->altpath=hptoi(str);
	getrec(buf,F_CDT,LEN_FCDT,str);
	f->cdt=atol(str);

	if(f->size == 0) {					// only read disk if f->size == 0
		struct stat st;
		getoldfilepath(cfg,f,str);
		if(stat(str, &st) == 0) {
			f->size = (int32_t)st.st_size;
			f->date = (time32_t)st.st_mtime;
		} else
			f->size = -1;	// indicates file does not exist
	}
#if 0
	if((f->size>0L) && cur_cps)
		f->timetodl=(ushort)(f->size/(ulong)cur_cps);
	else
#endif
		f->timetodl=0;

	getrec(buf,F_DESC,LEN_FDESC,f->desc);
	getrec(buf,F_ULER,LEN_ALIAS,f->uler);
	getrec(buf,F_TIMESDLED,5,str);
	f->timesdled=atoi(str);
	getrec(buf,F_OPENCOUNT,3,str);
	f->opencount=atoi(str);
	if(buf[F_MISC]!=ETX)
		f->misc=buf[F_MISC]-' ';
	else
		f->misc=0;
	return(TRUE);
}

/****************************************************************************/
/* Puts filedata into DIR_code.DAT file                                     */
/* Called from removefiles                                                  */
/****************************************************************************/
BOOL putfiledat(scfg_t* cfg, oldfile_t* f)
{
    char buf[F_LEN+1],str[MAX_PATH+1],tmp[128];
    int file;
    long length;

	putrec(buf,F_CDT,LEN_FCDT,ultoa(f->cdt,tmp,10));
	putrec(buf,F_DESC,LEN_FDESC,f->desc);
	putrec(buf,F_DESC+LEN_FDESC,2, "\r\n");
	putrec(buf,F_ULER,LEN_ALIAS+5,f->uler);
	putrec(buf,F_ULER+LEN_ALIAS+5,2, "\r\n");
	putrec(buf,F_TIMESDLED,5,ultoa(f->timesdled,tmp,10));
	putrec(buf,F_TIMESDLED+5,2, "\r\n");
	putrec(buf,F_OPENCOUNT,3,ultoa(f->opencount,tmp,10));
	putrec(buf,F_OPENCOUNT+3,2, "\r\n");
	buf[F_MISC]=(char)f->misc+' ';
	putrec(buf,F_ALTPATH,2,hexplus(f->altpath,tmp));
	putrec(buf,F_ALTPATH+2,2, "\r\n");
	SAFEPRINTF2(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_WRONLY|O_BINARY,SH_DENYRW))==-1) {
		return(FALSE); 
	}
	length=(long)filelength(file);
	if(length%F_LEN) {
		close(file);
		return(FALSE); 
	}
	if(f->datoffset>length) {
		close(file);
		return(FALSE); 
	}
	lseek(file,f->datoffset,SEEK_SET);
	if(write(file,buf,F_LEN)!=F_LEN) {
		close(file);
		return(FALSE); 
	}
	length=(long)filelength(file);
	close(file);
	if(length%F_LEN) {
		return(FALSE);
	}
	return(TRUE);
}

/****************************************************************************/
/* Gets file data from dircode.ixb file										*/
/* Need fields .name and .dir filled.                                       */
/* only fills .offset, .dateuled, and .datedled                             */
/****************************************************************************/
BOOL getfileixb(scfg_t* cfg, oldfile_t* f)
{
	char			str[MAX_PATH+1],fname[13];
	uchar *	ixbbuf;
	int				file;
	long			l,length;

	SAFEPRINTF2(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=(long)filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		return(FALSE); 
	}
	if((ixbbuf=(uchar *)malloc(length))==NULL) {
		close(file);
		return(FALSE); 
	}
	if(read(file,ixbbuf,length)!=length) {
		close(file);
		free(ixbbuf);
		return(FALSE); 
	}
	close(file);
	SAFECOPY(fname,f->name);
	for(l=8;l<12;l++)	/* Turn FILENAME.EXT into FILENAMEEXT */
		fname[l]=fname[l+1];
	for(l=0;l<length;l+=F_IXBSIZE) {
		SAFEPRINTF(str,"%11.11s",ixbbuf+l);
		if(!stricmp(str,fname))
			break; 
	}
	if(l>=length) {
		free(ixbbuf);
		return(FALSE); 
	}
	l+=11;
	f->datoffset=ixbbuf[l]|((long)ixbbuf[l+1]<<8)|((long)ixbbuf[l+2]<<16);
	f->dateuled=ixbbuf[l+3]|((long)ixbbuf[l+4]<<8)
		|((long)ixbbuf[l+5]<<16)|((long)ixbbuf[l+6]<<24);
	f->datedled=ixbbuf[l+7]|((long)ixbbuf[l+8]<<8)
		|((long)ixbbuf[l+9]<<16)|((long)ixbbuf[l+10]<<24);
	free(ixbbuf);
	return(TRUE);
}

/****************************************************************************/
/* Updates the datedled and dateuled index record fields for a file			*/
/****************************************************************************/
BOOL putfileixb(scfg_t* cfg, oldfile_t* f)
{
	char	str[MAX_PATH+1],fname[13];
	uchar*	ixbbuf;
	int		file;
	long	l,length;

	SAFEPRINTF2(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDWR|O_BINARY,SH_DENYRW))==-1) {
		return(FALSE); 
	}
	length=(long)filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		return(FALSE); 
	}
	if((ixbbuf=(uchar *)malloc(length))==NULL) {
		close(file);
		return(FALSE); 
	}
	if(read(file,ixbbuf,length)!=length) {
		close(file);
		free(ixbbuf);
		return(FALSE); 
	}
	SAFECOPY(fname,f->name);
	for(l=8;l<12;l++)	/* Turn FILENAME.EXT into FILENAMEEXT */
		fname[l]=fname[l+1];
	for(l=0;l<length;l+=F_IXBSIZE) {
		SAFEPRINTF(str,"%11.11s",ixbbuf+l);
		if(!stricmp(str,fname))
			break; 
	}
	free(ixbbuf);

	if(l>=length) {
		close(file);
		return(FALSE); 
	}
	
	lseek(file,l+11+3,SEEK_SET);

	write(file,&f->dateuled,4);
	write(file,&f->datedled,4);

	close(file);

	return(TRUE);
}

int openextdesc(scfg_t* cfg, uint dirnum)
{
	char str[MAX_PATH+1];
	SAFEPRINTF2(str,"%s%s.exb",cfg->dir[dirnum]->data_dir,cfg->dir[dirnum]->code);
	return nopen(str,O_RDONLY);
}

void closeextdesc(int file)
{
	if(file >= 0)
		close(file);
}

void getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext)
{
	int file;

	memset(ext,0,F_EXBSIZE+1);
	if((file=openextdesc(cfg, dirnum))==-1)
		return;
	lseek(file,(datoffset/F_LEN)*F_EXBSIZE,SEEK_SET);
	read(file,ext,F_EXBSIZE);
	close(file);
}

// fast (operates on open .exb file)
void fgetextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext, int file)
{
	lseek(file,(datoffset/F_LEN)*F_EXBSIZE,SEEK_SET);
	read(file,ext,F_EXBSIZE);
}

void putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext)
{
	char str[MAX_PATH+1],nulbuf[F_EXBSIZE];
	int file;

	strip_ansi(ext);
	strip_invalid_attr(ext);	/* eliminate bogus ctrl-a codes */
	memset(nulbuf,0,sizeof(nulbuf));
	SAFEPRINTF2(str,"%s%s.exb",cfg->dir[dirnum]->data_dir,cfg->dir[dirnum]->code);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return;
	lseek(file,0L,SEEK_END);
	while(filelength(file)<(long)(datoffset/F_LEN)*F_EXBSIZE)
		write(file,nulbuf,sizeof(nulbuf));
	lseek(file,(datoffset/F_LEN)*F_EXBSIZE,SEEK_SET);
	write(file,ext,F_EXBSIZE);
	close(file);
}

/****************************************************************************/
/* Update the upload date for the file 'f'                                  */
/****************************************************************************/
int update_uldate(scfg_t* cfg, oldfile_t* f)
{
	char str[MAX_PATH+1],fname[13];
	int i,file;
	long l,length;

	/*******************/
	/* Update IXB File */
	/*******************/
	SAFEPRINTF2(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=nopen(str,O_RDWR))==-1)
		return(errno); 
	length=(long)filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		return(-1); 
	}
	SAFECOPY(fname,f->name);
	for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
		fname[i]=fname[i+1];
	for(l=0;l<length;l+=F_IXBSIZE) {
		read(file,str,F_IXBSIZE);      /* Look for the filename in the IXB file */
		str[11]=0;
		if(!stricmp(fname,str)) break; 
	}
	if(l>=length) {
		close(file);
		return(-2); 
	}
	lseek(file,l+14,SEEK_SET);
	write(file,&f->dateuled,4);
	close(file);

	/*******************************************/
	/* Update last upload date/time stamp file */
	/*******************************************/
	SAFEPRINTF2(str,"%s%s.dab",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return(errno);

	write(file,&f->dateuled,4);
	close(file); 
	return(0);
}

bool upgrade_file_bases(bool hash)
{
	int result;
	ulong total_files = 0;
	time_t start = time(NULL);

	printf("Upgrading File Bases...\n");

	for(int i = 0; i < scfg.total_dirs; i++) {
		smb_t smb;

		(void)smb_init_dir(&scfg, &smb, i);
		if((result = smb_open(&smb)) != SMB_SUCCESS) {
			fprintf(stderr, "Error %d (%s) opening %s\n", result, smb.last_error, smb.file);
			return false;
		}
		smb.status.attr = SMB_FILE_DIRECTORY;
		if(!hash || (scfg.dir[i]->misc & DIR_NOHASH))
			smb.status.attr |= SMB_NOHASH;
		smb.status.max_age = scfg.dir[i]->maxage;
		smb.status.max_msgs = scfg.dir[i]->maxfiles;
		if((result = smb_create(&smb)) != SMB_SUCCESS) {
			fprintf(stderr, "Error %d (%s) creating %s\n", result, smb.last_error, smb.file);
			return false;
		}

		char str[MAX_PATH+1];
		int file;
		int extfile = openextdesc(&scfg, i);

		sprintf(str,"%s%s.ixb",scfg.dir[i]->data_dir,scfg.dir[i]->code);
		if((file=open(str,O_RDONLY|O_BINARY))==-1) {
			smb_close(&smb);
			closeextdesc(extfile);
			continue;
		}
		long l=(long)filelength(file);
		if(!l) {
			close(file);
			smb_close(&smb);
			closeextdesc(extfile);
			continue;
		}
		uchar* ixbbuf;
		if((ixbbuf=(uchar *)malloc(l))==NULL) {
			close(file);
			printf("\7ERR_ALLOC %s %lu\n",str,l);
			smb_close(&smb);
			closeextdesc(extfile);
			continue;
		}
		if(read(file,ixbbuf,l)!=(int)l) {
			close(file);
			printf("\7ERR_READ %s %lu\n",str,l);
			free(ixbbuf);
			smb_close(&smb);
			closeextdesc(extfile);
			continue;
		}
		close(file);
		size_t file_count = l / F_IXBSIZE;
		oldfile_t* filelist = malloc(sizeof(*filelist) * file_count);
		if(filelist == NULL) {
			printf("malloc failure");
			free(ixbbuf);
			smb_close(&smb);
			closeextdesc(extfile);
			return false;
		}
		memset(filelist, 0, sizeof(*filelist) * file_count);
		oldfile_t* f = filelist;
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

		time_t latest = 0;
		for(size_t fi = 0; fi < file_count; fi++) {
			f = &filelist[fi];
			if(!getfiledat(&scfg, f)) {
				fprintf(stderr, "\nError getting file data for %s %s\n", scfg.dir[i]->code, f->name);
				continue;
			}
			char fpath[MAX_PATH+1];
			getoldfilepath(&scfg, f, fpath);
			file_t file;
			memset(&file, 0, sizeof(file));
			file.hdr.when_imported.time = f->dateuled;
			file.hdr.last_downloaded = f->datedled;
			file.hdr.times_downloaded = f->timesdled;
			smb_hfield_str(&file, SMB_FILENAME, getfname(fpath));
			smb_hfield_str(&file, SMB_FILEDESC, f->desc);
			smb_hfield_str(&file, SENDER, f->uler);
			smb_hfield_bin(&file, SMB_COST, f->cdt);
			if(f->misc&FM_ANON)
				file.hdr.attr |= FILE_ANONYMOUS;
			{
				const char* body = NULL;
				char extdesc[F_EXBSIZE+1] = {0};
				if(f->misc&FM_EXTDESC && extfile > 0) {
					fgetextdesc(&scfg, i, f->datoffset, extdesc, extfile);
					truncsp(extdesc);
					if(*extdesc)
						body = extdesc;
				}
				result = smb_addfile(&smb, &file, SMB_FASTALLOC, body, /* contents: */NULL, fpath);
			}
			if(result != SMB_SUCCESS) {
				fprintf(stderr, "\n!Error %d (%s) adding file to %s\n", result, smb.last_error, smb.file);
			} else {
				total_files++;
				time_t diff = time(NULL) - start;
				printf("\r%-16s (%-5u bases remain) %lu files imported (%lu files/second)"
					, scfg.dir[i]->code, scfg.total_dirs - (i + 1), total_files, (ulong)(diff ? total_files / diff : total_files));
			}
			if(f->dateuled > latest)
				latest = f->dateuled;
			smb_freefilemem(&file);
		}
		free(filelist);
		off_t new_count = filelength(fileno(smb.sid_fp)) / smb_idxreclen(&smb);
		smb_close(&smb);
		if(latest > 0)
			update_newfiletime(&smb, latest);
		closeextdesc(extfile);
		free(ixbbuf);
		if(new_count != file_count) {
			printf("\n%s: New file base index has %u records instead of %u\n"
				,scfg.dir[i]->code, (uint)new_count, (uint)file_count);
		}
	}
	time_t diff = time(NULL) - start;
	printf("\r%lu files imported in %u directories (%lu files/second)%40s\n"
		,total_files, scfg.total_dirs, (ulong)(diff ? total_files / diff : total_files), "");

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
	if(!load_cfg(&scfg, NULL, TRUE, /* node: **/FALSE, error, sizeof(error))) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		return EXIT_FAILURE + __COUNTER__;
	}

	if(!upgrade_file_bases(/* hash: */true))
		return EXIT_FAILURE + __COUNTER__;

	printf("Upgrade successful.\n");
    return EXIT_SUCCESS;
}
