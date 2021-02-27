/* Synchronet message base (SMB) alloc/free routines */
// vi: tabstop=4

/* $Id: smballoc.c,v 1.14 2019/04/11 01:00:29 rswindell Exp $ */

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

#include "smblib.h"
#include "genwrap.h"

/****************************************************************************/
/* Finds unused space in data file based on block allocation table and		*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of data (in bytes, not blocks)				*/
/* Assumes smb_open_da() has been called									*/
/* smb_close_da() should be called after									*/
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_allocdat(smb_t* smb, ulong length, uint16_t refs)
{
    uint16_t  i;
	ulong	j,l,blocks,offset=0L;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_datblocks(length);
	j=0;	/* j is consecutive unused block counter */
	fflush(smb->sda_fp);
	rewind(smb->sda_fp);
	while(!feof(smb->sda_fp) && (long)offset>=0) {
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i))
			break;
		offset+=SDT_BLOCK_LEN;
		if(!i) j++;
		else   j=0;
		if(j==blocks) {
			offset-=(blocks*SDT_BLOCK_LEN);
			break; 
		} 
	}
	if((long)offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s invalid data offset: %lu", __FUNCTION__, offset);
		return(SMB_ERR_DAT_OFFSET);
	}
	clearerr(smb->sda_fp);
	if(fseek(smb->sda_fp,(offset/SDT_BLOCK_LEN)*sizeof(refs),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s seeking to: %ld", __FUNCTION__
			,(offset/SDT_BLOCK_LEN)*sizeof(refs));
		return(SMB_ERR_SEEK);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&refs,sizeof(refs),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation bytes at offset %ld", __FUNCTION__
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(refs));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sda_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_fallocdat(smb_t* smb, ulong length, uint16_t refs)
{
	ulong	l,blocks,offset;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sda_fp);
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	if(fseek(smb->sda_fp,0L,SEEK_END)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s rewinding", __FUNCTION__);
		return(SMB_ERR_SEEK);
	}
	offset=(ftell(smb->sda_fp)/sizeof(refs))*SDT_BLOCK_LEN;
	if((long)offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid data offset: %lu", __FUNCTION__, offset);
		return(SMB_ERR_DAT_OFFSET);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&refs,sizeof(refs),1,smb->sda_fp))
			break;
	fflush(smb->sda_fp);
	if(l<blocks) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s writing allocation bytes", __FUNCTION__);
		return(SMB_ERR_WRITE);
	}
	return(offset);
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/* Always unlocks the SMB header (when not hyper-alloc)						*/
/****************************************************************************/
int SMBCALL smb_freemsgdat(smb_t* smb, ulong offset, ulong length, uint16_t refs)
{
	BOOL	da_opened=FALSE;
	int		retval=SMB_SUCCESS;
	uint16_t	i;
	long	l,blocks;
	ulong	sda_offset;
	off_t	flen;

	if(smb->status.attr&SMB_HYPERALLOC)	/* do nothing */
		return(SMB_SUCCESS);

	blocks = smb_datblocks(length);

	if(blocks < 1)
		return SMB_SUCCESS;	// Nothing to do

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=SMB_SUCCESS)
			return(i);
		da_opened=TRUE;
	}
	flen = filelength(fileno(smb->sda_fp));
	if(flen < sizeof(uint16_t))
		return 0;	// Nothing to do
	
	if(!smb->locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return SMB_ERR_LOCK;

	clearerr(smb->sda_fp);
	// Free from the last block first
	for(l=blocks-1; l >= 0; l--) {
		sda_offset=((offset/SDT_BLOCK_LEN)+l)*sizeof(i);
		if(fseek(smb->sda_fp,sda_offset,SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' seeking to %lu (0x%lX) of allocation file", __FUNCTION__
				,get_errno(),STRERROR(get_errno())
				,sda_offset,sda_offset);
			retval=SMB_ERR_SEEK;
			break;
		}
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading allocation record at offset %ld", __FUNCTION__
				,sda_offset);
			retval=SMB_ERR_READ;
			break;
		}
		if(refs==SMB_ALL_REFS || refs>i)
			i=0;			/* don't want to go negative */
		else
			i-=refs;

		// Completely free? and at end of SDA? Just truncate record from end of file
		if(i == 0 && ftell(smb->sda_fp) == flen) {
			if(chsize(fileno(smb->sda_fp), sda_offset) == 0) {
				flen = sda_offset;
				continue;
			}
		}
		
		if(fseek(smb->sda_fp,-(int)sizeof(i),SEEK_CUR)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' seeking backwards 2 bytes in allocation file", __FUNCTION__
				,get_errno(),STRERROR(get_errno()));
			retval=SMB_ERR_SEEK;
			break;
		}
		if(!fwrite(&i,sizeof(i),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation bytes at offset %ld", __FUNCTION__
				,sda_offset);
			retval=SMB_ERR_WRITE; 
			break;
		}
	}
	fflush(smb->sda_fp);
	if(filelength(fileno(smb->sdt_fp)) / SDT_BLOCK_LEN > (long)(filelength(fileno(smb->sda_fp)) / sizeof(uint16_t)))
		chsize(fileno(smb->sdt_fp), (filelength(fileno(smb->sda_fp)) / sizeof(uint16_t)) * SDT_BLOCK_LEN);
	if(da_opened)
		smb_close_da(smb);
	smb_unlocksmbhdr(smb);
	return(retval);
}

/****************************************************************************/
/* Adds to data allocation records for blocks starting at 'offset'          */
/* SMB header should be locked before calling this function					*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_incdat(smb_t* smb, ulong offset, ulong length, uint16_t refs)
{
	uint16_t	i;
	ulong	l,blocks;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	for(l=0;l<blocks;l++) {
		if(fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*sizeof(i),SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s seeking to %ld", __FUNCTION__
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(i));
			return(SMB_ERR_SEEK);
		}
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading allocation record at offset %ld", __FUNCTION__
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(i));
			return(SMB_ERR_READ);
		}
		i+=refs;
		if(fseek(smb->sda_fp,-(int)sizeof(i),SEEK_CUR)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s rewinding %ld", __FUNCTION__, -(long)sizeof(i));
			return(SMB_ERR_SEEK);
		}
		if(!fwrite(&i,sizeof(i),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation record at offset %ld", __FUNCTION__
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(i));
			return(SMB_ERR_WRITE); 
		}
	}
	return fflush(smb->sda_fp); /* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/* Increments data allocation records (message references) by number of		*/
/* header references specified (usually 1)									*/
/* The opposite function of smb_freemsg()									*/
/* Always unlocks the SMB header (when not hyper-alloc)						*/
/****************************************************************************/
int SMBCALL smb_incmsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs)
{
	int		i=SMB_SUCCESS;
	BOOL	da_opened=FALSE;
	uint16_t	x;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=SMB_SUCCESS)
			return(i);
		da_opened=TRUE;
	}

	if(!smb->locked && smb_locksmbhdr(smb)!=SMB_SUCCESS)
		return SMB_ERR_LOCK;

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_incdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,refs))!=SMB_SUCCESS)
			break; 
	}
	smb_unlocksmbhdr(smb);

	if(da_opened)
		smb_close_da(smb);

	return(i);
}

/****************************************************************************/
/* De-allocates blocks for header record									*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsghdr(smb_t* smb, ulong offset, ulong length)
{
	uchar	c=0;
	long	l,blocks;
	off_t	sha_offset;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sha_fp);
	blocks = smb_hdrblocks(length);
	if(blocks < 1)
		return SMB_ERR_HDR_LEN;

	sha_offset = offset/SHD_BLOCK_LEN;
	if(filelength(fileno(smb->sha_fp)) <= (sha_offset + blocks)) {
		if(chsize(fileno(smb->sha_fp), sha_offset) == 0) {
			chsize(fileno(smb->shd_fp), smb->status.header_offset + offset);
			return SMB_SUCCESS;
		}
	}

	if(fseek(smb->sha_fp, sha_offset, SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s seeking to %ld", __FUNCTION__, sha_offset);
		return(SMB_ERR_SEEK);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation record", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
	return fflush(smb->sha_fp);	/* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/****************************************************************************/
int SMBCALL smb_freemsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs)
{
	int		i;
	uint16_t	x;

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_freemsgdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,refs))!=SMB_SUCCESS)
			return(i); 
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Frees all allocated header and data blocks (1 reference) for 'msg'       */
/****************************************************************************/
int SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg)
{
	int 	i;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	if((i=smb_freemsg_dfields(smb,msg,1))!=SMB_SUCCESS)
		return(i);

	return(smb_freemsghdr(smb,msg->idx.offset-smb->status.header_offset
		,msg->hdr.length));
}

/****************************************************************************/
/* Finds unused space in header file based on block allocation table and	*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of header (in bytes, not blocks) 			*/
/* Assumes smb_open_ha() has been called									*/
/* smb_close_ha() should be called after									*/
/* Returns negative value on error 											*/
/****************************************************************************/
long SMBCALL smb_allochdr(smb_t* smb, ulong length)
{
	uchar	c;
	ulong	i,l,blocks,offset=0;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_hdrblocks(length);
	i=0;	/* i is consecutive unused block counter */
	fflush(smb->sha_fp);
	rewind(smb->sha_fp);
	while(!feof(smb->sha_fp)) {
		if(smb_fread(smb,&c,sizeof(c),smb->sha_fp)!=sizeof(c)) 
			break;
		offset+=SHD_BLOCK_LEN;
		if(!c) i++;
		else   i=0;
		if(i==blocks) {
			offset-=(blocks*SHD_BLOCK_LEN);
			break; 
		} 
	}
	clearerr(smb->sha_fp);
	if(fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s seeking to %ld", __FUNCTION__, offset/SHD_BLOCK_LEN);
		return(SMB_ERR_SEEK);
	}
	for(l=0;l<blocks;l++)
		if(fputc(1,smb->sha_fp)!=1) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation record", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for header, but doesn't search for unused blocks          */
/* Returns negative value on error 											*/
/****************************************************************************/
long SMBCALL smb_fallochdr(smb_t* smb, ulong length)
{
	uchar	c=1;
	ulong	l,blocks,offset;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_hdrblocks(length);
	fflush(smb->sha_fp);
	clearerr(smb->sha_fp);
	if(fseek(smb->sha_fp,0L,SEEK_END)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s rewinding", __FUNCTION__);
		return(SMB_ERR_SEEK);
	}
	offset=ftell(smb->sha_fp)*SHD_BLOCK_LEN;
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing allocation record", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/************************************************************************/
/* Allocate header blocks using Hyper Allocation						*/
/* this function should be most likely not be called from anywhere but	*/
/* smb_addmsghdr()														*/
/************************************************************************/
long SMBCALL smb_hallochdr(smb_t* smb)
{
	ulong offset;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->shd_fp);
	if(fseek(smb->shd_fp,0L,SEEK_END)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s rewinding", __FUNCTION__);
		return(SMB_ERR_SEEK);
	}
	offset=ftell(smb->shd_fp);
	if(offset<smb->status.header_offset) 	/* Header file truncated?!? */
		return(smb->status.header_offset);

	offset-=smb->status.header_offset;		/* SMB headers not included */

	/* Even block boundry */
	offset+=PAD_LENGTH_FOR_ALIGNMENT(offset,SHD_BLOCK_LEN);

	return(offset);
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
long SMBCALL smb_hallocdat(smb_t* smb)
{
	long offset;

	if(smb->sdt_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sdt_fp);
	offset=filelength(fileno(smb->sdt_fp));
	if(offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid file length: %lu", __FUNCTION__,(ulong)offset);
		return(SMB_ERR_FILE_LEN);
	}
	if(fseek(smb->sdt_fp,0L,SEEK_END)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s rewinding", __FUNCTION__);
		return(SMB_ERR_SEEK);
	}
	offset=ftell(smb->sdt_fp);
	if(offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid file offset: %ld", __FUNCTION__,offset);
		return(SMB_ERR_DAT_OFFSET);
	}

	/* Make sure even block boundry */
	offset+=PAD_LENGTH_FOR_ALIGNMENT(offset,SDT_BLOCK_LEN);

	return(offset);
}
