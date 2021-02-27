/* getstats.c */

/* Synchronet C-export statistics retrieval functions */

/* $Id: getstats.c,v 1.5 2020/04/11 04:01:35 rswindell Exp $ */

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

/****************************************************************************/
/* Reads data from dsts.dab into stats structure                            */
/* If node is zero, reads from ctrl\dsts.dab, otherwise from each node		*/
/****************************************************************************/
BOOL DLLCALL getstats(scfg_t* cfg, char node, stats_t* stats)
{
    char str[MAX_PATH+1];
    int file;

    sprintf(str,"%sdsts.dab",node ? cfg->node_path[node-1] : cfg->ctrl_dir);
    if((file=nopen(str,O_RDONLY))==-1) {
        return(FALSE); 
	}
    lseek(file,4L,SEEK_SET);    /* Skip update time/date */
	/* TODO: Direct read of unpacked struct */
    read(file,stats,sizeof(stats_t));
    close(file);
	return(TRUE);
}

/****************************************************************************/
/* Returns the number of files in the directory 'dirnum'                    */
/****************************************************************************/
long DLLCALL getfiles(scfg_t* cfg, uint dirnum)
{
	char str[256];
	long l;

	if(dirnum>=cfg->total_dirs)	/* out of range */
		return(0);
	sprintf(str,"%s%s.ixb",cfg->dir[dirnum]->data_dir, cfg->dir[dirnum]->code);
	l=(long)flength(str);
	if(l>0L)
		return(l/F_IXBSIZE);
	return(0);
}

/****************************************************************************/
/* Returns total number of posts in a sub-board 							*/
/****************************************************************************/
ulong DLLCALL getposts(scfg_t* cfg, uint subnum)
{
	char str[128];
	ulong l;

	sprintf(str,"%s%s.sid",cfg->sub[subnum]->data_dir,cfg->sub[subnum]->code);
	l=(long)flength(str);
	if((long)l==-1)
		return(0);
	return(l/sizeof(idxrec_t));
}

BOOL inc_sys_upload_stats(scfg_t* cfg, ulong files, ulong bytes)
{
	char	str[MAX_PATH+1];
	int		file;
	uint32_t	val;

	sprintf(str,"%sdsts.dab",cfg->ctrl_dir);
	if((file=nopen(str,O_RDWR))==-1) 
		return(FALSE);

	lseek(file,20L,SEEK_SET);   /* Skip timestamp, logons and logons today */
	read(file,&val,4);        /* Uploads today         */
	val+=files;
	lseek(file,-4L,SEEK_CUR);
	write(file,&val,4);
	read(file,&val,4);        /* Upload bytes today    */
	val+=bytes;
	lseek(file,-4L,SEEK_CUR);
	write(file,&val,4);
	close(file);
	return(TRUE);
}

BOOL inc_sys_download_stats(scfg_t* cfg, ulong files, ulong bytes)
{
	char	str[MAX_PATH+1];
	int		file;
	uint32_t	val;

	sprintf(str,"%sdsts.dab",cfg->ctrl_dir);
	if((file=nopen(str,O_RDWR))==-1) 
		return(FALSE);

	lseek(file,28L,SEEK_SET);   /* Skip timestamp, logons and logons today */
	read(file,&val,4);        /* Downloads today         */
	val+=files;
	lseek(file,-4L,SEEK_CUR);
	write(file,&val,4);
	read(file,&val,4);        /* Download bytes today    */
	val+=bytes;
	lseek(file,-4L,SEEK_CUR);
	write(file,&val,4);
	close(file);
	return(TRUE);
}
