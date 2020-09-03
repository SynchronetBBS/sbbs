/* lzh.h */

/* Synchronet LZH compression library */

/* $Id: lzh.h,v 1.13 2019/03/22 21:29:12 rswindell Exp $ */

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

#ifndef _LZH_H_
#define _LZH_H_

#ifdef LZHEXPORT
	#undef LZHEXPORT
#endif

#ifdef _WIN32
	#ifdef __BORLANDC__
		#define LZHCALL
	#else
		#define LZHCALL
	#endif
	#ifdef LZHDLL	/* LZH functions in DLL */
		#ifdef LZH_EXPORTS
			#define LZHEXPORT __declspec( dllexport )
		#else
			#define LZHEXPORT __declspec( dllimport )
		#endif
	#else			/* self-contained executable */
		#define LZHEXPORT
	#endif
#else	/* !_WIN32 */
	#define LZHCALL
	#define LZHEXPORT
#endif

#include "gen_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
LZHEXPORT int32_t LZHCALL lzh_encode(uint8_t *inbuf, int32_t inlen, uint8_t *outbuf);
LZHEXPORT int32_t LZHCALL lzh_decode(uint8_t *inbuf, int32_t inlen, uint8_t *outbuf);
#ifdef __cplusplus
}
#endif

#ifdef __WATCOMC__	/* Use MSC standard (prepended underscore) */
#pragma aux lzh_encode			"_*"
#pragma aux lzh_decode          "_*"
#endif

#endif /* Do not add anything after this line */
