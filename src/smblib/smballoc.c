/* Synchronet message base (SMB) alloc/free routines */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "smblib.h"
#include "genwrap.h"

/****************************************************************************/
/* Would data of 'length' bytes stored at 'offset' fit within the offsets	*/
/* the SMB format can express? (see SMB_MAX_DAT_OFFSET)						*/
/****************************************************************************/
static bool valid_dat_offset(off_t offset, off_t length)
{
	if (offset < 0 || length < 0)
		return false;
	return offset <= SMB_MAX_DAT_OFFSET - length;
}

/****************************************************************************/
/* Reads the 'blocks' consecutive data allocation records starting at		*/
/* 'sda_offset' (a byte offset into the .sda file) in one read, rather than	*/
/* one seek+read per record: on a network share each of those is a round	*/
/* trip to the file server.													*/
/* Returns a malloc'd array the caller must free, or NULL on error.			*/
/****************************************************************************/
static uint16_t* read_dat_alloc(smb_t* smb, off_t sda_offset, uint blocks, const char* func, int* result)
{
	uint16_t* rec;

	if ((rec = malloc(blocks * sizeof(*rec))) == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s malloc failure of %u allocation records", func, blocks);
		*result = SMB_ERR_MEM;
		return NULL;
	}
	clearerr(smb->sda_fp);
	if (fseeko(smb->sda_fp, sda_offset, SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s %d '%s' seeking to %" PRIdOFF " of allocation file", func
		              , get_errno(), strerror(get_errno()), sda_offset);
		free(rec);
		*result = SMB_ERR_SEEK;
		return NULL;
	}
	if (smb_fread(smb, rec, blocks * sizeof(*rec), smb->sda_fp) != blocks * sizeof(*rec)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s reading %u allocation records at offset %" PRIdOFF, func
		              , blocks, sda_offset);
		free(rec);
		*result = SMB_ERR_READ;
		return NULL;
	}
	*result = SMB_SUCCESS;
	return rec;
}

/****************************************************************************/
/* Writes 'blocks' consecutive data allocation records to 'sda_offset' in	*/
/* one write																*/
/****************************************************************************/
static int write_dat_alloc(smb_t* smb, off_t sda_offset, const uint16_t* rec, uint blocks, const char* func)
{
	if (blocks < 1)
		return SMB_SUCCESS;
	if (fseeko(smb->sda_fp, sda_offset, SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s %d '%s' seeking to %" PRIdOFF " of allocation file", func
		              , get_errno(), strerror(get_errno()), sda_offset);
		return SMB_ERR_SEEK;
	}
	if (fwrite(rec, sizeof(*rec), blocks, smb->sda_fp) != blocks) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s writing %u allocation records at offset %" PRIdOFF, func
		              , blocks, sda_offset);
		return SMB_ERR_WRITE;
	}
	return SMB_SUCCESS;
}

/****************************************************************************/
/* Finds unused space in data file based on block allocation table and		*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of data (in bytes, not blocks)				*/
/* Assumes smb_open_da() has been called									*/
/* smb_close_da() should be called after									*/
/* Returns negative on error												*/
/****************************************************************************/
off_t smb_allocdat(smb_t* smb, off_t length, uint16_t refs)
{
	uint16_t i;
	uint     j, l, blocks;
	off_t    offset = 0;

	if (smb->sda_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	blocks = smb_datblocks(length);
	j = 0;    /* j is consecutive unused block counter */
	fflush(smb->sda_fp);
	rewind(smb->sda_fp);
	while (!feof(smb->sda_fp) && offset <= SMB_MAX_DAT_OFFSET) {
		if (smb_fread(smb, &i, sizeof(i), smb->sda_fp) != sizeof(i))
			break;
		offset += SDT_BLOCK_LEN;
		if (!i)
			j++;
		else
			j = 0;
		if (j == blocks) {
			offset -= ((off_t)blocks * SDT_BLOCK_LEN);
			break;
		}
	}
	if (!valid_dat_offset(offset, (off_t)blocks * SDT_BLOCK_LEN)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid data offset: %" PRIdOFF " (%u blocks, max %" PRIdOFF ")"
		              , __FUNCTION__, offset, blocks, SMB_MAX_DAT_OFFSET);
		return SMB_ERR_DAT_OFFSET;
	}
	clearerr(smb->sda_fp);
	if (fseeko(smb->sda_fp, (offset / SDT_BLOCK_LEN) * sizeof(refs), SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s seeking to: %" PRIdOFF, __FUNCTION__
		              , (offset / SDT_BLOCK_LEN) * sizeof(refs));
		return SMB_ERR_SEEK;
	}
	for (l = 0; l < blocks; l++)
		if (!fwrite(&refs, sizeof(refs), 1, smb->sda_fp)) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s writing allocation bytes at offset %" PRIdOFF, __FUNCTION__
			              , ((offset / SDT_BLOCK_LEN) + l) * sizeof(refs));
			return SMB_ERR_WRITE;
		}
	fflush(smb->sda_fp);
	return offset;
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
off_t smb_fallocdat(smb_t* smb, off_t length, uint16_t refs)
{
	uint  l, blocks;
	off_t offset;

	if (smb->sda_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	fflush(smb->sda_fp);
	clearerr(smb->sda_fp);
	blocks = smb_datblocks(length);
	if (fseek(smb->sda_fp, 0L, SEEK_END)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s rewinding", __FUNCTION__);
		return SMB_ERR_SEEK;
	}
	offset = ftello(smb->sda_fp);
	if (offset < 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s %d '%s' telling allocation file position", __FUNCTION__
		              , get_errno(), strerror(get_errno()));
		return SMB_ERR_SEEK;
	}
	offset = (offset / (off_t)sizeof(refs)) * SDT_BLOCK_LEN;
	if (!valid_dat_offset(offset, (off_t)blocks * SDT_BLOCK_LEN)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid data offset: %" PRIdOFF " (%u blocks, max %" PRIdOFF ")"
		              , __FUNCTION__, offset, blocks, SMB_MAX_DAT_OFFSET);
		return SMB_ERR_DAT_OFFSET;
	}
	for (l = 0; l < blocks; l++)
		if (!fwrite(&refs, sizeof(refs), 1, smb->sda_fp))
			break;
	fflush(smb->sda_fp);
	if (l < blocks) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s writing allocation bytes", __FUNCTION__);
		return SMB_ERR_WRITE;
	}
	return offset;
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/* Always unlocks the SMB header (when not hyper-alloc)						*/
/****************************************************************************/
int smb_freemsgdat(smb_t* smb, off_t offset, uint length, uint16_t refs)
{
	bool      da_opened = false;
	int       retval = SMB_SUCCESS;
	int       i;
	uint16_t* rec;
	uint      l, blocks, keep;
	off_t     sda_offset;
	off_t     flen;
	off_t     sda_blocks;
	off_t     sdt_blocks;

	if (offset < 0)
		return SMB_ERR_DAT_OFFSET;

	if (smb->status.attr & SMB_HYPERALLOC) /* do nothing */
		return SMB_SUCCESS;

	blocks = smb_datblocks(length);

	if (blocks < 1)
		return SMB_SUCCESS; // Nothing to do

	if (smb->sda_fp == NULL) {
		if ((i = smb_open_da(smb)) != SMB_SUCCESS)
			return i;
		da_opened = true;
	}
	flen = filelength(fileno(smb->sda_fp));
	if (flen < sizeof(uint16_t))
		return 0;   // Nothing to do

	if (!smb->smbhdr_locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return SMB_ERR_LOCK;

	sda_offset = (offset / SDT_BLOCK_LEN) * (off_t)sizeof(*rec);
	if ((rec = read_dat_alloc(smb, sda_offset, blocks, __FUNCTION__, &retval)) != NULL) {
		for (l = 0; l < blocks; l++) {
			if (refs == SMB_ALL_REFS || refs > rec[l])
				rec[l] = 0;     /* don't want to go negative */
			else
				rec[l] -= refs;
		}
		// Completely free records at the end of the SDA? Just truncate them from the end of file
		keep = blocks;
		if (sda_offset + (off_t)blocks * (off_t)sizeof(*rec) == flen) {
			while (keep > 0 && rec[keep - 1] == 0)
				keep--;
			if (keep < blocks
			    && chsize(fileno(smb->sda_fp), sda_offset + (off_t)keep * (off_t)sizeof(*rec)) != 0)
				keep = blocks;
		}
		retval = write_dat_alloc(smb, sda_offset, rec, keep, __FUNCTION__);
		free(rec);
	}
	fflush(smb->sda_fp);
	sda_blocks = filelength(fileno(smb->sda_fp)) / (off_t)sizeof(uint16_t);
	sdt_blocks = filelength(fileno(smb->sdt_fp)) / SDT_BLOCK_LEN;
	if (sda_blocks >= 0 && sdt_blocks > sda_blocks)
		if (chsize(fileno(smb->sdt_fp), sda_blocks * SDT_BLOCK_LEN) != 0)
			retval = SMB_ERR_TRUNCATE;
	if (da_opened)
		smb_close_da(smb);
	smb_unlocksmbhdr(smb);
	return retval;
}

/****************************************************************************/
/* Adds to data allocation records for blocks starting at 'offset'          */
/* SMB header should be locked before calling this function					*/
/* Returns non-zero on error												*/
/****************************************************************************/
int smb_incdat(smb_t* smb, off_t offset, uint length, uint16_t refs)
{
	uint16_t* rec;
	uint      l, blocks;
	off_t     sda_offset;
	int       retval;

	if (smb->sda_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	blocks = smb_datblocks(length);
	if (blocks < 1)
		return SMB_SUCCESS;
	sda_offset = (offset / SDT_BLOCK_LEN) * (off_t)sizeof(*rec);
	if ((rec = read_dat_alloc(smb, sda_offset, blocks, __FUNCTION__, &retval)) == NULL)
		return retval;
	for (l = 0; l < blocks; l++)
		rec[l] += refs;
	retval = write_dat_alloc(smb, sda_offset, rec, blocks, __FUNCTION__);
	free(rec);
	if (retval != SMB_SUCCESS)
		return retval;
	return fflush(smb->sda_fp); /* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/* Increments data allocation records (message references) by number of		*/
/* header references specified (usually 1)									*/
/* The opposite function of smb_freemsg()									*/
/* Always unlocks the SMB header (when not hyper-alloc)						*/
/****************************************************************************/
int smb_incmsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs)
{
	int      i = SMB_SUCCESS;
	bool     da_opened = false;
	uint16_t x;

	if (smb->status.attr & SMB_HYPERALLOC)  /* Nothing to do */
		return SMB_SUCCESS;

	if (smb->sda_fp == NULL) {
		if ((i = smb_open_da(smb)) != SMB_SUCCESS)
			return i;
		da_opened = true;
	}

	if (!smb->smbhdr_locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return SMB_ERR_LOCK;

	for (x = 0; x < msg->hdr.total_dfields; x++) {
		if ((i = smb_incdat(smb, msg->hdr.offset + msg->dfield[x].offset
		                    , msg->dfield[x].length, refs)) != SMB_SUCCESS)
			break;
	}
	smb_unlocksmbhdr(smb);

	if (da_opened)
		smb_close_da(smb);

	return i;
}

/****************************************************************************/
/* De-allocates blocks for header record									*/
/* Returns non-zero on error												*/
/****************************************************************************/
int smb_freemsghdr(smb_t* smb, off_t offset, uint length)
{
	uchar c = 0;
	int   l, blocks;
	off_t sha_offset;

	if (smb->status.attr & SMB_HYPERALLOC)  /* Nothing to do */
		return SMB_SUCCESS;

	if (smb->sha_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	clearerr(smb->sha_fp);
	blocks = smb_hdrblocks(length);
	if (blocks < 1) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s invalid header length: %u", __FUNCTION__, length);
		return SMB_ERR_HDR_LEN;
	}

	sha_offset = offset / SHD_BLOCK_LEN;
	if (filelength(fileno(smb->sha_fp)) <= (sha_offset + blocks)) {
		if (chsize(fileno(smb->sha_fp), sha_offset) == 0) {
			if (chsize(fileno(smb->shd_fp), (off_t)smb->status.header_offset + offset) != 0) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s header truncation failure", __FUNCTION__);
				return SMB_ERR_TRUNCATE;
			}
			return SMB_SUCCESS;
		}
	}

	if (fseeko(smb->sha_fp, sha_offset, SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s seeking to %" PRIdOFF, __FUNCTION__, sha_offset);
		return SMB_ERR_SEEK;
	}
	for (l = 0; l < blocks; l++)
		if (!fwrite(&c, 1, 1, smb->sha_fp)) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s writing allocation record", __FUNCTION__);
			return SMB_ERR_WRITE;
		}
	return fflush(smb->sha_fp); /* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/****************************************************************************/
int smb_freemsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs)
{
	int      i;
	uint16_t x;

	for (x = 0; x < msg->hdr.total_dfields; x++) {
		if ((i = smb_freemsgdat(smb, msg->hdr.offset + msg->dfield[x].offset
		                        , msg->dfield[x].length, refs)) != SMB_SUCCESS)
			return i;
	}
	return SMB_SUCCESS;
}

/****************************************************************************/
/* Frees all allocated header and data blocks (1 reference) for 'msg'       */
/****************************************************************************/
int smb_freemsg(smb_t* smb, smbmsg_t* msg)
{
	int i;

	if (smb->status.attr & SMB_HYPERALLOC)  /* Nothing to do */
		return SMB_SUCCESS;

	if (!smb_valid_hdr_offset(smb, msg->idx.offset))
		return SMB_ERR_HDR_OFFSET;

	if ((i = smb_freemsg_dfields(smb, msg, 1)) != SMB_SUCCESS)
		return i;

	return smb_freemsghdr(smb, msg->idx.offset - smb->status.header_offset
	                      , msg->hdr.length);
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
off_t smb_allochdr(smb_t* smb, uint length)
{
	uchar c;
	uint  i, l, blocks, offset = 0;

	if (smb->sha_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	blocks = smb_hdrblocks(length);
	i = 0;    /* i is consecutive unused block counter */
	fflush(smb->sha_fp);
	rewind(smb->sha_fp);
	while (!feof(smb->sha_fp)) {
		if (smb_fread(smb, &c, sizeof(c), smb->sha_fp) != sizeof(c))
			break;
		offset += SHD_BLOCK_LEN;
		if (!c)
			i++;
		else
			i = 0;
		if (i == blocks) {
			offset -= (blocks * SHD_BLOCK_LEN);
			break;
		}
	}
	clearerr(smb->sha_fp);
	if (fseek(smb->sha_fp, offset / SHD_BLOCK_LEN, SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s seeking to %d", __FUNCTION__, offset / SHD_BLOCK_LEN);
		return SMB_ERR_SEEK;
	}
	for (l = 0; l < blocks; l++)
		if (fputc(1, smb->sha_fp) != 1) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s writing allocation record", __FUNCTION__);
			return SMB_ERR_WRITE;
		}
	fflush(smb->sha_fp);
	return offset;
}

/****************************************************************************/
/* Allocates space for header, but doesn't search for unused blocks          */
/* Returns negative value on error 											*/
/****************************************************************************/
off_t smb_fallochdr(smb_t* smb, uint length)
{
	uchar c = 1;
	uint  l, blocks;
	off_t offset;

	if (smb->sha_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	blocks = smb_hdrblocks(length);
	fflush(smb->sha_fp);
	clearerr(smb->sha_fp);
	if (fseek(smb->sha_fp, 0L, SEEK_END)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s rewinding", __FUNCTION__);
		return SMB_ERR_SEEK;
	}
	offset = ftello(smb->sha_fp);
	if (offset < 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s %d '%s' telling allocation file position", __FUNCTION__
		              , get_errno(), strerror(get_errno()));
		return SMB_ERR_SEEK;
	}
	offset *= SHD_BLOCK_LEN;
	if (offset > SMB_MAX_HDR_OFFSET - ((off_t)blocks * SHD_BLOCK_LEN)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid header offset: %" PRIdOFF " (%u blocks, max %" PRIdOFF ")"
		              , __FUNCTION__, offset, blocks, SMB_MAX_HDR_OFFSET);
		return SMB_ERR_HDR_OFFSET;
	}
	for (l = 0; l < blocks; l++)
		if (!fwrite(&c, 1, 1, smb->sha_fp)) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s writing allocation record", __FUNCTION__);
			return SMB_ERR_WRITE;
		}
	fflush(smb->sha_fp);
	return offset;
}

/************************************************************************/
/* Allocate header blocks using Hyper Allocation						*/
/* this function should be most likely not be called from anywhere but	*/
/* smb_addmsghdr()														*/
/************************************************************************/
off_t smb_hallochdr(smb_t* smb)
{
	off_t offset;

	if (smb->shd_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	fflush(smb->shd_fp);
	if (fseek(smb->shd_fp, 0L, SEEK_END)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s rewinding", __FUNCTION__);
		return SMB_ERR_SEEK;
	}
	offset = ftello(smb->shd_fp);
	if (offset < (off_t)smb->status.header_offset)    /* Header file truncated?!? */
		return smb->status.header_offset;

	offset -= smb->status.header_offset;      /* SMB headers not included */

	/* Even block boundry */
	offset += PAD_LENGTH_FOR_ALIGNMENT(offset, SHD_BLOCK_LEN);

	if (offset > SMB_MAX_HDR_OFFSET) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid header offset: %" PRIdOFF " (max %" PRIdOFF ")"
		              , __FUNCTION__, offset, SMB_MAX_HDR_OFFSET);
		return SMB_ERR_HDR_OFFSET;
	}

	return offset;
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
off_t smb_hallocdat(smb_t* smb)
{
	off_t offset;

	if (smb->sdt_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	fflush(smb->sdt_fp);
	offset = filelength(fileno(smb->sdt_fp));
	if (offset < 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid file length: %" PRIdOFF, __FUNCTION__, offset);
		return SMB_ERR_FILE_LEN;
	}
	if (fseek(smb->sdt_fp, 0L, SEEK_END)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s rewinding", __FUNCTION__);
		return SMB_ERR_SEEK;
	}
	offset = ftello(smb->sdt_fp);
	if (offset < 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid file offset: %" PRIdOFF, __FUNCTION__, offset);
		return SMB_ERR_DAT_OFFSET;
	}

	/* Make sure even block boundry */
	offset += PAD_LENGTH_FOR_ALIGNMENT(offset, SDT_BLOCK_LEN);

	if (offset > SMB_MAX_DAT_OFFSET) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s invalid data offset: %" PRIdOFF " (max %" PRIdOFF ")"
		              , __FUNCTION__, offset, SMB_MAX_DAT_OFFSET);
		return SMB_ERR_DAT_OFFSET;
	}

	return offset;
}
