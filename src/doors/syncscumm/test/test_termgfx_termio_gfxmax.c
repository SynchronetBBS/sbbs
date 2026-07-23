/* Regression test for the xterm oversized-sixel discard (M2 wire-capture
 * finding): full frames were emitted at 1368x906 -- over xterm's ~1000x1000
 * default XTSMGRAPHICS ceiling -- and xterm discards an oversized sixel
 * WHOLE rather than clipping it, so later dirty boxes (under the limit)
 * rendered over stale content.  termgfx_termio sent the XTSMGRAPHICS query
 * (termgfx_term_probe: ESC[?2;1S) but never parsed the reply, so the fit
 * path never learned the ceiling.
 *
 * This exercises, in one session:
 *   1. A big canvas report with NO XTSMGRAPHICS reply and NO XTVERSION
 *      reply -- i.e. a genuinely silent terminal that never identified
 *      itself as xterm. Per the refined ceiling policy (termgfx_termio.c's
 *      g_gfx_max_w doc comment), an exact canvas report is now TRUSTED
 *      absent a positive xterm identification -- this is exactly the
 *      Windows Terminal / Foot case (both answer ESC[14t, neither answers
 *      XTSMGRAPHICS or XTVERSION, and both render far past 1000x1000
 *      fine), so the emitted sixel must fit the canvas, NOT be clamped
 *      to TERMGFX_SIXEL_SAFE_MAX. (The "terminal positively identified as
 *      xterm" case that DOES still clamp here lives in its own binary,
 *      test_termgfx_termio_xterm_ceiling.c.)
 *   2. The XTSMGRAPHICS reply (ESC[?2;0;700;700S) arriving AFTER that
 *      first frame, with the SAME framebuffer content presented again -- the
 *      geometry change alone (no pixel changed) must still force a fresh
 *      full frame clamped to <=700x700 (an authoritative report always
 *      outranks the canvas-trust default), not a deduped/dirty no-op
 *      against the stale, larger last-sent frame.
 *   3. A LARGER XTSMGRAPHICS reply (ESC[?2;0;2000;2000S) landing after
 *      that -- a terminal that reports a ceiling above SAFE_MAX must get
 *      the bigger frame the report actually allows, not get pinned to
 *      1000x1000 by a leftover default.
 *
 * Separate binary from test_termgfx_termio.c because termgfx_termio keeps file-static
 * session state with no reset.  cc'd + run by unit_termgfx_termio.sh. */
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
	argv[0] = (char *)"test_termgfx_termio_gfxmax";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* DA1 with sixel (param 4), non-SyncTERM, the 24x80 grid CPR, and a big
	 * exact canvas -- but NO XTSMGRAPHICS reply and NO XTVERSION reply
	 * (never sent in this test at all -- a genuinely silent terminal). */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;1500;2400t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);
	assert(termgfx_termio_is_syncterm() == 0);

	/* First present: no XTSMGRAPHICS reply on record and the terminal never
	 * identified itself as xterm -- the exact canvas report must be TRUSTED
	 * (Ph past TERMGFX_SIXEL_SAFE_MAX, up to the canvas/TERMGFX_SCALE_MAX fit),
	 * not clamped down to it. This is the Windows Terminal / Foot case: both
	 * answer ESC[14t and neither answers XTSMGRAPHICS or XTVERSION. */
	memset(idx, 5, sizeof idx);
	memset(pal, 0x30, sizeof pal);
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* emitted */
	sixel_raster(out, n, &ph, &pv);
	assert(ph > 1000);   /* NOT clamped to SAFE_MAX: an exact, un-xterm'd canvas is trusted */
	assert(ph <= 2400);   /* still inside the reported canvas width (ESC[4;h;w t: 2400 wide) */
	assert(pv > 0 && pv <= 1500);   /* inside the reported canvas height (1500 tall) */
	printf("TERMGFX_TERMIO_GFXMAX pre-reply OK (Ph=%d Pv=%d)\n", ph, pv);

	/* Now an XTSMGRAPHICS reply lands, reporting a ceiling well below both
	 * TERMGFX_SIXEL_SAFE_MAX and the pre-reply (canvas-trusted) fit size --
	 * an authoritative report always outranks the canvas-trust default, so
	 * this must shrink the emitted frame. It also exercises the late-reply
	 * path: the geometry-only change (fit result shrinks under the new
	 * ceiling, canvas itself unchanged) must force a full fresh frame, not
	 * a dedupe/dirty no-op against the stale, larger last-sent frame. */
	{
		const char *r = "\x1b[?2;0;700;700S";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();

	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* a real frame went out, not a no-op */
	sixel_raster(out, n, &ph, &pv);
	assert(ph > 0 && ph <= 700);
	assert(pv > 0 && pv <= 700);

	printf("TERMGFX_TERMIO_GFXMAX post-reply OK (Ph=%d Pv=%d)\n", ph, pv);

	/* Now a BIGGER XTSMGRAPHICS reply lands (a terminal with a larger real
	 * ceiling than xterm's assumed default) -- above SAFE_MAX this time, not
	 * just above the 700x700 from the last step. The same content presented
	 * again must grow past SAFE_MAX (1000) -- up to what THIS report allows
	 * -- not stay pinned at the earlier (now stale) 700x700 clamp or at the
	 * SAFE_MAX default. */
	{
		const char *r = "\x1b[?2;0;2000;2000S";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();

	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* a real frame went out, not a no-op */
	sixel_raster(out, n, &ph, &pv);
	assert(ph > 1000);   /* past SAFE_MAX: the bigger report's ceiling applies */
	assert(ph <= 2000);
	assert(pv <= 2000);

	printf("TERMGFX_TERMIO_GFXMAX bigger-report OK (Ph=%d Pv=%d)\n", ph, pv);
	return 0;
}
