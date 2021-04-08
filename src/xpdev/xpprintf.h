/* Deuce's vs[n]printf() replacement */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _XPPRINTF_H_
#define _XPPRINTF_H_

#include <stdarg.h>
#include "wrapdll.h"

/* Supported printf argument types */
#define XP_PRINTF_TYPE_AUTO			0
#define XP_PRINTF_TYPE_INT			1
#define XP_PRINTF_TYPE_UINT			2
#define XP_PRINTF_TYPE_CHAR			3		/* Assumes a signed char */
#define XP_PRINTF_TYPE_SCHAR		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_UCHAR		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_SHORT		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_USHORT		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_LONG			4
#define XP_PRINTF_TYPE_ULONG		5
#if !defined(_MSC_VER) && !defined(__BORLANDC__)
	#define XP_PRINTF_TYPE_LONGLONG		6
	#define XP_PRINTF_TYPE_ULONGLONG	7
#endif
#define XP_PRINTF_TYPE_CHARP		8
#define XP_PRINTF_TYPE_DOUBLE		9
#define XP_PRINTF_TYPE_FLOAT		XP_PRINTF_TYPE_DOUBLE	/* Floats are promoted to doubles */
#define XP_PRINTF_TYPE_LONGDOUBLE	10
#define XP_PRINTF_TYPE_VOIDP		11
#define XP_PRINTF_TYPE_INTMAX		12	/* Not currently implemented */
#define XP_PRINTF_TYPE_UINTMAX		13	/* Not currently implemented */
#define XP_PRINTF_TYPE_PTRDIFF		14	/* Not currently implemented */
#define XP_PRINTF_TYPE_SIZET		15

#define XP_PRINTF_CONVERT		(1<<31)	/* OR with type to request a conversion - Not implemented */

#if defined(__cplusplus)
extern "C" {
#endif
DLLEXPORT void xp_asprintf_free(char *format);
DLLEXPORT char* xp_asprintf_start(const char *format);
DLLEXPORT char* xp_asprintf_next(char *format, int type, ...);
DLLEXPORT char* xp_asprintf_end(char *format, size_t *endlen);
DLLEXPORT char* xp_asprintf(const char *format, ...);
DLLEXPORT char* xp_vasprintf(const char *format, va_list va);
DLLEXPORT int xp_printf_get_type(const char *format);
#if defined(_MSC_VER) || defined(__MSVCRT__) || defined(__BORLANDC__)
DLLEXPORT int vasprintf(char **strptr, const char *format, va_list va);
DLLEXPORT int asprintf(char **strptr, const char *format, ...);
#endif

#if defined(__cplusplus)
}
#endif

#endif
