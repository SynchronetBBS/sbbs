/* Synchronet message base (SMB) hash-related functions */

/* $Id: smbhash.c,v 1.36 2019/04/11 01:00:30 rswindell Exp $ */
// vi: tabstop=4

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

#include <time.h>		/* time()	*/
#include <string.h>		/* strdup() */
#include <ctype.h>		/* isspace()*/
#include "smblib.h"
#include "md5.h"
#include "crc16.h"
#include "crc32.h"
#include "genwrap.h"

/* If return value is SMB_ERR_NOT_FOUND, hash file is left open */
int SMBCALL smb_findhash(smb_t* smb, hash_t** compare, hash_t* found_hash, 
						 long source_mask, BOOL mark)
{
	int		retval;
	BOOL	found=FALSE;
	size_t	c,count;
	hash_t	hash;

	if(found_hash!=NULL)
		memset(found_hash,0,sizeof(hash_t));

	if((retval=smb_open_hash(smb))!=SMB_SUCCESS)
		return(retval);

	COUNT_LIST_ITEMS(compare, count);

	if(count && source_mask!=SMB_HASH_SOURCE_NONE) {

		rewind(smb->hash_fp);
		clearerr(smb->hash_fp);
		while(!feof(smb->hash_fp)) {
			if(smb_fread(smb,&hash,sizeof(hash),smb->hash_fp)!=sizeof(hash))
				break;

			if(hash.flags==0)
				continue;		/* invalid hash record (!?) */

			if((source_mask&(1<<hash.source))==0)	/* not checking this source type */
				continue;

			for(c=0;compare[c]!=NULL;c++) {

				if(compare[c]->source!=hash.source)
					continue;	/* wrong source */
				if(compare[c]->length!=hash.length)
					continue;	/* wrong source length */
				if(compare[c]->flags&SMB_HASH_MARKED)
					continue;	/* already marked */
				if((compare[c]->flags&SMB_HASH_PROC_COMP_MASK)!=(hash.flags&SMB_HASH_PROC_COMP_MASK))
					continue;	/* wrong pre-process flags */
				if((compare[c]->flags&hash.flags&SMB_HASH_MASK)==0)	
					continue;	/* no matching hashes */
				if((compare[c]->flags&hash.flags&SMB_HASH_CRC16)
					&& compare[c]->crc16!=hash.crc16)
					continue;	/* wrong crc-16 */
				if((compare[c]->flags&hash.flags&SMB_HASH_CRC32)
					&& compare[c]->crc32!=hash.crc32)
					continue;	/* wrong crc-32 */
				if((compare[c]->flags&hash.flags&SMB_HASH_MD5)
					&& memcmp(compare[c]->md5,hash.md5,sizeof(hash.md5)))
					continue;	/* wrong MD5 */
				
				/* successful match! */
				break;	/* can't match more than one, so stop comparing */
			}

			if(compare[c]==NULL)
				continue;	/* no match */

			found=TRUE;

			if(found_hash!=NULL)
				memcpy(found_hash,&hash,sizeof(hash));

			if(!mark)
				break;

			compare[c]->flags|=SMB_HASH_MARKED;
		}
		if(found) {
			smb_close_hash(smb);
			return(SMB_SUCCESS);
		}
	}

	/* hash file left open */
	return(SMB_ERR_NOT_FOUND);
}

int SMBCALL smb_addhashes(smb_t* smb, hash_t** hashes, BOOL skip_marked)
{
	int		retval;
	size_t	h;

	COUNT_LIST_ITEMS(hashes, h);
	if(!h) { /* nothing to add */
		smb_close_hash(smb);
		return(SMB_SUCCESS);
	}

	if((retval=smb_open_hash(smb))!=SMB_SUCCESS)
		return(retval);

	fseek(smb->hash_fp,0,SEEK_END);

	for(h=0;hashes[h]!=NULL;h++) {

		/* skip hashes marked by smb_findhash() */
		if(skip_marked && hashes[h]->flags&SMB_HASH_MARKED)	
			continue;	
	
		/* can't think of any reason to strip SMB_HASH_MARKED flag right now */
		if(smb_fwrite(smb,hashes[h],sizeof(hash_t),smb->hash_fp)!=sizeof(hash_t)) {
			retval=SMB_ERR_WRITE;
			break;
		}
	}

	smb_close_hash(smb);

	return(retval);
}

static char* strip_chars(uchar* dst, const uchar* src, uchar* set)
{
	while(*src) {
		if(strchr((char *)set,*src)==NULL)
			*(dst++)=*src;
		src++;
	}
	*dst=0;

	return((char *)dst);
}

static char* strip_ctrla(uchar* dst, const uchar* src)
{
	while(*src) {
		if(*src==CTRL_A) {
			src++;
			if(*src)
				src++;
		}
		else
			*(dst++)=*(src++);
	}
	*dst=0;

	return((char *)dst);
}

/* Allocates and calculates hashes of data (based on flags)					*/
/* Returns NULL on failure													*/
hash_t* SMBCALL smb_hash(ulong msgnum, uint32_t t, unsigned source, unsigned flags
						 ,const void* data, size_t length)
{
	hash_t*	hash;

	if(length==0)		/* Don't hash 0-length sources (e.g. empty/blank message bodies) */
		return(NULL);

	if((hash=(hash_t*)malloc(sizeof(hash_t)))==NULL)
		return(NULL);

	memset(hash,0,sizeof(hash_t));
	hash->number=msgnum;
	hash->time=t;
	hash->length=length;
	hash->source=source;
	hash->flags=flags;
	if(flags&SMB_HASH_CRC16)
		hash->crc16=crc16((char*)data,length);
	if(flags&SMB_HASH_CRC32)
		hash->crc32=crc32((char*)data,length);
	if(flags&SMB_HASH_MD5)
		MD5_calc(hash->md5,data,length);

	return(hash);
}

/* Allocates and calculates hashes of data (based on flags)					*/
/* Supports string hash "pre-processing" (e.g. lowercase, strip whitespace)	*/
/* Returns NULL on failure													*/
hash_t* SMBCALL smb_hashstr(ulong msgnum, uint32_t t, unsigned source, unsigned flags
							,const char* str)
{
	char*	p=NULL;
	hash_t*	hash;

	if(flags&SMB_HASH_PROC_MASK) {	/* string pre-processing */
		if((p=strdup(str))==NULL)
			return(NULL);
		if(flags&SMB_HASH_STRIP_CTRL_A)
			strip_ctrla((uchar *)p,(uchar *)p);
		if(flags&SMB_HASH_STRIP_WSP)
			strip_chars((uchar *)p,(uchar *)p,(uchar *)" \t\r\n");
		if(flags&SMB_HASH_LOWERCASE)
			strlwr(p);
	}

	if(p!=NULL) {
		hash=smb_hash(msgnum, t, source, flags, p, strlen(p));
		free(p);
	} else
		hash=smb_hash(msgnum, t, source, flags, str, strlen(str));

	return(hash);
}

/* Allocates and calculates all hashes for a single message					*/
/* Returns NULL on failure													*/
hash_t** SMBCALL smb_msghashes(smbmsg_t* msg, const uchar* body, long source_mask)
{
	size_t		h=0;
	uchar		flags=SMB_HASH_CRC16|SMB_HASH_CRC32|SMB_HASH_MD5;
	hash_t**	hashes;	/* This is a NULL-terminated list of hashes */
	hash_t*		hash;
	time_t		t=time(NULL);

	if((hashes=(hash_t**)malloc(sizeof(hash_t*)*(SMB_HASH_SOURCE_TYPES+1)))==NULL)
		return(NULL);

	memset(hashes, 0, sizeof(hash_t*)*(SMB_HASH_SOURCE_TYPES+1));

	if(msg->id!=NULL && (source_mask&(1<<SMB_HASH_SOURCE_MSG_ID)) &&
		(hash=smb_hashstr(msg->hdr.number, (uint32_t)t, SMB_HASH_SOURCE_MSG_ID, flags, msg->id))!=NULL)
		hashes[h++]=hash;

	if(msg->ftn_msgid!=NULL	&& (source_mask&(1<<SMB_HASH_SOURCE_FTN_ID)) &&
		(hash=smb_hashstr(msg->hdr.number, (uint32_t)t, SMB_HASH_SOURCE_FTN_ID, flags, msg->ftn_msgid))!=NULL)
		hashes[h++]=hash;

	if(body!=NULL && (source_mask&(1<<SMB_HASH_SOURCE_BODY)) &&
		(hash=smb_hashstr(msg->hdr.number, (uint32_t)t, SMB_HASH_SOURCE_BODY, flags|SMB_HASH_STRIP_WSP|SMB_HASH_STRIP_CTRL_A, (const char *)body))!=NULL)
		hashes[h++]=hash;

	if(msg->subj!=NULL && (source_mask&(1<<SMB_HASH_SOURCE_SUBJECT))) {
		char*	p=msg->subj;
		while(*p) {
			char* tp=strchr(p,':');
			char* sp=strchr(p,' ');
			if(tp!=NULL && (sp==NULL || tp<sp)) {
				p=tp+1;
				SKIP_WHITESPACE(p);
				continue;
			}
			break;
		}
		if((hash=smb_hashstr(msg->hdr.number, (uint32_t)t, SMB_HASH_SOURCE_SUBJECT, flags, p))!=NULL)
			hashes[h++]=hash;
	}

	return(hashes);
}

void SMBCALL smb_freehashes(hash_t** hashes)
{
	size_t		n;

	FREE_LIST(hashes,n);
}

/* Calculates and stores the hashes for a single message					*/
int SMBCALL smb_hashmsg(smb_t* smb, smbmsg_t* msg, const uchar* text, BOOL update)
{
	size_t		n;
	int			retval=SMB_SUCCESS;
	hash_t		found;
	hash_t**	hashes;	/* This is a NULL-terminated list of hashes */

	if(smb->status.attr&(SMB_EMAIL|SMB_NOHASH))
		return(SMB_SUCCESS);

	hashes=smb_msghashes(msg,text,SMB_HASH_SOURCE_DUPE);

	if(smb_findhash(smb, hashes, &found, SMB_HASH_SOURCE_DUPE, update)==SMB_SUCCESS && !update) {
		retval=SMB_DUPE_MSG;
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s duplicate %s: %s found in message #%lu", __FUNCTION__
			,smb_hashsourcetype(found.source)
			,smb_hashsource(msg,found.source)
			,(ulong)found.number);
	} else
		if((retval=smb_addhashes(smb,hashes,/* skip_marked? */TRUE))==SMB_SUCCESS)
			msg->flags|=MSG_FLAG_HASHED;

	FREE_LIST(hashes,n);

	return(retval);
}

/* length=0 specifies ASCIIZ data											*/
int SMBCALL smb_getmsgidx_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length)
{
	int			retval;
	size_t		n=2;
	hash_t**	hashes;
	hash_t		found;

	if((hashes=(hash_t**)calloc(n, sizeof(hash_t*)))==NULL)
		return(SMB_ERR_MEM);

	if(length==0)
		hashes[0]=smb_hashstr(0,0,source,flags,data);
	else
		hashes[0]=smb_hash(0,0,source,flags,data,length);
	if(hashes[0]==NULL) {
		FREE_LIST(hashes,n);
		return(SMB_ERR_MEM);
	}

	if((retval=smb_findhash(smb, hashes, &found, 1<<source, FALSE))==SMB_SUCCESS) {
		if(found.number==0)
			retval=SMB_FAILURE;	/* use better error value here? */
		else {
			msg->hdr.number=found.number;
			retval=smb_getmsgidx(smb, msg);
		}
	}

	FREE_LIST(hashes,n);

	return(retval);
}

int SMBCALL smb_getmsghdr_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length)
{
	int retval;

	if((retval=smb_getmsgidx_by_hash(smb,msg,source,flags,data,length))!=SMB_SUCCESS)
		return(retval);

	if((retval=smb_lockmsghdr(smb,msg))!=SMB_SUCCESS)
		return(retval);

	retval=smb_getmsghdr(smb,msg);

	smb_unlockmsghdr(smb,msg); 

	return(retval);
}

uint16_t SMBCALL smb_subject_crc(const char* subj)
{
	char*	str;
	uint16_t	crc;

	if(subj==NULL)
		return(0);

	while(!strnicmp(subj,"RE:",3)) {
		subj+=3;
		while(*subj==' ')
			subj++; 
	}

	if((str=strdup(subj))==NULL)
		return(0xffff);

	strlwr(str);
	crc=crc16(str,0	/* auto-length */);
	free(str);

	return(crc);
}

uint16_t SMBCALL smb_name_crc(const char* name)
{
	char*	str;
	uint16_t	crc;

	if(name==NULL)
		return(0);

	if((str=strdup(name))==NULL)
		return(0xffff);

	strlwr(str);
	crc=crc16(str,0	/* auto-length */);
	free(str);

	return(crc);
}
