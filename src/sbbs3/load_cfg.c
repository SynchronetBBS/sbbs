/* load_cfg.c */

/* Synchronet configuration load routines (exported) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "text.h"	/* TOTAL_TEXT */

static void prep_cfg(scfg_t* cfg);
static void free_attr_cfg(scfg_t* cfg);

int 	lprintf(int level, const char *fmt, ...);	/* log output */

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
BOOL DLLCALL load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error)
{
	int		i;
	long	line=0L;
#ifdef SBBS
	FILE 	*instream;
	char	str[256],fname[13];
#endif

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

#ifdef SBBS
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
			if((text[i]=readtext(&line,instream,i))==NULL) {
				i--;
				break;
			}
		fclose(instream);

		if(i<TOTAL_TEXT) {
			sprintf(error,"line %lu in %s: Less than TOTAL_TEXT (%u) strings defined in %s."
				,i,fname
				,TOTAL_TEXT,fname);
			return(FALSE); 
		}
	}
#endif

    /* Override com-port settings */
    cfg->com_base=0xf;	/* All nodes use FOSSIL */
    cfg->com_port=1;	/* All nodes use "COM1" */

	if(prep)
		prep_cfg(cfg);

	/* Auto-toggle daylight savings time in US time-zones */
	sys_timezone(cfg);

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
			SAFECOPY(cfg->sub[i]->tagline,cfg->qnet_tagline);

		/* default origin line */
		if(!cfg->sub[i]->origline[0])
			SAFECOPY(cfg->sub[i]->origline,cfg->origline);

		/* A sub-board's internal code is the combination of the grp's code_prefix & the sub's code_suffix */
		SAFEPRINTF2(cfg->sub[i]->code,"%s%s"
			,cfg->grp[cfg->sub[i]->grp]->code_prefix
			,cfg->sub[i]->code_suffix);

		strlwr(cfg->sub[i]->code); 		/* data filenames are all lowercase */

		prep_path(cfg->sub[i]->post_sem);
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

	for(i=0;i<cfg->total_xtrns;i++) 
	{
		strlwr(cfg->xtrn[i]->code);
		prep_dir(cfg->ctrl_dir, cfg->xtrn[i]->path, sizeof(cfg->xtrn[i]->path));
	}
	for(i=0;i<cfg->total_events;i++) {
		strlwr(cfg->event[i]->code); 	/* data filenames are all lowercase */
		prep_dir(cfg->ctrl_dir, cfg->event[i]->dir, sizeof(cfg->event[i]->dir));
	}
	for(i=0;i<cfg->total_xedits;i++) 
		strlwr(cfg->xedit[i]->code);

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
			lprintf(LOG_WARNING,"!ERROR %d creating directory: %s",errno,path);
			return(FALSE); 
		} 
	}
	else
		closedir(dir);
	
	return(TRUE);
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
			sprintf(str,"%s%c%s",base,PATH_DELIM,path);
	} else
		strcpy(str,path);

#ifdef __unix__				/* Change backslashes to forward slashes on Unix */
	for(p=str;*p;p++)
		if(*p=='\\') 
			*p='/';
#endif

	backslashcolon(str);
	strcat(str,".");                /* Change C: to C:. and C:\SBBS\ to C:\SBBS\. */
	FULLPATH(abspath,str,buflen);	/* Change C:\SBBS\NODE1\..\EXEC to C:\SBBS\EXEC */
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

/****************************************************************************/
/* Auto-toggle daylight savings time in US time-zones						*/
/****************************************************************************/
ushort DLLCALL sys_timezone(scfg_t* cfg)
{
	time_t	now;
	struct tm tm;

	if(cfg->sys_misc&SM_AUTO_DST && !OTHER_ZONE(cfg->sys_timezone) && cfg->sys_timezone&US_ZONE) {
		now=time(NULL);
		if(localtime_r(&now,&tm)!=NULL) {
			if(tm.tm_isdst>0)
				cfg->sys_timezone|=DAYLIGHT;
			else if(tm.tm_isdst==0)
				cfg->sys_timezone&=~DAYLIGHT;
		}
	}

	return(cfg->sys_timezone);
}
