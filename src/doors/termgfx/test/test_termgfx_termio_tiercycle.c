/* test_termgfx_termio_tiercycle.c -- termgfx_termio_tier_cycle() steps through
 * the tiers the client can actually draw.
 *
 * It exists for testability as much as for the player. SyncTERM is the only
 * terminal that reaches the JXL tier, so it is also the only one that never
 * exercises the SIXEL path -- including the parts written specifically FOR
 * SyncTERM, where a dirty box carries no palette and a palette change has to
 * ride a delta on the first box. Without a way to pin the tier, that code could
 * only run on a SyncTERM talked out of JXL, and nothing could do that.
 *
 * The client here answers the JXL capability probe with "yes" (ESC[=1;1-), so
 * both tiers are available and the cycle has somewhere to go.
 *
 * Exercises the API, not a keystroke: which KEY cycles is the door's business
 * (syncscumm spends F4 on it, syncrpg already spends F4 on its resolution
 * toggle), so binding one here would test the wrong layer.
 *
 * Its own binary because termgfx_termio keeps file-static session state with no
 * reset. cc'd + run by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static char out[1 << 20];

static int drain(int fd)
{
	ssize_t n;
	int     t = 0;

	while ((n = recv(fd, out + t, sizeof out - 1 - t, MSG_DONTWAIT)) > 0)
		t += n;
	out[t] = 0;
	return t;
}

/* Which encoder drew it: sixel is a DCS (ESC P), JXL an APC (ESC _). */
static const char *drew(void)
{
	if (strstr(out, "\x1bP") != NULL)
		return "sixel";
	if (strstr(out, "\x1b_") != NULL)
		return "jxl";
	return "nothing";
}

int main(void)
{
	int            sv[2];
	char           fdarg[32];
	char *         av[3];
	static uint8_t idx[320 * 200], pal[768];
	int            i;
	const char *   first, *second;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	av[0] = (char *)"tiercycle"; av[1] = fdarg; av[2] = NULL;
	assert(termgfx_termio_init(2, av) == 1);
	termgfx_termio_flush();
	drain(sv[0]);

	/* A SyncTERM that can do both: DA1 with sixel, the CTerm DA, and a "yes"
	 * to the JXL capability probe. */
	{
		const char *r = "\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;600;1200t" "\x1b[6;20;20t"
		                "\x1b[=67;84;101;114;109;1;332c" "\x1b[=1;1-n";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_is_syncterm() == 1);

	for (i = 0; i < (int)sizeof idx; i++)
		idx[i] = (uint8_t)(i & 15);
	memset(pal, 0x40, sizeof pal);

	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	drain(sv[0]);
	first = drew();

	termgfx_termio_tier_cycle();
	termgfx_termio_flush();
	drain(sv[0]);

	idx[1234] ^= 0xff;   /* change something, or the frame de-dupes and draws nothing */
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	drain(sv[0]);
	second = drew();

	/* The point: a SyncTERM starts on JXL and the cycle reaches sixel, which is
	 * the only way that path is exercised on the terminal it was written for. */
	assert(strcmp(first, "jxl") == 0);
	assert(strcmp(second, "sixel") == 0);
	printf("TERMGFX_TERMIO_TIERCYCLE %s -> cycle -> %s OK\n", first, second);
	return 0;
}
