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

static void prep_cfg(scfg_t* cfg);
static void free_attr_cfg(scfg_t* cfg);

char *	readtext(long *line, FILE *stream);
int 	lprintf(char *fmt, ...);

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
BOOL DLLCALL load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error)
{
	char	str[256],fname[13];
	int		i;
	long	line=0L;
	FILE 	*instream;

	if(cfg->size!=sizeof(scfg_t)) {
		sprintf(error,"cfg->size (%ld) != sizeof(scfg_t) (%d)"
			,cfg->size,sizeof(scfg_t));
		return(FALSE);
	}

	free_cfg(cfg);	/* free allocated config parameters */

	cfg->prepped=FALSE;	/* reset prepped flag */

	if(cfg->node_num<1)
		cfg->node_num=1;

	backslash(cfg->ctrl_dir);
	if(read_main_cfg(cfg, error)==FALSE)
		return(FALSE);

	if(prep)
		for(i=0;i<cfg->sys_nodes;i++) 
			prep_dir(cfg->ctrl_dir, cfg->node_path[i], sizeof(cfg->node_path[i]));

	SAFECOPY(cfg->node_dir,cfg->node_path[cfg->node_num-1]);
	prep_dir(cfg->ctrl_dir, cfg->node_dir, sizeof(cfg->node_dir));
	if(read_node_cfg(cfg, error)==FALSE)
		return(FALSE);
	if(read_msgs_cfg(cfg, error)==FALSE)
		return(FALSE);
	if(read_file_cfg(cfg, error)==FALSE)
		return(FALSE);
	if(read_xtrn_cfg(cfg, error)==FALSE)
		return(FALSE);
	if(read_chat_cfg(cfg, error)==FALSE)
		return(FALSE);
	if(read_attr_cfg(cfg, error)==FALSE)
		return(FALSE);

	if(text!=NULL) {

		/* Free existing text if allocated */
		free_text(text);

		strcpy(fname,"text.dat");
		sprintf(str,"%s%s",cfg->ctrl_dir,fname);
		if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
			sprintf(error,"%d opening %s",errno,str);
			return(FALSE); 
		}
		for(i=0;i<TOTAL_TEXT && !feof(instream) && !ferror(instream);i++)
			if((text[i]=readtext(&line,instream))==NULL) {
				i--;
				break;
			}
		fclose(instream);

		if(i<TOTAL_TEXT) {
			sprintf(error,"line %lu in %s: Less than TOTAL_TEXT (%u) strings defined in %s."
				,line,fname
				,TOTAL_TEXT,fname);
			return(FALSE); 
		}
	}

    /* Override com-port settings */
    cfg->com_base=0xf;	/* All nodes use FOSSIL */
    cfg->com_port=1;	/* All nodes use "COM1" */

	if(prep)
		prep_cfg(cfg);

	return(TRUE);
}

/****************************************************************************/
/* Prepare configuration for run-time (resolve relative paths, etc)			*/
/****************************************************************************/
void prep_cfg(scfg_t* cfg)
{
	int i;

#if 0 /* def __unix__ */
	strlwr(cfg->text_dir);	/* temporary Unix-compatibility hack */
	strlwr(cfg->temp_dir);	/* temporary Unix-compatibility hack */
	strlwr(cfg->data_dir);	/* temporary Unix-compatibility hack */
	strlwr(cfg->exec_dir);	/* temporary Unix-compatibility hack */
#endif

	/* Fix-up paths */
	prep_dir(cfg->ctrl_dir, cfg->data_dir, sizeof(cfg->data_dir));
	prep_dir(cfg->ctrl_dir, cfg->logs_dir, sizeof(cfg->logs_dir));
	prep_dir(cfg->ctrl_dir, cfg->exec_dir, sizeof(cfg->exec_dir));
	prep_dir(cfg->ctrl_dir, cfg->mods_dir, sizeof(cfg->mods_dir));
	prep_dir(cfg->ctrl_dir, cfg->text_dir, sizeof(cfg->text_dir));

	prep_dir(cfg->ctrl_dir, cfg->netmail_dir, sizeof(cfg->netmail_dir));
	prep_dir(cfg->ctrl_dir, cfg->echomail_dir, sizeof(cfg->echomail_dir));
	prep_dir(cfg->ctrl_dir, cfg->fidofile_dir, sizeof(cfg->fidofile_dir));

	prep_path(cfg->netmail_sem);
	prep_path(cfg->echomail_sem);
	prep_path(cfg->inetmail_sem);

#if 0 /* def __unix__ */
	/* temporary hack for Unix compatibility */
	strlwr(cfg->logon_mod);
	strlwr(cfg->logoff_mod);
	strlwr(cfg->newuser_mod);
	strlwr(cfg->login_mod);
	strlwr(cfg->logout_mod);
	strlwr(cfg->sync_mod);
	strlwr(cfg->expire_mod);
#endif

	for(i=0;i<cfg->total_subs;i++) {

		if(!cfg->sub[i]->data_dir[0])	/* no data storage path specified */
			sprintf(cfg->sub[i]->data_dir,"%ssubs",cfg->data_dir);
		prep_dir(cfg->ctrl_dir, cfg->sub[i]->data_dir, sizeof(cfg->sub[i]->data_dir));

		/* default QWKnet tagline */
		if(!cfg->sub[i]->tagline[0])
			strcpy(cfg->sub[i]->tagline,cfg->qnet_tagline);

		/* default echomail semaphore */
		if(!cfg->sub[i]->echomail_sem[0])
			strcpy(cfg->sub[i]->echomail_sem,cfg->echomail_sem);

		/* default origin line */
		if(!cfg->sub[i]->origline[0])
			strcpy(cfg->sub[i]->origline,cfg->origline);

		/* A sub-board's internal code is the combination of the grp's code_prefix & the sub's code_suffix */
		sprintf(cfg->sub[i]->code,"%s%s"
			,cfg->grp[cfg->sub[i]->grp]->code_prefix
			,cfg->sub[i]->code_suffix);

		strlwr(cfg->sub[i]->code); 		/* data filenames are all lowercase */
	}

	for(i=0;i<cfg->total_libs;i++) {
		if(cfg->lib[i]->parent_path[0])
			prep_dir(cfg->ctrl_dir, cfg->lib[i]->parent_path, sizeof(cfg->lib[i]->parent_path));
	}

	for(i=0;i<cfg->total_dirs;i++) {

		if(!cfg->dir[i]->data_dir[0])	/* no data storage path specified */
			sprintf(cfg->dir[i]->data_dir,"%sdirs",cfg->data_dir);
		prep_dir(cfg->ctrl_dir, cfg->dir[i]->data_dir, sizeof(cfg->dir[i]->data_dir));

		if(!cfg->dir[i]->path[0])		/* no file storage path specified */
            sprintf(cfg->dir[i]->path,"%sdirs/%s/",cfg->data_dir,cfg->dir[i]->code);
		else if(cfg->lib[cfg->dir[i]->lib]->parent_path[0])
			prep_dir(cfg->lib[cfg->dir[i]->lib]->parent_path, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));
		else
			prep_dir(cfg->ctrl_dir, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));

		/* A directory's internal code is the combination of the lib's code_prefix & the dir's code_suffix */
		sprintf(cfg->dir[i]->code,"%s%s"
			,cfg->lib[cfg->dir[i]->lib]->code_prefix
			,cfg->dir[i]->code_suffix);

		strlwr(cfg->dir[i]->code); 		/* data filenames are all lowercase */

		prep_path(cfg->dir[i]->upload_sem);
	}


	/* make data filenames are all lowercase */
	for(i=0;i<cfg->total_shells;i++)
		strlwr(cfg->shell[i]->code);

	for(i=0;i<cfg->total_gurus;i++)
		strlwr(cfg->guru[i]->code); 

	for(i=0;i<cfg->total_txtsecs;i++)
		strlwr(cfg->txtsec[i]->code);

	for(i=0;i<cfg->total_xtrnsecs;i++)
		strlwr(cfg->xtrnsec[i]->code);

	for(i=0;i<cfg->total_xtrns;i++) {
		prep_dir(cfg->ctrl_dir, cfg->xtrn[i]->path, sizeof(cfg->xtrn[i]->path));
	}
	for(i=0;i<cfg->total_events;i++) {
		strlwr(cfg->event[i]->code); 	/* data filenames are all lowercase */
		prep_dir(cfg->ctrl_dir, cfg->event[i]->dir, sizeof(cfg->event[i]->dir));
	}

	cfg->prepped=TRUE;	/* data prepared for run-time, DO NOT SAVE TO DISK! */
}

void DLLCALL free_cfg(scfg_t* cfg)
{
	free_node_cfg(cfg);
	free_main_cfg(cfg);
	free_msgs_cfg(cfg);
	free_file_cfg(cfg);
	free_chat_cfg(cfg);
	free_xtrn_cfg(cfg);
	free_attr_cfg(cfg);
}

void DLLCALL free_text(char* text[])
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

	if(inpath[0]==0)
		return(FALSE);

	SAFECOPY(path,inpath);

	/* Remove trailing '.' if present */
	if(path[strlen(path)-1]=='.')
		path[strlen(path)-1]=0;

	/* Remove trailing slash if present */
	if(path[strlen(path)-1]=='\\' || path[strlen(path)-1]=='/')
		path[strlen(path)-1]=0;

	dir=opendir(path);
	if(dir==NULL) {
		/* lprintf("Creating directory: %s",path); */
		if(MKDIR(path)) {
			lprintf("!ERROR %d creating directory: %s",errno,path);
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
			return(NULL); 
		}
		return(NULL); 
	}
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
			break; 
		}
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
				continue; 
			}
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
						tmp[2]=0; 
					}
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
					break; 
			}
			continue; 
		}
		str[j]=buf[i++]; 
	}
	str[j]=0;
	if((p=(char *)calloc(1,j+2))==NULL) { /* +1 for terminator, +1 for YNQX line */
		lprintf("Error allocating %u bytes of memory from text.dat",j);
		return(NULL); 
	}
	strcpy(p,str);
	return(p);
}

/****************************************************************************/
/* Reads in ATTR.CFG and initializes the associated variables               */
/****************************************************************************/
BOOL read_attr_cfg(scfg_t* cfg, char* error)
{
	char*	p;
    char    str[256],fname[13];
	long	offset=0;
    FILE    *instream;

	strcpy(fname,"attr.cfg");
	sprintf(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
		sprintf(error,"%d opening %s",errno,str);
		return(FALSE); 
	}
	FREE_AND_NULL(cfg->color);
	if((cfg->color=malloc(MIN_COLORS))==NULL) {
		sprintf(error,"Error allocating memory (%u bytes) for colors"
			,MIN_COLORS);
		return(FALSE);
	}
	memset(cfg->color,LIGHTGRAY|HIGH,MIN_COLORS);	
	for(cfg->total_colors=0;!feof(instream) && !ferror(instream);cfg->total_colors++) {
		if(readline(&offset,str,4,instream)==NULL)
			break;
		if(cfg->total_colors>=MIN_COLORS) {
			if((p=realloc(cfg->color,cfg->total_colors+1))==NULL)
				break;
			cfg->color=p;
		}
		cfg->color[cfg->total_colors]=attrstr(str); 
	}
	fclose(instream);
	if(cfg->total_colors<MIN_COLORS)
		cfg->total_colors=MIN_COLORS;
	return(TRUE);
}

static void free_attr_cfg(scfg_t* cfg)
{
	if(cfg->color!=NULL)
		FREE_AND_NULL(cfg->color);
	cfg->total_colors=0;
}

char* DLLCALL prep_dir(char* base, char* path, size_t buflen)
{
#ifdef __unix__
	char	*p;
#endif
	char	str[MAX_PATH+1];
	char	abspath[MAX_PATH+1];
	char	ch;

	if(!path[0])
		return(path);
	if(path[0]!='\\' && path[0]!='/' && path[1]!=':') {	/* Relative directory */
		ch=*lastchar(base);
		if(ch=='\\' || ch=='/')
			sprintf(str,"%s%s",base,path);
		else
			sprintf(str,"%s%c%s",base,BACKSLASH,path);
	} else
		strcpy(str,path);

#ifdef __unix__				/* Change backslashes to forward slashes on Unix */
	for(p=str;*p;p++)
		if(*p=='\\') 
			*p='/';
#endif

	backslashcolon(str);
	strcat(str,".");                // Change C: to C:. and C:\SBBS\ to C:\SBBS\.
	FULLPATH(abspath,str,buflen);	// Change C:\SBBS\NODE1\..\EXEC to C:\SBBS\EXEC
	backslash(abspath);

	sprintf(path,"%.*s",(int)(buflen-1),abspath);
	return(path);
}

char* prep_path(char* path)
{
#ifdef __unix__				/* Change backslashes to forward slashes on Unix */
	char	*p;

	for(p=path;*p;p++)
		if(*p=='\\') 
			*p='/';
#endif

	return(path);
}
