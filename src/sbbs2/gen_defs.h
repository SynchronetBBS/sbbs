/* GEN_DEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _GEN_DEFS_H
#define _GEN_DEFS_H

/************************************************************************/
/* General (application independant) type definitions and macros		*/
/* tabstop: 4 (as usual)												*/
/************************************************************************/

									/* Control characters */
#define STX 	0x02				/* Start of text			^B	*/
#define ETX 	0x03				/* End of text				^C	*/
#define BS		0x08				/* Back space				^H	*/
#define TAB 	0x09				/* Horizontal tabulation	^I	*/
#define LF		0x0a				/* Line feed				^J	*/
#define FF		0x0c				/* Form feed				^L	*/
#define CR		0x0d				/* Carriage return			^M	*/
#define ESC 	0x1b				/* Escape					^[	*/
#define SP		0x20				/* Space						*/

									/* Unsigned type short-hands	*/
#define uchar	unsigned char
#define ushort  unsigned short
#define uint    unsigned int
#define ulong   unsigned long

/****************************************************************************/
/* MALLOC/FREE Macros for various compilers and environments				*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/* FAR16 is used to create a far (32-bit) pointer in 16-bit compilers		*/
/* HUGE16 is used to create a huge (32-bit) pointer in 16-bit compilers 	*/
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	#define HUGE16 huge
	#define FAR16 far
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
	#define HUGE16
	#define FAR16
	#define REALLOC realloc
	#define LMALLOC malloc
	#define MALLOC malloc
	#define LFREE free
	#define FREE free
#endif


#endif /* Don't add anything after this #endif statement */
