/* Synchronet message base (SMB) high-level "add message" function */

/* $Id: smbadd.c,v 1.46 2020/04/12 06:09:33 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include <time.h>
#include <string.h>	/* strlen(), memset() */
#include "smblib.h"
#include "genwrap.h"
#include "crc32.h"
#include "lzh.h"

/****************************************************************************/
/****************************************************************************/
int SMBCALL smb_addmsg(smb_t* smb, smbmsg_t* msg, int storage, long dupechk_hashes
					   ,uint16_t xlat, const uchar* body, const uchar* tail)
{
	uchar*		lzhbuf=NULL;
	long		lzhlen;
	int			retval;
	size_t		n;
	size_t		l;
	off_t		length;
	size_t		taillen=0;
	size_t		bodylen=0;
	size_t		chklen=0;
	long		offset;
	uint32_t	crc=0xffffffff;
	hash_t		found;
	hash_t**	hashes=NULL;	/* This is a NULL-terminated list of hashes */
	smbmsg_t	remsg;

	if(!SMB_IS_OPEN(smb)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}

	if(filelength(fileno(smb->shd_fp))<1) {	 /* Create it if it doesn't exist */
		/* smb->status.max_crcs, max_msgs, max_age, and attr should be pre-initialized */
		if((retval=smb_create(smb))!=SMB_SUCCESS) 
			return(retval);
	}

	if(!smb->locked && smb_locksmbhdr(smb)!=SMB_SUCCESS)
		return SMB_ERR_LOCK;

	msg->hdr.total_dfields = 0;

	/* try */
	do {

		if((retval = smb_getstatus(smb)) != SMB_SUCCESS)
			break;

		msg->hdr.number=smb->status.last_msg+1;
		if(!(smb->status.attr&(SMB_EMAIL|SMB_NOHASH))) {

			hashes=smb_msghashes(msg,body,SMB_HASH_SOURCE_DUPE);

			if(smb_findhash(smb, hashes, &found, dupechk_hashes, /* mark? */FALSE)==SMB_SUCCESS) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s duplicate %s: %s found in message #%lu"
					,__FUNCTION__
					,smb_hashsourcetype(found.source)
					,smb_hashsource(msg,found.source)
					,(ulong)found.number);
				retval=SMB_DUPE_MSG;
				break;
			}
		}

		if(tail!=NULL && (taillen=strlen((const char *)tail))>0)
			taillen+=sizeof(xlat);	/* xlat string terminator */

		if(body!=NULL && (bodylen=strlen((const char *)body))>0) {

			/* Remove white-space from end of message text */
			chklen=bodylen;
			while(chklen && body[chklen-1]<=' ')
				chklen--;

			/* Calculate CRC-32 of message text (before encoding, if any) */
			if(smb->status.max_crcs && dupechk_hashes&(1<<SMB_HASH_SOURCE_BODY)) {
				for(l=0;l<chklen;l++)
					crc=ucrc32(body[l],crc); 
				crc=~crc;

				/* Add CRC to CRC history (and check for duplicates) */
				if((retval=smb_addcrc(smb,crc))!=SMB_SUCCESS)
					break;
			}

			bodylen+=sizeof(xlat);	/* xlat string terminator */

			/* LZH compress? */
			if(xlat==XLAT_LZH && bodylen+taillen>=SDT_BLOCK_LEN
				&& (lzhbuf=(uchar *)malloc(bodylen*2))!=NULL) {
				lzhlen=lzh_encode((uchar*)body,bodylen-sizeof(xlat),lzhbuf);
				if(lzhlen>1
					&& smb_datblocks(lzhlen+(sizeof(xlat)*2)+taillen) 
						< smb_datblocks(bodylen+taillen)) {
					bodylen=lzhlen+(sizeof(xlat)*2); 	/* Compressible */
					body=lzhbuf; 
				} else
					xlat=XLAT_NONE;
			} else
				xlat=XLAT_NONE;
		}

		length=bodylen+taillen;

		if(length) {

			if(length >= 0x80000000 || length < 0) {
				sprintf(smb->last_error,"%s message length: 0x%"PRIXMAX, __FUNCTION__, (intmax_t)length);
				retval=SMB_ERR_DAT_LEN;
				break;
			}

			/* Allocate Data Blocks */
			if(smb->status.attr&SMB_HYPERALLOC) {
				offset=smb_hallocdat(smb);
				storage=SMB_HYPERALLOC; 
			} else {
				if((retval=smb_open_da(smb))!=SMB_SUCCESS)
					break;
				if(storage==SMB_FASTALLOC)
					offset=smb_fallocdat(smb,length,1);
				else { /* SMB_SELFPACK */
					offset=smb_allocdat(smb,length,1);
					storage=SMB_SELFPACK; 
				}
				smb_close_da(smb); 
			}

			if(offset<0) {
				retval=offset;
				break;
			}
			msg->hdr.offset=offset;

			smb_fseek(smb->sdt_fp,offset,SEEK_SET);

			if(bodylen) {
				if((retval=smb_dfield(msg,TEXT_BODY,bodylen))!=SMB_SUCCESS)
					break;

				if(xlat!=XLAT_NONE) {	/* e.g. XLAT_LZH */
					if(smb_fwrite(smb,&xlat,sizeof(xlat),smb->sdt_fp)!=sizeof(xlat)) {
						safe_snprintf(smb->last_error,sizeof(smb->last_error)
							,"%s writing body xlat string"
							,__FUNCTION__);
						retval=SMB_ERR_WRITE;
						break;
					}
					bodylen-=sizeof(xlat);
				}
				xlat=XLAT_NONE;	/* xlat string terminator */
				if(smb_fwrite(smb,&xlat,sizeof(xlat),smb->sdt_fp)!=sizeof(xlat)) {
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"%s writing body xlat terminator"
						,__FUNCTION__);
					retval=SMB_ERR_WRITE;
					break;
				}
				bodylen-=sizeof(xlat);

				if(smb_fwrite(smb,body,bodylen,smb->sdt_fp)!=bodylen) {
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"%s writing body (%ld bytes)"
						,__FUNCTION__
						,(long)bodylen);
					retval=SMB_ERR_WRITE;
					break;
				}
			}

			if(taillen) {
				if((retval=smb_dfield(msg,TEXT_TAIL,taillen))!=SMB_SUCCESS)
					break;

				xlat=XLAT_NONE;	/* xlat string terminator */
				if(smb_fwrite(smb,&xlat,sizeof(xlat),smb->sdt_fp)!=sizeof(xlat)) {
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"%s writing tail xlat terminator"
						,__FUNCTION__);
					retval=SMB_ERR_WRITE;
					break;
				}

				if(smb_fwrite(smb,tail,taillen-sizeof(xlat),smb->sdt_fp)!=taillen-sizeof(xlat)) {
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"%s writing tail (%ld bytes)"
						,__FUNCTION__
						,(long)(taillen-sizeof(xlat)));
					retval=SMB_ERR_WRITE;
					break;
				}
			}

			for(l=length;l%SDT_BLOCK_LEN;l++) {
				if(smb_fputc(0,smb->sdt_fp)!=0)
					break;
			}
			if(l%SDT_BLOCK_LEN) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s writing data padding"
					,__FUNCTION__);
				retval=SMB_ERR_WRITE;
				break;
			}

			fflush(smb->sdt_fp);
		}

		if(msg->hdr.when_imported.time==0) {
			msg->hdr.when_imported.time=(uint32_t)time(NULL);
			msg->hdr.when_imported.zone=0;	/* how do we detect system TZ? */
		}
		if(msg->hdr.when_written.time==0)	/* Uninitialized */
			msg->hdr.when_written = msg->hdr.when_imported;

		/* Look-up thread_back if RFC822 Reply-ID was specified */
		if(msg->hdr.thread_back==0 && msg->reply_id!=NULL) {
			if(smb_getmsgidx_by_msgid(smb,&remsg,msg->reply_id)==SMB_SUCCESS)
				msg->hdr.thread_back=remsg.idx.number;	/* needed for threading backward */
		}

		/* Look-up thread_back if FTN REPLY was specified */
		if(msg->hdr.thread_back==0 && msg->ftn_reply!=NULL) {
			if(smb_getmsgidx_by_ftnid(smb,&remsg,msg->ftn_reply)==SMB_SUCCESS)
				msg->hdr.thread_back=remsg.idx.number;	/* needed for threading backward */
		}

		/* Auto-thread linkage */
		if(msg->hdr.thread_back) {
			memset(&remsg,0,sizeof(remsg));
			remsg.hdr.number=msg->hdr.thread_back;
			if(smb_getmsgidx(smb, &remsg)==SMB_SUCCESS	/* valid thread back */
				&& smb_lockmsghdr(smb,&remsg)==SMB_SUCCESS) {

				do { /* try */

					if(smb_getmsghdr(smb, &remsg)!=SMB_SUCCESS)
						break;

					if(msg->hdr.thread_id==0) {	/* no thread_id pre-specified */
						if((msg->hdr.thread_id=remsg.hdr.thread_id) == 0)
							msg->hdr.thread_id=remsg.hdr.number;
					}

					/* Add RFC-822 Reply-ID if original message has RFC Message-ID */
					if(msg->reply_id==NULL && remsg.id!=NULL
						&& smb_hfield_str(msg,RFC822REPLYID,remsg.id)!=SMB_SUCCESS)
						break;

					/* Add FidoNet Reply if original message has FidoNet MSGID */
					if(msg->ftn_reply==NULL && remsg.ftn_msgid!=NULL
						&& smb_hfield_str(msg,FIDOREPLYID,remsg.ftn_msgid)!=SMB_SUCCESS)
						break;

					smb_updatethread(smb, &remsg, msg->hdr.number);

				} while(0); /* finally */

				smb_unlockmsghdr(smb, &remsg);
				smb_freemsgmem(&remsg);
			}
		}

		if(!(smb->status.attr&(SMB_EMAIL|SMB_NOHASH))
			&& smb_addhashes(smb,hashes,/* skip_marked? */FALSE)==SMB_SUCCESS)
			msg->flags|=MSG_FLAG_HASHED;
		if(msg->to==NULL)	/* no recipient, don't add header (required for bulkmail) */
			break;

		retval=smb_addmsghdr(smb,msg,storage); /* calls smb_unlocksmbhdr() */

	} while(0);
	/* finally */

	if(retval!=SMB_SUCCESS)
		smb_freemsg_dfields(smb,msg,1);

	if(smb->locked)
		smb_unlocksmbhdr(smb);
	FREE_AND_NULL(lzhbuf);
	FREE_LIST(hashes,n);

	return(retval);
}

int SMBCALL smb_addvote(smb_t* smb, smbmsg_t* msg, int storage)
{
	int			retval;

	if(!SMB_IS_OPEN(smb)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}

	if(filelength(fileno(smb->shd_fp)) < 1) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s header file empty", __FUNCTION__);
		return SMB_ERR_NOT_FOUND;
	}

	if(!(msg->hdr.attr&MSG_VOTE) || msg->hdr.attr&MSG_POLL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s incorrect attr: %04hX", __FUNCTION__, msg->hdr.attr);
		return SMB_ERR_HDR_ATTR;
	}

	if(msg->hdr.thread_back == 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s thread_back field missing", __FUNCTION__);
		return SMB_ERR_HDR_FIELD;
	}

	msg->hdr.type = SMB_MSG_TYPE_BALLOT;

	if(msg->hdr.when_imported.time == 0) {
		msg->hdr.when_imported.time = (uint32_t)time(NULL);
		msg->hdr.when_imported.zone = 0;
	}
	if(msg->hdr.when_written.time == 0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	retval = smb_addmsghdr(smb, msg, storage);

	return retval;
}

int SMBCALL smb_addpoll(smb_t* smb, smbmsg_t* msg, int storage)
{
	int			retval;

	if(!SMB_IS_OPEN(smb)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}

	if(msg->subj == NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error), "%s subject field missing", __FUNCTION__);
		return SMB_ERR_HDR_FIELD;
	}

	if(smb_get_hfield(msg, SMB_POLL_ANSWER, NULL) == NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error), "%s poll answers missing", __FUNCTION__);
		return SMB_ERR_HDR_FIELD;
	}

	if(filelength(fileno(smb->shd_fp)) < 1) {	 /* Create it if it doesn't exist */
		/* smb->status.max_crcs, max_msgs, max_age, and attr should be pre-initialized */
		if((retval=smb_create(smb)) != SMB_SUCCESS) 
			return retval;
	}

	msg->hdr.attr |= MSG_POLL;
	msg->hdr.type = SMB_MSG_TYPE_POLL;

	if(msg->hdr.when_imported.time == 0) {
		msg->hdr.when_imported.time = (uint32_t)time(NULL);
		msg->hdr.when_imported.zone = 0;
	}
	if(msg->hdr.when_written.time == 0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	retval = smb_addmsghdr(smb, msg, storage);

	return retval;
}

int SMBCALL smb_addpollclosure(smb_t* smb, smbmsg_t* msg, int storage)
{
	smbmsg_t	remsg;
	int			retval;

	if(!SMB_IS_OPEN(smb)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}

	if(filelength(fileno(smb->shd_fp)) < 1) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s header file empty", __FUNCTION__);
		return SMB_ERR_NOT_FOUND;
	}

	if(msg->hdr.thread_back == 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s thread_back field missing", __FUNCTION__);
		return SMB_ERR_HDR_FIELD;
	}

	memset(&remsg, 0, sizeof(remsg));
	remsg.hdr.number = msg->hdr.thread_back;
	if((retval = smb_getmsgidx(smb, &remsg)) != SMB_SUCCESS)
		return retval;
	if((retval = smb_lockmsghdr(smb,&remsg)) != SMB_SUCCESS)
		return retval;
	if((retval = smb_getmsghdr(smb, &remsg)) != SMB_SUCCESS) {
		smb_unlockmsghdr(smb, &remsg);
		return retval;
	}

	if(remsg.hdr.auxattr&POLL_CLOSED) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s closed poll", __FUNCTION__);
		smb_freemsgmem(&remsg);
		smb_unlockmsghdr(smb, &remsg);
		return SMB_CLOSED;
	}

	if(!smb_msg_is_from(&remsg, msg->from, msg->from_net.type, msg->from_net.addr)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s wrong pollster", __FUNCTION__);
		smb_freemsgmem(&remsg);
		smb_unlockmsghdr(smb, &remsg);
		return SMB_UNAUTHORIZED;
	}

	remsg.hdr.auxattr |= POLL_CLOSED;
	retval = smb_putmsghdr(smb, &remsg);
	smb_freemsgmem(&remsg);
	smb_unlockmsghdr(smb, &remsg);
	if(retval != SMB_SUCCESS)
		return retval;

	msg->hdr.attr |= MSG_POLL_CLOSURE;
	msg->hdr.type = SMB_MSG_TYPE_POLL_CLOSURE;

	if(msg->hdr.when_imported.time == 0) {
		msg->hdr.when_imported.time = (uint32_t)time(NULL);
		msg->hdr.when_imported.zone = 0;
	}
	if(msg->hdr.when_written.time == 0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	retval = smb_addmsghdr(smb, msg, storage);

	return retval;
}
