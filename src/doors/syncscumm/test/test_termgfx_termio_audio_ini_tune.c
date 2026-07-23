/* test_termgfx_termio_audio_ini_tune.c -- a sysop tuning key changes what actually
 * reaches the wire, not merely what parses.
 *
 * "enabled = false" (test_termgfx_termio_audio_ini_off.c) proves the ini is read at
 * all, but it can only ever prove a NEGATIVE: zero bytes is also what a
 * never-opened stream, a broken feed, or a typo'd section name produce. This
 * test pins a POSITIVE, and picks the one key whose effect is visible from
 * outside the module: chunk_ms sets how much audio each shipped chunk holds,
 * so it sets how MANY chunks a fixed feed closes.
 *
 * The arithmetic is what makes it a proof rather than a smoke test. One
 * second of PCM at the 100ms default closes at most 10 chunks, so at most 10
 * A;Queue APCs can exist on the wire. Seeded with "chunk_ms = 50" the same
 * second closes 20. Asserting >= 15 is therefore UNSATISFIABLE unless the
 * ini value displaced the default -- a test that cannot pass by accident.
 *
 * Needs a real syncscumm.ini in CWD; unit_termgfx_termio.sh runs this binary from a
 * temp directory seeded with the key. Its own binary because termgfx_termio keeps
 * file-static session state with no reset. cc'd + run by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

/* Mirrors termgfx_termio.c's own internal TERMGFX_AUDIO_RATE default (24000) --
 * termgfx_termio.h no longer exposes it as a public macro the way
 * door/termgfx_termio.h once did, so the test keeps its own copy rather than guess a
 * literal. */
#define TERMGFX_AUDIO_RATE 24000

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n;
	size_t  tot = 0;

	while ((n = recv(fd, buf + tot, cap - 1 - tot, MSG_DONTWAIT)) > 0) {
		tot += (size_t)n;
		if (tot >= cap - 1)
			break;
	}
	buf[tot] = '\0';
	return (int)tot;
}

/* Count non-overlapping occurrences of `needle` in `hay`. Same reason
 * test_termgfx_termio_audio.c has one: "A;" alone cannot tell a shipped chunk from
 * the caps handshake's A;Volume, so only the chunk-specific marker counts. */
static int count_occurrences(const char *hay, const char *needle)
{
	int         n = 0;
	const char *p = hay;
	size_t      len = strlen(needle);

	while ((p = strstr(p, needle)) != NULL) {
		n++;
		p += len;
	}
	return n;
}

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	static char out[262144];
	int         queued;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio_ini_tune";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Digital-audio caps reply + a JXL "no" to latch the graphics settle
	 * flag (see test_termgfx_termio_audio.c). */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		termgfx_termio_pump();
	}
	assert(termgfx_termio_audio_available() == 1);

	{
		static int16_t pcm[TERMGFX_AUDIO_RATE * 2];   /* 1s of stereo, non-silent */
		int            i;

		for (i = 0; i < TERMGFX_AUDIO_RATE; i++) {
			pcm[2 * i]     = (int16_t)(i * 37);
			pcm[2 * i + 1] = (int16_t)(i * -37);
		}
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);   /* clear the caps handshake */
		termgfx_termio_audio_stream(pcm, TERMGFX_AUDIO_RATE);
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);
		queued = count_occurrences(out, "A;Queue");
		/* 20 chunks close at the seeded 50ms; the cushion is held then
		 * released, so >= 15 leaves room for cushion/boundary effects while
		 * staying far above the 4 that the TERMGFX_CHUNK_MS default (250ms)
		 * could ever produce from a 1s feed. That gap is the whole point of
		 * the test -- it is what proves the key reached the encoder rather
		 * than merely parsing -- and seeding the SHORTEST chunk against the
		 * longest default keeps it as wide as the clamp allows. */
		assert(queued >= 15);
	}

	printf("TERMGFX_TERMIO_AUDIO_INI_TUNE OK (%d chunks queued)\n", queued);
	return 0;
}
