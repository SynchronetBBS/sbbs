/* load_cfg.c */

/* Synchronet configuration load routines (exported) */

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

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
BOOL DLLCALL load_cfg(scfg_t* cfg, char* text[])
{
	char	str[256],fname[13];
	int		i;
	long	line=0L;
	FILE 	*instream;
	read_cfg_text_t txt;

	free_cfg(cfg);	/* free allocated config parameters */

	memset(&txt,0,sizeof(txt));
	txt.openerr=	"!ERROR: opening %s for read.";
	txt.error= 		"!ERROR: offset %lu in %s:";
	txt.allocerr=	"!ERROR: allocating %u bytes of memory.";

	if(cfg->node_num<1)
		cfg->node_num=1;

	backslash(cfg->ctrl_dir);
	if(read_main_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	sprintf(cfg->node_dir,"%.*s",sizeof(cfg->node_dir)-1,cfg->node_path[cfg->node_num-1]);
	if(read_node_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	if(read_msgs_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	if(read_file_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	if(read_xtrn_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	if(read_chat_cfg(cfg, &txt)==FALSE)
		return(FALSE);
	if(read_attr_cfg(cfg, &txt)==FALSE)
		return(FALSE);

	if(text!=NULL) {

		/* Free existing text if allocated */
		free_text(text);

		strcpy(fname,"text.dat");
		sprintf(str,"%s%s",cfg->ctrl_dir,fname);
		if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
			lprintf(txt.openerr,str);
			return(FALSE); }
		if(txt.reading && txt.reading[0])
			lprintf(txt.reading,fname);

		for(i=0;i<TOTAL_TEXT && !feof(instream) && !ferror(instream);i++)
			if((text[i]=readtext(&line,instream))==NULL) {
				i--;
				break;
			}
		fclose(instream);

		if(i<TOTAL_TEXT) {
			lprintf(txt.error,line,fname);
			lprintf("Less than TOTAL_TEXT (%u) strings defined in %s."
				,TOTAL_TEXT,fname);
			return(FALSE); }

		/****************************/
		/* Read in static text data */
		/****************************/
		if(txt.readit && txt.readit[0])
			lprintf(txt.readit,fname);
	}

    /* Override com-port settings */
    cfg->com_base=0xf;	/* All nodes use FOSSIL */
    cfg->com_port=1;	/* All nodes use "COM1" */

	return(TRUE);
}

void free_cfg(scfg_t* cfg)
{
	free_node_cfg(cfg);
	free_main_cfg(cfg);
	free_msgs_cfg(cfg);
	free_file_cfg(cfg);
	free_chat_cfg(cfg);
	free_xtrn_cfg(cfg);
}

void free_text(char* text[])
{
	int i;

	if(text==NULL)
		return;

	for(i=0;i<TOTAL_TEXT;i++) {
		FREE_AND_NULL(text[i]); 
	}
}

/****************************************************************************/
/* If the directory 'path' doesn't exist, create it.                      	*/
/****************************************************************************/
BOOL md(char *inpath)
{
	DIR*	dir;
	char	path[MAX_PATH+1];

	sprintf(path,"%.*s",MAX_PATH,inpath);

	/* Truncate '.' if present */
	if(path[0]!=0 && path[strlen(path)-1]=='.')
		path[strlen(path)-1]=0;

	dir=opendir(path);
	if(dir==NULL) {
		lprintf("Creating directory: %s",path);
		if(_mkdir(path)) {
			lprintf("!Error %d: Fix configuration or make directory by "
				"hand.",errno);
			return(FALSE); 
		} 
	}
	else
		closedir(dir);
	
	return(TRUE);
}

/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
char *readtext(long *line,FILE *stream)
{
	char buf[2048],str[2048],tmp[256],*p,*p2;
	int i,j,k;

	if(!fgets(buf,256,stream))
		return(NULL);
	if(line)
		(*line)++;
	if(buf[0]=='#')
		return(NULL);
	p=strrchr(buf,'"');
	if(!p) {
		if(line) {
			lprintf("No quotation marks in line %d of text.dat",*line);
			return(NULL); }
		return(NULL); }
	if(*(p+1)=='\\')	/* merge multiple lines */
		while(strlen(buf)<2000) {
			if(!fgets(str,255,stream))
				return(NULL);
			if(line)
				(*line)++;
			p2=strchr(str,'"');
			if(!p2)
				continue;
			strcpy(p,p2+1);
			p=strrchr(p,'"');
			if(p && *(p+1)=='\\')
				continue;
			break; }
	*(p)=0;
	k=strlen(buf);
	for(i=1,j=0;i<k;j++) {
		if(buf[i]=='\\')	{ /* escape */
			i++;
			if(isdigit(buf[i])) {
				str[j]=atoi(buf+i); 	/* decimal, NOT octal */
				if(isdigit(buf[++i]))	/* skip up to 3 digits */
					if(isdigit(buf[++i]))
						i++;
				continue; }
			switch(buf[i++]) {
				case '\\':
					str[j]='\\';
					break;
				case '?':
					str[j]='?';
					break;
				case 'x':
					tmp[0]=buf[i++];        /* skip next character */
					tmp[1]=0;
					if(isxdigit(buf[i])) {  /* if another hex digit, skip too */
						tmp[1]=buf[i++];
						tmp[2]=0; }
					str[j]=(char)ahtoul(tmp);
					break;
				case '\'':
					str[j]='\'';
					break;
				case '"':
					str[j]='"';
					break;
				case 'r':
					str[j]=CR;
					break;
				case 'n':
					str[j]=LF;
					break;
				case 't':
					str[j]=TAB;
					break;
				case 'b':
					str[j]=BS;
					break;
				case 'a':
					str[j]=BEL;
					break;
				case 'f':
					str[j]=FF;
					break;
				case 'v':
					str[j]=11;	/* VT */
					break;
				default:
					str[j]=buf[i];
					break; }
			continue; }
		str[j]=buf[i++]; }
	str[j]=0;
	if((p=(char *)MALLOC(j+1))==NULL) {
		lprintf("Error allocating %u bytes of memory from text.dat",j);
		return(NULL); }
	strcpy(p,str);
	return(p);
}

/****************************************************************************/
/* Reads in ATTR.CFG and initializes the associated variables               */
/****************************************************************************/
BOOL read_attr_cfg(scfg_t* cfg, read_cfg_text_t* txt)
{
    char    str[256],fname[13];
    int     i;
	long	offset=0;
    FILE    *instream;

	strcpy(fname,"attr.cfg");
	sprintf(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
		lprintf(txt->openerr,str);
		return(FALSE); }
	if(txt->reading && txt->reading[0])
		lprintf(txt->reading,fname);
	for(i=0;i<TOTAL_COLORS && !feof(instream) && !ferror(instream);i++) {
		readline(&offset,str,4,instream);
		cfg->color[i]=attrstr(str); }
	if(i<TOTAL_COLORS) {
		lprintf(txt->error,offset,fname);
		lprintf("Less than TOTAL_COLORS (%u) defined in %s"
			,TOTAL_COLORS,fname);
		return(FALSE); }
	fclose(instream);
	if(txt->readit && txt->readit[0])
		lprintf(txt->readit,fname);
	return(TRUE);
}

