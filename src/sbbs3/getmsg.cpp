/* Synchronet message retrieval functions */

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

/***********************************************************************/
/* Functions that do i/o with messages (posts/mail/auto) or their data */
/***********************************************************************/

#include "sbbs.h"
#include "utf8.h"

/****************************************************************************/
/* Loads an SMB message from the open msg base the fastest way possible 	*/
/* first by offset, and if that's the wrong message, then by number.        */
/* Returns >=0 if the message was loaded and left locked, otherwise < 0.	*/
/* !WARNING!: If you're going to write the msg index back to disk, you must */
/* Call this function with a msg->idx.offset of 0 (so msg->offset will be	*/
/* initialized correctly)													*/
/****************************************************************************/
int sbbs_t::loadmsg(smbmsg_t *msg, ulong number)
{
	char str[128];
	int i;

	if(msg->idx.offset) {				/* Load by offset if specified */

		if((i=smb_lockmsghdr(&smb,msg)) != SMB_SUCCESS) {
			errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
			return i;
		}

		i=smb_getmsghdr(&smb,msg);
		if(i==SMB_SUCCESS) {
			if(msg->hdr.number==number)
				return msg->total_hfields;
			/* Wrong offset  */
			smb_freemsgmem(msg);
		}

		smb_unlockmsghdr(&smb,msg);
	}

	msg->hdr.number=number;
	if((i=smb_getmsgidx(&smb,msg))!=SMB_SUCCESS)				 /* Message is deleted */
		return i;
	if((i=smb_lockmsghdr(&smb,msg))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return i;
	}
	if((i=smb_getmsghdr(&smb,msg))!=SMB_SUCCESS) {
		SAFEPRINTF4(str,"(%06" PRIX32 ") #%" PRIu32 "/%lu %s",msg->idx.offset,msg->idx.number
			,number,smb.file);
		smb_unlockmsghdr(&smb,msg);
		errormsg(WHERE,ERR_READ,str,i,smb.last_error);
		return i;
	}
	return msg->total_hfields;
}

/* Synchronized with atcode()! */
void sbbs_t::show_msgattr(smbmsg_t* msg)
{
	uint16_t attr = msg->hdr.attr;
	uint16_t poll = attr&MSG_POLL_VOTE_MASK;
	uint32_t auxattr = msg->hdr.auxattr;
	uint32_t netattr = msg->hdr.netattr;

	char attr_str[64];
	safe_snprintf(attr_str, sizeof(attr_str), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
		,attr&MSG_PRIVATE	? "Private  "   :nulstr
		,attr&MSG_SPAM		? "SPAM  "      :nulstr
		,attr&MSG_READ		? "Read  "      :nulstr
		,attr&MSG_DELETE	? "Deleted  "   :nulstr
		,attr&MSG_KILLREAD	? "Kill  "      :nulstr
		,attr&MSG_ANONYMOUS ? "Anonymous  " :nulstr
		,attr&MSG_LOCKED	? "Locked  "    :nulstr
		,attr&MSG_PERMANENT ? "Permanent  " :nulstr
		,attr&MSG_MODERATED ? "Moderated  " :nulstr
		,attr&MSG_VALIDATED ? "Validated  " :nulstr
		,attr&MSG_REPLIED	? "Replied  "	:nulstr
		,attr&MSG_NOREPLY	? "NoReply  "	:nulstr
		,poll == MSG_POLL	? "Poll  "		:nulstr
		,poll == MSG_POLL && auxattr&POLL_CLOSED ? "(Closed)  "	:nulstr
	);

	char auxattr_str[64];
	safe_snprintf(auxattr_str, sizeof(auxattr_str), "%s%s%s%s%s%s%s"
		,auxattr&MSG_FILEREQUEST? "FileRequest  "   :nulstr
		,auxattr&MSG_FILEATTACH	? "FileAttach  "    :nulstr
		,auxattr&MSG_MIMEATTACH	? "MimeAttach  "	:nulstr
		,auxattr&MSG_KILLFILE	? "KillFile  "      :nulstr
		,auxattr&MSG_RECEIPTREQ	? "ReceiptReq  "	:nulstr
		,auxattr&MSG_CONFIRMREQ	? "ConfirmReq  "    :nulstr
		,auxattr&MSG_NODISP		? "DontDisplay  "	:nulstr
		);

	char netattr_str[64];
	safe_snprintf(netattr_str, sizeof(netattr_str), "%s%s%s%s%s%s%s%s"
		,netattr&MSG_LOCAL		? "Local  "			:nulstr
		,netattr&MSG_INTRANSIT	? "InTransit  "     :nulstr
		,netattr&MSG_SENT		? "Sent  "			:nulstr
		,netattr&MSG_KILLSENT	? "KillSent  "      :nulstr
		,netattr&MSG_HOLD		? "Hold  "			:nulstr
		,netattr&MSG_CRASH		? "Crash  "			:nulstr
		,netattr&MSG_IMMEDIATE	? "Immediate  "		:nulstr
		,netattr&MSG_DIRECT		? "Direct  "		:nulstr
		);

	bprintf(text[MsgAttr], attr_str, auxattr_str, netattr_str
		,nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr
		,nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr
		,nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr, nulstr);
}

/* Returns a CP437 text.dat string converted to UTF-8, when appropriate */
const char* sbbs_t::msghdr_text(const smbmsg_t* msg, uint index)
{
	if(msg == NULL || !(msg->hdr.auxattr & MSG_HFIELDS_UTF8))
		return text[index];

	if(cp437_to_utf8_str(text[index], msghdr_utf8_text, sizeof(msghdr_utf8_text), /* min-char-val: */'\x80') < 1)
		return text[index];

	return msghdr_utf8_text;
}

// Returns a CP437 version of a message header field or UTF-8 if can_utf8 is true
// Doesn't do CP437->UTF-8 conversion
const char* sbbs_t::msghdr_field(const smbmsg_t* msg, const char* str, char* buf, bool can_utf8)
{
	if(msg == NULL || !(msg->hdr.auxattr & MSG_HFIELDS_UTF8))
		return str;

	if(can_utf8)
		return str;

	if(buf == NULL)
		buf = msgghdr_field_cp437_str;

	strncpy(buf, str, sizeof(msgghdr_field_cp437_str) - 1);
	utf8_to_cp437_inplace(buf);

	return buf;
}

/****************************************************************************/
/* Displays a message header to the screen                                  */
/****************************************************************************/
void sbbs_t::show_msghdr(smb_t* smb, smbmsg_t* msg, const char* subject, const char* from, const char* to)
{
	char	str[MAX_PATH+1];
	char	age[64];
	char	*sender=NULL;
	int 	i;
	smb_t	saved_smb = this->smb;
	long	pmode = 0;

	if(smb != NULL)
		this->smb = *smb;	// Needed for @-codes and JS bbs.smb_* properties
	current_msg = msg;		// Needed for @-codes and JS bbs.msg_* properties
	current_msg_subj = msg->subj;
	current_msg_from = msg->from;
	current_msg_to = msg->to;
	if(msg->hdr.auxattr & MSG_HFIELDS_UTF8)
		pmode |= P_UTF8;

	if(subject != NULL)
		current_msg_subj = subject;
	if(from != NULL)
		current_msg_from = from;
	if(to != NULL)
		current_msg_to = to;

	attr(LIGHTGRAY);
	if(row != 0) {
		if(useron.misc&CLRSCRN)
			outchar(FF);
		else
			CRLF;
	}
	msghdr_tos = (row == 0);
	if(!menu("msghdr", P_NOERROR)) {
		bprintf(pmode, msghdr_text(msg, MsgSubj), current_msg_subj);
		if(msg->tags && *msg->tags)
			bprintf(text[MsgTags], msg->tags);
		if(msg->hdr.attr || msg->hdr.netattr || (msg->hdr.auxattr & ~MSG_HFIELDS_UTF8))
			show_msgattr(msg);
		if(current_msg_to != NULL && *current_msg_to != 0) {
			bprintf(pmode, msghdr_text(msg, MsgTo), current_msg_to);
			if(msg->to_net.addr!=NULL)
				bprintf(text[MsgToNet],smb_netaddrstr(&msg->to_net,str));
			if(msg->to_ext)
				bprintf(text[MsgToExt],msg->to_ext);
		}
		if(msg->cc_list != NULL)
			bprintf(text[MsgCarbonCopyList], msg->cc_list);
		if(current_msg_from != NULL && (!(msg->hdr.attr&MSG_ANONYMOUS) || SYSOP)) {
			bprintf(pmode, msghdr_text(msg, MsgFrom), current_msg_from);
			if(msg->from_ext)
				bprintf(text[MsgFromExt],msg->from_ext);
			if(msg->from_net.addr!=NULL)
				bprintf(text[MsgFromNet],smb_netaddrstr(&msg->from_net,str));
		}
		if(!(msg->hdr.attr&MSG_POLL) && (msg->upvotes || msg->downvotes))
			bprintf(text[MsgVotes]
				,msg->upvotes, msg->user_voted==1 ? text[PollAnswerChecked] : nulstr
				,msg->downvotes, msg->user_voted==2 ? text[PollAnswerChecked] : nulstr
				,msg->upvotes - msg->downvotes);
		bprintf(text[MsgDate]
			,timestr(msg->hdr.when_written.time)
			,smb_zonestr(msg->hdr.when_written.zone,NULL)
			,age_of_posted_item(age, sizeof(age), msg->hdr.when_written.time - (smb_tzutc(msg->hdr.when_written.zone) * 60)));
		bputs(text[MsgHdrBodySeparator]);
	}
	for(i=0;i<msg->total_hfields;i++) {
		if(msg->hfield[i].type==SENDER)
			sender=(char *)msg->hfield_dat[i];
		if(msg->hfield[i].type==FORWARDED && sender)
			bprintf(text[ForwardedFrom],sender
				,timestr(*(time32_t *)msg->hfield_dat[i]));
	}
	this->smb = saved_smb;
	current_msg_subj = NULL;
	current_msg_from = NULL;
	current_msg_to = NULL;
}

/****************************************************************************/
/* Displays message header and text (if not deleted)                        */
/****************************************************************************/
bool sbbs_t::show_msg(smb_t* smb, smbmsg_t* msg, long p_mode, post_t* post)
{
	char*	txt;

	if((msg->hdr.type == SMB_MSG_TYPE_NORMAL && post != NULL && (post->upvotes || post->downvotes))
		|| msg->hdr.type == SMB_MSG_TYPE_POLL)
		msg->user_voted = smb_voted_already(smb, msg->hdr.number
					,cfg.sub[smb->subnum]->misc&SUB_NAME ? useron.name : useron.alias, NET_NONE, NULL);

	show_msghdr(smb, msg);

	int comments=0;
	for(int i = 0; i < msg->total_hfields; i++)
		if(msg->hfield[i].type == SMB_COMMENT) {
			bprintf("%s\r\n", (char*)msg->hfield_dat[i]);
			comments++;
		}
	if(comments)
		CRLF;

	if(msg->hdr.type == SMB_MSG_TYPE_POLL && post != NULL && smb->subnum < cfg.total_subs) {
		char* answer;
		int longest_answer = 0;

		for(int i = 0; i < msg->total_hfields; i++) {
			if(msg->hfield[i].type != SMB_POLL_ANSWER)
				continue;
			answer = (char*)msg->hfield_dat[i];
			int len = strlen(answer);
			if(len > longest_answer)
				longest_answer = len;
		}
		unsigned answers = 0;
		for(int i = 0; i < msg->total_hfields; i++) {
			if(msg->hfield[i].type != SMB_POLL_ANSWER)
				continue;
			answer = (char*)msg->hfield_dat[i];
			float pct = post->total_votes ? ((float)post->votes[answers] / post->total_votes)*100.0F : 0.0F;
			char str[128];
			int width = longest_answer;
			if(width < cols/3) width = cols/3;
			else if(width > cols-20)
				width = cols-20;
			bprintf(text[PollAnswerNumber], answers+1);
			bool results_visible = false;
			if((msg->hdr.auxattr&POLL_RESULTS_MASK) == POLL_RESULTS_OPEN)
				results_visible = true;
			else if((msg->from_net.type == NET_NONE && sub_op(smb->subnum))
				|| smb_msg_is_from(msg, cfg.sub[smb->subnum]->misc&SUB_NAME ? useron.name : useron.alias, NET_NONE, NULL))
				results_visible = true;
			else if((msg->hdr.auxattr&POLL_RESULTS_MASK) == POLL_RESULTS_CLOSED)
				results_visible = (msg->hdr.auxattr&POLL_CLOSED) ? true : false;
			else if((msg->hdr.auxattr&POLL_RESULTS_MASK) != POLL_RESULTS_SECRET)
				results_visible = msg->user_voted ? true : false;
			if(results_visible) {
				safe_snprintf(str, sizeof(str), text[PollAnswerFmt]
					,width, width, answer, post->votes[answers], pct);
				backfill(str, pct, cfg.color[clr_votes_full], cfg.color[clr_votes_empty]);
				if(msg->user_voted&(1<<answers))
					bputs(text[PollAnswerChecked]);
			} else {
				attr(cfg.color[clr_votes_empty]);
				bputs(answer);
			}
			CRLF;
			answers++;
		}
		if(!msg->user_voted && !(useron.misc&EXPERT) && !(msg->hdr.auxattr&POLL_CLOSED) && !(useron.rest&FLAG('V')))
			mnemonics(text[VoteInThisPollNow]);
		return true;
	}
	if((txt=smb_getmsgtxt(smb, msg, GETMSGTXT_BODY_ONLY)) == NULL)
		return false;
	char* p = txt;
	if(!(console&CON_RAW_IN)) {
		if(strstr(txt, "\x1b[") == NULL)	// Don't word-wrap raw ANSI text
			p_mode|=P_WORDWRAP;
		p = smb_getplaintext(msg, txt);
		if(p == NULL)
			p = txt;
		else if(*p != '\0')
			bprintf(text[MIMEDecodedPlainTextFmt]
				, msg->text_charset == NULL ? "unspecified (US-ASCII)" : msg->text_charset
				, msg->text_subtype == NULL ? "plain" : msg->text_subtype);
	}
	truncsp(p);
	SKIP_CRLF(p);
	if(smb_msg_is_utf8(msg)) {
		if(!term_supports(UTF8))
			utf8_normalize_str(txt);
		p_mode |= P_UTF8;
	}
	if(smb->subnum < cfg.total_subs) {
		p_mode |= cfg.sub[smb->subnum]->pmode;
		p_mode &= ~cfg.sub[smb->subnum]->n_pmode;
	}
	if(console & CON_RAW_IN)
		p_mode = P_NOATCODES;
	putmsg(p, p_mode, msg->columns);
	smb_freemsgtxt(txt);
	if(column)
		CRLF;
	if((txt=smb_getmsgtxt(smb,msg,GETMSGTXT_TAIL_ONLY))==NULL)
		return false;

	putmsg(txt, p_mode&(~P_WORDWRAP));
	smb_freemsgtxt(txt);
	return true;
}

void sbbs_t::download_msg_attachments(smb_t* smb, smbmsg_t* msg, bool del)
{
	char str[256];
	char fpath[MAX_PATH+1];
	char* txt;
	int attachment_index = 0;
	bool found = true;
	while(found && (txt=smb_getmsgtxt(smb, msg, 0)) != NULL) {
		char filename[MAX_PATH+1] = {0};
		uint32_t filelen = 0;
		uint8_t* filedata;
		if((filedata = smb_getattachment(msg, txt, filename, sizeof(filename), &filelen, attachment_index++)) != NULL
			&& filename[0] != 0 && filelen > 0) {
			char tmp[32];
			SAFEPRINTF2(str, text[DownloadAttachedFileQ], filename, ultoac(filelen,tmp));
			if(!noyes(str)) {
				SAFEPRINTF2(fpath, "%s%s", cfg.temp_dir, filename);
				FILE* fp = fopen(fpath, "wb");
				if(fp == NULL)
					errormsg(WHERE, ERR_OPEN, fpath, 0);
				else {
					int result = fwrite(filedata, filelen, 1, fp);
					fclose(fp);
					if(!result)
						errormsg(WHERE, ERR_WRITE, fpath, filelen);
					else
						sendfile(fpath, useron.prot, "attachment");
				}
			}
		} else
			found = false;
		smb_freemsgtxt(txt);
	}

	if(msg->hdr.auxattr&MSG_FILEATTACH) {  /* Attached file */
		char subj[FIDO_SUBJ_LEN];
		int result = smb_getmsgidx(smb, msg);
		if(result != SMB_SUCCESS) {
			errormsg(WHERE, ERR_READ, "index", result, smb->last_error);
			return;
		}
		SAFECOPY(subj, msg->subj);					/* filenames (multiple?) in title */
		char *p,*tp,ch;
		tp=subj;
		while(online) {
			p=strchr(tp,' ');
			if(p) *p=0;
			tp=getfname(tp);
			if(strcspn(tp, ILLEGAL_FILENAME_CHARS) == strlen(tp)) {
				SAFEPRINTF3(fpath,"%sfile/%04u.in/%s"  /* path is path/fname */
					,cfg.data_dir, msg->idx.to, tp);
				if(!fexistcase(fpath) && msg->idx.from)
					SAFEPRINTF3(fpath,"%sfile/%04u.out/%s"  /* path is path/fname */
						,cfg.data_dir, msg->idx.from,tp);
				long length=(long)flength(fpath);
				if(length<1)
					bprintf(text[FileDoesNotExist], tp);
				else if(!(useron.exempt&FLAG('T')) && cur_cps && !SYSOP
					&& length/(long)cur_cps>(time_t)timeleft)
					bputs(text[NotEnoughTimeToDl]);
				else {
					char 	tmp[512];
					int		i;
					SAFEPRINTF2(str, text[DownloadAttachedFileQ]
						,getfname(fpath),ultoac(length,tmp));
					if(length>0L && text[DownloadAttachedFileQ][0] && yesno(str)) {
						{	/* Remote User */
							xfer_prot_menu(XFER_DOWNLOAD);
							mnemonics(text[ProtocolOrQuit]);
							strcpy(str,"Q");
							for(i=0;i<cfg.total_prots;i++)
								if(cfg.prot[i]->dlcmd[0]
									&& chk_ar(cfg.prot[i]->ar,&useron,&client)) {
									sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
									SAFECAT(str,tmp);
								}
							ch=(char)getkeys(str,0);
							for(i=0;i<cfg.total_prots;i++)
								if(cfg.prot[i]->dlcmd[0] && ch==cfg.prot[i]->mnemonic
									&& chk_ar(cfg.prot[i]->ar,&useron,&client))
									break;
							if(i<cfg.total_prots) {
								int error = protocol(cfg.prot[i], XFER_DOWNLOAD, fpath, nulstr, false);
								if(checkprotresult(cfg.prot[i],error,fpath)) {
									if(del)
										(void)remove(fpath);
									logon_dlb+=length;	/* Update stats */
									logon_dls++;
									useron.dls=(ushort)adjustuserrec(&cfg,useron.number
										,U_DLS,5,1);
									useron.dlb=adjustuserrec(&cfg,useron.number
										,U_DLB,10,length);
									bprintf(text[FileNBytesSent]
										,getfname(fpath),ultoac(length,tmp));
									SAFEPRINTF(str
										,"downloaded attached file: %s"
										,getfname(fpath));
									logline("D-",str);
								}
								autohangup();
							}
						}
					}
				}
			}
			if(!p)
				break;
			tp=p+1;
			while(*tp==' ') tp++;
		}
		// Remove the *.in directory, only if its empty
		SAFEPRINTF2(fpath, "%sfile/%04u.in", cfg.data_dir, msg->idx.to);
		rmdir(fpath);
	}
}

/****************************************************************************/
/* Writes message header and text data to a text file						*/
/****************************************************************************/
bool sbbs_t::msgtotxt(smb_t* smb, smbmsg_t* msg, const char *fname, bool header, ulong gettxt_mode)
{
	char	*buf;
	char	tmp[128];
	int 	i;
	FILE	*out;

	if((out=fnopen(&i,fname,O_WRONLY|O_CREAT|O_APPEND))==NULL) {
		errormsg(WHERE,ERR_OPEN,fname,0);
		return false;
	}
	if(header) {
		fprintf(out,"\r\n");
		fprintf(out,"Subj : %s\r\n",msg->subj);
		fprintf(out,"To   : %s",msg->to);
		if(msg->to_net.addr)
			fprintf(out," (%s)",smb_netaddrstr(&msg->to_net,tmp));
		if(msg->to_ext)
			fprintf(out," #%s",msg->to_ext);
		fprintf(out,"\r\nFrom : %s",msg->from);
		if(msg->from_ext && !(msg->hdr.attr&MSG_ANONYMOUS))
			fprintf(out," #%s",msg->from_ext);
		if(msg->from_net.addr)
			fprintf(out," (%s)",smb_netaddrstr(&msg->from_net,tmp));
		fprintf(out,"\r\nDate : %.24s %s"
			,timestr(msg->hdr.when_written.time)
			,smb_zonestr(msg->hdr.when_written.zone,NULL));
		fprintf(out,"\r\n\r\n");
	}

	bool result = false;
	buf=smb_getmsgtxt(smb, msg, gettxt_mode);
	if(buf!=NULL) {
		strip_invalid_attr(buf);
		fputs(buf,out);
		smb_freemsgtxt(buf);
		result = true;
	} else if(smb_getmsgdatlen(msg)>2)
		errormsg(WHERE,ERR_READ,smb->file,smb_getmsgdatlen(msg));
	fclose(out);
	return result;
}

/****************************************************************************/
/* Returns message number posted at or after time							*/
/****************************************************************************/
ulong sbbs_t::getmsgnum(uint subnum, time_t t)
{
    int			i;
	smb_t		smb;
	idxrec_t	idx;

	if(!t)
		return(0);

	ZERO_VAR(smb);
	SAFEPRINTF2(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open_index(&smb)) != SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return 0;
	}
	int result = smb_getmsgidx_by_time(&smb, &idx, t);
	smb_close(&smb);
	if(result >= SMB_SUCCESS)
		return idx.number - 1;
	return ~0;
}

/****************************************************************************/
/* Returns the time of the message number pointed to by 'ptr'               */
/****************************************************************************/
time_t sbbs_t::getmsgtime(uint subnum, ulong ptr)
{
	int 		i;
	smb_t		smb;
	smbmsg_t	msg;
	idxrec_t	lastidx;

	ZERO_VAR(smb);
	SAFEPRINTF2(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0);
	}
	if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
		smb_close(&smb);
		return(0);
	}
	msg.idx_offset=0;
	msg.hdr.number=0;
	if(smb_getmsgidx(&smb,&msg)) {				/* Get first message index */
		smb_close(&smb);
		return(0);
	}
	if(!ptr || msg.idx.number>=ptr) {			/* ptr is before first message */
		smb_close(&smb);
		return(msg.idx.time);   				/* so return time of first msg */
	}

	if(smb_getlastidx(&smb,&lastidx)) { 			 /* Get last message index */
		smb_close(&smb);
		return(0);
	}
	if(lastidx.number<ptr) {					/* ptr is after last message */
		smb_close(&smb);
		return(lastidx.time);	 				/* so return time of last msg */
	}

	msg.idx.time=0;
	msg.hdr.number=ptr;
	if(!smb_getmsgidx(&smb,&msg)) {
		smb_close(&smb);
		return(msg.idx.time);
	}

	if(ptr-msg.idx.number < lastidx.number-ptr) {
		msg.idx_offset=0;
		msg.idx.number=0;
		while(msg.idx.number<ptr) {
			msg.hdr.number=0;
			if(smb_getmsgidx(&smb,&msg) || msg.idx.number>=ptr)
				break;
			msg.idx_offset++;
		}
		smb_close(&smb);
		return(msg.idx.time);
	}

	ptr--;
	while(ptr) {
		msg.hdr.number=ptr;
		if(!smb_getmsgidx(&smb,&msg))
			break;
		ptr--;
	}
	smb_close(&smb);
	return(msg.idx.time);
}


/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* number of the last message in the sub (0) if no messages.				*/
/****************************************************************************/
ulong sbbs_t::getlastmsg(uint subnum, uint32_t *ptr, time_t *t)
{
	int 		i;
	ulong		total;
	smb_t		smb;
	idxrec_t	idx;

	if(ptr)
		(*ptr)=0;
	if(t)
		(*t)=0;
	if(subnum>=cfg.total_subs)
		return(0);

	ZERO_VAR(smb);
	SAFEPRINTF2(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0);
	}

	if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
		smb_close(&smb);
		return(0);
	}
	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return(0);
	}
	if((i=smb_getlastidx(&smb,&idx))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
		return(0);
	}
	if(cfg.sub[subnum]->misc & SUB_NOVOTING)
		total = (long)filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);
	else
		total = smb_msg_count(&smb, (1 << SMB_MSG_TYPE_NORMAL) | (1 << SMB_MSG_TYPE_POLL));
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	if(ptr)
		(*ptr)=idx.number;
	if(t)
		(*t)=idx.time;
	return(total);
}

