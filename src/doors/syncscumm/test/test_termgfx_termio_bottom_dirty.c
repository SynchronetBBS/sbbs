/* Regression test for the bottom-of-image dirty-rect strand (stale mouse-
 * cursor sprite left un-erased at the BOTTOM of a static scene, confirmed
 * live on Windows Terminal sixel -- see docs/superpowers/sdd/syncscumm-m3/
 * fix-sixel-bottom-cursor-report.md).
 *
 * Root cause: termgfx_dirty_sixel_present()'s per-box height snap rounds a
 * box's height UP to a whole vstep (LCM(cell,6)), then -- only at the frame
 * bottom -- rounds back DOWN to stay vstep-aligned. At this test's geometry
 * (canvas 1200x600, cell 20x20 -> vstep=LCM(20,6)=60; eh=580, ehc=576) a
 * dirty box whose changed rows fall in [540,576) rounds down to rh=0 and
 * gets `continue`-dropped entirely: its changed pixels never reach the
 * wire. If some OTHER box in the same present() still sends fine, the
 * caller sees a nonzero return and takes the dirty-rect path rather than
 * falling back to a full frame -- so the bottom strand is never repainted
 * on an otherwise-static scene (only the cursor moving would ever touch
 * those rows again).
 *
 * The fix: when the bottom clamp would leave the box's changed rows
 * [ry+rh, ry2) uncovered, `return 0` (the function's existing "fall back
 * to a full frame" signal) instead of shortening/dropping the box.
 *
 * ATOMICITY (review follow-up): the stranding check runs in a pre-pass over
 * ALL boxes, before any box is out_put() to the wire -- not inline in the
 * emit loop. A frame that dirties an earlier (mid-screen) box AND a later
 * bottom-strand box is exactly the real cursor case (an erase box above the
 * cursor's old spot + the bottom box itself), and boxes are written to the
 * wire as each one succeeds in raster/top-to-bottom order. If the check ran
 * inline, the mid-screen box's bytes would already be on the wire by the
 * time the bottom box strands and returns 0 -- so the caller, seeing 0,
 * would ALSO send a full frame, stacking a partial dirty box plus a full
 * frame on the same present(). The pre-pass makes the fallback atomic:
 * either nothing is sent and the caller sends a full frame, or every
 * emitted box is complete -- mirroring the invariant the JXL present
 * function documents (termgfx_termio.c: "the caller never stacks a full frame on
 * top of partial rects").
 *
 * This test proves it two ways, both in one session (fresh g_last state
 * needed between them, hence the three presents rather than two separate
 * binaries):
 *   1. A frame that dirties BOTH a mid-screen tile AND a bottom-row tile
 *      (tile row 12, fb rows 192..199) must force a FULL frame -- and, with
 *      the atomic fix, ONLY the full frame: exactly one sixel raster header
 *      on the wire, at the full ehc height (576), with no smaller dirty
 *      raster (the mid-screen box's own) ahead of it. Pre-fix (no pre-pass),
 *      the mid-screen box sends first and its own small raster header hits
 *      the wire before the bottom box strands and forces the full frame --
 *      TWO raster headers, the first well under full height; that is the
 *      RED failure this test catches.
 *   2. A LATER frame that dirties only ANOTHER mid-screen tile (nothing in
 *      the bottom row) must still take the cheap dirty-rect path -- no
 *      full-height raster header, but the dirty path's DECSC ("\x1b7")/
 *      DECRC ("\x1b8") per-box wrap must be present. This is the bandwidth
 *      guard: the fix must not turn every present() into a full frame.
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

/* Scan the WHOLE buffer for every sixel DCS raster-attribute header
 * ("Pan;Pad;Ph;Pv"), returning the LARGEST Ph/Pv seen and how many headers
 * were found. With the atomic fallback, a bottom-strand present() must
 * carry exactly ONE header (the full frame) -- count_out is what lets the
 * test assert that directly, rather than only checking the max size and
 * potentially missing an earlier, already-sent partial-box header. */
static void sixel_raster_max(const char *s, int n, int *max_ph_out, int *max_pv_out, int *count_out)
{
	int i, max_ph = -1, max_pv = -1, count = 0;

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
				count++;
				if (ph > max_ph)
					max_ph = ph;
				if (pv > max_pv)
					max_pv = pv;
			}
		}
	}
	*max_ph_out = max_ph;
	*max_pv_out = max_pv;
	*count_out = count;
}

/* Set a 16x16 (or shorter, at the fb bottom) tile to a fill value -- tx/ty
 * are tile coordinates (TERMGFX_TILE == 16, checked against termgfx_termio.c). */
static void fill_tile(uint8_t *fb, int tx, int ty, int tile, int fb_w, int fb_h, uint8_t val)
{
	int x0 = tx * tile, y0 = ty * tile;
	int rows = tile, cols = tile, r;
	if (y0 + rows > fb_h)
		rows = fb_h - y0;
	if (x0 + cols > fb_w)
		cols = fb_w - x0;
	for (r = 0; r < rows; r++)
		memset(fb + (size_t)(y0 + r) * fb_w + x0, val, cols);
}

int main(void)
{
	int            sv[2];
	static char    out[262144];
	static uint8_t idx1[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t idx2[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t idx3[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n, ph, pv, cnt;
	const int      TILE = 16;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_bottom_dirty";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* DA1 with sixel (param 4), non-SyncTERM, the 24x80 grid CPR, an EXACT
	 * 1200x600 canvas (ESC[4;h;wt) and an EXACT 20x20 cell (ESC[6;h;wt) --
	 * the live-trace geometry that produces vstep=60, eh=580, ehc=576. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;600;1200t" "\x1b[6;20;20t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);
	assert(termgfx_termio_is_syncterm() == 0);

	/* First frame: flat fill, establishes g_last (always a full frame --
	 * g_have_last is 0 going in). */
	memset(idx1, 3, sizeof idx1);
	memset(pal, 0x30, sizeof pal);
	termgfx_termio_present(idx1, pal);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Second frame: dirty a mid-screen tile (tx=2,ty=2 -> fb rows 32..47)
	 * AND a bottom-row tile (tx=8,ty=12 -> fb rows 192..199, tile row 12 is
	 * the door's last, 8px-tall row -- exactly where a bottom cursor sits).
	 * The mid-screen tile is scanned first (termgfx_coalesce() visits tiles
	 * top-to-bottom), so pre-fix it sends fine on its own and the caller
	 * never learns the bottom tile got dropped. */
	memcpy(idx2, idx1, sizeof idx1);
	fill_tile(idx2, 2, 2, TILE, TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, 9);
	fill_tile(idx2, 8, 12, TILE, TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, 9);

	termgfx_termio_present(idx2, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);

	sixel_raster_max(out, n, &ph, &pv, &cnt);
	/* THE FIX (atomic): a bottom-row change that the height snap can't
	 * fully cover must force a full frame, and ONLY the full frame -- no
	 * partial dirty box may have already reached the wire ahead of it.
	 * That means exactly ONE sixel raster header total, at (or very near)
	 * the full ehc height (576). Pre-fix (no pre-pass) this fails two ways:
	 * either no full-height header appears at all (pv stays under 500,
	 * the mid-screen box's own small height), or -- once the inner check
	 * alone forced the fallback -- TWO headers appear (the mid-screen
	 * box's small one plus the full frame), i.e. cnt > 1. */
	assert(cnt == 1);
	assert(pv >= 500);
	printf("TERMGFX_TERMIO_BOTTOM_DIRTY bottom-strand forces full frame OK (Ph=%d Pv=%d cnt=%d)\n", ph, pv, cnt);

	/* Third frame: dirty ONLY another mid-screen tile (tx=5,ty=5 -> fb rows
	 * 80..95), nothing in the bottom row. This must NOT force a full frame
	 * -- the bandwidth guard: the fix must only trigger when a change
	 * actually touches the stranded bottom region. */
	memcpy(idx3, idx2, sizeof idx2);
	fill_tile(idx3, 5, 5, TILE, TERMGFX_TERMIO_FB_W, TERMGFX_TERMIO_FB_H, 9);

	termgfx_termio_present(idx3, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);

	assert(strstr(out, "\x1b" "7") != NULL);   /* dirty path's DECSC wrap: took the cheap path */
	sixel_raster_max(out, n, &ph, &pv, &cnt);
	assert(cnt == 1);                          /* one small dirty box, no full-frame storm */
	assert(pv > 0 && pv < 500);                /* no full-height raster header emitted */
	printf("TERMGFX_TERMIO_BOTTOM_DIRTY non-bottom change stays dirty-rect OK (Ph=%d Pv=%d cnt=%d)\n", ph, pv, cnt);

	return 0;
}
