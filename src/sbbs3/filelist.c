/* filelist.c */

/* Utility to create list of files from Synchronet file directories */
/* Default list format is FILES.BBS, but file size, uploader, upload date */
/* and other information can be included. */

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

#define FILELIST_VER "3.00"

#define MAX_NOTS 25

scfg_t scfg;

char *crlf="\r\n";

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
int lprintf(char *fmat, ...)
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


#define ALL 	(1L<<0)
#define PAD 	(1L<<1)
#define HDR 	(1L<<2)
#define CDT_	(1L<<3)
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
	uchar	*datbuf,*ixbbuf;
	int 	i,j,file,dirnum,libnum,desc_off,lines,nots=0
			,omode=O_WRONLY|O_CREAT|O_TRUNC;
	ulong	l,m,n,cdt,misc=0,total_cdt=0,total_files=0,datbuflen;
	time_t	uld,dld;
	FILE	*in,*out=NULL;

	sscanf("$Revision$" + 11, "%s", revision);

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
		printf("\n   usage: FILELIST <dir_code or * for ALL> [switches] [outfile]\n");
		printf("\n");
		printf("switches: -lib name All directories of specified library\n");
		printf("          -not code Exclude specific directory\n");
		printf("          -cat      Concatenate to existing outfile\n");
		printf("          -pad      Pad filename with spaces\n");
		printf("          -hdr      Include directory headers\n");
		printf("          -cdt      Include credit value\n");
		printf("          -tot      Include credit totals\n");
		printf("          -uln      Include uploader's name\n");
		printf("          -uld      Include upload date\n");
		printf("          -dfd      Include DOS file date\n");
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
		exit(0); }

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\nSBBSCTRL environment variable not set.\n");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); 
	}

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
	prep_dir(scfg.data_dir, scfg.temp_dir);

	if(!(scfg.sys_misc&SM_LOCAL_TZ))
		putenv("TZ=UTC0");

	dirnum=libnum=-1;
	if(argv[1][0]=='*')
		misc|=ALL;
	else if(argv[1][0]!='-') {
		strupr(argv[1]);
		for(i=0;i<scfg.total_dirs;i++)
			if(!stricmp(argv[1],scfg.dir[i]->code))
				break;
		if(i>=scfg.total_dirs) {
			printf("\nDirectory code '%s' not found.\n",argv[1]);
			exit(1); }
		dirnum=i; }
	for(i=1;i<argc;i++) {
		if(!stricmp(argv[i],"-lib")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and -lib parameters were used.\n");
				exit(1); }
			i++;
			if(i>=argc) {
				printf("\nLibrary short name must follow -lib parameter.\n");
				exit(1); }
			strupr(argv[i]);
			for(j=0;j<scfg.total_libs;j++)
				if(!stricmp(scfg.lib[j]->sname,argv[i]))
					break;
			if(j>=scfg.total_libs) {
				printf("\nLibrary short name '%s' not found.\n",argv[i]);
				exit(1); }
			libnum=j; }
		else if(!stricmp(argv[i],"-not")) {
			if(nots>=MAX_NOTS) {
				printf("\nMaximum number of -not options (%u) exceeded.\n"
					,MAX_NOTS);
				exit(1); }
			i++;
			if(i>=argc) {
				printf("\nDirectory internal code must follow -not parameter.\n");
				exit(1); }
			sprintf(not[nots++],"%.8s",argv[i]); }
		else if(!stricmp(argv[i],"-all")) {
			if(dirnum!=-1) {
				printf("\nBoth directory code and -all parameters were used.\n");
				exit(1); }
			if(libnum!=-1) {
				printf("\nBoth library name and -all parameters were used.\n");
				exit(1); }
			misc|=ALL; }
		else if(!stricmp(argv[i],"-pad"))
			misc|=PAD;
		else if(!stricmp(argv[i],"-cat"))
			omode=O_WRONLY|O_CREAT|O_APPEND;
		else if(!stricmp(argv[i],"-hdr"))
			misc|=HDR;
		else if(!stricmp(argv[i],"-cdt"))
			misc|=CDT_;
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
			misc|=(HDR|PAD|CDT_|PLUS|MINUS);

		else if(i!=1) {
			if(argv[i][0]=='*') {
				misc|=AUTO;
				continue; }
			if((j=nopen(argv[i],omode))==-1) {
				printf("\nError opening/creating %s for output.\n",argv[i]);
				exit(1); }
			out=fdopen(j,"wb"); } }

	if(!out && !(misc&AUTO)) {
		printf("\nOutput file not specified, using FILES.BBS in each "
			"directory.\n");
		misc|=AUTO; }

	for(i=0;i<scfg.total_dirs;i++) {
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
		sprintf(str,"%s%s.IXB",scfg.dir[i]->data_dir,scfg.dir[i]->code);
		if((file=nopen(str,O_RDONLY))==-1)
			continue;
		l=filelength(file);
		if(misc&AUTO) {
			sprintf(str,"%sFILES.BBS",scfg.dir[i]->path);
			if((j=nopen(str,omode))==-1) {
				printf("\nError opening/creating %s for output.\n",str);
				exit(1); }
			out=fdopen(j,"wb"); }
		if(misc&HDR) {
			sprintf(fname,"%-*s      %-*s       Files: %4lu"
				,LEN_GSNAME,scfg.lib[scfg.dir[i]->lib]->sname
				,LEN_SLNAME,scfg.dir[i]->lname,l/F_IXBSIZE);
			fprintf(out,"%s\r\n",fname);
			memset(fname,'-',strlen(fname));
			fprintf(out,"%s\r\n",fname); }
		if(!l) {
			close(file);
			if(misc&AUTO) fclose(out);
			continue; }
		if((ixbbuf=(char *)MALLOC(l))==NULL) {
			close(file);
			if(misc&AUTO) fclose(out);
			printf("\7ERR_ALLOC %s %lu\n",str,l);
			continue; }
		if(read(file,ixbbuf,l)!=(int)l) {
			close(file);
			if(misc&AUTO) fclose(out);
			printf("\7ERR_READ %s %lu\n",str,l);
			FREE((char *)ixbbuf);
			continue; }
		close(file);
		sprintf(str,"%s%s.DAT",scfg.dir[i]->data_dir,scfg.dir[i]->code);
		if((file=nopen(str,O_RDONLY))==-1) {
			printf("\7ERR_OPEN %s %u\n",str,O_RDONLY);
			FREE((char *)ixbbuf);
			if(misc&AUTO) fclose(out);
			continue; }
		datbuflen=filelength(file);
		if((datbuf=MALLOC(datbuflen))==NULL) {
			close(file);
			printf("\7ERR_ALLOC %s %lu\n",str,datbuflen);
			FREE((char *)ixbbuf);
			if(misc&AUTO) fclose(out);
			continue; }
		if(read(file,datbuf,datbuflen)!=(int)datbuflen) {
			close(file);
			printf("\7ERR_READ %s %lu\n",str,datbuflen);
			FREE((char *)datbuf);
			FREE((char *)ixbbuf);
			if(misc&AUTO) fclose(out);
			continue; }
		close(file);
		m=0L;
		while(m<l && !ferror(out)) {
			for(j=0;j<12 && m<l;j++)
				if(j==8)
					str[j]='.';
				else
					str[j]=ixbbuf[m++]; /* Turns FILENAMEEXT into FILENAME.EXT */
			str[j]=0;
			unpadfname(str,fname);
			fprintf(out,"%-12.12s",misc&PAD ? str : fname);
			total_files++;
			n=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
			uld=(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)|((long)ixbbuf[m+5]<<16)
				|((long)ixbbuf[m+6]<<24));
			dld=(ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)|((long)ixbbuf[m+9]<<16)
				|((long)ixbbuf[m+10]<<24));
			m+=11;

			if(n>=datbuflen 							/* index out of bounds */
				|| datbuf[n+F_DESC+LEN_FDESC]!=CR) {	/* corrupted data */
				fprintf(stderr,"\n\7%s%s is corrupted!\n"
					,scfg.dir[i]->data_dir,scfg.dir[i]->code);
				exit(-1); }


			if(misc&PLUS && datbuf[n+F_MISC]!=ETX
				&& (datbuf[n+F_MISC]-SP)&FM_EXTDESC)
				fputc('+',out);
			else
				fputc(SP,out);

			desc_off=12;
			if(misc&(CDT_|TOT)) {
				getrec((char *)&datbuf[n],F_CDT,LEN_FCDT,str);
				cdt=atol(str);
				total_cdt+=cdt;
				if(misc&CDT_) {
					fprintf(out,"%7lu",cdt);
					desc_off+=7; } }

			if(misc&MINUS) {
				sprintf(str,"%s%s",scfg.dir[i]->path,fname);
				if(!fexist(str))
					fputc('-',out);
				else
					fputc(SP,out); }
			else
				fputc(SP,out);
			desc_off++;

			if(misc&DFD) {
				sprintf(str,"%s%s",scfg.dir[i]->path,fname);
				fprintf(out,"%s ",unixtodstr(&scfg,fdate(str),str));
				desc_off+=9; }

			if(misc&ULD) {
				fprintf(out,"%s ",unixtodstr(&scfg,uld,str));
				desc_off+=9; }

			if(misc&ULN) {
				getrec((char *)&datbuf[n],F_ULER,25,str);
				fprintf(out,"%-25s ",str);
				desc_off+=26; }

			if(misc&DLD) {
				fprintf(out,"%s ",unixtodstr(&scfg,dld,str));
				desc_off+=9; }

			if(misc&DLS) {
				getrec((char *)&datbuf[n],F_TIMESDLED,5,str);
				j=atoi(str);
				fprintf(out,"%5u ",j);
				desc_off+=6; }

			if(datbuf[n+F_MISC]!=ETX && (datbuf[n+F_MISC]-SP)&FM_EXTDESC)
				ext=1;	/* extended description exists */
			else
				ext=0;	/* it doesn't */

			if(!(misc&NOD) && !(misc&NOE && ext)) {
				getrec((char *)&datbuf[n],F_DESC,LEN_FDESC,str);
				fprintf(out,"%s",str); }

			if(misc&EXT && ext) {							/* Print ext desc */

				sprintf(str,"%s%s.EXB",scfg.dir[i]->data_dir,scfg.dir[i]->code);
				if(!fexist(str))
					continue;
				if((j=nopen(str,O_RDONLY))==-1) {
					printf("\7ERR_OPEN %s %u\n",str,O_RDONLY);
					continue; }
				if((in=fdopen(j,"rb"))==NULL) {
					close(j);
					continue; }
				fseek(in,(n/F_LEN)*512L,SEEK_SET);
				lines=0;
				if(!(misc&NOE)) {
					fprintf(out,"\r\n");
					lines++; }
				while(!feof(in) && !ferror(in)
					&& ftell(in)<(long)((n/F_LEN)+1)*512L) {
					if(!fgets(str,128,in) || !str[0])
						break;
					stripctrlz(str);
					if(lines) {
						if(misc&JST)
							fprintf(out,"%*s",desc_off,"");
						fputc(SP,out);				/* indent one character */ }
					fprintf(out,"%s",str);
					lines++; }
				fclose(in); }
			fprintf(out,"\r\n"); }
		FREE((char *)datbuf);
		FREE((char *)ixbbuf);
		fprintf(out,"\r\n"); /* blank line at end of dir */
		if(misc&AUTO) fclose(out); }

	if(misc&TOT && !(misc&AUTO))
		fprintf(out,"TOTALS\n------\n%lu credits/bytes in %lu files.\r\n"
			,total_cdt,total_files);
	printf("\nDone.\n");
	return(0);
}
