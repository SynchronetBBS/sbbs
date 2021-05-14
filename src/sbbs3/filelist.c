/* Utility to create list of files from Synchronet file directories */
/* Default list format is FILES.BBS, but file size, uploader, upload date */
/* and other information can be included. */

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

#include "datewrap.h"
#include "load_cfg.h"
#include "str_util.h"
#include "date_str.h"
#include "nopen.h"
#include "filedat.h"
#include "dat_rec.h"
#include "smblib.h"
#include <stdarg.h>

#define FILELIST_VER "3.19"

#define MAX_NOTS 25

scfg_t scfg;

char *crlf="\r\n";

/****************************************************************************/
/****************************************************************************/
int lprintf(int level, const char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	truncsp(sbuf);
	printf("%s\n",sbuf);
	return(chcount);
}

void stripctrlz(char *str)
{
	char tmp[1024];
	int i,j,k;

	k=strlen(str);
	for(i=j=0;i<k;i++)
		if(str[i]!=0x1a)
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
}

char* byteStr(unsigned long value)
{
	static char tmp[128];

	if(value>=(1024*1024*1024))
		sprintf(tmp, "%5.1fG", value/(1024.0*1024.0*1024.0));
	else if(value>=(1024*1024))
		sprintf(tmp, "%5.1fM", value/(1024.0*1024.0));
	else if(value>=1024)
		sprintf(tmp, "%5.1fK", value/1024.0); 
	else
		sprintf(tmp, "%5luB", value);
	return tmp;
}

void fprint_extdesc(FILE* fp, char* ext_desc, int desc_off)
{
	for(char* line = strtok(ext_desc, "\n"); line != NULL; line = strtok(NULL, "\n")) {
		truncsp(line);
		fprintf(fp, "\n%*s %s", desc_off, "", line);
	}
}

#define ALL 	(1L<<0)
#define PAD 	(1L<<1)
#define HDR 	(1L<<2)
#define CREDITS	(1L<<3)
#define EXT 	(1L<<4)
#define ULN 	(1L<<5)
#define ULD 	(1L<<6)
#define DLS 	(1L<<7)
#define DLD 	(1L<<8)
#define NOD 	(1L<<9)
#define PLUS	(1L<<10)
#define MINUS	(1L<<11)
#define JST 	(1L<<12)
#define NOE 	(1L<<13)
#define DFD 	(1L<<14)
#define TOT 	(1L<<15)
#define AUTO	(1L<<16)

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	char	revision[16];
	char	error[512];
	char	*p,str[256],fname[256],ext,not[MAX_NOTS][9];
	int 	i,j,dirnum,libnum,desc_off,lines,nots=0;
	char*	omode="w";
	char*	pattern=NULL;
	ulong	m,cdt,misc=0,total_cdt=0,total_files=0,dir_files;
	long	max_age=0;
	FILE*	out=NULL;

	sscanf("$Revision: 1.22 $", "%*s %s", revision);

	fprintf(stderr,"\nFILELIST v%s-%s (rev %s) - Generate Synchronet File "
		"Directory Lists\n"
		,FILELIST_VER
		,PLATFORM_DESC
		,revision
		);

	if(argc<2 
		|| strcmp(argv[1],"-?")==0 
		|| strcmp(argv[1],"-help")==0 
		|| strcmp(argv[1],"--help")==0 
		|| strcmp(argv[1],"/?")==0
		) {
		printf("\n   usage: FILELIST <dir_code or - for ALL> [switches] [outfile]\n");
		printf("\n");
		printf("switches: -lib name All directories of specified library\n");
		printf("          -not code Exclude specific directory\n");
		printf("          -new days Include only new files in listing (days since upload)\n");
		printf("          -inc pattern Only list files matching 'pattern'\n");
		printf("          -cat      Concatenate to existing 'outfile'\n");
		printf("          -pad      Pad filename with spaces\n");
		printf("          -hdr      Include directory headers\n");
		printf("          -cdt      Include credit value\n");
		printf("          -tot      Include credit totals\n");
		printf("          -uln      Include uploader's name\n");
		printf("          -uld      Include upload date\n");
		printf("          -dfd      Include current file date\n");
		printf("          -dld      Include download date\n");
		printf("          -dls      Include total downloads\n");
		printf("          -nod      Exclude normal descriptions\n");
		printf("          -noe      Exclude normal descriptions, if extended "
			"exists\n");
		printf("          -ext      Include extended descriptions\n");
		printf("          -jst      Justify extended descriptions under normal\n");
		printf("          -+        Include extended description indicator (+)\n");
		printf("          --        Include offline file indicator (-)\n");
		printf("          -*        Short-hand for -pad -hdr -cdt -+ --\n");
		exit(0); 
	}

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\nSBBSCTRL environment variable not set.\n");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1);
	}

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */TRUE));

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg, /* text: */NULL, /* prep: */TRUE, /* node: */FALSE, error, sizeof(error))) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}
	SAFECOPY(scfg.temp_dir,"../temp");
	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));

	dirnum=libnum=-1;
	if(argv[1][0]=='*' || strcmp(argv[1],"-")==0)
		misc|=ALL;
	else if(argv[1][0]!='-') {
		strupr(argv[1]);
		for(i=0;i<scfg.total_dirs;i++)
			if(!stricmp(argv[1],scfg.dir[i]->code))
				break;
		if(i>=scfg.total_dirs) {
			printf("\nDirectory code '%s' not found.\n",argv[1]);
			exit(1); 
		}
		dirnum=i; 
	}
	for(i=1;i<argc;i++) {
		if(!stricmp(argv[i],"-lib")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and -lib parameters were used.\n");
				exit(1); 
			}
			i++;
			if(i>=argc) {
				printf("\nLibrary short name must follow -lib parameter.\n");
				exit(1); 
			}
			strupr(argv[i]);
			for(j=0;j<scfg.total_libs;j++)
				if(!stricmp(scfg.lib[j]->sname,argv[i]))
					break;
			if(j>=scfg.total_libs) {
				printf("\nLibrary short name '%s' not found.\n",argv[i]);
				exit(1); 
			}
			libnum=j; 
		}
		else if(!stricmp(argv[i],"-not")) {
			if(nots>=MAX_NOTS) {
				printf("\nMaximum number of -not options (%u) exceeded.\n"
					,MAX_NOTS);
				exit(1); 
			}
			i++;
			if(i>=argc) {
				printf("\nDirectory internal code must follow -not parameter.\n");
				exit(1); 
			}
			sprintf(not[nots++],"%.8s",argv[i]); 
		}
		else if(!stricmp(argv[i],"-all")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and -all parameters were used.\n");
				exit(1); 
			}
			if(libnum!=-1) {
				printf("\nBoth library name and -all parameters were used.\n");
				exit(1); 
			}
			misc|=ALL; 
		}
		else if(!stricmp(argv[i],"-new")) {
			i++;
			if(i>=argc) {
				printf("\nDays since upload must follow -new parameter.\n");
				exit(1); 
			}
			max_age=strtol(argv[i],NULL,0);
		}
		else if(!stricmp(argv[i],"-inc")) {
			i++;
			if(i>=argc) {
				printf("\nFilename pattern must follow -inc parameter.\n");
				exit(1); 
			}
			pattern = argv[i];
		}
		else if(!stricmp(argv[i],"-pad"))
			misc|=PAD;
		else if(!stricmp(argv[i],"-cat"))
			omode="a";
		else if(!stricmp(argv[i],"-hdr"))
			misc|=HDR;
		else if(!stricmp(argv[i],"-cdt"))
			misc|=CREDITS;
		else if(!stricmp(argv[i],"-tot"))
			misc|=TOT;
		else if(!stricmp(argv[i],"-ext"))
			misc|=EXT;
		else if(!stricmp(argv[i],"-uln"))
			misc|=ULN;
		else if(!stricmp(argv[i],"-uld"))
			misc|=ULD;
		else if(!stricmp(argv[i],"-dld"))
			misc|=DLD;
		else if(!stricmp(argv[i],"-dfd"))
			misc|=DFD;
		else if(!stricmp(argv[i],"-dls"))
			misc|=DLS;
		else if(!stricmp(argv[i],"-nod"))
			misc|=NOD;
		else if(!stricmp(argv[i],"-jst"))
			misc|=(EXT|JST);
		else if(!stricmp(argv[i],"-noe"))
			misc|=(EXT|NOE);
		else if(!stricmp(argv[i],"-+"))
			misc|=PLUS;
		else if(!stricmp(argv[i],"--"))
			misc|=MINUS;
		else if(!stricmp(argv[i],"-*"))
			misc|=(HDR|PAD|CREDITS|PLUS|MINUS);

		else if(i!=1) {
			if(argv[i][0]=='*' || strcmp(argv[i],"-")==0) {
				misc|=AUTO;
				continue; 
			}
			if((out=fopen(argv[i], omode)) == NULL) {
				perror(argv[i]);
				exit(1); 
			}
		} 
	}

	if(!out && !(misc&AUTO)) {
		printf("\nOutput file not specified, using FILES.BBS in each "
			"directory.\n");
		misc|=AUTO; 
	}

	for(i=0;i<scfg.total_dirs;i++) {
		dir_files=0;
		if(!(misc&ALL) && i!=dirnum && scfg.dir[i]->lib!=libnum)
			continue;
		for(j=0;j<nots;j++)
			if(!stricmp(not[j],scfg.dir[i]->code))
				break;
		if(j<nots)
			continue;
		if(misc&AUTO && scfg.dir[i]->seqdev) 	/* CD-ROM */
			continue;
		printf("\n%-*s %s",LEN_GSNAME,scfg.lib[scfg.dir[i]->lib]->sname,scfg.dir[i]->lname);

		smb_t smb;
		int result = smb_open_dir(&scfg, &smb, i);
		if(result != SMB_SUCCESS) {
			fprintf(stderr, "!ERROR %d (%s) opening file base: %s\n", result, smb.last_error, scfg.dir[i]->code);
			continue;
		}
		time_t t = 0;
		if(max_age)
			t = time(NULL) - (max_age * 24 * 60 * 60);
		ulong file_count;
		file_t* file_list = loadfiles(&smb
			,/* filespec: */pattern, /* time: */t, file_detail_extdesc, scfg.dir[i]->sort, &file_count);

		if(misc&AUTO) {
			sprintf(str,"%sFILES.BBS",scfg.dir[i]->path);
			if((out=fopen(str, omode)) == NULL) {
				perror(str);
				exit(1); 
			}
		}
		if(misc&HDR) {
			sprintf(fname,"%-*s      %-*s       Files: %4lu"
				,LEN_GSNAME,scfg.lib[scfg.dir[i]->lib]->sname
				,LEN_SLNAME,scfg.dir[i]->lname, (ulong)smb.status.total_files);
			fprintf(out,"%s\n",fname);
			memset(fname,'-',strlen(fname));
			fprintf(out,"%s\n",fname); 
		}
		if(!smb.status.total_files) {
			if(misc&AUTO) fclose(out);
			continue; 
		}
		int longest_filename = 12;
		for(m = 0; m < file_count; m++) {
			int fnamelen = strlen(file_list[m].name);
			if(fnamelen > longest_filename)
				longest_filename = fnamelen;
		}
		for(m = 0; m < file_count && !ferror(out); m++) {
			file_t file = file_list[m];

			if(misc&PAD) {
				char* ext = getfext(file.name);
				if(ext == NULL)
					ext="";
				fprintf(out,"%-*.*s%s"
					, (int)(longest_filename - strlen(ext))
					, (int)(strlen(file.name) - strlen(ext))
					, file.name, ext);
			} else
				fprintf(out,"%-*s", longest_filename, file.name);

			total_files++;
			dir_files++;

			if(misc&PLUS && file.extdesc != NULL && file.extdesc[0])
				fputc('+',out);
			else
				fputc(' ',out);

			desc_off = longest_filename;
			if(misc&(CREDITS|TOT)) {
				cdt=file.cost;
				total_cdt+=cdt;
				if(misc&CREDITS) {
//					fprintf(out,"%7lu",cdt);
					desc_off += fprintf(out, "%7s", byteStr(cdt));
				} 
			}

			if(misc&MINUS) {
				sprintf(str,"%s%s",scfg.dir[i]->path,file.name);
				if(!fexistcase(str))
					fputc('-',out);
				else
					fputc(' ',out); 
			}
			else
				fputc(' ',out);
			desc_off++;

			if(misc&DFD) {
				// TODO: Fix to support alt-file-paths:
				sprintf(str,"%s%s",scfg.dir[i]->path,file.name);
				desc_off += fprintf(out,"%s ",unixtodstr(&scfg,(time32_t)fdate(str),str));
			}

			if(misc&ULD) {
				desc_off += fprintf(out,"%s ",unixtodstr(&scfg,file.hdr.when_imported.time,str));
			}

			if(misc&ULN) {
				desc_off += fprintf(out,"%-25s ",file.from);
			}

			if(misc&DLD) {
				desc_off += fprintf(out,"%s ",unixtodstr(&scfg,file.hdr.last_downloaded,str));
			}

			if(misc&DLS) {
				desc_off += fprintf(out,"%5u ",file.hdr.times_downloaded);
			}

			if(file.extdesc != NULL && file.extdesc[0])
				ext=1;	/* extended description exists */
			else
				ext=0;	/* it doesn't */

			if(!(misc&NOD) && !(misc&NOE && ext)) {
				fprintf(out,"%s",file.desc); 
			}

			if(misc&EXT && ext) {							/* Print ext desc */

				lines=0;
				if(!(misc&NOE)) {
					truncsp((char*)file.extdesc);
					fprint_extdesc(out, file.extdesc, (misc&JST) ? desc_off : 0);
					lines++; 
				}
			}
			fprintf(out,"\n"); 
		}
		smb_close(&smb);
		if(dir_files)
			fprintf(out,"\n"); /* blank line at end of dir */
		if(misc&AUTO) fclose(out); 
		freefiles(file_list, file_count);
	}

	if(misc&TOT && !(misc&AUTO))
		fprintf(out,"TOTALS\n------\n%lu credits/bytes in %lu files.\n"
			,total_cdt,total_files);
	printf("\nDone.\n");
	return(0);
}
