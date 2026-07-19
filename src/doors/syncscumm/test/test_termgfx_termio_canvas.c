/* Regression test for the canvas-size startup hold (the Foot geometry bug):
 * the first frame must be HELD until the terminal's ESC[4;h;wt text-area pixel
 * report lands, so it isn't fit to the default 640x400 guess and rendered
 * small in the upper-left before the real window size is known.  Separate
 * binary from test_termgfx_termio.c because termgfx_termio keeps file-static session state
 * with no reset.  cc'd + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "termgfx_termio.h"

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n, total = 0;
	while ((n = recv(fd, buf + total, cap - 1 - total, MSG_DONTWAIT)) > 0)
		total += n;
	buf[total] = 0;
	return (int)total;
}

/* Pixel width Ph from a sixel DCS raster-attribute header ("Pan;Pad;Ph;Pv);
 * -1 if none present. */
static int sixel_ph(const char *s, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (s[i] == '"') {
			int field = 0, ph = 0, j = i + 1;
			while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == ';')) {
				if (s[j] == ';')
					field++;
				else if (field == 2)
					ph = ph * 10 + (s[j] - '0');
				j++;
			}
			if (field >= 2)
				return ph;
		}
	}
	return -1;
}

int main(void)
{
	int            sv[2];
	static char    out[262144];
	static uint8_t idx[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n, ph;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_canvas";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Foot-style: DA1 with sixel (param 4), non-SyncTERM, plus the 999;999 CPR
	 * char-dims reply -- but NOT the ESC[4;h;wt pixel report yet, so the exact
	 * canvas is still unknown. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);
	assert(termgfx_termio_is_syncterm() == 0);

	/* Present before the canvas report: within the startup grace with the
	 * canvas still unknown, the frame must be HELD -- nothing on the wire. */
	memset(idx, 5, sizeof idx);
	memset(pal, 0x30, sizeof pal);
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") == NULL);   /* held: no sixel emitted yet */

	/* The text-area pixel report lands: 1200x780, much larger than the
	 * 640x400 default. */
	{
		const char *r = "\x1b[4;780;1200t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();

	/* Present now: the frame emits, fit to the REAL canvas -- its sixel pixel
	 * width must reflect the 1200-wide window, not the 640-wide default. */
	idx[0] = 9;
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* now emitted */
	ph = sixel_ph(out, n);
	assert(ph > 640);   /* fit to the 1200-wide canvas, not the 640 default */

	printf("SST_IO_CANVAS OK (Ph=%d)\n", ph);
	return 0;
}
