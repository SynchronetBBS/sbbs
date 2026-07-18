/* Regression test for the bottom-of-image dirty-rect strand (stale mouse-
 * cursor sprite left un-erased at the BOTTOM of a static scene, confirmed
 * live on Windows Terminal sixel -- see docs/superpowers/sdd/syncscumm-m3/
 * fix-sixel-bottom-cursor-report.md).
 *
 * Root cause: sst_dirty_sixel_present()'s per-box height snap rounds a
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
 * This test proves it two ways, both in one session (fresh g_last state
 * needed between them, hence the three presents rather than two separate
 * binaries):
 *   1. A frame that dirties BOTH a mid-screen tile AND a bottom-row tile
 *      (tile row 12, fb rows 192..199) must force a FULL frame -- a raster
 *      header at (or near) the full ehc height (576) must appear on the
 *      wire. Pre-fix, the mid-screen box alone sends fine (nonzero
 *      return), so no such full-height raster header ever appears; that is
 *      the RED failure this test catches.
 *   2. A LATER frame that dirties only ANOTHER mid-screen tile (nothing in
 *      the bottom row) must still take the cheap dirty-rect path -- no
 *      full-height raster header, but the dirty path's DECSC ("\x1b7")/
 *      DECRC ("\x1b8") per-box wrap must be present. This is the bandwidth
 *      guard: the fix must not turn every present() into a full frame.
 *
 * Separate binary from the other sst_io tests because sst_io keeps
 * file-static session state with no reset. cc'd + run by unit_sst_io.sh. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sst_io.h"

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n, total = 0;
	while ((n = recv(fd, buf + total, cap - 1 - total, MSG_DONTWAIT)) > 0)
		total += n;
	buf[total] = 0;
	return (int)total;
}

/* Scan the WHOLE buffer for every sixel DCS raster-attribute header
 * ("Pan;Pad;Ph;Pv") and return the LARGEST Ph/Pv seen -- a present() call
 * can legitimately carry more than one DCS (an earlier dirty box already
 * queued before a later box in the same loop forces a full-frame fallback),
 * so the tell for "was a full frame sent" is the presence of a raster
 * header at (or near) the full image height, not merely a header existing
 * at all. */
static void sixel_raster_max(const char *s, int n, int *max_ph_out, int *max_pv_out)
{
	int i, max_ph = -1, max_pv = -1;

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
				if (ph > max_ph)
					max_ph = ph;
				if (pv > max_pv)
					max_pv = pv;
			}
		}
	}
	*max_ph_out = max_ph;
	*max_pv_out = max_pv;
}

/* Set a 16x16 (or shorter, at the fb bottom) tile to a fill value -- tx/ty
 * are tile coordinates (SST_TILE == 16, checked against sst_io.c). */
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
	static uint8_t idx1[SST_FB_W * SST_FB_H];
	static uint8_t idx2[SST_FB_W * SST_FB_H];
	static uint8_t idx3[SST_FB_W * SST_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n, ph, pv;
	const int      TILE = 16;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_sst_io_bottom_dirty";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(sst_io_init(2, argv) == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	/* DA1 with sixel (param 4), non-SyncTERM, the 24x80 grid CPR, an EXACT
	 * 1200x600 canvas (ESC[4;h;wt) and an EXACT 20x20 cell (ESC[6;h;wt) --
	 * the live-trace geometry that produces vstep=60, eh=580, ehc=576. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;600;1200t" "\x1b[6;20;20t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	sst_io_pump();
	assert(sst_io_have_sixel() == 1);
	assert(sst_io_is_syncterm() == 0);

	/* First frame: flat fill, establishes g_last (always a full frame --
	 * g_have_last is 0 going in). */
	memset(idx1, 3, sizeof idx1);
	memset(pal, 0x30, sizeof pal);
	sst_io_present(idx1, pal);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	/* Second frame: dirty a mid-screen tile (tx=2,ty=2 -> fb rows 32..47)
	 * AND a bottom-row tile (tx=8,ty=12 -> fb rows 192..199, tile row 12 is
	 * the door's last, 8px-tall row -- exactly where a bottom cursor sits).
	 * The mid-screen tile is scanned first (sst_coalesce() visits tiles
	 * top-to-bottom), so pre-fix it sends fine on its own and the caller
	 * never learns the bottom tile got dropped. */
	memcpy(idx2, idx1, sizeof idx1);
	fill_tile(idx2, 2, 2, TILE, SST_FB_W, SST_FB_H, 9);
	fill_tile(idx2, 8, 12, TILE, SST_FB_W, SST_FB_H, 9);

	sst_io_present(idx2, pal);
	sst_io_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);

	sixel_raster_max(out, n, &ph, &pv);
	/* THE FIX: a bottom-row change that the height snap can't fully cover
	 * must force a full frame -- proven by a raster header at (or very
	 * near) the full ehc height (576), not just SOME raster header (the
	 * mid-screen box's own small one, rh<=140, would otherwise satisfy a
	 * weaker check). Pre-fix this fails: the largest Pv seen is the
	 * mid-screen box's own small height, well under 500. */
	assert(pv >= 500);
	printf("SST_IO_BOTTOM_DIRTY bottom-strand forces full frame OK (Ph=%d Pv=%d)\n", ph, pv);

	/* Third frame: dirty ONLY another mid-screen tile (tx=5,ty=5 -> fb rows
	 * 80..95), nothing in the bottom row. This must NOT force a full frame
	 * -- the bandwidth guard: the fix must only trigger when a change
	 * actually touches the stranded bottom region. */
	memcpy(idx3, idx2, sizeof idx2);
	fill_tile(idx3, 5, 5, TILE, SST_FB_W, SST_FB_H, 9);

	sst_io_present(idx3, pal);
	sst_io_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);

	assert(strstr(out, "\x1b" "7") != NULL);   /* dirty path's DECSC wrap: took the cheap path */
	sixel_raster_max(out, n, &ph, &pv);
	assert(pv > 0 && pv < 500);                /* no full-height raster header emitted */
	printf("SST_IO_BOTTOM_DIRTY non-bottom change stays dirty-rect OK (Ph=%d Pv=%d)\n", ph, pv);

	return 0;
}
