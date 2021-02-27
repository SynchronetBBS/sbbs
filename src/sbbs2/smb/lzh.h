/* LZH.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifdef __NT__
#define LZHCALL _pascal
#else
#define LZHCALL
#endif

#ifndef uchar
#define uchar unsigned char
#endif

long LZHCALL lzh_encode(uchar *inbuf, long inlen, uchar *outbuf);
long LZHCALL lzh_decode(uchar *inbuf, long inlen, uchar *outbuf);

#ifdef __WATCOMC__	/* Use MSC standard (prepended underscore) */
#pragma aux lzh_encode			"_*"
#pragma aux lzh_decode          "_*"
#endif
