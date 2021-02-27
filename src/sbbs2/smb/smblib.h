/* SMBLIB.H */

/* Developed 1990-2000 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _SMBLIB_H
#define _SMBLIB_H

#include "lzh.h"

#if defined(__WATCOMC__) || defined(__TURBOC__) || defined(_MSC_VER)
#	include <io.h>
#	include <share.h>
#endif

#if defined(__WATCOMC__) || defined(__TURBOC__)
#	include <mem.h>
#else
#	include <memory.h>
#endif

#if defined(__WATCOMC__)
#	include <dos.h>
#elif defined(__TURBOC__)
#	include <dir.h>
#endif

#if defined(_WIN32)
#	ifndef __FLAT__
#	define __FLAT__
#	endif
#	define SMBCALL __stdcall	/* VB Compatible */
#	if defined (EXPORT32)
#		undef EXPORT32
#	endif
#	if defined(SMBDLL)
#		define EXPORT32 __declspec( dllexport )
#	else
#		define EXPORT32 __declspec( dllimport )
#	endif
#elif defined(__FLAT__)
#	if defined(SMBDLL)
#		define SMBCALL	_pascal
#		define EXPORT32 _export
#	else
#		define SMBCALL
#		define EXPORT32
#	endif
#
#else
#	define SMBCALL
#	define EXPORT32
#endif

#include <malloc.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "smbdefs.h"

#define SMB_STACK_LEN		4			/* Max msg bases in smb_stack() 	*/
#define SMB_STACK_POP       0           /* Pop a msg base off of smb_stack() */
#define SMB_STACK_PUSH      1           /* Push a msg base onto smb_stack() */
#define SMB_STACK_XCHNG     2           /* Exchange msg base w/last pushed */

#define GETMSGTXT_TAILS 	1			/* Get message tail(s) too */

#ifdef __cplusplus
extern "C" {
#endif

EXPORT32 int 	SMBCALL smb_ver(void);
EXPORT32 char *	SMBCALL smb_lib_ver(void);
EXPORT32 int 	SMBCALL smb_open(smb_t *smb);
EXPORT32 void	SMBCALL smb_close(smb_t *smb);
EXPORT32 int 	SMBCALL smb_open_da(smb_t *smb);
EXPORT32 void	SMBCALL smb_close_da(smb_t *smb);
EXPORT32 int 	SMBCALL smb_open_ha(smb_t *smb);
EXPORT32 void	SMBCALL smb_close_ha(smb_t *smb);
EXPORT32 int 	SMBCALL smb_create(smb_t *smb);
EXPORT32 int 	SMBCALL smb_stack(smb_t *smb, int op);
EXPORT32 int 	SMBCALL smb_trunchdr(smb_t *smb);
EXPORT32 int 	SMBCALL smb_locksmbhdr(smb_t *smb);
EXPORT32 int 	SMBCALL smb_getstatus(smb_t *smb);
EXPORT32 int 	SMBCALL smb_putstatus(smb_t *smb);
EXPORT32 int 	SMBCALL smb_unlocksmbhdr(smb_t *smb);
EXPORT32 int 	SMBCALL smb_getmsgidx(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_getlastidx(smb_t *smb, idxrec_t *idx);
EXPORT32 uint	SMBCALL smb_getmsghdrlen(smbmsg_t *msg);
EXPORT32 ulong	SMBCALL smb_getmsgdatlen(smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_lockmsghdr(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_getmsghdr(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_unlockmsghdr(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_addcrc(smb_t *smb, ulong crc);
EXPORT32 int 	SMBCALL smb_hfield(smbmsg_t *msg, ushort type, ushort length, void *data);
EXPORT32 int 	SMBCALL smb_dfield(smbmsg_t *msg, ushort type, ulong length);
EXPORT32 int 	SMBCALL smb_addmsghdr(smb_t *smb, smbmsg_t *msg,int storage);
EXPORT32 int 	SMBCALL smb_putmsg(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_putmsgidx(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_putmsghdr(smb_t *smb, smbmsg_t *msg);
EXPORT32 void	SMBCALL smb_freemsgmem(smbmsg_t *msg);
EXPORT32 ulong	SMBCALL smb_hdrblocks(ulong length);
EXPORT32 ulong	SMBCALL smb_datblocks(ulong length);
EXPORT32 long	SMBCALL smb_allochdr(smb_t *smb, ulong length);
EXPORT32 long	SMBCALL smb_fallochdr(smb_t *smb, ulong length);
EXPORT32 long	SMBCALL smb_hallochdr(smb_t *smb);
EXPORT32 long	SMBCALL smb_allocdat(smb_t *smb, ulong length, ushort headers);
EXPORT32 long	SMBCALL smb_fallocdat(smb_t *smb, ulong length, ushort headers);
EXPORT32 long	SMBCALL smb_hallocdat(smb_t *smb);
EXPORT32 int 	SMBCALL smb_incdat(smb_t *smb, ulong offset, ulong length, ushort headers);
EXPORT32 int 	SMBCALL smb_freemsg(smb_t *smb, smbmsg_t *msg);
EXPORT32 int 	SMBCALL smb_freemsgdat(smb_t *smb, ulong offset, ulong length, ushort headers);
EXPORT32 int 	SMBCALL smb_freemsghdr(smb_t *smb, ulong offset, ulong length);
EXPORT32 void	SMBCALL smb_freemsgtxt(char HUGE16 *buf);
EXPORT32 char HUGE16 * SMBCALL smb_getmsgtxt(smb_t *smb, smbmsg_t *msg, ulong mode);

/* FILE pointer I/O functions */

EXPORT32 int 	SMBCALL smb_feof(FILE *fp);
EXPORT32 int 	SMBCALL smb_ferror(FILE *fp);
EXPORT32 int 	SMBCALL smb_fflush(FILE *fp);
EXPORT32 int 	SMBCALL smb_fgetc(FILE *fp);
EXPORT32 int 	SMBCALL smb_fputc(int ch, FILE *fp);
EXPORT32 int 	SMBCALL smb_fseek(FILE *fp, long offset, int whence);
EXPORT32 long	SMBCALL smb_ftell(FILE *fp);
EXPORT32 long	SMBCALL smb_fread(void HUGE16 *buf, long bytes, FILE *fp);
EXPORT32 long	SMBCALL smb_fwrite(void HUGE16 *buf, long bytes, FILE *fp);
EXPORT32 long	SMBCALL smb_fgetlength(FILE *fp);
EXPORT32 int 	SMBCALL smb_fsetlength(FILE *fp, long length);
EXPORT32 void	SMBCALL smb_rewind(FILE *fp);
EXPORT32 void	SMBCALL smb_clearerr(FILE *fp);

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
