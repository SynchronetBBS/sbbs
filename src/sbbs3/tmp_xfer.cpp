/* Synchronet temp directory file transfer routines */

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

#include "sbbs.h"

/*****************************************************************************/
/* Temp directory section. Files must be extracted here and both temp_uler   */
/* and temp_uler fields should be filled before entrance.                    */
/*****************************************************************************/
void sbbs_t::temp_xfer()
{
	if(!cfg.tempxfer_mod[0]) {
		bprintf(text[DirectoryDoesNotExist], "temp (module)");
		return; 
	}
	exec_bin(cfg.tempxfer_mod, &main_csi);
}

/****************************************************************************/
/* Creates a text file named NEWFILES.DAT in the temp directory that        */
/* all new files since p-date. Returns number of files in list.             */
/****************************************************************************/
ulong sbbs_t::create_filelist(const char *name, long mode)
{
    char	str[256];
	FILE*	fp;
	uint	i,j,d;
	ulong	l,k;

	if(online == ON_REMOTE)
		bprintf(text[CreatingFileList],name);
	SAFEPRINTF2(str,"%s%s",cfg.temp_dir,name);
	if((fp = fopen(str,"ab")) == NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY|O_APPEND);
		return(0);
	}
	k=0;
	if(mode&FL_ULTIME) {
		fprintf(fp, "New files since: %s\r\n", timestr(ns_time));
	}
	unsigned total_dirs = 0;
	for(i=0; i < usrlibs ;i++)
		total_dirs += usrdirs[i];
	for(i=j=d=0;i<usrlibs;i++) {
		for(j=0;j<usrdirs[i];j++,d++) {
			progress(text[Scanning], d, total_dirs);
			if(mode&FL_ULTIME /* New-scan */
				&& (cfg.lib[usrlib[i]]->offline_dir==usrdir[i][j]
				|| cfg.dir[usrdir[i][j]]->misc&DIR_NOSCAN))
				continue;
			l=listfiles(usrdir[i][j], nulstr, fp, mode);
			if((long)l==-1)
				break;
			k+=l;
		}
		if(j<usrdirs[i])
			break;
	}
	progress(text[Done], d, total_dirs);
	if(k>1) {
		fprintf(fp,"\r\n%ld Files Listed.\r\n",k);
	}
	fclose(fp);
	if(k)
		bprintf(text[CreatedFileList],name);
	else {
		if(online == ON_REMOTE)
			bputs(text[NoFiles]);
		SAFEPRINTF2(str,"%s%s",cfg.temp_dir,name);
		remove(str);
	}
	return(k);
}

/****************************************************************************/
/* This function returns the command line for the temp file extension for	*/
/* current user online. 													*/
/****************************************************************************/
const char* sbbs_t::temp_cmd(void)
{
	int i;

	if(!cfg.total_fcomps) {
		errormsg(WHERE,ERR_CHK,"compressible file types",0);
		return(nulstr); 
	}
	for(i=0;i<cfg.total_fcomps;i++)
		if(!stricmp(useron.tmpext,cfg.fcomp[i]->ext)
			&& chk_ar(cfg.fcomp[i]->ar,&useron,&client))
			return(cfg.fcomp[i]->cmd);
	return(cfg.fcomp[0]->cmd);
}
