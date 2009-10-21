#ifndef _MACROS_H_
#define _MACROS_H_

#include <stdio.h>
#include <errno.h>

#define CRASH		fprintf(stderr, "Unhandled exception at %s:%d errno=%d\n", __FILE__,__LINE__,errno)

#define CRASHF(...)	fprintf(stderr, __VA_ARGS__); CRASH

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
};

#define TEXT(text)	d_line(config.textcolor, text)
#define PART(text)	d_part(config.textcolor, text)

#define PLAYER(text)	d_part(global_plycol, text)

#define SAY(text)	d_line(global_talkcol, text)
#define SAYPART(text)	d_part(global_talkcol, text)

#endif
