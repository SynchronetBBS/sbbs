/* Copyright (C), 2007 by Stephen Hurd */

#include <ciolib.h>
#include <gen_defs.h>
#include <vidmodes.h>

#include "syncterm.h"
#include "term.h"
#include "uifcinit.h"

void
get_term_win_size(int *width, int *height, int *pixelw, int *pixelh, int *nostatus)
{
	struct  text_info txtinfo;
	int               vmode = find_vmode(fake_mode);

	gettextinfo(&txtinfo);

	if ((vmode != -1) && (txtinfo.screenwidth >= vparams[vmode].cols)) {
		*width = vparams[vmode].cols;
	}
	else {
		if (txtinfo.screenwidth < 80) {
			*width = 40;
		}
		else {
			if (txtinfo.screenwidth < 132)
				*width = 80;
			else
				*width = 132;
		}
	}
	*height = txtinfo.screenheight;
	if (vmode != -1) {
		if ((cio_api.options & CONIO_OPT_SET_PIXEL) == 0) {
			if (txtinfo.screenheight >= vparams[vmode].rows)
				*height = vparams[vmode].rows;
		}
	}
	if (!*nostatus)
		(*height)--;

	if (vmode == -1) {
		if (pixelw)
			*pixelw = *width * 8;
		if (pixelh)
			*pixelh = *height * 16;
	}
	else {
		if (pixelw)
			*pixelw = *width * vparams[vmode].charwidth;
		if (pixelh)
			*pixelh = *height * vparams[vmode].charheight;
	}
}

int
drawwin(void)
{
	char             *winbuf;
	char             *p;
	char              str[32];
	int               x, y, c;
	struct  text_info txtinfo;

	gettextinfo(&txtinfo);

	strcpy(str, "         ");

	get_term_win_size(&term.width, &term.height, NULL, NULL, &term.nostatus);

	if (settings.left_just)
		term.x = 2;
	else
		term.x = (txtinfo.screenwidth - term.width) / 2 + 2;
	term.y = (txtinfo.screenheight - term.height) / 2 + 2;
	if ((winbuf = (char *)alloca(txtinfo.screenheight * txtinfo.screenwidth * 2)) == NULL) {
		uifcmsg("Cannot allocate memory for terminal buffer", "`Memory error`\n\n"
		    "Either your system is dangerously low on resources or your\n"
		    "window is farking huge!");
		return -1;
	}
	c = 0;
	for (y = 0; y < txtinfo.screenheight; y++) {
		p = str;
		for (x = 0; x < y; x++) {
			p++;
			if (!*p)
				p = str;
		}
		for (x = 0; x < txtinfo.screenwidth; x++) {
			winbuf[c++] = *(p++);
			if (!*p)
				p = str;
			if (settings.left_just)
				winbuf[c++] = 0;
			else
				winbuf[c++] = YELLOW | (BLUE << 4);
		}
	}
	puttext(1, 1, txtinfo.screenwidth, txtinfo.screenheight, winbuf);
	return 0;
}
