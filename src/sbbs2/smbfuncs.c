/* SMBFUNCS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "smbdefs.h"

int 	SMBCALL (smb_ver)(void);
char *	SMBCALL (smb_lib_ver)(void);
int 	SMBCALL (smb_open)(smb_t *smb);
void	SMBCALL (smb_close)(smb_t *smb);
int 	SMBCALL (smb_open_da)(smb_t *smb);
void	SMBCALL (smb_close_da)(smb_t *smb);
int 	SMBCALL (smb_open_ha)(smb_t *smb);
void	SMBCALL (smb_close_ha)(smb_t *smb);
int 	SMBCALL (smb_create)(smb_t *smb);
int 	SMBCALL (smb_stack)(smb_t *smb, int op);
int 	SMBCALL (smb_trunchdr)(smb_t *smb);
int 	SMBCALL (smb_locksmbhdr)(smb_t *smb);
int 	SMBCALL (smb_getstatus)(smb_t *smb);
int 	SMBCALL (smb_putstatus)(smb_t *smb);
int 	SMBCALL (smb_unlocksmbhdr)(smb_t *smb);
int 	SMBCALL (smb_getmsgidx)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_getlastidx)(smb_t *smb, idxrec_t *idx);
uint	SMBCALL (smb_getmsghdrlen)(smbmsg_t *msg);
ulong	SMBCALL (smb_getmsgdatlen)(smbmsg_t *msg);
int 	SMBCALL (smb_lockmsghdr)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_getmsghdr)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_unlockmsghdr)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_addcrc)(smb_t *smb, ulong crc);
int 	SMBCALL (smb_hfield)(smbmsg_t *msg, ushort type, ushort length
				,void *data);
int 	SMBCALL (smb_dfield)(smbmsg_t *msg, ushort type, ulong length);
int 	SMBCALL (smb_addmsghdr)(smb_t *smb, smbmsg_t *msg,int storage);
int 	SMBCALL (smb_putmsg)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_putmsgidx)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_putmsghdr)(smb_t *smb, smbmsg_t *msg);
void	SMBCALL (smb_freemsgmem)(smbmsg_t *msg);
ulong	SMBCALL (smb_hdrblocks)(ulong length);
ulong	SMBCALL (smb_datblocks)(ulong length);
long	SMBCALL (smb_allochdr)(smb_t *smb, ulong length);
long	SMBCALL (smb_fallochdr)(smb_t *smb, ulong length);
long	SMBCALL (smb_hallochdr)(smb_t *smb);
long	SMBCALL (smb_allocdat)(smb_t *smb, ulong length, ushort headers);
long	SMBCALL (smb_fallocdat)(smb_t *smb, ulong length, ushort headers);
long	SMBCALL (smb_hallocdat)(smb_t *smb);
int 	SMBCALL (smb_incdat)(smb_t *smb, ulong offset, ulong length
				,ushort headers);
int 	SMBCALL (smb_freemsg)(smb_t *smb, smbmsg_t *msg);
int 	SMBCALL (smb_freemsgdat)(smb_t *smb, ulong offset, ulong length
				,ushort headers);
int 	SMBCALL (smb_freemsghdr)(smb_t *smb, ulong offset, ulong length);
char HUGE16 * SMBCALL smb_getmsgtxt(smb_t *smb, smbmsg_t *msg, ulong mode);
void	SMBCALL (smb_freemsgtxt)(char HUGE16 *buf);

/* FILE pointer I/O functions */

int 	SMBCALL (smb_feof(FILE *fp);
int 	SMBCALL (smb_ferror(FILE *fp);
int 	SMBCALL (smb_fflush(FILE *fp);
int 	SMBCALL (smb_fgetc(FILE *fp);
int 	SMBCALL (smb_fputc(int ch, FILE *fp);
int 	SMBCALL (smb_fseek(FILE *fp, long offset, int whence);
long	SMBCALL (smb_ftell(FILE *fp);
long	SMBCALL (smb_fread(char HUGE16 *buf, long bytes, FILE *fp);
long	SMBCALL (smb_fwrite(char HUGE16 *buf, long bytes, FILE *fp);
long	SMBCALL (smb_fgetlength(FILE *fp);
int 	SMBCALL (smb_fsetlength(FILE *fp, long length);
void	SMBCALL (smb_rewind(FILE *fp);
void	SMBCALL (smb_clearerr(FILE *fp);

/* LZH functions */

long	SMBCALL (lzh_encode(uchar *inbuf, long inlen, uchar *outbuf);
long	SMBCALL (lzh_decode(uchar *inbuf, long inlen, uchar *outbuf);

#endif /* Don't add anything after this #endif statement */
