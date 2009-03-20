/* text_sec.cpp */

/* Synchronet general text file (g-file) section */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2009 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define MAX_TXTSECS 	500 /* Maximum number of text file sections 	*/
#define MAX_TXTFILES    500 /* Maximum number of text files per section */

/****************************************************************************/
/* General Text File Section.                                               */
/* Called from function main_sec                                            */
/* Returns 1 if no text sections available, 0 otherwise.                    */
/****************************************************************************/
int sbbs_t::text_sec()
{
	char	str[256],usemenu
			,*file[MAX_TXTFILES],addpath[83],addstr[83],*buf,ch;
	char 	tmp[512];
	long	i,j,usrsec[MAX_TXTSECS],usrsecs,cursec;
    long    l,length;
    FILE    *stream;

	for(i=j=0;i<cfg.total_txtsecs;i++) {
		if(!chk_ar(cfg.txtsec[i]->ar,&useron,&client))
			continue;
		usrsec[j++]=i; }
	usrsecs=j;
	if(!usrsecs) {
		bputs(text[NoTextSections]);
		return(1); }
	action=NODE_RTXT;
	while(online) {
		sprintf(str,"%smenu/text_sec.*",cfg.text_dir);
		if(fexist(str))
			menu("text_sec");
		else {
			bputs(text[TextSectionLstHdr]);
			for(i=0;i<usrsecs && !msgabort();i++) {
				sprintf(str,text[TextSectionLstFmt],i+1,cfg.txtsec[usrsec[i]]->name);
				if(i<9) outchar(' ');
				bputs(str); } }
		ASYNC;
		mnemonics(text[WhichTextSection]);
		if((cursec=getnum(usrsecs))<1)
			break;
		cursec--;
		while(online) {
			sprintf(str,"%smenu/text%lu.*",cfg.text_dir,cursec+1);
			if(fexist(str)) {
				sprintf(str,"text%lu",cursec+1);
				menu(str);
				usemenu=1; }
			else {
				bprintf(text[TextFilesLstHdr],cfg.txtsec[usrsec[cursec]]->name);
				usemenu=0; }
			sprintf(str,"%stext/%s.ixt",cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code);
			j=0;
			if(fexist(str)) {
				if((stream=fnopen((int *)&i,str,O_RDONLY))==NULL) {
					errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
					return(0); }
				while(!ferror(stream) && !msgabort()) {  /* file open too long */
					if(!fgets(str,81,stream))
						break;
					str[strlen(str)-2]=0;   /* chop off CRLF */
					if((file[j]=(char *)malloc(strlen(str)+1))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,strlen(str)+1);
						continue; }
					strcpy(file[j],str);
					fgets(str,81,stream);
					if(!usemenu) bprintf(text[TextFilesLstFmt],j+1,str);
					j++; }
				fclose(stream); }
			ASYNC;
			if(SYSOP) {
				strcpy(str,"QARE?");
				mnemonics(text[WhichTextFileSysop]); }
			else {
				strcpy(str,"Q?");
				mnemonics(text[WhichTextFile]); }
			i=getkeys(str,j);
			if(!(i&0x80000000L)) {		  /* no file number */
				for(l=0;l<j;l++)
					free(file[l]);
				if((i=='E' || i=='R') && !j)
					continue; }
			if(i=='Q' || !i)
				break;
			if(i==-1) {  /* ctrl-c */
				for(i=0;i<j;i++)
					free(file[i]);
				return(0); }
			if(i=='?')  /* ? means re-list */
				continue;
			if(i=='A') {    /* Add text file */
				if(j) {
					bputs(text[AddTextFileBeforeWhich]);
					i=getnum(j+1);
					if(i<1)
						continue;
					i--;    /* number of file entries to skip */ }
				else
					i=0;
				bprintf(text[AddTextFilePath]
					,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code);
				if(!getstr(addpath,80,0))
					continue;
				strcat(addpath,crlf);
				bputs(text[AddTextFileDesc]);
				if(!getstr(addstr,74,0))
					continue;
				strcat(addstr,crlf);
				sprintf(str,"%stext/%s.ixt"
					,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code);
				if(i==j) {  /* just add to end */
					if((i=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
						errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
						return(0); }
					write(i,addpath,strlen(addpath));
					write(i,addstr,strlen(addstr));
					close(i);
					continue; }
				j=i; /* inserting in middle of file */
				if((stream=fnopen((int *)&i,str,O_RDWR))==NULL) {
					errormsg(WHERE,ERR_OPEN,str,O_RDWR);
					return(0); }
				length=filelength(i);
				for(i=0;i<j;i++) {  /* skip two lines for each entry */
					fgets(tmp,81,stream);
					fgets(tmp,81,stream); }
				l=ftell(stream);
				if((buf=(char *)malloc(length-l))==NULL) {
					fclose(stream);
					errormsg(WHERE,ERR_ALLOC,str,length-l);
					return(0); }
				fread(buf,1,length-l,stream);
				fseek(stream,l,SEEK_SET); /* go back to where we need to insert */
				fputs(addpath,stream);
				fputs(addstr,stream);
				fwrite(buf,1,length-l,stream);
				fclose(stream);
				free(buf);
				continue; }
			if(i=='R' || i=='E') {   /* Remove or Edit text file */
				ch=(char)i;
				if(ch=='R')
					bputs(text[RemoveWhichTextFile]);
				else
					bputs(text[EditWhichTextFile]);
				i=getnum(j);
				if(i<1)
					continue;
				sprintf(str,"%stext/%s.ixt"
					,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code);
				j=i-1;
				if((stream=fnopen(NULL,str,O_RDONLY))==NULL) {
					errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
					return(0); }
				for(i=0;i<j;i++) {  /* skip two lines for each entry */
					fgets(tmp,81,stream);
					fgets(tmp,81,stream); }
				fgets(addpath,81,stream);
				truncsp(addpath);
				fclose(stream);
				if(!strchr(addpath,'\\') && !strchr(addpath,'/'))
					sprintf(tmp,"%stext/%s/%s"
						,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code,addpath);
				else
					strcpy(tmp,addpath);
				if(ch=='R') {               /* Remove */
					if(fexist(tmp)) {
						sprintf(str,text[DeleteTextFileQ],tmp);
						if(!noyes(str))
							if(remove(tmp)) errormsg(WHERE,ERR_REMOVE,tmp,0); }
					sprintf(str,"%stext/%s.ixt"
						,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code);
					removeline(str,addpath,2,0); }
				else {                      /* Edit */
					strcpy(str,tmp);
					editfile(str); }
				continue; }
			i=(i&~0x80000000L)-1;
			if(!strchr(file[i],'\\') && !strchr(file[i],'/'))
				sprintf(str,"%stext/%s/%s"
					,cfg.data_dir,cfg.txtsec[usrsec[cursec]]->code,file[i]);
			else
				strcpy(str,file[i]);
			attr(LIGHTGRAY);
			printfile(str,0);
			sprintf(str,"%s read text file: %s"
				,useron.alias,file[i]);
			logline("T-",str);
			pause();
			sys_status&=~SS_ABORT;
			for(i=0;i<j;i++)
				free(file[i]); } }
	return(0);
}

