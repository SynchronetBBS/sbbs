/* Regression test for the sysop "sixel_max" ini override (M2 ceiling
 * refinement): syncscumm.ini's root-section "sixel_max" key is the highest-
 * precedence source in the effective sixel ceiling (sst_io.c's g_gfx_max_w
 * doc comment) -- it must beat even an XTSMGRAPHICS reply reporting a
 * bigger ceiling, not just the TERMGFX_SIXEL_SAFE_MAX/canvas-trust
 * defaults the other sst_io_* tests exercise.
 *
 * Unlike the other sst_io tests, this one needs a real syncscumm.ini
 * present in CWD when sst_io_init() runs (sst_read_ini() fopen()s it
 * relative to CWD, the same way door/syncscumm.cpp's resolveSubtitles()
 * reads "subtitles" from the same file) -- so unit_sst_io.sh invokes this
 * binary from a dedicated temp directory seeded with "sixel_max = 800",
 * rather than from $HERE/$DOOR like the others.
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
	static uint8_t idx[SST_FB_W * SST_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n, ph, pv;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_sst_io_sixelmax_override";
	argv[1] = fdarg;
	argv[2] = NULL;

	/* sst_io_init() -> sst_read_ini() reads CWD's syncscumm.ini here,
	 * picking up "sixel_max = 800" (unit_sst_io.sh seeds it before running
	 * this binary from that directory). */
	assert(sst_io_init(2, argv) == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	/* DA1 with sixel (param 4), non-SyncTERM, the 24x80 grid CPR, a big
	 * exact canvas, AND an XTSMGRAPHICS reply reporting a ceiling (2000)
	 * well above both SAFE_MAX and the sysop's 800 override -- the override
	 * must win regardless. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;1500;2400t"
		                 "\x1b[?2;0;2000;2000S";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	sst_io_pump();
	assert(sst_io_have_sixel() == 1);
	assert(sst_io_is_syncterm() == 0);

	memset(idx, 5, sizeof idx);
	memset(pal, 0x30, sizeof pal);
	sst_io_present(idx, pal);
	sst_io_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "\x1bP") != NULL);   /* emitted */
	sixel_raster(out, n, &ph, &pv);
	assert(ph > 0 && ph <= 800);   /* sysop override beats the 2000x2000 XTSMGRAPHICS reply */
	assert(pv > 0 && pv <= 800);

	printf("SST_IO_SIXELMAX_OVERRIDE OK (Ph=%d Pv=%d)\n", ph, pv);
	return 0;
}
