/* Unit test for sst_io's non-graphics-terminal gate (M2 review finding #1):
 * a terminal that ANSWERS the capability probe advertising neither sixel nor
 * JXL must get a one-time plain-text notice and a quit request, NOT a stream
 * of unrenderable sixel DCS.  Separate binary from test_sst_io.c because
 * sst_io keeps file-static session state with no reset, so exercising the
 * gate cleanly needs a fresh process.  cc'd + run by unit_sst_io.sh. */
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

int main(void)
{
	int            sv[2];
	static char    out[65536];
	static uint8_t idx[SST_FB_W * SST_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_sst_io_nogfx";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(sst_io_init(2, argv) == 1);
	assert(sst_io_active() == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	/* A DA1 reply WITHOUT the sixel cap (no param 4) -- a plain VT220-class
	 * terminal -- plus a JXL "not supported" reply (Q=1;JXL=0). The latter
	 * sets g_jxl_done, so the gate's settle wait is satisfied immediately and
	 * the test needs no 2s real-time delay. */
	{
		const char *replies = "\x1b[?62;1;6c" "\x1b[=1;0-n";
		assert(send(sv[0], replies, strlen(replies), 0) > 0);
		sst_io_pump();
	}
	assert(sst_io_have_sixel() == 0);
	assert(sst_io_jxl_supported() == 0);

	/* Present a changed frame: the gate must fire -- quit requested, the wire
	 * carries the plain-text notice, and NO sixel DCS (ESC P) is emitted. */
	memset(idx, 3, sizeof idx);
	memset(pal, 0x20, sizeof pal);
	sst_io_present(idx, pal);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	assert(sst_io_quit_requested() == 1);
	assert(strstr(out, "sixel or JXL") != NULL);   /* the notice text */
	assert(strstr(out, "\x1bP") == NULL);          /* NOT a sixel DCS */

	/* One-shot: a second present must not repeat the notice, nor start
	 * emitting sixel. */
	idx[0] = 9;
	sst_io_present(idx, pal);
	sst_io_flush();
	n = drain(sv[0], out, sizeof out);
	assert(strstr(out, "sixel or JXL") == NULL);   /* not repeated */
	assert(strstr(out, "\x1bP") == NULL);          /* still no sixel */
	(void)n;

	printf("SST_IO_NOGFX OK\n");
	return 0;
}
