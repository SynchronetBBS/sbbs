#ifndef _MACROS_H_
#define _MACROS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CRASH		{ fprintf(stderr, "Unhandled exception at %s:%d errno=%d\n", __FILE__,__LINE__,errno); abort(); }

#define CRASHF(...)	({ fprintf(stderr, __VA_ARGS__); CRASH; })

enum colors {
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	GREY,
	LIGHT_GREY=GREY,

	BRIGHT_BLACK,
	DARK_GREY=BRIGHT_BLACK,
	BRIGHT_BLUE,
	BRIGHT_GREEN,
	BRIGHT_CYAN,
	BRIGHT_RED,
	BRIGHT_MAGENTA,
	BRIGHT_BROWN,
	YELLOW=BRIGHT_BROWN,
	BRIGHT_GREY,
	WHITE=BRIGHT_GREY,

	D_DONE = 127
};

#define mkstring(len, ch) ({ \
	char *mkstring_tmp_str = (char *)alloca((len)+1); \
\
	if(mkstring_tmp_str == NULL) \
		CRASH; \
	memset(mkstring_tmp_str, (ch), (len)); \
	mkstring_tmp_str[(len)] = 0; \
	mkstring_tmp_str; \
})

#define supcasestr(dest, str) ({ \
	char *upcasestr_tmp_ptr; \
\
	strcpy(dest, str); \
	for(upcasestr_tmp_ptr=dest; *upcasestr_tmp_ptr; upcasestr_tmp_ptr++) \
		*upcasestr_tmp_ptr=toupper(*upcasestr_tmp_ptr); \
	dest; \
})

#define upcasestr(str) ({ \
	char *upcasestr_tmp_str = (char *)alloca(strlen(str)+1); \
	supcasestr(upcasestr_tmp_str,str); \
})

#define scommastr(str, val) (char *)({ \
	char *scommastr_tmp_ptr; \
\
	if(sprintf(str, "%lld", (long long)(val))<0) \
		CRASH; \
	for(scommastr_tmp_ptr=strchr(str, 0)-3; scommastr_tmp_ptr > str; scommastr_tmp_ptr-=3) { \
		memmove(scommastr_tmp_ptr+1, scommastr_tmp_ptr, strlen(scommastr_tmp_ptr)+1); \
		*scommastr_tmp_ptr=','; \
	} \
	str; \
})

#define commastr(val) (char *)({ \
	char *acommastr_tmp_str = (char *)alloca(64); \
	scommastr(acommastr_tmp_str, val); \
	acommastr_tmp_str; \
})

#define Asprintf(fmt, ...) (char *)({ \
	char *Asprintf_tmp_str; \
	int  len=snprintf(NULL, 0, fmt, __VA_ARGS__); \
\
	Asprintf_tmp_str=(char *)alloca(len+1); \
	snprintf(Asprintf_tmp_str, len+1, fmt, __VA_ARGS__); \
	Asprintf_tmp_str; \
})

#define ljust(str, len) Asprintf("%-*s", len, str)

#define kingstring(sex)		(sex==Male?"King":"Queen")

#define PART(...)	d(config.textcolor, __VA_ARGS__, NULL)			// GREEN
#define PLAYER(...)	d(config.plycolor, __VA_ARGS__, NULL)			// BRIGHT_GREEN

#define TEXT(...)	dl(config.textcolor, __VA_ARGS__, NULL)	// GREEN
#define SAY(...)	dl(config.talkcolor, __VA_ARGS__, NULL)			// BRIGHT_MAGENTA
#define BAD(...)	dl(config.badcolor, __VA_ARGS__, NULL)			// BRIGHT_RED
#define GOOD(...)	dl(config.goodcolor, __VA_ARGS__, NULL)			// WHITE
#define HEADER(...)	dl(config.headercolor, __VA_ARGS__, NULL)		// MAGENTA
#define NOTICE(...)	dl(config.noticecolor, __VA_ARGS__, NULL)
#define TITLE(...)	dl(config.titlecolor, __VA_ARGS__, NULL)
#define EVENT(...)	dl(config.eventcolor, __VA_ARGS__, NULL)
#define D(...)		d(..., NULL)
#define DC(...)		d(..., D_DONE)
#define DL(...)		d(..., NULL)
#define DLC(...)	d(..., D_DONE)

#endif
