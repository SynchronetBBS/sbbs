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

#define upcasestr(str) ({ \
	char *upcasestr_tmp_str = (char *)alloca(strlen(str)+1); \
	char *upcasestr_tmp_ptr; \
\
	strcpy(upcasestr_tmp_str, str); \
	for(upcasestr_tmp_ptr=upcasestr_tmp_str; *upcasestr_tmp_ptr; upcasestr_tmp_ptr++) \
		*upcasestr_tmp_ptr=toupper(*upcasestr_tmp_ptr); \
	upcasestr_tmp_str; \
})

#define commastr(val) (char *)({ \
	char *acommastr_tmp_str = (char *)alloca(64); \
	char *acommastr_tmp_ptr; \
\
	sprintf(acommastr_tmp_str, "%lld", (long long)(val)); \
	for(acommastr_tmp_ptr=strchr(acommastr_tmp_str, 0)-3; acommastr_tmp_ptr > acommastr_tmp_str; acommastr_tmp_ptr-=3) { \
		memmove(acommastr_tmp_ptr+1, acommastr_tmp_ptr, strlen(acommastr_tmp_ptr)+1); \
		*acommastr_tmp_ptr=','; \
	} \
	acommastr_tmp_str; \
})

#define Asprintf(fmt, ...) (char *)({ \
	char *Asprintf_tmp_str; \
	int  len=vsnprintf(NULL, 0, fmt, __VA_ARGS__); \
\
	Asprintf_tmp_str=(char *)alloca(len+1); \
	vsnprintf(Asprintf_tmp_str, len+1, fmt, __VA_ARGS__); \
	Asprintf_tmp_strAsprintf_tmp_str; \
})

#define ljust(str, len) Asprintf("%.*s", len, str)

#define PART(text)	d(config.textcolor, text, NULL)		// GREEN
#define PLAYER(text)	d(config.plycolor, text, NULL)		// BRIGHT_GREEN

#define TEXT(text)	dl(config.textcolor, text, NULL)	// GREEN
#define SAY(text)	dl(config.talkcolor, text, NULL)	// BRIGHT_MAGENTA
#define BAD(text)	dl(config.badcolor, text, NULL)		// BRIGHT_RED
#define GOOD(text)	dl(config.goodcolor, text, NULL)	// WHITE
#define HEADER(text)	dl(config.headercolor, text, NULL)	// MAGENTA
#define NOTICE(text)	dl(config.noticecolor, text, NULL)
#define TITLE(text)	dl(config.titlecolor, text, NULL)

#endif
