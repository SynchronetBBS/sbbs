#ifndef _MACROS_H_
#define _MACROS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CRASH		{ fprintf(stderr, "Unhandled exception at %s:%d errno=%d\n", __FILE__,__LINE__,errno); abort(); }

#define CRASHF(...)	({ fprintf(stderr, __VA_ARGS__); CRASH; })

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

#define MANY_MONEY(x)		((x)==1?config.moneytype2:config.moneytype3)

#define SLEEP(x)		({	int sleep_msecs=x; struct timeval tv; \
						tv.tv_sec=(sleep_msecs/1000); tv.tv_usec=((sleep_msecs%1000)*1000); \
						select(0,NULL,NULL,NULL,&tv); })

#define PART(...)	d(config.textcolor, __VA_ARGS__, NULL)			// GREEN
#define PLAYER(...)	d(config.plycolor, __VA_ARGS__, NULL)			// BRIGHT_GREEN

#define TEXT(...)	d(config.textcolor, __VA_ARGS__, NULL); nl()	// GREEN
#define SAY(...)	d(config.talkcolor, __VA_ARGS__, NULL); nl()			// BRIGHT_MAGENTA
#define BAD(...)	d(config.badcolor, __VA_ARGS__, NULL); nl()			// BRIGHT_RED
#define GOOD(...)	d(config.goodcolor, __VA_ARGS__, NULL); nl()			// WHITE
#define HEADER(...)	d(config.headercolor, __VA_ARGS__, NULL); nl()		// MAGENTA
#define NOTICE(...)	d(config.noticecolor, __VA_ARGS__, NULL); nl()
#define TITLE(...)	d(config.titlecolor, __VA_ARGS__, NULL); nl()
#define EVENT(...)	d(config.eventcolor, __VA_ARGS__, NULL); nl()

#endif
