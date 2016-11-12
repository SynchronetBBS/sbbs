/* Synchronet message base (SMB) library function prototypes */

/* $Id$ */

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

#ifndef _SMBLIB_H
#define _SMBLIB_H

#include "lzh.h"

#ifdef SMBEXPORT
	#undef SMBEXPORT
#endif

#ifdef _WIN32
	#ifdef __BORLANDC__
		#define SMBCALL __stdcall
	#else
		#define SMBCALL
	#endif
	#if defined(SMB_IMPORTS) || defined(SMB_EXPORTS)
		#if defined(SMB_IMPORTS)
			#define SMBEXPORT __declspec( dllimport )
		#else
			#define SMBEXPORT __declspec( dllexport )
		#endif
	#else	/* self-contained executable */
		#define SMBEXPORT
	#endif
#elif defined __unix__
	#define SMBCALL
	#define SMBEXPORT
#else
	#define SMBCALL
	#define SMBEXPORT
#endif

#include "smbdefs.h"

#define SMB_SUCCESS			0			/* Successful result/return code */
#define SMB_FAILURE			-1			/* Generic error (discouraged) */

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
#define SMB_ERR_MEM			-300		/* Memory allocation error */

#define SMB_DUPE_MSG		1			/* Duplicate message detected by smb_addcrc() */

#define SMB_STACK_LEN		4			/* Max msg bases in smb_stack() 	*/
#define SMB_STACK_POP       0           /* Pop a msg base off of smb_stack()*/
#define SMB_STACK_PUSH      1           /* Push a msg base onto smb_stack() */
#define SMB_STACK_XCHNG     2           /* Exchange msg base w/last pushed	*/

#define SMB_ALL_REFS		0			/* Free all references to data		*/

#define GETMSGTXT_TAILS 		(1<<0)	/* Get message tail(s)				*/
#define GETMSGTXT_NO_BODY		(1<<1)	/* Don't retrieve message body		*/
#define GETMSGTXT_NO_HFIELDS	(1<<2)	/* Don't include text header fields	*/
#define GETMSGTXT_PLAIN			(1<<3)	/* Get plaintext portion only of MIME-encoded body (all, otherwise) */
#define GETMSGTXT_BODY_ONLY		GETMSGTXT_NO_HFIELDS
#define GETMSGTXT_TAIL_ONLY		(GETMSGTXT_TAILS|GETMSGTXT_NO_BODY|GETMSGTXT_NO_HFIELDS)
#define GETMSGTXT_ALL			GETMSGTXT_TAILS

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

SMBEXPORT int 		SMBCALL smb_ver(void);
SMBEXPORT char*		SMBCALL smb_lib_ver(void);
SMBEXPORT int 		SMBCALL smb_open(smb_t* smb);
SMBEXPORT void		SMBCALL smb_close(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_create(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_stack(smb_t* smb, int op);
SMBEXPORT int 		SMBCALL smb_trunchdr(smb_t* smb);
SMBEXPORT int		SMBCALL smb_lock(smb_t* smb);
SMBEXPORT int		SMBCALL smb_unlock(smb_t* smb);
SMBEXPORT BOOL		SMBCALL smb_islocked(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_locksmbhdr(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_getstatus(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_putstatus(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_unlocksmbhdr(smb_t* smb);
SMBEXPORT int 		SMBCALL smb_getmsgidx(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_getfirstidx(smb_t* smb, idxrec_t *idx);
SMBEXPORT int 		SMBCALL smb_getlastidx(smb_t* smb, idxrec_t *idx);
SMBEXPORT ulong		SMBCALL smb_getmsghdrlen(smbmsg_t* msg);
SMBEXPORT ulong		SMBCALL smb_getmsgdatlen(smbmsg_t* msg);
SMBEXPORT ulong		SMBCALL smb_getmsgtxtlen(smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_lockmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_getmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_unlockmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_addcrc(smb_t* smb, uint32_t crc);

SMBEXPORT int 		SMBCALL smb_hfield_add(smbmsg_t* msg, uint16_t type, size_t length, void* data, BOOL insert);
SMBEXPORT int		SMBCALL smb_hfield_add_str(smbmsg_t* msg, uint16_t type, const char* str, BOOL insert);
SMBEXPORT int		SMBCALL	smb_hfield_replace(smbmsg_t* msg, uint16_t type, size_t length, void* data);
SMBEXPORT int		SMBCALL	smb_hfield_replace_str(smbmsg_t* msg, uint16_t type, const char* str);
SMBEXPORT int		SMBCALL smb_hfield_append(smbmsg_t* msg, uint16_t type, size_t length, void* data);
SMBEXPORT int		SMBCALL smb_hfield_append_str(smbmsg_t* msg, uint16_t type, const char* data);
SMBEXPORT int		SMBCALL smb_hfield_add_list(smbmsg_t* msg, hfield_t** hfield_list, void** hfield_dat, BOOL insert);
SMBEXPORT int		SMBCALL smb_hfield_add_netaddr(smbmsg_t* msg, uint16_t type, const char* str, uint16_t* nettype, BOOL insert);
/* Convenience macro: */
#define smb_hfield_bin(msg, type, data) smb_hfield_add(msg, type, sizeof(data), &(data), /* insert: */FALSE)
/* Backward compatibility macros: */
#define smb_hfield(msg,type,len,data)	smb_hfield_add(msg, type, len, data, /* insert: */FALSE)
#define smb_hfield_str(msg, type, str)	smb_hfield_add_str(msg, type, str, /* insert: */FALSE)
#define smb_hfield_netaddr(msg, type, str, nettype) smb_hfield_add_netaddr(msg, type, str, nettype, /* insert: */FALSE)

SMBEXPORT int 		SMBCALL smb_dfield(smbmsg_t* msg, uint16_t type, ulong length);
SMBEXPORT void*		SMBCALL smb_get_hfield(smbmsg_t* msg, uint16_t type, hfield_t* hfield);
SMBEXPORT int 		SMBCALL smb_addmsghdr(smb_t* smb, smbmsg_t* msg, int storage);
SMBEXPORT int 		SMBCALL smb_putmsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_putmsgidx(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 		SMBCALL smb_putmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT void		SMBCALL smb_freemsgmem(smbmsg_t* msg);
SMBEXPORT void		SMBCALL smb_freemsghdrmem(smbmsg_t* msg);
SMBEXPORT ulong		SMBCALL smb_hdrblocks(ulong length);
SMBEXPORT ulong		SMBCALL smb_datblocks(ulong length);
SMBEXPORT int		SMBCALL	smb_copymsgmem(smb_t* smb, smbmsg_t* destmsg, smbmsg_t* srcmsg);
SMBEXPORT int		SMBCALL smb_tzutc(int16_t timezone);
SMBEXPORT int		SMBCALL smb_updatethread(smb_t* smb, smbmsg_t* remsg, ulong newmsgnum);
SMBEXPORT int		SMBCALL smb_updatemsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT BOOL		SMBCALL smb_valid_hdr_offset(smb_t* smb, ulong offset);
SMBEXPORT int		SMBCALL smb_init_idx(smb_t* smb, smbmsg_t* msg);
SMBEXPORT BOOL		SMBCALL	smb_voted_already(smb_t*, uint32_t msgnum, const char* name, enum smb_net_type, void* net_addr);

/* smbadd.c */
SMBEXPORT int		SMBCALL smb_addmsg(smb_t* smb, smbmsg_t* msg, int storage, long dupechk_hashes
						,uint16_t xlat, const uchar* body, const uchar* tail);
SMBEXPORT int		SMBCALL smb_addvote(smb_t* smb, smbmsg_t* msg, int storage);

/* smballoc.c */
SMBEXPORT long		SMBCALL smb_allochdr(smb_t* smb, ulong length);
SMBEXPORT long		SMBCALL smb_fallochdr(smb_t* smb, ulong length);
SMBEXPORT long		SMBCALL smb_hallochdr(smb_t* smb);
SMBEXPORT long		SMBCALL smb_allocdat(smb_t* smb, ulong length, uint16_t int16_trefs);
SMBEXPORT long		SMBCALL smb_fallocdat(smb_t* smb, ulong length, uint16_t refs);
SMBEXPORT long		SMBCALL smb_hallocdat(smb_t* smb);
SMBEXPORT int		SMBCALL smb_incmsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs);
SMBEXPORT int 		SMBCALL smb_incmsgdat(smb_t* smb, ulong offset, ulong length, uint16_t refs);
SMBEXPORT int 		SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int		SMBCALL smb_freemsg_dfields(smb_t* smb, smbmsg_t* msg, uint16_t refs);
SMBEXPORT int 		SMBCALL smb_freemsgdat(smb_t* smb, ulong offset, ulong length, uint16_t refs);
SMBEXPORT int 		SMBCALL smb_freemsghdr(smb_t* smb, ulong offset, ulong length);
SMBEXPORT void		SMBCALL smb_freemsgtxt(char* buf);

/* smbhash.c */
SMBEXPORT int		SMBCALL smb_findhash(smb_t* smb, hash_t** compare_list, hash_t* found
										 ,long source_mask, BOOL mark);
SMBEXPORT int		SMBCALL smb_hashmsg(smb_t* smb, smbmsg_t* msg, const uchar* text, BOOL update);
SMBEXPORT hash_t*	SMBCALL	smb_hash(ulong msgnum, uint32_t time, unsigned source
								,unsigned flags, const void* data, size_t length);
SMBEXPORT hash_t*	SMBCALL	smb_hashstr(ulong msgnum, uint32_t time, unsigned source
								,unsigned flags, const char* str);

SMBEXPORT hash_t**	SMBCALL smb_msghashes(smbmsg_t* msg, const uchar* text, long source_mask);
SMBEXPORT int		SMBCALL smb_addhashes(smb_t* smb, hash_t** hash_list, BOOL skip_marked);
SMBEXPORT uint16_t	SMBCALL smb_name_crc(const char* name);
SMBEXPORT uint16_t	SMBCALL smb_subject_crc(const char *subj);
SMBEXPORT void		SMBCALL smb_freehashes(hash_t**);
SMBEXPORT int		SMBCALL	smb_getmsgidx_by_time(smb_t*, idxrec_t*, time_t);

/* Fast look-up functions (using hashes) */
SMBEXPORT int 		SMBCALL smb_getmsgidx_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length);
SMBEXPORT int 		SMBCALL smb_getmsghdr_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length);

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
SMBEXPORT char*		SMBCALL smb_hfieldtype(uint16_t type);
SMBEXPORT uint16_t	SMBCALL smb_hfieldtypelookup(const char*);
SMBEXPORT char*		SMBCALL smb_dfieldtype(uint16_t type);
SMBEXPORT char*		SMBCALL smb_faddrtoa(fidoaddr_t* addr, char* outstr);
SMBEXPORT char*		SMBCALL smb_netaddr(net_t* net);
SMBEXPORT char*		SMBCALL smb_netaddrstr(net_t* net, char* fidoaddr_buf);
SMBEXPORT char*		SMBCALL	smb_nettype(enum smb_net_type);
SMBEXPORT char*		SMBCALL smb_zonestr(int16_t zone, char* outstr);
SMBEXPORT char*		SMBCALL smb_hashsource(smbmsg_t* msg, int source);
SMBEXPORT char*		SMBCALL smb_hashsourcetype(uchar type);
SMBEXPORT fidoaddr_t SMBCALL smb_atofaddr(const fidoaddr_t* sys_addr, const char *str);
SMBEXPORT enum smb_net_type SMBCALL smb_netaddr_type(const char* addr);
SMBEXPORT enum smb_net_type SMBCALL smb_get_net_type_by_addr(const char* addr);
/* smbdump.c */
SMBEXPORT void		SMBCALL smb_dump_msghdr(FILE* fp, smbmsg_t* msg);

/* smbtxt.c */
SMBEXPORT char*		SMBCALL smb_getmsgtxt(smb_t* smb, smbmsg_t* msg, ulong mode);
SMBEXPORT char*		SMBCALL smb_getplaintext(smbmsg_t* msg, char* buf);

/* smbfile.c */
SMBEXPORT int 		SMBCALL smb_feof(FILE* fp);
SMBEXPORT int 		SMBCALL smb_ferror(FILE* fp);
SMBEXPORT int 		SMBCALL smb_fflush(FILE* fp);
SMBEXPORT int 		SMBCALL smb_fgetc(FILE* fp);
SMBEXPORT int 		SMBCALL smb_fputc(int ch, FILE* fp);
SMBEXPORT int 		SMBCALL smb_fseek(FILE* fp, long offset, int whence);
SMBEXPORT long		SMBCALL smb_ftell(FILE* fp);
SMBEXPORT size_t	SMBCALL smb_fread(smb_t*, void* buf, size_t bytes, FILE* fp);
SMBEXPORT size_t	SMBCALL smb_fwrite(smb_t*, const void* buf, size_t bytes, FILE* fp);
SMBEXPORT long		SMBCALL smb_fgetlength(FILE* fp);
SMBEXPORT int 		SMBCALL smb_fsetlength(FILE* fp, long length);
SMBEXPORT void		SMBCALL smb_rewind(FILE* fp);
SMBEXPORT void		SMBCALL smb_clearerr(FILE* fp);
SMBEXPORT int 		SMBCALL smb_open_fp(smb_t* smb, FILE**, int share);
SMBEXPORT void		SMBCALL smb_close_fp(FILE**);
					
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this #endif statement */
