#ifndef _XPPRINTF_H_
#define _XPPRINTF_H_

#include <stdarg.h>

/* Supported printf argument types */
#define XP_PRINTF_TYPE_AUTO			0
#define XP_PRINTF_TYPE_INT			1
#define XP_PRINTF_TYPE_UINT			2
#define XP_PRINTF_TYPE_CHAR			XP_PRINTF_TYPE_INT	/* Assumes a signed char */
#define XP_PRINTF_TYPE_SCHAR		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_UCHAR		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_SHORT		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_USHORT		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_LONG			3
#define XP_PRINTF_TYPE_ULONG		4
#if !defined(_MSC_VER) && !defined(__BORLANDC__)
	#define XP_PRINTF_TYPE_LONGLONG		5
	#define XP_PRINTF_TYPE_ULONGLONG	6
#endif
#define XP_PRINTF_TYPE_CHARP		7
#define XP_PRINTF_TYPE_DOUBLE		8
#define XP_PRINTF_TYPE_FLOAT		XP_PRINTF_TYPE_DOUBLE	/* Floats are promoted to doubles */
#define XP_PRINTF_TYPE_LONGDOUBLE	9
#define XP_PRINTF_TYPE_VOIDP		10
#define XP_PRINTF_TYPE_INTMAX		11	/* Not currently implemented */
#define XP_PRINTF_TYPE_UINTMAX		12	/* Not currently implemented */
#define XP_PRINTF_TYPE_PTRDIFF		13	/* Not currently implemented */
#define XP_PRINTF_TYPE_SIZET		14

#define XP_PRINTF_CONVERT		(1<<31)	/* OR with type to request a conversion - Not implemented */

#if defined(__cplusplus)
extern "C" {
#endif
char *xp_asprintf_start(const char *format);
char *xp_asprintf_next(char *format, int type, ...);
char *xp_asprintf_end(char *format);
char *xp_asprintf(const char *format, ...);
char *xp_vasprintf(const char *format, va_list va);
int xp_printf_get_type(const char *format);
#if defined(__cplusplus)
}
#endif

#endif
