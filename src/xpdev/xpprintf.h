#ifndef _XPPRINTF_H_
#define _XPPRINTF_H_

/* Supported printf argument types */
#define XP_PRINTF_TYPE_INT			0
#define XP_PRINTF_TYPE_UINT			1
#define XP_PRINTF_TYPE_CHAR			XP_PRINTF_TYPE_INT	/* Assumes a signed char */
#define XP_PRINTF_TYPE_SCHAR		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_UCHAR		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_SHORT		XP_PRINTF_TYPE_INT
#define XP_PRINTF_TYPE_USHORT		XP_PRINTF_TYPE_UINT
#define XP_PRINTF_TYPE_LONG			2
#define XP_PRINTF_TYPE_ULONG		3
#define XP_PRINTF_TYPE_LONGLONG		4
#define XP_PRINTF_TYPE_ULONGLONG	5
#define XP_PRINTF_TYPE_CHARP		6
#define XP_PRINTF_TYPE_DOUBLE		7
#define XP_PRINTF_TYPE_FLOAT		XP_PRINTF_TYPE_DOUBLE	/* Floats are promoted to doubles */
#define XP_PRINTF_TYPE_LONGDOUBLE	8
#define XP_PRINTF_TYPE_VOIDP		9
#define XP_PRINTF_TYPE_INTMAX		10	/* Not currently implemented */
#define XP_PRINTF_TYPE_PTRDIFF		11	/* Not currently implemented */
#define XP_PRINTF_TYPE_SIZET		12
#define XP_PRINTF_TYPE_AUTO			13

#if defined(__cplusplus)
extern "C" {
#endif
char *xp_asprintf_start(char *format);
char *xp_asprintf_next(char *format, int type, ...);
char *xp_asprintf_end(char *format);
#if defined(__cplusplus)
}
#endif

#endif
