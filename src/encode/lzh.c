/* lzh.c */

/* Synchronet LZH compression library */

/* $Id$ */

/**************************************************************************** 
 * @format.tab-size 4		(Plain Text/Source Code File Header)			* 
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Rob Swindell's conversion of 1988 LZH (LHarc) encoding functions			* 
 * Based on Japanese version 29-NOV-1988									* 
 * LZSS coded by Haruhiko Okumura											* 
 * Adaptive Huffman Coding coded by Haruyasu Yoshizaki						* 
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* FreeBSD's malloc.h is deprecated, it drops a warning and */
/* #includes <stdlib.h>, which is already here.             */
#if !defined(__unix__)
	#include <malloc.h>
#endif

#include "lzh.h"

/****************************************************************************/
/* Memory allocation macros for various compilers and environments			*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	#if defined(__TURBOC__)
		#define REALLOC(x,y) farrealloc(x,y)
		#define LMALLOC(x) farmalloc(x)
		#define MALLOC(x) farmalloc(x)
		#define LFREE(x) farfree(x)
		#define FREE(x) farfree(x)
	#elif defined(__WATCOMC__)
		#define REALLOC realloc
		#define LMALLOC(x) halloc(x,1)	/* far heap, but slow */
		#define MALLOC malloc			/* far heap, but 64k max */
		#define LFREE hfree
		#define FREE free
	#else	/* Other 16-bit Compiler */
		#define REALLOC realloc
		#define LMALLOC malloc
		#define MALLOC malloc
		#define LFREE free
		#define FREE free
	#endif
#else		/* 32-bit Compiler or Small Memory Model */
	#define REALLOC realloc
	#define LMALLOC malloc
	#define MALLOC malloc
	#define LFREE free
	#define FREE free
#endif



/* LZSS Parameters */

#define LZH_N			4096	/* Size of string buffer */
#define LZH_F			60		/* Size of look-ahead buffer */
#define LZH_THRESHOLD	2
#define LZH_NIL 		LZH_N	/* End of tree's node  */

#ifdef LZH_DYNAMIC_BUF

unsigned char *lzh_text_buf;
short int	lzh_match_position, lzh_match_length,
	  *lzh_lson, *lzh_rson, *lzh_dad;

#else

unsigned char lzh_text_buf[LZH_N + LZH_F - 1];
short int	  lzh_match_position, lzh_match_length,
		lzh_lson[LZH_N + 1], lzh_rson[LZH_N + 257], lzh_dad[LZH_N + 1];

#endif


void lzh_init_tree(void)  /* Initializing tree */
{
	short int  i;

	for (i = LZH_N + 1; i <= LZH_N + 256; i++)
		lzh_rson[i] = LZH_NIL;			/* root */
	for (i = 0; i < LZH_N; i++)
		lzh_dad[i] = LZH_NIL;			/* node */
}

/******************************/
/* Inserting node to the tree */
/* Only used during encoding  */
/******************************/
void lzh_insert_node(short int r)
{
	short int  i, p, cmp;
	unsigned char  *key;
	unsigned c;

	cmp = 1;
	key = lzh_text_buf+r;
	p = LZH_N + 1 + key[0];
	lzh_rson[r] = lzh_lson[r] = LZH_NIL;
	lzh_match_length = 0;
	for ( ; ; ) {
		if (cmp >= 0) {
			if (lzh_rson[p] != LZH_NIL)
				p = lzh_rson[p];
			else {
				lzh_rson[p] = r;
				lzh_dad[r] = p;
				return;
			}
		} else {
			if (lzh_lson[p] != LZH_NIL)
				p = lzh_lson[p];
			else {
				lzh_lson[p] = r;
				lzh_dad[r] = p;
				return;
			}
		}
		for (i = 1; i < LZH_F; i++)
			if ((cmp = key[i] - lzh_text_buf[p + i]) != 0)
				break;
		if (i > LZH_THRESHOLD) {
			if (i > lzh_match_length) {
				lzh_match_position = ((r - p) & (LZH_N - 1)) - 1;
				if ((lzh_match_length = i) >= LZH_F)
					break;
			}
			if (i == lzh_match_length) {
				if ((c = ((r - p) & (LZH_N - 1)) - 1) 
					< (unsigned)lzh_match_position) {
					lzh_match_position = c;
				}
			}
		}
	}
	lzh_dad[r] = lzh_dad[p];
	lzh_lson[r] = lzh_lson[p];
	lzh_rson[r] = lzh_rson[p];
	lzh_dad[lzh_lson[p]] = r;
	lzh_dad[lzh_rson[p]] = r;
	if (lzh_rson[lzh_dad[p]] == p)
		lzh_rson[lzh_dad[p]] = r;
	else
		lzh_lson[lzh_dad[p]] = r;
	lzh_dad[p] = LZH_NIL;  /* remove p */
}

void lzh_delete_node(short int p)  /* Deleting node from the tree */
{
	short int  q;

	if (lzh_dad[p] == LZH_NIL)
		return;			/* unregistered */
	if (lzh_rson[p] == LZH_NIL)
		q = lzh_lson[p];
	else
	if (lzh_lson[p] == LZH_NIL)
		q = lzh_rson[p];
	else {
		q = lzh_lson[p];
		if (lzh_rson[q] != LZH_NIL) {
			do {
				q = lzh_rson[q];
			} while (lzh_rson[q] != LZH_NIL);
			lzh_rson[lzh_dad[q]] = lzh_lson[q];
			lzh_dad[lzh_lson[q]] = lzh_dad[q];
			lzh_lson[q] = lzh_lson[p];
			lzh_dad[lzh_lson[p]] = q;
		}
		lzh_rson[q] = lzh_rson[p];
		lzh_dad[lzh_rson[p]] = q;
	}
	lzh_dad[q] = lzh_dad[p];
	if (lzh_rson[lzh_dad[p]] == p)
		lzh_rson[lzh_dad[p]] = q;
	else
		lzh_lson[lzh_dad[p]] = q;
	lzh_dad[p] = LZH_NIL;
}

/* Huffman coding parameters */

#define LZH_N_CHAR		(256 - LZH_THRESHOLD + LZH_F)
					/* character code (= 0..LZH_N_CHAR-1) */
#define LZH_T		(LZH_N_CHAR * 2 - 1)	/* Size of table */
#define LZH_R		(LZH_T - 1) 		/* root position */
#define MAX_FREQ	0x8000
					/* update when cumulative frequency */
					/* reaches to this value */

/*
 * Tables for encoding/decoding upper 6 bits of
 * sliding dictionary pointer
 */
/* encoder table */
uchar lzh_p_len[64] = {
	0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

uchar lzh_p_code[64] = {
	0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
	0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
	0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
	0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
	0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
	0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
	0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* decoder table */
uchar lzh_d_code[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
	0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
	0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
	0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
	0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
	0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
	0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
	0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
	0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
	0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
	0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
	0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
	0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
	0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

uchar lzh_d_len[256] = {
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};

#ifdef LZH_DYNAMIC_BUF

unsigned short *lzh_freq=NULL;	 /* cumulative freq table */

/*
 * pointing parent nodes.
 * area [LZH_T..(LZH_T + LZH_N_CHAR - 1)] are pointers for leaves
 */
short int *lzh_prnt=NULL;

/* pointing children nodes (son[], son[] + 1)*/
short int *lzh_son=NULL;

#else	/* STATIC */

unsigned short lzh_freq[LZH_T + 1];   /* cumulative freq table */
short int lzh_prnt[LZH_T + LZH_N_CHAR];
short int lzh_son[LZH_T + 1];		  /* bug fixed by Digital Dynamics */

#endif


unsigned short lzh_getbuf = 0;		/* Was just "unsigned" fixed 04/12/95 */
uchar lzh_getlen = 0;

int lzh_getbit(uchar *inbuf, long *incnt, long inlen)    /* get one bit */
{
	short int i;

	while (lzh_getlen <= 8) {
		if((*incnt)>=inlen)
			i=0;
		else
			i=inbuf[(*incnt)++];
		lzh_getbuf |= i << (8 - lzh_getlen);
		lzh_getlen += 8;
	}
	i = lzh_getbuf;
	lzh_getbuf <<= 1;
	lzh_getlen--;
	return (i < 0);
}

short int lzh_getbyte(uchar *inbuf, long *incnt, long inlen)   /* get a byte */
{
	unsigned short i;

	while (lzh_getlen <= 8) {
		if((*incnt)>=inlen)
			i=0;
		else
			i=inbuf[(*incnt)++];
		lzh_getbuf |= i << (8 - lzh_getlen);
		lzh_getlen += 8;
	}
	i = lzh_getbuf;
	lzh_getbuf <<= 8;
	lzh_getlen -= 8;
	return i >> 8;
}

unsigned lzh_putbuf = 0;
uchar lzh_putlen = 0;

/* output c bits */
void lzh_putcode(short int l, unsigned short c, uchar *outbuf, long *outlen)
{
	lzh_putbuf |= c >> lzh_putlen;
	if ((lzh_putlen += l) >= 8) {
		outbuf[(*outlen)++]=(lzh_putbuf >> 8);
		if ((lzh_putlen -= 8) >= 8) {
			outbuf[(*outlen)++]=lzh_putbuf;
			lzh_putlen -= 8;
			lzh_putbuf = c << (l - lzh_putlen);
		} else {
			lzh_putbuf <<= 8;
		}
	}
}


/* initialize freq tree */

void lzh_start_huff(void)
{
	short int i, j;

lzh_getbuf = 0; 	/* Added by Digital Dynamics for repeating operations */
lzh_getlen = 0;
lzh_putbuf = 0;
lzh_putlen = 0;

	for (i = 0; i < LZH_N_CHAR; i++) {
		lzh_freq[i] = 1;
		lzh_son[i] = i + LZH_T;
		lzh_prnt[i + LZH_T] = i;
	}
	i = 0; j = LZH_N_CHAR;
	while (j <= LZH_R) {
		lzh_freq[j] = lzh_freq[i] + lzh_freq[i + 1];
		lzh_son[j] = i;
		lzh_prnt[i] = lzh_prnt[i + 1] = j;
		i += 2; j++;
	}
	lzh_freq[LZH_T] = 0xffff;
    lzh_prnt[LZH_R] = 0;
}


/* reconstruct freq tree */

void lzh_reconst(void)
{
	short int i, j, k;
	unsigned short f, l;

	/* halven cumulative freq for leaf nodes */
	j = 0;
	for (i = 0; i < LZH_T; i++) {
		if (lzh_son[i] >= LZH_T) {
			lzh_freq[j] = (lzh_freq[i] + 1) / 2;
			lzh_son[j] = lzh_son[i];
			j++;
		}
	}
	/* make a tree : first, connect children nodes */
	for (i = 0, j = LZH_N_CHAR; j < LZH_T; i += 2, j++) {
		k = i + 1;
		f = lzh_freq[j] = lzh_freq[i] + lzh_freq[k];
		for (k = j - 1; f < lzh_freq[k]; k--);
		k++;
		l = (j - k) * 2;
		
		/* movmem() is Turbo-C dependent
		   rewritten to memmove() by Kenji */
		
		/* movmem(&lzh_freq[k], &lzh_freq[k + 1], l); */
		(void)memmove(lzh_freq+k+1,lzh_freq+k, l);
		lzh_freq[k] = f;
		/* movmem(&lzh_son[k], &lzh_son[k + 1], l); */
		(void)memmove(lzh_son+k+1,lzh_son+k, l);
		lzh_son[k] = i;
	}
	/* connect parent nodes */
	for (i = 0; i < LZH_T; i++) {
		if ((k = lzh_son[i]) >= LZH_T) {
			lzh_prnt[k] = i;
		} else {
			lzh_prnt[k] = lzh_prnt[k + 1] = i;
		}
	}
}

/* update freq tree */

void lzh_update(short int c)
{
	short int i, j, k, l;

	if (lzh_freq[LZH_R] == MAX_FREQ) {
		lzh_reconst();
	}
	c = lzh_prnt[c + LZH_T];
	do {
		k = ++lzh_freq[c];

		/* swap nodes to keep the tree freq-ordered */
		if (k > lzh_freq[l = c + 1]) {
			while (k > lzh_freq[++l]);
			l--;
			lzh_freq[c] = lzh_freq[l];
			lzh_freq[l] = k;

			i = lzh_son[c];
			lzh_prnt[i] = l;
			if (i < LZH_T) lzh_prnt[i + 1] = l;

			j = lzh_son[l];
			lzh_son[l] = i;

			lzh_prnt[j] = c;
			if (j < LZH_T) lzh_prnt[j + 1] = c;
			lzh_son[c] = j;

			c = l;
		}
	} while ((c = lzh_prnt[c]) != 0);	/* do it until reaching the root */
}

unsigned short lzh_code, lzh_len;

void lzh_encode_char(unsigned short c, uchar *outbuf, long *outlen)
{
	unsigned short i;
	short int j, k;

	i = 0;
	j = 0;
	k = lzh_prnt[c + LZH_T];

	/* search connections from leaf node to the root */
	do {
		i >>= 1;

		/*
		if node's address is odd, output 1
		else output 0
		*/
		if (k & 1) i += 0x8000;

		j++;
	} while ((k = lzh_prnt[k]) != LZH_R);
	lzh_putcode(j, i, outbuf, outlen);
	lzh_code = i;
	lzh_len = j;
	lzh_update(c);
}

void lzh_encode_position(unsigned short c, uchar *outbuf, long *outlen)
{
	unsigned short i;

	/* output upper 6 bits with encoding */
	i = c >> 6;
	lzh_putcode(lzh_p_len[i], (unsigned short)(lzh_p_code[i] << 8), outbuf, outlen);

	/* output lower 6 bits directly */
	lzh_putcode(6, (unsigned short)((c & 0x3f) << 10), outbuf, outlen);
}

void lzh_encode_end(uchar *outbuf, long *outlen)
{
	if (lzh_putlen) {
		outbuf[(*outlen)++]=(lzh_putbuf >> 8);
	}
}

short int lzh_decode_char(uchar *inbuf, long *incnt, long inlen)
{
	unsigned short c;

	c = lzh_son[LZH_R];

	/*
	 * start searching tree from the root to leaves.
	 * choose node #(lzh_son[]) if input bit == 0
	 * else choose #(lzh_son[]+1) (input bit == 1)
	 */
	while (c < LZH_T) {
		c += lzh_getbit(inbuf,incnt,inlen);
		c = lzh_son[c];
	}
	c -= LZH_T;
	lzh_update(c);
	return c;
}

short int lzh_decode_position(uchar *inbuf, long *incnt, long inlen)
{
	unsigned short i, j, c;

	/* decode upper 6 bits from given table */
	i = lzh_getbyte(inbuf,incnt,inlen);
	c = (unsigned)lzh_d_code[i] << 6;
	j = lzh_d_len[i];

	/* input lower 6 bits directly */
	j -= 2;
	while (j--) {
		i = (i << 1) + lzh_getbit(inbuf,incnt,inlen);
	}
	return c | (i & 0x3f);
}

/* Compression */

/* Encoding/Compressing */
/* Returns length of outbuf */
long LZHCALL lzh_encode(uchar *inbuf, long inlen, uchar *outbuf)
{
	short int  i, c, len, r, s, last_match_length;
	long incnt,outlen; /* textsize=0; */

#ifdef LZH_DYNAMIC_BUF

	if((lzh_text_buf=(uchar *)MALLOC(LZH_N + LZH_F - 1))==NULL)
		return(-1);
	if((lzh_freq=(unsigned short*)MALLOC((LZH_T + 1)*sizeof(unsigned short)))==NULL) {
		FREE(lzh_text_buf);
		return(-1); }
	if((lzh_prnt=(short *)MALLOC((LZH_T + LZH_N_CHAR)*sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_freq);
		return(-1); }
	if((lzh_son=(short *)MALLOC((LZH_T + 1) * sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		return(-1); }
	if((lzh_lson=(short *)MALLOC((LZH_N + 1)*sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		FREE(lzh_son);
		return(-1); }
	if((lzh_rson=(short *)MALLOC((LZH_N + 257)*sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		FREE(lzh_son);
		FREE(lzh_lson);
		return(-1); }
	if((lzh_dad=(short *)MALLOC((LZH_N + 1)*sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		FREE(lzh_son);
        FREE(lzh_lson);
		FREE(lzh_rson);
		return(-1); }
#endif

	incnt=0;
	memcpy(outbuf,&inlen,sizeof(inlen));
	outlen=sizeof(inlen);
	if(!inlen) {
#ifdef LZH_DYNAMIC_BUF
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		FREE(lzh_son);
        FREE(lzh_lson);
        FREE(lzh_rson);
		FREE(lzh_dad);
#endif
		return(outlen); }
	lzh_start_huff();
	lzh_init_tree();
	s = 0;
	r = LZH_N - LZH_F;
	for (i = s; i < r; i++)
		lzh_text_buf[i] = ' ';
	for (len = 0; len < LZH_F && incnt<inlen; len++)
		lzh_text_buf[r + len] = inbuf[incnt++];
	/* textsize = len; */
	for (i = 1; i <= LZH_F; i++)
		lzh_insert_node((short)(r - i));
	lzh_insert_node(r);
	do {
		if (lzh_match_length > len)
			lzh_match_length = len;
		if (lzh_match_length <= LZH_THRESHOLD) {
			lzh_match_length = 1;
			lzh_encode_char(lzh_text_buf[r],outbuf,&outlen);
		} else {
			lzh_encode_char((unsigned short)(255 - LZH_THRESHOLD + lzh_match_length)
				,outbuf,&outlen);
			lzh_encode_position(lzh_match_position
				,outbuf,&outlen);
		}
		last_match_length = lzh_match_length;
		for (i = 0; i < last_match_length && incnt<inlen; i++) {
			lzh_delete_node(s);
			c=inbuf[incnt++];
			lzh_text_buf[s] = (uchar)c;
			if (s < LZH_F - 1)
				lzh_text_buf[s + LZH_N] = (uchar)c;
			s = (s + 1) & (LZH_N - 1);
			r = (r + 1) & (LZH_N - 1);
			lzh_insert_node(r);
		}
/***
		if ((textsize += i) > printcount) {
			printf("%12ld\r", textsize);
			printcount += 1024;
		}
***/
		while (i++ < last_match_length) {
			lzh_delete_node(s);
			s = (s + 1) & (LZH_N - 1);
			r = (r + 1) & (LZH_N - 1);
			if (--len) lzh_insert_node(r);
		}
	} while (len > 0);
	lzh_encode_end(outbuf,&outlen);
/*
	printf("input: %ld (%ld) bytes\n", inlen,textsize);
	printf("output: %ld bytes\n", outlen);
	printf("output/input: %.3f\n", (double)outlen / inlen);
*/

#ifdef LZH_DYNAMIC_BUF
	FREE(lzh_text_buf);
	FREE(lzh_prnt);
	FREE(lzh_freq);
	FREE(lzh_son);
	FREE(lzh_lson);
	FREE(lzh_rson);
	FREE(lzh_dad);
#endif

	return(outlen);
}

/* Decoding/Uncompressing */
/* Returns length of outbuf */
long LZHCALL lzh_decode(uchar *inbuf, long inlen, uchar *outbuf)
{
	short int  i, j, k, r, c;
	unsigned long int  count;
	long incnt,textsize;

#ifdef LZH_DYNAMIC_BUF

	if((lzh_text_buf=(uchar *)MALLOC((LZH_N + LZH_F - 1)*2))==NULL)
		return(-1);
	if((lzh_freq=(unsigned short *)MALLOC((LZH_T + 1)*sizeof(unsigned short)))
		==NULL) {
		FREE(lzh_text_buf);
		return(-1); }
	if((lzh_prnt=(short *)MALLOC((LZH_T + LZH_N_CHAR)*sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_freq);
		return(-1); }
	if((lzh_son=(short *)MALLOC((LZH_T + 1) * sizeof(short)))==NULL) {
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		return(-1); }

#endif

	incnt=0;
	memcpy(&textsize,inbuf,sizeof(textsize));
	incnt+=sizeof(textsize);
	if (textsize == 0) {
#ifdef LZH_DYNAMIC_BUF
		FREE(lzh_text_buf);
		FREE(lzh_prnt);
		FREE(lzh_freq);
		FREE(lzh_son);
#endif
		return(textsize); }
	lzh_start_huff();
	for (i = 0; i < LZH_N - LZH_F; i++)
		*(lzh_text_buf+i) = ' ';
	r = LZH_N - LZH_F;
    for (count = 0; count < (unsigned long)textsize; ) {
		c = lzh_decode_char(inbuf,&incnt,inlen);
		if (c < 256) {
			outbuf[count]=(uchar)c;
#if 0
			if(r>(LZH_N + LZH_F - 1) || r<0) {
				printf("Overflow! (%d)\n",r);
				getch();
				exit(-1); }
#endif
			*(lzh_text_buf+r) = (uchar)c;
			r++;
			r &= (LZH_N - 1);
			count++;
		} else {
			i = (r - lzh_decode_position(inbuf,&incnt,inlen) - 1)
				& (LZH_N - 1);
			j = c - 255 + LZH_THRESHOLD;
			for (k = 0; k < j && count<(unsigned long)textsize; k++) {
				c = lzh_text_buf[(i + k) & (LZH_N - 1)];
				outbuf[count]=(uchar)c;
#if 0
				if(r>(LZH_N + LZH_F - 1) || r<0) {
					printf("Overflow! (%d)\n",r);
					exit(-1); }
#endif
				*(lzh_text_buf+r) = (uchar)c;
				r++;
				r &= (LZH_N - 1);
				count++;
			}
		}
	}
/***
	printf("%12ld\n", count);
***/

#ifdef LZH_DYNAMIC_BUF
	FREE(lzh_text_buf);
	FREE(lzh_prnt);
	FREE(lzh_freq);
	FREE(lzh_son);
#endif

return(count);
}


