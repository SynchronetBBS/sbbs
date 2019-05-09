/* Synchronet configuration load routines (exported) */

/* $Id$ */

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

#include "sbbs.h"
#include "text.h"	/* TOTAL_TEXT */
#ifdef USE_CRYPTLIB
#include "cryptlib.h"
#endif

static void prep_cfg(scfg_t* cfg);
static void free_attr_cfg(scfg_t* cfg);

int 	lprintf(int level, const char *fmt, ...);	/* log output */

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
BOOL DLLCALL load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error)
{
	int		i;
#ifdef SBBS
	long	line=0L;
	FILE 	*instream;
	char	str[256],fname[13];
#endif

	if(cfg->size!=sizeof(scfg_t)) {
		sprintf(error,"cfg->size (%"PRIu32") != sizeof(scfg_t) (%" XP_PRIsize_t "d)"
			,cfg->size,sizeof(scfg_t));
		return(FALSE);
	}

	free_cfg(cfg);	/* free allocated config parameters */

	cfg->prepped=FALSE;	/* reset prepped flag */
	cfg->tls_certificate = -1;

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
		for(i=0;i<TOTAL_TEXT;i++)
			if((text[i]=readtext(&line,instream,i))==NULL) {
				i--;
				break;
			}
		fclose(instream);

		if(i<TOTAL_TEXT) {
			sprintf(error,"line %d in %s: Less than TOTAL_TEXT (%u) strings defined in %s."
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

		/* A directory's internal code is the combination of the lib's code_prefix & the dir's code_suffix */
		SAFEPRINTF2(cfg->dir[i]->code,"%s%s"
			,cfg->lib[cfg->dir[i]->lib]->code_prefix
			,cfg->dir[i]->code_suffix);

		strlwr(cfg->dir[i]->code); 		/* data filenames are all lowercase */

		if(!cfg->dir[i]->path[0])		/* no file storage path specified */
            sprintf(cfg->dir[i]->path,"%s%s/",cfg->dir[i]->data_dir,cfg->dir[i]->code);
		else if(cfg->lib[cfg->dir[i]->lib]->parent_path[0])
			prep_dir(cfg->lib[cfg->dir[i]->lib]->parent_path, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));
		else
			prep_dir(cfg->ctrl_dir, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));

		prep_path(cfg->dir[i]->upload_sem);
	}

	for(i=0;i<cfg->total_libs;i++) {
		if((cfg->lib[i]->misc&LIB_DIRS) == 0 || cfg->lib[i]->parent_path[0] == 0)
			continue;
		char path[MAX_PATH+1];
		SAFECOPY(path, cfg->lib[i]->parent_path);
		backslash(path);
		strcat(path, ALLFILES);
		glob_t g;
		if(glob(path, GLOB_MARK, NULL, &g))
			continue;
   		for(uint gi=0;gi<g.gl_pathc;gi++) {
			char* p = g.gl_pathv[gi];
			char* tp = lastchar(p);
			if(*tp != '/')
				continue;
			*tp = 0;
			dir_t dir;
			memset(&dir, 0, sizeof(dir));
			dir.lib = i;
			dir.misc = DIR_FILES;
			SAFECOPY(dir.path, p);
			backslash(dir.path);
			SAFECOPY(dir.lname, getfname(p));
			SAFECOPY(dir.sname, dir.lname);
			char code_suffix[LEN_EXTCODE+1];
			SAFECOPY(code_suffix, dir.lname);
			prep_code(code_suffix, cfg->lib[i]->code_prefix);
			SAFECOPY(dir.code_suffix, code_suffix);
			SAFEPRINTF2(dir.code,"%s%s"
				,cfg->lib[i]->code_prefix
				,dir.code_suffix);

			dir_t** new_dirs;
			if((new_dirs=(dir_t **)realloc(cfg->dir, sizeof(dir_t *)*(cfg->total_dirs+2)))==NULL)
				continue;
			cfg->dir  = new_dirs;
			if((cfg->dir[cfg->total_dirs] = malloc(sizeof(dir_t))) == NULL)
				continue;
			*cfg->dir[cfg->total_dirs++] = dir;
		}
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

	if (!cfg->prepped)
		cfg->tls_certificate = -1;
	cfg->prepped=TRUE;	/* data prepared for run-time, DO NOT SAVE TO DISK! */
}

void DLLCALL free_cfg(scfg_t* cfg)
{
#ifdef USE_CRYPTLIB
	if (cfg->tls_certificate != -1 && cfg->prepped)
		cryptDestroyContext(cfg->tls_certificate);
#endif
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

	if(!isdir(path)) {
		/* lprintf("Creating directory: %s",path); */
		if(mkpath(path)) {
			lprintf(LOG_WARNING,"!ERROR %d (%s) creating directory: %s", errno, strerror(errno), path);
			return(FALSE); 
		} 
	}
	
	return(TRUE);
}

/****************************************************************************/
/* Reads in ATTR.CFG and initializes the associated variables               */
/****************************************************************************/
BOOL read_attr_cfg(scfg_t* cfg, char* error)
{
	uint*	clr;
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
	if((cfg->color=malloc(MIN_COLORS * sizeof(uint)))==NULL) {
		sprintf(error,"Error allocating memory (%u bytes) for colors"
			,MIN_COLORS);
		fclose(instream);
		return(FALSE);
	}
	/* Setup default colors here: */
	memset(cfg->color,LIGHTGRAY|HIGH,MIN_COLORS);
	cfg->color[clr_votes_full] = WHITE|BG_MAGENTA;
	cfg->color[clr_progress_full] = CYAN|HIGH|BG_BLUE;
	for(cfg->total_colors=0;!feof(instream) && !ferror(instream);cfg->total_colors++) {
		if(readline(&offset,str,4,instream)==NULL)
			break;
		if(cfg->total_colors>=MIN_COLORS) {
			if((clr=realloc(cfg->color,(cfg->total_colors+1) * sizeof(uint)))==NULL)
				break;
			cfg->color=clr;
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

char* DLLCALL prep_dir(const char* base, char* path, size_t buflen)
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

/* Prepare a string to be used as an internal code */
/* Return the usable code */
char* prep_code(char *str, const char* prefix)
{
	char tmp[1024];
	int i,j;

	if(prefix!=NULL) {	/* skip the grp/lib prefix, if specified */
		i=strlen(prefix);
		if(i && strnicmp(str,prefix,i)==0 && strlen(str)!=i)
			str+=i;
	}
	for(i=j=0;str[i] && i<sizeof(tmp);i++)
		if(str[i]>' ' && !(str[i]&0x80) && str[i]!='*' && str[i]!='?' && str[i]!='.'
			&& strchr(ILLEGAL_FILENAME_CHARS,str[i])==NULL)
			tmp[j++]=toupper(str[i]);
	tmp[j]=0;
	strcpy(str,tmp);
	if(j>LEN_CODE) {	/* Extra chars? Strip symbolic chars */
		for(i=j=0;str[i];i++)
			if(isalnum(str[i]))
				tmp[j++]=str[i];
		tmp[j]=0;
		strcpy(str,tmp);
	}
	str[LEN_CODE]=0;
	return(str);
}

/****************************************************************************/
/* Auto-toggle daylight savings time in US time-zones						*/
/****************************************************************************/
ushort DLLCALL sys_timezone(scfg_t* cfg)
{
	time_t	now;
	struct tm tm;

	if(cfg->sys_misc&SM_AUTO_DST && SMB_TZ_HAS_DST(cfg->sys_timezone)) {
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
