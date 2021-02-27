/* SMBLIB.H */

#ifndef _SMBLIB_H
#define _SMBLIB_H

#if defined(__WATCOMC__) || defined(__TURBOC__)
#	include <io.h>
#	include <mem.h>
#	include <share.h>
#else
#	include <memory.h>
#endif

#ifdef __WATCOMC__
#	include <dos.h>
#elif defined(__TURBOC__)
#	include <dir.h>
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

#define GLOBAL extern	/* turn smbvars.c files into header */

#include "smbvars.c"

#define SMB_STACK_LEN		4			/* Max msg bases in smb_stack() 	*/
#define SMB_STACK_POP       0           /* Pop a msg base off of smb_stack() */
#define SMB_STACK_PUSH      1           /* Push a msg base onto smb_stack() */
#define SMB_STACK_XCHNG     2           /* Exchange msg base w/last pushed */

int 	smb_open(int retry_time);
void	smb_close(void);
int 	smb_open_da(int retry_time);
int 	smb_open_ha(int retry_time);
int 	smb_create(ulong max_crcs, ulong max_msgs, ushort max_age, ushort attr
				  ,int retry_time);
int 	smb_stack(int op);
int 	smb_trunchdr(int retry_time);
int 	smb_locksmbhdr(int retry_time);
int 	smb_getstatus(smbstatus_t *status);
int 	smb_putstatus(smbstatus_t status);
int 	smb_unlocksmbhdr(void);
int 	smb_getmsgidx(smbmsg_t *msg);
int 	smb_getlastidx(idxrec_t *idx);
uint	smb_getmsghdrlen(smbmsg_t msg);
ulong	smb_getmsgdatlen(smbmsg_t msg);
int 	smb_lockmsghdr(smbmsg_t msg, int retry_time);
int 	smb_getmsghdr(smbmsg_t *msg);
int 	smb_unlockmsghdr(smbmsg_t msg);
int 	smb_addcrc(ulong max_crcs, ulong crc, int retry_time);
int 	smb_hfield(smbmsg_t *msg, ushort type, ushort length, void *data);
int 	smb_dfield(smbmsg_t *msg, ushort type, ulong length);
int 	smb_addmsghdr(smbmsg_t *msg, smbstatus_t *status, int storage
				,int retry_time);
int 	smb_putmsg(smbmsg_t msg);
int 	smb_putmsgidx(smbmsg_t msg);
int 	smb_putmsghdr(smbmsg_t msg);
void	smb_freemsgmem(smbmsg_t msg);
ulong	smb_hdrblocks(ulong length);
ulong	smb_datblocks(ulong length);
long	smb_allochdr(ulong length);
long	smb_fallochdr(ulong length);
long	smb_hallochdr(ulong header_offset);
long	smb_allocdat(ulong length, ushort headers);
long	smb_fallocdat(ulong length, ushort headers);
long	smb_hallocdat(void);
int 	smb_incdat(ulong offset, ulong length, ushort headers);
int 	smb_freemsg(smbmsg_t msg, smbstatus_t status);
int 	smb_freemsgdat(ulong offset, ulong length, ushort headers);
int 	smb_freemsghdr(ulong offset, ulong length);

#endif /* Don't add anything after this #endif statement */
