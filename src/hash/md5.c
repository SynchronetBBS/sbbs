/* md5.c - RSA Data Security, Inc., MD5 Message-Digest Algorithm */

/* $Id: md5.c,v 1.7 2012/10/23 07:59:36 deuce Exp $ */

/* NOTE: Numerous changes have been made; the following notice is
included to satisfy legal requirements.

Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
*/

#include <memory.h>
#include "md5.h"

#if !defined(BIG_ENDIAN)
	#define LITTLE_ENDIAN	/* Little Endian by default */
#endif

void MD5CALL MD5_open(MD5 *md5)
{
  md5->count[0] = md5->count[1] = 0;
  /* Load magic initialization constants.*/
  md5->state[0] = 0x67452301;
  md5->state[1] = 0xefcdab89;
  md5->state[2] = 0x98badcfe;
  md5->state[3] = 0x10325476;
}

/* Constants for MD5Transform routine. */

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

/* F, G, H and I are basic MD5 functions. */

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits. */

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* Round1, Round2, Round3, and Round4 transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation.
*/

#define Round1(a, b, c, d, x, s, ac) { \
 (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT((a), (s)); \
 (a) += (b); \
  }
#define Round2(a, b, c, d, x, s, ac) { \
 (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT((a), (s)); \
 (a) += (b); \
  }
#define Round3(a, b, c, d, x, s, ac) { \
 (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT((a), (s)); \
 (a) += (b); \
  }
#define Round4(a, b, c, d, x, s, ac) { \
 (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT((a), (s)); \
 (a) += (b); \
  }


/* MD5 basic transformation. Transforms state based on block. */

static void MD5Transform(uint32_t state[4], const BYTE block[64])
{
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[MD5_DIGEST_SIZE];
  /* Move contents of block to x, putting bytes in little-endian order. */
  #ifdef LITTLE_ENDIAN
    memcpy(x, block, 64);
  #else
  {
    unsigned int i, j;
    for (i = j = 0; i < MD5_DIGEST_SIZE; i++, j+= 4)
    {
      x[i] = (uint32_t) block[j] | (uint32_t) block[j+1] << 8 |
        (uint32_t) block[j+2] << 16 | (uint32_t) block[j+3] << 24;
    }
  }
  #endif
  /* Round 1 */
  Round1(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  Round1(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  Round1(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  Round1(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  Round1(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  Round1(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  Round1(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  Round1(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  Round1(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  Round1(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  Round1(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  Round1(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  Round1(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  Round1(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  Round1(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  Round1(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */
  /* Round 2 */
  Round2(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  Round2(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  Round2(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  Round2(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  Round2(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  Round2(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  Round2(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  Round2(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  Round2(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  Round2(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  Round2(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  Round2(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  Round2(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  Round2(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  Round2(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  Round2(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
  /* Round 3 */
  Round3(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  Round3(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  Round3(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  Round3(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  Round3(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  Round3(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  Round3(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  Round3(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  Round3(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  Round3(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  Round3(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  Round3(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  Round3(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  Round3(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  Round3(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  Round3(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
  /* Round 4 */
  Round4(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  Round4(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  Round4(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  Round4(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  Round4(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  Round4(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  Round4(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  Round4(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  Round4(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  Round4(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  Round4(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  Round4(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  Round4(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  Round4(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  Round4(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  Round4(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  /* Zeroize sensitive information. */
  memset(x, 0, sizeof(x));
}

void MD5CALL MD5_digest(MD5 *md5, const void *input, size_t inputLen)
{
  unsigned int i, index, partLen;
  /* Compute number of bytes mod 64 */
  index = (unsigned int)((md5->count[0] >> 3) & 0x3F);
  /* Update number of bits */
  if ((md5->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3))
    md5->count[1]++;
  md5->count[1] += ((uint32_t)inputLen >> 29);
  partLen = 64 - index;
  /* Transform as many times as possible.*/
  if (inputLen >= partLen)
  {
    memcpy(&md5->buffer[index], input, partLen);
    MD5Transform(md5->state, md5->buffer);
    for (i = partLen; i + 63 < inputLen; i += 64)
      MD5Transform(md5->state, (BYTE *) input + i);
    index = 0;
  }
  else
    i = 0;
  /* Buffer remaining input */
  memcpy(&md5->buffer[index], (char *) input + i, inputLen-i);
}

/* ENCODE packs a 32-bit unsigned integer into 4 bytes in little-endian
   order.
*/

#ifdef LITTLE_ENDIAN
#define ENCODE(p,n) *(uint32_t *)(p) = n
#else
#define ENCODE(p,n) (p)[0]=n,(p)[1]=n>>8,(p)[2]=n>>16,(p)[3]=n>>24
#endif

void MD5CALL MD5_close(MD5 *md5, BYTE digest[MD5_DIGEST_SIZE])
{
  BYTE bits[8];
  unsigned int index, padLen;
  static BYTE PADDING[64] =
  {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
  /* Save number of bits */
  ENCODE(bits, md5->count[0]);
  ENCODE(bits+4, md5->count[1]);
  /* Pad out to 56 mod 64. */
  index = (unsigned int)((md5->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5_digest(md5, PADDING, padLen);
  /* Append length (before padding) */
  MD5_digest(md5, bits, 8);
  /* Store state in digest */
  ENCODE(digest, md5->state[0]);
  ENCODE(digest+4, md5->state[1]);
  ENCODE(digest+8, md5->state[2]);
  ENCODE(digest+12, md5->state[3]);
  /* Zeroize sensitive information. */
  memset(md5, 0, sizeof(MD5));
}

BYTE* MD5CALL MD5_calc(BYTE digest[MD5_DIGEST_SIZE], const void* buf, size_t len)
{
	MD5 ctx;

	MD5_open(&ctx);
	MD5_digest(&ctx,buf,len);
	MD5_close(&ctx,digest);

	return(digest);
}

/* conversion for 16 character binary md5 to hex */

BYTE* MD5CALL MD5_hex(BYTE* to, const BYTE digest[MD5_DIGEST_SIZE])
{
	BYTE const* from = digest;
    static char *hexdigits = "0123456789abcdef";
    const BYTE *end = digest + MD5_DIGEST_SIZE;
    char *d = (char *)to;

    while (from < end) {
		*d++ = hexdigits[(*from >> 4)];
		*d++ = hexdigits[(*from & 0x0F)];
		from++;
    }
    *d = '\0';
    return to;
}

#ifdef MD5_TEST

int main(int argc, char**argv)
{
	int		i;
	char	hexbuf[(MD5_DIGEST_SIZE*2)+1];
	BYTE	digest[MD5_DIGEST_SIZE];

	for(i=1;i<argc;i++)
		printf("%s\n"
			,MD5_hex(hexbuf,MD5_calc(digest,argv[i],strlen(argv[i]))));

	return 0;
}

#endif
