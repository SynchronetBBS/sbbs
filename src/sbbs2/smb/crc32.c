/* CRC32.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* 32-bit CRC of a buffer */

#include "smblib.h"
#include "crc32.h"

#ifdef __NT__
#	define CRCCALL _pascal
#else
#	define CRCCALL
#endif

ulong CRCCALL crc32(char *buf, ulong len)
{
	ulong l,crc=0xffffffff;

for(l=0;l<len;l++)
	crc=ucrc32(buf[l],crc);
return(~crc);
}
