/* smblib.h */

/* Synchronet message base (SMB) library function prototypes */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef __FLAT__
	#define __FLAT__	/* only supporting 32-bit targets now */
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

#define SMB_STACK_LEN		4			/* Max msg bases in smb_stack() 	*/
#define SMB_STACK_POP       0           /* Pop a msg base off of smb_stack()*/
#define SMB_STACK_PUSH      1           /* Push a msg base onto smb_stack() */
#define SMB_STACK_XCHNG     2           /* Exchange msg base w/last pushed	*/

#define GETMSGTXT_TAILS 	(1<<0)		/* Get message tail(s)				*/
#define GETMSGTXT_NO_BODY	(1<<1)		/* Do not retrieve message body		*/

#define SMB_IS_OPEN(smb)	((smb)->shd_fp!=NULL)

#ifdef __cplusplus
extern "C" {
#endif

SMBEXPORT int 	SMBCALL smb_ver(void);
SMBEXPORT char*	SMBCALL smb_lib_ver(void);
SMBEXPORT int 	SMBCALL smb_open(smb_t* smb);
SMBEXPORT void	SMBCALL smb_close(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_open_da(smb_t* smb);
SMBEXPORT void	SMBCALL smb_close_da(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_open_ha(smb_t* smb);
SMBEXPORT void	SMBCALL smb_close_ha(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_create(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_stack(smb_t* smb, int op);
SMBEXPORT int 	SMBCALL smb_trunchdr(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_locksmbhdr(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_getstatus(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_putstatus(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_unlocksmbhdr(smb_t* smb);
SMBEXPORT int 	SMBCALL smb_getmsgidx(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_getfirstidx(smb_t* smb, idxrec_t *idx);
SMBEXPORT int 	SMBCALL smb_getlastidx(smb_t* smb, idxrec_t *idx);
SMBEXPORT uint	SMBCALL smb_getmsghdrlen(smbmsg_t* msg);
SMBEXPORT ulong	SMBCALL smb_getmsgdatlen(smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_lockmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_getmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_unlockmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_addcrc(smb_t* smb, ulong crc);
SMBEXPORT int 	SMBCALL smb_hfield(smbmsg_t* msg, ushort type, size_t length, void* data);
SMBEXPORT int 	SMBCALL smb_dfield(smbmsg_t* msg, ushort type, ulong length);
SMBEXPORT void* SMBCALL smb_get_hfield(smbmsg_t* msg, ushort type, hfield_t* hfield);
SMBEXPORT char* SMBCALL smb_hfieldtype(ushort type);
SMBEXPORT char* SMBCALL smb_dfieldtype(ushort type);
SMBEXPORT int 	SMBCALL smb_addmsghdr(smb_t* smb, smbmsg_t* msg,int storage);
SMBEXPORT int 	SMBCALL smb_putmsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_putmsgidx(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_putmsghdr(smb_t* smb, smbmsg_t* msg);
SMBEXPORT void	SMBCALL smb_freemsgmem(smbmsg_t* msg);
SMBEXPORT ulong	SMBCALL smb_hdrblocks(ulong length);
SMBEXPORT ulong	SMBCALL smb_datblocks(ulong length);
SMBEXPORT long	SMBCALL smb_allochdr(smb_t* smb, ulong length);
SMBEXPORT long	SMBCALL smb_fallochdr(smb_t* smb, ulong length);
SMBEXPORT long	SMBCALL smb_hallochdr(smb_t* smb);
SMBEXPORT long	SMBCALL smb_allocdat(smb_t* smb, ulong length, ushort headers);
SMBEXPORT long	SMBCALL smb_fallocdat(smb_t* smb, ulong length, ushort headers);
SMBEXPORT long	SMBCALL smb_hallocdat(smb_t* smb);
SMBEXPORT int	SMBCALL smb_incmsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_incdat(smb_t* smb, ulong offset, ulong length, ushort headers);
SMBEXPORT int 	SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg);
SMBEXPORT int 	SMBCALL smb_freemsgdat(smb_t* smb, ulong offset, ulong length, ushort headers);
SMBEXPORT int 	SMBCALL smb_freemsghdr(smb_t* smb, ulong offset, ulong length);
SMBEXPORT void	SMBCALL smb_freemsgtxt(char* buf);
SMBEXPORT int	SMBCALL	smb_copymsgmem(smbmsg_t* destmsg, smbmsg_t* srcmsg);
SMBEXPORT char* SMBCALL smb_getmsgtxt(smb_t* smb, smbmsg_t* msg, ulong mode);
SMBEXPORT int	SMBCALL smb_tzutc(short timezone);

/* FILE pointer I/O functions */

SMBEXPORT int 	SMBCALL smb_feof(FILE* fp);
SMBEXPORT int 	SMBCALL smb_ferror(FILE* fp);
SMBEXPORT int 	SMBCALL smb_fflush(FILE* fp);
SMBEXPORT int 	SMBCALL smb_fgetc(FILE* fp);
SMBEXPORT int 	SMBCALL smb_fputc(int ch, FILE* fp);
SMBEXPORT int 	SMBCALL smb_fseek(FILE* fp, long offset, int whence);
SMBEXPORT long	SMBCALL smb_ftell(FILE* fp);
SMBEXPORT long	SMBCALL smb_fread(void HUGE16* buf, long bytes, FILE* fp);
SMBEXPORT long	SMBCALL smb_fwrite(void HUGE16* buf, long bytes, FILE* fp);
SMBEXPORT long	SMBCALL smb_fgetlength(FILE* fp);
SMBEXPORT int 	SMBCALL smb_fsetlength(FILE* fp, long length);
SMBEXPORT void	SMBCALL smb_rewind(FILE* fp);
SMBEXPORT void	SMBCALL smb_clearerr(FILE* fp);

#ifdef __cplusplus
}
#endif

#ifdef __WATCOMC__	/* Use MSC standard (prepended underscore) */
#pragma aux smb_ver 			"_*"
#pragma aux smb_lib_ver 		"_*"
#pragma aux smb_open			"_*"
#pragma aux smb_close			"_*"
#pragma aux smb_open_da 		"_*"
#pragma aux smb_close_da		"_*"
#pragma aux smb_open_ha 		"_*"
#pragma aux smb_close_ha		"_*"
#pragma aux smb_create			"_*"
#pragma aux smb_stack			"_*"
#pragma aux smb_trunchdr		"_*"
#pragma aux smb_locksmbhdr		"_*"
#pragma aux smb_getstatus		"_*"
#pragma aux smb_putstatus		"_*"
#pragma aux smb_unlocksmbhdr	"_*"
#pragma aux smb_getmsgidx		"_*"
#pragma aux smb_getfirstidx		"_*"
#pragma aux smb_getlastidx		"_*"
#pragma aux smb_getmsghdrlen	"_*"
#pragma aux smb_getmsgdatlen	"_*"
#pragma aux smb_lockmsghdr		"_*"
#pragma aux smb_getmsghdr		"_*"
#pragma aux smb_unlockmsghdr	"_*"
#pragma aux smb_addcrc			"_*"
#pragma aux smb_hfield			"_*"
#pragma aux smb_dfield			"_*"
#pragma aux smb_addmsghdr		"_*"
#pragma aux smb_putmsg			"_*"
#pragma aux smb_putmsgidx		"_*"
#pragma aux smb_putmsghdr		"_*"
#pragma aux smb_freemsgmem		"_*"
#pragma aux smb_hdrblocks		"_*"
#pragma aux smb_datblocks		"_*"
#pragma aux smb_allochdr		"_*"
#pragma aux smb_fallochdr		"_*"
#pragma aux smb_hallochdr		"_*"
#pragma aux smb_allocdat		"_*"
#pragma aux smb_fallocdat		"_*"
#pragma aux smb_hallocdat		"_*"
#pragma aux smb_incdat			"_*"
#pragma aux smb_freemsg 		"_*"
#pragma aux smb_freemsgdat		"_*"
#pragma aux smb_freemsghdr		"_*"
#pragma aux smb_getmsgtxt		"_*"
#pragma aux smb_freemsgtxt		"_*"
#pragma aux smb_feof			"_*"
#pragma aux smb_ferror			"_*"
#pragma aux smb_fflush			"_*"
#pragma aux smb_fgetc			"_*"
#pragma aux smb_fputc			"_*"
#pragma aux smb_fseek			"_*"
#pragma aux smb_ftell			"_*"
#pragma aux smb_fread			"_*"
#pragma aux smb_fwrite			"_*"
#pragma aux smb_fgetlength		"_*"
#pragma aux smb_fsetlength		"_*"
#pragma aux smb_rewind			"_*"
#pragma aux smb_clearerr		"_*"
#pragma aux lzh_encode			"_*"
#pragma aux lzh_decode			"_*"
#endif	/* Watcom */


#endif /* Don't add anything after this #endif statement */
