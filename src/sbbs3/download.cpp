/* Synchronet file download routines */

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
#include "telnet.h"
#include "filedat.h"

/****************************************************************************/
/* Call this function *AFTER* a file has been successfully downloaded		*/
/* Updates downloader, uploader and downloaded file data                    */
/* Must have offset, dir and name fields filled prior to call.              */
/****************************************************************************/
void sbbs_t::downloadedfile(file_t* f)
{
    char		str[MAX_PATH+1];
	char 		tmp[512];
	off_t		length;

	length = getfilesize(&cfg, f);
	if(length > 0 && !(cfg.dir[f->dir]->misc&DIR_NOSTAT)) {
		logon_dlb += length;  /* Update 'this call' stats */
		logon_dls++;
	}
	bprintf(text[FileNBytesSent],f->name,ultoac((ulong)length,tmp));
	snprintf(str, sizeof str, "downloaded %s (%" PRId64 " bytes) from %s %s"
		,f->name
		,length
		,cfg.lib[cfg.dir[f->dir]->lib]->sname
		,cfg.dir[f->dir]->sname);
	logline("D-",str);

	user_downloaded_file(&cfg, &useron, &client, f->dir, f->name, length);
	mqtt_file_download(mqtt, &useron, f->dir, f->name, length, &client);

	user_event(EVENT_DOWNLOAD);
}

/****************************************************************************/
/* This function is to be called when a file is successfully downloaded,	*/
/* to re-calculate and store the user's download through-put (in CPS).		*/
/****************************************************************************/
void sbbs_t::downloadedbytes(off_t size, time_t elapsed)
{
	if(elapsed <= 1)
		cur_cps = (uint)size;
	else
		cur_cps = (uint)(size / elapsed);
	putuserdec32(useron.number, USER_DLCPS, cur_cps);
}

/****************************************************************************/
/* This function is called when a file is unsuccessfully downloaded.        */
/* It logs the transfer time and checks for possible leech protocol use.    */
/****************************************************************************/
void sbbs_t::notdownloaded(off_t size, time_t elapsed)
{
    char	str[256],tmp2[256];
	char 	tmp[512];

	SAFEPRINTF2(str,"Estimated Time: %s  Transfer Time: %s"
		,sectostr(cur_cps ? (uint)(size/cur_cps) : 0,tmp)
		,sectostr((uint)(elapsed),tmp2));
	logline(nulstr,str);
	if(cfg.leech_pct && cur_cps                 /* leech detection */
		&& elapsed>=cfg.leech_sec
		&& elapsed>=(double)(size/cur_cps)*(double)cfg.leech_pct/100.0) {
		lprintf(LOG_ERR, "Node %d Possible use of leech protocol (leech=%u  downloads=%u)"
			,cfg.node_num, useron.leech+1,useron.dls);
		useron.leech = (uchar)adjustuserval(&cfg, useron.number, USER_LEECH, 1);
	}
}

const char* sbbs_t::protcmdline(prot_t* prot, enum XFER_TYPE type)
{
	switch(type) {
		case XFER_UPLOAD:
			return(prot->ulcmd);
		case XFER_DOWNLOAD:
			return(prot->dlcmd);
		case XFER_BATCH_UPLOAD:
			return(prot->batulcmd);
		case XFER_BATCH_DOWNLOAD:
			return(prot->batdlcmd);
	}

	return("invalid transfer type");
}

void sbbs_t::data_transfer_begin(uchar& local_binary_tx, uchar& remote_binary_tx)
{
	sys_status|=SS_FILEXFER;	/* disable spy during file xfer */
	local_binary_tx = telnet_local_option[TELNET_BINARY_TX];
	remote_binary_tx = telnet_remote_option[TELNET_BINARY_TX];
	/* enable telnet binary transmission in both directions */
	request_telnet_opt(TELNET_DO,TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WILL,TELNET_BINARY_TX);
	console |= CON_RAW_IN;
}

void sbbs_t::data_transfer_end(uchar local_binary_tx, uchar remote_binary_tx)
{
	sys_status&=~SS_FILEXFER;
	/* Got back to Text/NVT mode */
	request_telnet_opt(local_binary_tx, TELNET_BINARY_TX);
	request_telnet_opt(remote_binary_tx, TELNET_BINARY_TX);
	console &= ~CON_RAW_IN;
}

/****************************************************************************/
/* Handles start and stop routines for transfer protocols                   */
/****************************************************************************/
int sbbs_t::protocol(prot_t* prot, enum XFER_TYPE type
					 ,const char *fpath, const char *fspec, bool cd, bool autohangup, time_t* elapsed)
{
	char	protlog[256],*p;
	char*	cmdline;
	char	msg[256];
    int		i;
	long	ex_mode;
	FILE*	stream;

	SAFEPRINTF(protlog,"%sPROTOCOL.LOG",cfg.node_dir);
	remove(protlog);                        /* Deletes the protocol log */
	autohang=false;
	if(autohangup) {
		if(useron.misc&AUTOHANG)
			autohang=true;
		else if(text[HangUpAfterXferQ][0])
			autohang=yesno(text[HangUpAfterXferQ]);
	}
	if(sys_status&SS_ABORT || !online) {	/* if ctrl-c or hangup */
		return(-1);
	}
	bprintf(text[StartXferNow], prot->name);
	if(cd)
		p=cfg.temp_dir;
	else
		p=NULL;

	ex_mode = EX_BIN;
	if(prot->misc&PROT_NATIVE)
		ex_mode|=EX_NATIVE;
#ifdef __unix__		/* file xfer progs may use stdio on Unix */
	if(!(prot->misc&PROT_SOCKET))
		ex_mode|=EX_STDIO;
#endif

	cmdline=cmdstr(protcmdline(prot,type), fpath, fspec, NULL, ex_mode);
	SAFEPRINTF(msg,"Transferring %s",cmdline);
	spymsg(msg);
	uchar local_binary_tx, remote_binary_tx;
	data_transfer_begin(local_binary_tx, remote_binary_tx);
	time_t start = time(NULL);
	i=external(cmdline,ex_mode,p);
	time_t end = time(NULL);
	data_transfer_end(local_binary_tx, remote_binary_tx);

	// Save DSZLOG to logfile
	if((stream=fnopen(NULL,protlog,O_RDONLY))!=NULL) {
		while(!feof(stream) && !ferror(stream)) {
			if(!fgets(protlog,sizeof(protlog),stream))
				break;
			truncsp(protlog);
			logline(LOG_DEBUG,nulstr,protlog);
		}
		fclose(stream);
	}

	CRLF;
	if(autohang) sys_status|=SS_PAUSEOFF;	/* Pause off after download */
	if(elapsed != nullptr) {
		*elapsed = end - start;
		if(*elapsed < 0)
			*elapsed = 0;
	}
	return(i);
}

/****************************************************************************/
/* Invokes the timed auto-hangup after transfer routine                     */
/****************************************************************************/
void sbbs_t::autohangup()
{
    int		a,c;
	char	k;
	char 	tmp[512];

	if(online!=ON_REMOTE)
		return;
	sync();
	sys_status&=~SS_PAUSEOFF;		/* turn pause back on */
	rioctl(IOFI);
	if(!autohang) return;
	lncntr=0;
	bputs(text[Disconnecting]);
	attr(GREEN);
	outchar('[');
	for(c=9,a=0;c>-1 && online && !a;c--) {
		checkline();
		attr(LIGHTGRAY|HIGH);
		bputs(ultoa(c,tmp,10));
		attr(GREEN);
		outchar(']');
		while((k=inkey(K_NONE,DELAY_AUTOHG))!=0 && online) {
			if(toupper(k)=='H') {
				c=0;
				break;
			}
			if(toupper(k)=='A') {
				a=1;
				break;
			}
		}
		if(!a) {
			outchar(BS);
			outchar(BS);
		}
	}
	if(c==-1) {
		bputs(text[Disconnected]);
		hangup();
	}
	else
		CRLF;
}

bool sbbs_t::checkdszlog(const char* fpath)
{
	char	path[MAX_PATH+1];
	char	rpath[MAX_PATH+1];
	char	str[MAX_PATH+128];
	char*	p;
	char	code;
	ulong	bytes;
	FILE*	fp;
	bool	success=false;

	SAFEPRINTF(path,"%sPROTOCOL.LOG",cfg.node_dir);
	if((fp=fopen(path,"r"))==NULL)
		return false;

	SAFECOPY(rpath, fpath);
	fexistcase(rpath);

	while(!ferror(fp)) {
		if(!fgets(str,sizeof(str),fp))
			break;
		if((p=strrchr(str,' '))!=NULL)
			*p=0;	/* we don't care about no stinking serial number */
		p=str;
		code=*p;
		FIND_WHITESPACE(p);	// Skip code
		SKIP_WHITESPACE(p);
		bytes=strtoul(p,NULL,10);
		FIND_WHITESPACE(p);	// Skip bytes
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip DTE rate
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip "bps" rate
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip CPS
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip "cps"
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip errors
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip "errors"
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip flows
		SKIP_WHITESPACE(p);
		FIND_WHITESPACE(p);	// Skip block size
		SKIP_WHITESPACE(p);
		p=getfname(p);	/* DSZ stores fullpath, BiModem doesn't */
		if(stricmp(p, getfname(fpath))==0 || stricmp(p, getfname(rpath))==0) {
			/* E for Error or L for Lost Carrier or s for Skipped */
			/* or only sent 0 bytes! */
			if(code!='E' && code!='L' && code!='s' && bytes!=0)
				success=true;
			break;
		}
	}
	fclose(fp);
	return(success);
}


/****************************************************************************/
/* Checks dsz compatible log file for errors in transfer                    */
/* Returns 1 if the file in the struct file_t was successfully transfered   */
/****************************************************************************/
bool sbbs_t::checkprotresult(prot_t* prot, int error, const char* fpath)
{
	bool success;

	if(prot->misc&PROT_DSZLOG)
		success=checkdszlog(fpath);
	else
		success=(error==0);

	if(!success)
		bprintf(text[FileNotSent], getfname(fpath));

	return success;
}

bool sbbs_t::checkprotresult(prot_t* prot, int error, file_t* f)
{
	char str[512];
	char tmp[128];
	char fpath[MAX_PATH+1];

	getfilepath(&cfg, f, fpath);
	if(!checkprotresult(prot, error, fpath)) {
		if(f->dir >= 0 && f->dir<cfg.total_dirs)
			SAFEPRINTF4(str,"attempted to download %s (%s) from %s %s"
				,f->name,ultoac((ulong)f->size,tmp)
				,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
		else if(f->dir==cfg.total_dirs)
			SAFECOPY(str,"attempted to download QWK packet");
		else if(f->dir == cfg.total_dirs + 1)
			SAFEPRINTF(str,"attempted to download attached file: %s"
				,f->name);
		else
			SAFEPRINTF2(str,"attempted to download file (%s) from unknown dir: %ld"
				,f->name, (long)f->dir);
		logline(LOG_NOTICE,"D!",str);
		return false; 
	}
	return true;
}


/************************************************************************/
/* Wait (for a limited period of time) for sequential dev to become 	*/
/* available for file retrieval 										*/
/************************************************************************/
void sbbs_t::seqwait(uint devnum)
{
	char loop=0;
	int i;
	time_t start;
	node_t node;

	if(!devnum)
		return;
	for(start=now=time(NULL);online && now-start<90;now=time(NULL)) {
		if(msgabort())				/* max wait ^^^^ sec */
			break;
		getnodedat(cfg.node_num,&thisnode,true);	/* open and lock this record */
		for(i=1;i<=cfg.sys_nodes;i++) {
			if(i==cfg.node_num) continue;
			if(getnodedat(i,&node,true)==0) {
				if((node.status==NODE_INUSE || node.status==NODE_QUIET)
					&& node.action==NODE_RFSD && node.aux==devnum) {
					putnodedat(i,&node);
					break;
				}
				putnodedat(i,&node);
			}
		}
		if(i>cfg.sys_nodes) {
			thisnode.action=NODE_RFSD;
			thisnode.aux=devnum;
			putnodedat(cfg.node_num,&thisnode);	/* write devnum, unlock, and ret */
			return;
		}
		putnodedat(cfg.node_num,&thisnode);
		if(!loop)
			bprintf(text[WaitingForDeviceN],devnum);
		loop=1;
		mswait(100);
	}

}

bool sbbs_t::sendfile(char* fname, char prot, const char* desc, bool autohang)
{
	char	keys[128];
	char	tmp[128];
	char	ch;
	int		i;
	int		error;
	bool	result;

	i = protnum(prot, XFER_DOWNLOAD);
	if (i >= cfg.total_prots) {
		xfer_prot_menu(XFER_DOWNLOAD, &useron, keys, sizeof keys);
		SAFECAT(keys, quit_key(tmp));
		mnemonics(text[ProtocolOrQuit]);
		ch=(char)getkeys(keys,0);

		if(ch==quit_key() || sys_status&SS_ABORT)
			return false;
		i = protnum(ch, XFER_DOWNLOAD);
		if(i >= cfg.total_prots)
			return false;
	}
	time_t elapsed = 0;
	error = protocol(cfg.prot[i], XFER_DOWNLOAD, fname, fname, false, autohang, &elapsed);
	if(cfg.prot[i]->misc&PROT_DSZLOG)
		result = checkdszlog(fname);
	else
		result = (error == 0);

	if(result) {
		off_t length = flength(fname);
		logon_dlb += length;	/* Update stats */
		logon_dls++;
		useron.dls = (ushort)adjustuserval(&cfg, useron.number, USER_DLS, 1);
		useron.dlb = (uint32_t)adjustuserval(&cfg, useron.number, USER_DLB, length);
		downloadedbytes(length, elapsed);
		char bytes[32];
		u64toac(length, bytes);
		bprintf(text[FileNBytesSent], getfname(fname), bytes);
		char str[128];
		SAFEPRINTF3(str, "downloaded %s: %s (%s bytes)"
			,desc == NULL ? "file" : desc, fname, bytes);
		logline("D-",str);
		autohangup();
	} else {
		char str[128];
		bprintf(text[FileNotSent], getfname(fname));
		SAFEPRINTF2(str,"attempted to download %s: %s", desc == NULL ? "file" : desc, fname);
		logline(LOG_NOTICE,"D!",str);
	}
	return result;
}

// contains some copy/pasta from downloadedfile()
bool sbbs_t::sendfile(file_t* f, char prot, bool autohang)
{
	char path[MAX_PATH + 1];
	char str[256];

	SAFEPRINTF2(str, "from %s %s"
		,cfg.lib[cfg.dir[f->dir]->lib]->sname
		,cfg.dir[f->dir]->sname);
	bool result = sendfile(getfilepath(&cfg, f, path), prot, str, autohang);
	if(result == true) {
		if(cfg.dir[f->dir]->misc&DIR_TFREE && cur_cps)
			starttime += f->size / (ulong)cur_cps;
		off_t length = getfilesize(&cfg, f);
		if(length > 0 && !(cfg.dir[f->dir]->misc&DIR_NOSTAT)) {
			logon_dlb += length;  /* Update 'this call' stats */
			logon_dls++;
		}
		user_downloaded_file(&cfg, &useron, &client, f->dir, f->name, length);
		user_event(EVENT_DOWNLOAD);
	}
	return result;
}
