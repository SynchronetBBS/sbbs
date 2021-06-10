/* Synchronet message base (SMB) library function prototypes */

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

#ifndef _SMBLIB_H
#define _SMBLIB_H

#ifdef SMBEXPORT
	#undef SMBEXPORT
#endif

#ifdef _WIN32
	#if defined(SMB_IMPORTS) || defined(SMB_EXPORTS)
		#if defined(SMB_IMPORTS)
			#define SMBEXPORT __declspec( dllimport )
		#else
			#define SMBEXPORT __declspec( dllexport )
		#endif
	#else	/* self-contained executable */
		#define SMBEXPORT
	#endif
#else
	#define SMBEXPORT
#endif

#include "smbdefs.h"
#include "str_list.h"

#define SMB_SUCCESS			0			/* Successful result/return code */
#define SMB_FAILURE			-1			/* Generic error (discouraged) */
#define SMB_BAD_PARAMETER	-2			/* Invalid API function parameter value */

										/* Standard smblib errors values */
#define SMB_ERR_NOT_OPEN	-100		/* Message base not open */
#define SMB_ERR_HDR_LEN		-101		/* Invalid message header length (>64k) */
#define SMB_ERR_HDR_OFFSET	-102		/* Invalid message header offset */
#define SMB_ERR_HDR_ID		-103		/* Invalid header ID */
#define SMB_ERR_HDR_VER		-104		/* Unsupported version */
#define SMB_ERR_HDR_FIELD	-105		/* Missing header field */
#define SMB_ERR_HDR_ATTR	-106		/* Invalid message attributes */
#define SMB_ERR_NOT_FOUND	-110		/* Item not found */
#define SMB_ERR_DAT_OFFSET	-120		/* Invalid data offset (>2GB) */
#define SMB_ERR_DAT_LEN		-121		/* Invalid data length (>2GB) */
#define SMB_ERR_OPEN		-200		/* File open error */
#define SMB_ERR_SEEK		-201		/* File seek/setpos error */
#define SMB_ERR_LOCK		-202		/* File lock error */
#define SMB_ERR_READ		-203		/* File read error */
#define SMB_ERR_WRITE		-204		/* File write error */
#define SMB_ERR_TIMEOUT		-205		/* File operation timed-out */
#define SMB_ERR_FILE_LEN	-206		/* File length invalid */
#define SMB_ERR_DELETE		-207		/* File deletion error */
#define SMB_ERR_UNLOCK		-208		/* File unlock error */
#define SMB_ERR_RENAME		-209		/* File rename error */
#define SMB_ERR_MEM			-300		/* Memory allocation error */

#define SMB_DUPE_MSG		1			/* Duplicate message detected by smb_addcrc() */
#define SMB_CLOSED			2			/* Poll/thread is closed to replies/votes */
#define SMB_UNAUTHORIZED	3			/* Poll was posted by someone else */

#define SMB_ALL_REFS		0			/* Free all references to data		*/

										/* smb_getmsgtxt() mode flags		*/
#define GETMSGTXT_TAILS 		(1<<0)	/* Incude message tail(s)			*/
#define GETMSGTXT_NO_BODY		(1<<1)	/* Exclude message body				*/
#define GETMSGTXT_NO_HFIELDS	(1<<2)	/* Exclude text header fields		*/
#define GETMSGTXT_PLAIN			(1<<3)	/* Get text-plain or text-html portion only of MIME-encoded body (all, otherwise) */
										/* common smb_getmsgtxt() mode values */
#define GETMSGTXT_BODY_ONLY		GETMSGTXT_NO_HFIELDS
#define GETMSGTXT_TAIL_ONLY		(GETMSGTXT_TAILS|GETMSGTXT_NO_BODY|GETMSGTXT_NO_HFIELDS)
#define GETMSGTXT_ALL			GETMSGTXT_TAILS
#define GETMSGTXT_NO_TAILS		0		/* Exclude message tails(s)			*/

#define SMB_IS_OPEN(smb)	((smb)->shd_fp!=NULL)

/* Legacy API functions */
#define smb_incmsg(smb,msg)	smb_incmsg_dfields(smb,msg,1)
#define smb_incdat			smb_incmsgdat
#define smb_open_da(smb)	smb_open_fp(smb,&(smb)->sda_fp,SH_DENYRW)
#define smb_close_da(smb)	smb_close_fp(&(smb)->sda_fp)
#define smb_open_ha(smb)	smb_open_fp(smb,&(smb)->sha_fp,SH_DENYRW)
#define smb_close_ha(smb)	smb_close_fp(&(smb)->sha_fp)
#define smb_open_hash(smb)	smb_open_fp(smb,&(smb)->hash_fp,SH_DENYRW)
#define smb_close_hash(smb)	smb_close_fp(&(smb)->hash_fp)

#ifdef __cplusplus
extern "C" {
#endif

SMBEXPORT int 		smb_ver(void);
SMBEXPORT char*		smb_lib_ver(void);
SMBEXPORT int 		smb_open(smb_t*);
SMBEXPORT int		smb_open_index(smb_t*);
SMBEXPORT void		smb_close(smb_t*);
SMBEXPORT int 		smb_initsmbhdr(smb_t*);
SMBEXPORT int 		smb_create(smb_t*);
SMBEXPORT int 		smb_trunchdr(smb_t*);
SMBEXPORT int		smb_lock(smb_t*);
SMBEXPORT int		smb_unlock(smb_t*);
SMBEXPORT BOOL		smb_islocked(smb_t*);
SMBEXPORT int		Smb_initsmbhdr(smb_t*);
SMBEXPORT int 		smb_locksmbhdr(smb_t*);
SMBEXPORT int 		smb_getstatus(smb_t*);
SMBEXPORT int 		smb_putstatus(smb_t*);
SMBEXPORT int 		smb_unlocksmbhdr(smb_t*);
SMBEXPORT int 		smb_getmsgidx(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_getfirstidx(smb_t*, idxrec_t*);
SMBEXPORT int 		smb_getlastidx(smb_t*, idxrec_t*);
SMBEXPORT ulong		smb_getmsghdrlen(smbmsg_t*);
SMBEXPORT ulong		smb_getmsgdatlen(smbmsg_t*);
SMBEXPORT ulong		smb_getmsgtxtlen(smbmsg_t*);
SMBEXPORT int 		smb_lockmsghdr(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_getmsghdr(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_unlockmsghdr(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_addcrc(smb_t*, uint32_t crc);

SMBEXPORT int 		smb_hfield_add(smbmsg_t*, uint16_t type, size_t, void* data, BOOL insert);
SMBEXPORT int		smb_hfield_add_str(smbmsg_t*, uint16_t type, const char* str, BOOL insert);
SMBEXPORT int		smb_hfield_replace(smbmsg_t*, uint16_t type, size_t, void* data);
SMBEXPORT int		smb_hfield_replace_str(smbmsg_t*, uint16_t type, const char* str);
SMBEXPORT int		smb_hfield_append(smbmsg_t*, uint16_t type, size_t, void* data);
SMBEXPORT int		smb_hfield_append_str(smbmsg_t*, uint16_t type, const char* data);
SMBEXPORT int		smb_hfield_add_list(smbmsg_t*, hfield_t** hfield_list, void** hfield_dat, BOOL insert);
SMBEXPORT int		smb_hfield_add_netaddr(smbmsg_t*, uint16_t type, const char* str, uint16_t* nettype, BOOL insert);
SMBEXPORT int		smb_hfield_string(smbmsg_t*, uint16_t type, const char*);
/* Convenience macro: */
#define smb_hfield_bin(msg, type, data) smb_hfield_add(msg, type, sizeof(data), &(data), /* insert: */FALSE)
/* Backward compatibility macros: */
#define smb_hfield(msg,type,len,data)	smb_hfield_add(msg, type, len, data, /* insert: */FALSE)
#define smb_hfield_str(msg, type, str)	smb_hfield_add_str(msg, type, str, /* insert: */FALSE)
#define smb_hfield_netaddr(msg, type, str, nettype) smb_hfield_add_netaddr(msg, type, str, nettype, /* insert: */FALSE)

SMBEXPORT int 		smb_dfield(smbmsg_t*, uint16_t type, ulong length);
SMBEXPORT void*		smb_get_hfield(smbmsg_t*, uint16_t type, hfield_t** hfield);
SMBEXPORT int		smb_new_hfield(smbmsg_t*, uint16_t type, size_t, void* data);
SMBEXPORT int		smb_new_hfield_str(smbmsg_t*, uint16_t type, const char*);
SMBEXPORT int 		smb_addmsghdr(smb_t*, smbmsg_t*, int storage);
SMBEXPORT int 		smb_putmsg(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_putmsgidx(smb_t*, smbmsg_t*);
SMBEXPORT int 		smb_putmsghdr(smb_t*, smbmsg_t*);
SMBEXPORT void		smb_freemsgmem(smbmsg_t*);
SMBEXPORT void		smb_freemsghdrmem(smbmsg_t*);
SMBEXPORT ulong		smb_hdrblocks(ulong length);
SMBEXPORT ulong		smb_datblocks(off_t length);
SMBEXPORT int		smb_copymsgmem(smb_t*, smbmsg_t* destmsg, smbmsg_t* srcmsg);
SMBEXPORT int		smb_tzutc(int16_t timezone);
SMBEXPORT int		smb_updatethread(smb_t*, smbmsg_t* remsg, ulong newmsgnum);
SMBEXPORT int		smb_updatemsg(smb_t*, smbmsg_t*);
SMBEXPORT BOOL		smb_valid_hdr_offset(smb_t*, ulong offset);
SMBEXPORT int		smb_init_idx(smb_t*, smbmsg_t*);
SMBEXPORT uint16_t	smb_voted_already(smb_t*, uint32_t msgnum, const char* name, enum smb_net_type, void* net_addr);
SMBEXPORT BOOL		smb_msg_is_from(smbmsg_t*, const char* name, enum smb_net_type net_type, const void* net_addr);
SMBEXPORT uint32_t	smb_first_in_thread(smb_t*, smbmsg_t*, msghdr_t*);
SMBEXPORT uint32_t	smb_next_in_thread(smb_t*, smbmsg_t*, msghdr_t*);
SMBEXPORT uint32_t	smb_last_in_branch(smb_t*, smbmsg_t*);
SMBEXPORT uint32_t	smb_last_in_thread(smb_t*, smbmsg_t*);
SMBEXPORT size_t	smb_idxreclen(smb_t*);
SMBEXPORT uint32_t	smb_count_idx_records(smb_t*, uint16_t mask, uint16_t cmp);
SMBEXPORT BOOL		smb_msg_is_utf8(const smbmsg_t*);
SMBEXPORT size_t	smb_msg_count(smb_t*, unsigned types);
SMBEXPORT enum smb_msg_type smb_msg_type(smb_msg_attr_t);

/* smbadd.c */
SMBEXPORT int		smb_addmsg(smb_t*, smbmsg_t*, int storage, long dupechk_hashes
						,uint16_t xlat, const uchar* body, const uchar* tail);
SMBEXPORT int		smb_addvote(smb_t*, smbmsg_t*, int storage);
SMBEXPORT int		smb_addpoll(smb_t*, smbmsg_t*, int storage);
SMBEXPORT int		smb_addpollclosure(smb_t*, smbmsg_t*, int storage);

/* smballoc.c */
SMBEXPORT off_t		smb_allochdr(smb_t*, ulong length);
SMBEXPORT off_t		smb_fallochdr(smb_t*, ulong length);
SMBEXPORT off_t		smb_hallochdr(smb_t*);
SMBEXPORT off_t		smb_allocdat(smb_t*, off_t length, uint16_t int16_trefs);
SMBEXPORT off_t		smb_fallocdat(smb_t*, off_t length, uint16_t refs);
SMBEXPORT off_t		smb_hallocdat(smb_t*);
SMBEXPORT int		smb_incmsg_dfields(smb_t*, smbmsg_t*, uint16_t refs);
SMBEXPORT int 		smb_incmsgdat(smb_t*, off_t offset, ulong length, uint16_t refs);
SMBEXPORT int 		smb_freemsg(smb_t*, smbmsg_t*);
SMBEXPORT int		smb_freemsg_dfields(smb_t*, smbmsg_t*, uint16_t refs);
SMBEXPORT int 		smb_freemsgdat(smb_t*, off_t offset, ulong length, uint16_t refs);
SMBEXPORT int 		smb_freemsghdr(smb_t*, off_t offset, ulong length);
SMBEXPORT void		smb_freemsgtxt(char* buf);

/* smbhash.c */
SMBEXPORT int		smb_findhash(smb_t*, hash_t** compare_list, hash_t* found
										 ,long source_mask, BOOL mark);
SMBEXPORT int		smb_hashmsg(smb_t*, smbmsg_t*, const uchar* text, BOOL update);
SMBEXPORT hash_t*	smb_hash(ulong msgnum, uint32_t time, unsigned source
								,unsigned flags, const void* data, size_t);
SMBEXPORT hash_t*	smb_hashstr(ulong msgnum, uint32_t time, unsigned source
								,unsigned flags, const char* str);

SMBEXPORT hash_t**	smb_msghashes(smbmsg_t*, const uchar* text, long source_mask);
SMBEXPORT int		smb_addhashes(smb_t*, hash_t** hash_list, BOOL skip_marked);
SMBEXPORT uint16_t	smb_name_crc(const char* name);
SMBEXPORT uint16_t	smb_subject_crc(const char *subj);
SMBEXPORT void		smb_freehashes(hash_t**);
SMBEXPORT long		smb_getmsgidx_by_time(smb_t*, idxrec_t*, time_t);
SMBEXPORT int		smb_hashfile(const char* path, off_t, struct hash_data*);

/* Fast look-up functions (using hashes) */
SMBEXPORT int 		smb_getmsgidx_by_hash(smb_t*, smbmsg_t*, unsigned source
								 ,unsigned flags, const void* data, size_t);
SMBEXPORT int 		smb_getmsghdr_by_hash(smb_t*, smbmsg_t*, unsigned source
								 ,unsigned flags, const void* data, size_t);

/* 0-length specifies ASCIIZ data (length calculated automatically) */
#define smb_getmsgidx_by_hashstr(smb, msg, source, flags, data) \
		smb_getmsgidx_by_hash(smb, msg, source, flags, data, 0)
#define smb_getmsghdr_by_hashstr(smb, msg, source, flags, data) \
		smb_getmsghdr_by_hash(smb, msg, source, flags, data, 0)

/* Fast Message-ID based look-up macros (using hashes) */
#define smb_getmsgidx_by_msgid(smb, msg, id) \
		smb_getmsgidx_by_hashstr(smb, msg, SMB_HASH_SOURCE_MSG_ID, SMB_HASH_MASK, id)
#define smb_getmsgidx_by_ftnid(smb, msg, id) \
		smb_getmsgidx_by_hashstr(smb, msg, SMB_HASH_SOURCE_FTN_ID, SMB_HASH_MASK, id)
#define smb_getmsghdr_by_msgid(smb, msg, id) \
		smb_getmsghdr_by_hashstr(smb, msg, SMB_HASH_SOURCE_MSG_ID, SMB_HASH_MASK, id)
#define smb_getmsghdr_by_ftnid(smb, msg, id) \
		smb_getmsghdr_by_hashstr(smb, msg, SMB_HASH_SOURCE_FTN_ID, SMB_HASH_MASK, id)

/* smbstr.c */
SMBEXPORT char*		smb_hfieldtype(uint16_t type);
SMBEXPORT uint16_t	smb_hfieldtypelookup(const char*);
SMBEXPORT char*		smb_dfieldtype(uint16_t type);
SMBEXPORT char*		smb_faddrtoa(fidoaddr_t* addr, char* outstr);
SMBEXPORT char*		smb_netaddr(net_t* net);
SMBEXPORT char*		smb_netaddrstr(net_t* net, char* fidoaddr_buf);
SMBEXPORT char*		smb_nettype(enum smb_net_type);
SMBEXPORT char*		smb_zonestr(int16_t zone, char* outstr);
SMBEXPORT char*		smb_msgattrstr(int16_t attr, char* outstr, size_t maxlen);
SMBEXPORT char*		smb_auxattrstr(int32_t attr, char* outstr, size_t maxlen);
SMBEXPORT char*		smb_netattrstr(int32_t attr, char* outstr, size_t maxlen);
SMBEXPORT char*		smb_hashsource(smbmsg_t*, int source);
SMBEXPORT char*		smb_hashsourcetype(uchar type);
SMBEXPORT fidoaddr_t smb_atofaddr(const fidoaddr_t* sys_addr, const char *str);
SMBEXPORT enum smb_net_type smb_netaddr_type(const char* addr);
SMBEXPORT enum smb_net_type smb_get_net_type_by_addr(const char* addr);
/* smbdump.c */
SMBEXPORT void		smb_dump_msghdr(FILE*, smbmsg_t*);
SMBEXPORT str_list_t smb_msghdr_str_list(smbmsg_t*);

/* smbtxt.c */
SMBEXPORT char*		smb_getmsgtxt(smb_t*, smbmsg_t*, ulong mode);
SMBEXPORT char*		smb_getplaintext(smbmsg_t*, char* body);
SMBEXPORT uint8_t*	smb_getattachment(smbmsg_t*, char* body, char* filename, size_t filename_len, uint32_t* filelen, int index);
SMBEXPORT ulong		smb_countattachments(smb_t*, smbmsg_t*, const char* body);
SMBEXPORT void		smb_parse_content_type(const char* content_type, char** subtype, char** charset);

/* smbfile.c */
SMBEXPORT int 		smb_feof(FILE* fp);
SMBEXPORT int 		smb_ferror(FILE* fp);
SMBEXPORT int 		smb_fflush(FILE* fp);
SMBEXPORT int 		smb_fgetc(FILE* fp);
SMBEXPORT int 		smb_fputc(int ch, FILE* fp);
SMBEXPORT int 		smb_fseek(FILE* fp, off_t offset, int whence);
SMBEXPORT off_t		smb_ftell(FILE* fp);
SMBEXPORT size_t	smb_fread(smb_t*, void* buf, size_t bytes, FILE* fp);
SMBEXPORT size_t	smb_fwrite(smb_t*, const void* buf, size_t bytes, FILE* fp);
SMBEXPORT off_t		smb_fgetlength(FILE* fp);
SMBEXPORT int 		smb_fsetlength(FILE* fp, long length);
SMBEXPORT void		smb_rewind(FILE* fp);
SMBEXPORT void		smb_clearerr(FILE* fp);
SMBEXPORT int 		smb_open_fp(smb_t*, FILE**, int share);
SMBEXPORT void		smb_close_fp(FILE**);

/* New FileBase API: */
enum file_detail { file_detail_index, file_detail_normal, file_detail_extdesc, file_detail_metadata };
SMBEXPORT int		smb_addfile(smb_t*, smbfile_t*, int storage, const char* extdesc, const char* metadata, const char* path);
SMBEXPORT int		smb_addfile_withlist(smb_t*, smbfile_t*, int storage, const char* extdesc, str_list_t, const char* path);
SMBEXPORT int		smb_renewfile(smb_t*, smbfile_t*, int storage, const char* path);
SMBEXPORT int		smb_getfile(smb_t*, smbfile_t*, enum file_detail);
SMBEXPORT int		smb_putfile(smb_t*, smbfile_t*);
SMBEXPORT int		smb_findfile(smb_t*, const char* filename, smbfile_t*);
SMBEXPORT int		smb_loadfile(smb_t*, const char* filename, smbfile_t*, enum file_detail);
SMBEXPORT void		smb_freefilemem(smbfile_t*);
SMBEXPORT int		smb_removefile(smb_t*, smbfile_t*);
SMBEXPORT char*		smb_fileidxname(const char* filename, char* buf, size_t);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this #endif statement */
