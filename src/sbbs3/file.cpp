/* Synchronet file transfer-related sbbs_t class methods */

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
#include "filedat.h"

/****************************************************************************/
/* Prints all information of file in file_t structure 'f'					*/
/****************************************************************************/
void sbbs_t::showfileinfo(file_t* f, bool show_extdesc)
{
	char 	tmp[512];
	char	tmp2[64];
	char	path[MAX_PATH+1];

	current_file = f;
	getfilepath(&cfg, f, path);
	bprintf(P_TRUNCATE, text[FiLib], getusrlib(f->dir), cfg.lib[cfg.dir[f->dir]->lib]->lname);
	bprintf(P_TRUNCATE, text[FiDir], getusrdir(f->dir), cfg.dir[f->dir]->lname);
	bprintf(P_TRUNCATE, text[FiFilename],f->name);

	if(getfilesize(&cfg, f) >= 0)
		bprintf(P_TRUNCATE, text[FiFileSize], ultoac((ulong)f->size,tmp)
			, byte_estimate_to_str(f->size, tmp2, sizeof(tmp2), /* units: */1024, /* precision: */1));

	bprintf(P_TRUNCATE, text[FiCredits]
		,(cfg.dir[f->dir]->misc&DIR_FREE || !f->cost) ? "FREE" : ultoac((ulong)f->cost,tmp));
	if(getfilesize(&cfg, f) > 0 &&  f->size == f->file_idx.idx.size) {
#if 0 // I don't think anyone cares about the CRC-16 checksum value of a file
		if(f->file_idx.hash.flags & SMB_HASH_CRC16) {
			SAFEPRINTF(tmp, "%04x", f->file_idx.hash.data.crc16);
			bprintf(P_TRUNCATE, text[FiChecksum], "CRC-16", tmp);
		}
#endif
		if(f->file_idx.hash.flags & SMB_HASH_CRC32) {
			SAFEPRINTF(tmp, "%08x", f->file_idx.hash.data.crc32);
			bprintf(P_TRUNCATE, text[FiChecksum], "CRC-32", tmp);
		}
		if(f->file_idx.hash.flags & SMB_HASH_MD5)
			bprintf(P_TRUNCATE, text[FiChecksum], "MD5", MD5_hex(tmp, f->file_idx.hash.data.md5));
		if(f->file_idx.hash.flags & SMB_HASH_SHA1)
			bprintf(P_TRUNCATE, text[FiChecksum], "SHA-1", SHA1_hex(tmp, f->file_idx.hash.data.sha1));
	}
	if(f->desc && f->desc[0])
		bprintf(P_TRUNCATE, text[FiDescription],f->desc);
	if(f->tags && f->tags[0])
		bprintf(P_TRUNCATE, text[FiTags], f->tags);
	char* p = f->hdr.attr&MSG_ANONYMOUS ? text[UNKNOWN_USER] : f->from;
	if(p != NULL && *p != '\0')
		bprintf(P_TRUNCATE, text[FiUploadedBy], p);
	if(f->to_list != NULL && *f->to_list != '\0')
		bprintf(P_TRUNCATE, text[FiUploadedTo], f->to_list);
	bprintf(P_TRUNCATE, text[FiDateUled],timestr(f->hdr.when_imported.time));
	if(getfiletime(&cfg, f) > 0)
		bprintf(P_TRUNCATE, text[FiFileDate],timestr(f->time));
	bprintf(P_TRUNCATE, text[FiDateDled],f->hdr.last_downloaded ? timestr(f->hdr.last_downloaded) : "Never");
	bprintf(P_TRUNCATE, text[FiTimesDled],f->hdr.times_downloaded);
	ulong timetodl = gettimetodl(&cfg, f, cur_cps);
	if(timetodl > 0)
		bprintf(text[FiTransferTime],sectostr(timetodl,tmp));
	bputs(P_TRUNCATE, text[FileHdrDescSeparator]);
	if(show_extdesc && f->extdesc != NULL && *f->extdesc) {
		char* p = f->extdesc;
		SKIP_CRLF(p);
		truncsp(p);
		long p_mode = P_NOATCODES;
		if(!(console&CON_RAW_IN))
			p_mode |= P_WORDWRAP;
		putmsg(p, p_mode);
		newline();
	}
	if(f->size == -1) {
		bprintf(text[FileIsNotOnline],f->name);
		if(SYSOP)
			bprintf("%s\r\n",path);
	}
	current_file = NULL;
}

/****************************************************************************/
/* Prompts user for file specification. <CR> is *							*/
/* Returns padded file specification.                                       */
/* Returns NULL if input was aborted.                                       */
/****************************************************************************/
char * sbbs_t::getfilespec(char *str)
{
	bputs(text[FileSpecStarDotStar]);
	if(!getstr(str, MAX_FILENAME_LEN, K_NONE))
		strcpy(str, ALLFILES);
	if(sys_status&SS_ABORT)
		return(0);
	return(str);
}

/****************************************************************************/
/* Checks to see if filename matches filespec. Returns 1 if yes, 0 if no    */
/****************************************************************************/
extern "C" BOOL filematch(const char *filename, const char *filespec)
{
	return wildmatchi(filename, filespec, /* path: */FALSE);
}

/*****************************************************************************/
/* Checks the filename 'fname' for invalid symbol or character sequences     */
/*****************************************************************************/
bool sbbs_t::checkfname(char *fname)
{
    int		c=0,d;

	if(fname[0]=='-'
		|| strcspn(fname,ILLEGAL_FILENAME_CHARS)!=strlen(fname)
		|| strstr(fname, "..") != NULL) {
		lprintf(LOG_WARNING,"Suspicious filename attempt: '%s'",fname);
		hacklog((char *)"Filename", fname);
		return(false); 
	}
	d=strlen(fname);
	while(c<d) {
		if(fname[c]<=' ' || fname[c]&0x80)
			return(false);
		c++; 
	}
	return(true);
}

long sbbs_t::delfiles(const char *inpath, const char *spec, size_t keep)
{
	long result = ::delfiles(inpath, spec, keep);
	if(result < 0)
		errormsg(WHERE, ERR_REMOVE, inpath, result, spec);
	return result;
}

/****************************************************************************/
/* Remove credits or minutes and adjust statistics of uploader of file 'f'	*/
/****************************************************************************/
bool sbbs_t::removefcdt(file_t* f)
{
	char	str[128];
	char 	tmp[512];
	int		u;
	long	cdt;

	if((u=matchuser(&cfg,f->from,TRUE /*sysop_alias*/))==0) {
	   bputs(text[UnknownUser]);
	   return(false); 
	}
	cdt=0L;
	if(cfg.dir[f->dir]->misc&DIR_CDTMIN && cur_cps) {
		if(cfg.dir[f->dir]->misc&DIR_CDTUL)
			cdt=((ulong)(f->cost*(cfg.dir[f->dir]->up_pct/100.0))/cur_cps)/60;
		if(cfg.dir[f->dir]->misc&DIR_CDTDL
			&& f->hdr.times_downloaded)  /* all downloads */
			cdt+=((ulong)((long)f->hdr.times_downloaded
				*f->cost*(cfg.dir[f->dir]->dn_pct/100.0))/cur_cps)/60;
		adjustuserrec(&cfg,u,U_MIN,10,-cdt);
		sprintf(str,"%lu minute",cdt);
		sprintf(tmp,text[FileRemovedUserMsg]
			,f->name,cdt ? str : text[No]);
		putsmsg(&cfg,u,tmp);
	}
	else {
		if(cfg.dir[f->dir]->misc&DIR_CDTUL)
			cdt=(ulong)(f->cost*(cfg.dir[f->dir]->up_pct/100.0));
		if(cfg.dir[f->dir]->misc&DIR_CDTDL
			&& f->hdr.times_downloaded)  /* all downloads */
			cdt+=(ulong)((long)f->hdr.times_downloaded
				*f->cost*(cfg.dir[f->dir]->dn_pct/100.0));
		if(dir_op(f->dir)) {
			ultoa(cdt, str, 10);
			bputs(text[CreditsToRemove]);
			getstr(str, 10, K_NUMBER|K_LINE|K_EDIT|K_AUTODEL);
			if(sys_status&SS_ABORT)
				return false;
			cdt = atol(str); 
		}
		adjustuserrec(&cfg,u,U_CDT,10,-cdt);
		sprintf(tmp,text[FileRemovedUserMsg]
			,f->name,cdt ? ultoac(cdt,str) : text[No]);
		putsmsg(&cfg,u,tmp);
	}

	adjustuserrec(&cfg,u,U_ULB,10,(long)-f->size);
	adjustuserrec(&cfg,u,U_ULS,5,-1);
	return(true);
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::removefile(smb_t* smb, file_t* f)
{
	char str[256];
	int result;

	if((result = smb_removefile(smb ,f)) == SMB_SUCCESS) {
		SAFEPRINTF4(str,"%s removed %s from %s %s"
			,useron.alias
			,f->name
			,cfg.lib[cfg.dir[smb->dirnum]->lib]->sname,cfg.dir[smb->dirnum]->sname);
		logline("U-",str);
		return true;
	}
	errormsg(WHERE, ERR_REMOVE, f->name, result, smb->last_error);
	return false;
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::movefile(smb_t* smb, file_t* f, int newdir)
{
	if(findfile(&cfg, newdir, f->name, NULL)) {
		bprintf(text[FileAlreadyThere], f->name);
		return false; 
	}

	if(!addfile(&cfg, newdir, f, f->extdesc))
		return false;
	removefile(smb, f);
	bprintf(text[MovedFile],f->name
		,cfg.lib[cfg.dir[newdir]->lib]->sname,cfg.dir[newdir]->sname);
	char str[MAX_PATH+1];
	SAFEPRINTF4(str, "%s moved %s to %s %s",f->name
		,useron.alias
		,cfg.lib[cfg.dir[newdir]->lib]->sname
		,cfg.dir[newdir]->sname);
	logline(nulstr,str);

	/* move actual file */
	char oldpath[MAX_PATH + 1];
	getfilepath(&cfg, f, oldpath);
	f->dir = newdir;
	char newpath[MAX_PATH + 1];
	getfilepath(&cfg, f, newpath);
	mv(oldpath, newpath, /* copy */false); 
	
	return true;
}
