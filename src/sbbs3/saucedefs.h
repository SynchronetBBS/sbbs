// ANSI/Block/RIP artwork file SAUCE record definitions
// Derived from here: http://www.acid.org/info/sauce/sauce.htm
// vi: tabstop=4
// $Id: saucedefs.h,v 1.2 2018/02/12 04:03:34 rswindell Exp $

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
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _SAUCEDEFS_H_
#define _SAUCEDEFS_H_

#include "gen_defs.h"

#define SAUCE_SEPARATOR			CTRL_Z	// CPM/MS-DOS EOF
#define SAUCE_ID				"SAUCE"
#define SAUCE_COMMENT_BLOCK_ID	"COMNT"
#define SAUCE_VERSION			"00"
#define SAUCE_LEN_ID			5
#define SAUCE_LEN_VERSION		2
#define SAUCE_LEN_TITLE			35
#define SAUCE_LEN_AUTHOR		20
#define SAUCE_LEN_GROUP			20
#define SAUCE_LEN_DATE			8
#define SAUCE_LEN_TINFOS		22
#define SAUCE_LEN_COMMENT		64

enum sauce_datatype {
	 sauce_datatype_none
	,sauce_datatype_char
	,sauce_datatype_bitmap
	,sauce_datatype_vector
	,sauce_datatype_audio
	,sauce_datatype_bin
	,sauce_datatype_xbin
	,sauce_datatype_archive
	,sauce_datatype_exec
};

enum sauce_char_filetype {
	 sauce_char_filetype_ascii
	,sauce_char_filetype_ansi
	,sauce_char_filetype_ansimation
	,sauce_char_filetype_rip
	,sauce_char_filetype_pcboard
	,sauce_char_filetype_avatar
	,sauce_char_filetype_html
	,sauce_char_filetype_source
	,sauce_char_filetype_tundra
};

#define sauce_ansiflag_nonblink			(1<<0)	// High intensity BG, aka iCE colors
#define sauce_ansiflag_spacing_mask		(3<<1)	// Letter spacing
#define sauce_ansiflag_spacing_legacy	(0<<1)	// Legacy value. No preference.
#define sauce_ansiflag_spacing_8pix		(1<<1)	// 8-pixel font
#define sauce_ansiflag_spacing_9pix		(2<<1)	// 9-pixel font
#define sauce_ansiflag_ratio_mask		(3<<3)	// aspect ratio
#define sauce_ansiflag_ratio_legacy		(0<<3)	// Legacy value. No preference.
#define sauce_ansiflag_ratio_rect		(1<<3)	// Rectangular pixels
#define sauce_ansiflag_ratio_square		(2<<3)	// Square pixels

#if defined(__GNUC__)
	#define PACKED_STRUCT __attribute__((packed))
#else	/* non-GCC compiler */
	#pragma pack(push)
	#pragma pack(1)
	#define PACKED_STRUCT
#endif

typedef struct sauce {				/* SAUCE record */
	char		id[SAUCE_LEN_ID];
	char		ver[SAUCE_LEN_VERSION];
	char		title[SAUCE_LEN_TITLE];
	char		author[SAUCE_LEN_AUTHOR];
	char		group[SAUCE_LEN_GROUP];
	char		date[SAUCE_LEN_DATE];
	uint32_t	filesize;
	uint8_t		datatype;
	uint8_t		filetype;
	uint16_t	tinfo1;
	uint16_t	tinfo2;
	uint16_t	tinfo3;
	uint16_t	tinfo4;
	uint8_t		comments;	// Number of comments in the comment block
	uint8_t		tflags;
	char		tinfos[SAUCE_LEN_TINFOS];	// NUL-terminated (if less than full length)
} PACKED_STRUCT sauce_record_t;

#if !defined(__GNUC__)
	#pragma pack(pop)		/* original packing */
#endif

#endif	/* Don't add anything after this line */
