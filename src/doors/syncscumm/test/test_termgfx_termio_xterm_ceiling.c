/* Regression test for the refined sixel-ceiling policy's xterm branch (M2
 * ceiling refinement): an exact ESC[4;h;wt canvas report is now TRUSTED
 * absent an XTSMGRAPHICS reply (see test_termgfx_termio_gfxmax.c's first part, and
 * termgfx_termio.c's g_gfx_max_w doc comment) -- but ONLY when the terminal has not
 * positively identified itself as xterm. A real xterm session answers
 * ESC[14t (so the canvas IS exact) but, with window ops left at their
 * default-off, never answers XTSMGRAPHICS, and discards an oversized
 * declared sixel raster WHOLE rather than clipping it -- so trusting its
 * canvas report the way Windows Terminal/Foot are trusted would reproduce
 * the exact "SyncSCUMM is black in a big xterm" defect the ceiling policy
 * exists to prevent.
 *
 * This exercises, in one session: the OUTBOUND probe order (the XTVERSION
 * query must precede the ESC[14t canvas query, or a real xterm's first
 * frame races its own identification -- see the assertion below), then an
 * XTVERSION reply identifying the peer as "XTerm(397)", DA1 (sixel), the
 * 24x80 grid CPR, and a big exact canvas report (1500x2400) -- all with NO
 * XTSMGRAPHICS reply at all. The emitted sixel must still be clamped to
 * TERMGFX_SIXEL_SAFE_MAX, not fit to the big canvas.
 *
 * Separate binary from the other termgfx_termio tests because termgfx_termio keeps
 * file-static session state with no reset. cc'd + run by unit_termgfx_termio.sh. */
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

/* Pixel width Ph and height Pv from a sixel DCS raster-attribute header
 * ("Pan;Pad;Ph;Pv); -1/-1 if none present. */
static void sixel_raster(const char *s, int n, int *ph_out, int *pv_out)
{
	int i;

	*ph_out = -1;
	*pv_out = -1;
	for (i = 0; i < n; i++) {
		if (s[i] == '"') {
			int field = 0, ph = 0, pv = 0, j = i + 1;
			while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == ';')) {
				if (s[j] == ';')
					field++;
				else if (field == 2)
					ph = ph * 10 + (s[j] - '0');
				else if (field == 3)
					pv = pv * 10 + (s[j] - '0');
				j++;
			}
			if (field >= 3) {
				*ph_out = ph;
				*pv_out = pv;
				return;
			}
		}
	}
}

int main(void)
{
	int            sv[2];
	static char    out[262144];
	static uint8_t idx[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n, ph, pv;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_xterm_ceiling";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);

	/* The probe burst must solicit XTVERSION (ESC[>0q) BEFORE the ESC[14t
	 * canvas query: a terminal answers queries in receipt order, and the
	 * present-path startup holds release on the canvas/CPR replies alone --
	 * if the canvas query went out first, a real xterm's first frame would
	 * be emitted with g_canvas_exact set but its (unsolicited) XTVERSION
	 * identification still pending, trusting the big canvas the gate exists
	 * to demote. Pin the outbound order itself. */
	{
		const char *xtver  = strstr(out, "\x1b[>0q");
		const char *canvas = strstr(out, "\x1b[14t");
		assert(xtver != NULL && canvas != NULL);
		assert(xtver < canvas);
	}

	/* Replies arrive in the same order a real xterm would produce them --
	 * the XTVERSION identification FIRST (its query went out first), then
	 * DA1 with sixel (param 4), non-SyncTERM, the 24x80 grid CPR, and the
	 * big exact canvas -- delivered across separate pumps so the
	 * identification is on record before the canvas report ever is.
	 * NO XTSMGRAPHICS reply anywhere in this session. */
	{
		const char *r = "\x1bP>|XTerm(397)\x1b\\";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;1500;2400t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);
	assert(termgfx_termio_is_syncterm() == 0);

	/* No XTSMGRAPHICS report, but the terminal identified itself as xterm --
	 * the exact canvas must NOT be trusted here; fall all the way to
	 * TERMGFX_SIXEL_SAFE_MAX, same as a fully silent xterm. */
	memset(idx, 5, sizeof idx);
	memset(pal, 0x30, sizeof pal);
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* emitted */
	sixel_raster(out, n, &ph, &pv);
	assert(ph > 0 && ph <= 1000);   /* xterm ID demotes the exact canvas to SAFE_MAX */
	assert(pv > 0 && pv <= 1000);

	printf("SST_IO_XTERM_CEILING OK (Ph=%d Pv=%d)\n", ph, pv);
	return 0;
}
