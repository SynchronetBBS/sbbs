/* Synchronet C-exported statistics retrieval functions */

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

#include "getstats.h"
#include "nopen.h"
#include "smblib.h"

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
	char path[MAX_PATH + 1];
	off_t l;

	if(dirnum >= cfg->total_dirs)	/* out of range */
		return 0;
	SAFEPRINTF2(path, "%s%s.sid", cfg->dir[dirnum]->data_dir, cfg->dir[dirnum]->code);
	l = flength(path);
	if(l <= 0)
		return 0;
	return (long)(l / sizeof(fileidxrec_t));
}

/****************************************************************************/
/* Returns total number of posts in a sub-board 							*/
/****************************************************************************/
ulong DLLCALL getposts(scfg_t* cfg, uint subnum)
{
	if(cfg->sub[subnum]->misc & SUB_NOVOTING) {
		char path[MAX_PATH + 1];
		off_t l;

		SAFEPRINTF2(path, "%s%s.sid", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
		l = flength(path);
		if(l < sizeof(idxrec_t))
			return 0;
		return (ulong)(l / sizeof(idxrec_t));
	}
	smb_t smb = {{0}};
	SAFEPRINTF2(smb.file, "%s%s", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
	smb.retry_time = cfg->smb_retry_time;
	smb.subnum = subnum;
	if(smb_open_index(&smb) != SMB_SUCCESS)
		return 0;
	size_t result = smb_msg_count(&smb, (1 << SMB_MSG_TYPE_NORMAL) | (1 << SMB_MSG_TYPE_POLL));
	smb_close(&smb);
	return result;
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
