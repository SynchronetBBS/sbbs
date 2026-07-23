/* test_termgfx_termio_audio.c -- the audio capability probe and its replies.
 *
 * Asserts (a) the startup burst solicits the audio caps query, (b) a
 * digital-tier reply resolves termgfx_termio_audio_available() to 1 without eating
 * the settle window in real time, and (c) the two-stage Opus query is only
 * emitted once the caps reply has landed.
 *
 * termgfx_termio keeps file-static session state with no reset, so exercising the
 * probe cleanly needs a fresh process. cc'd + run by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

/* Mirrors termgfx_termio.c's own internal TERMGFX_AUDIO_RATE/TERMGFX_CHUNK_MS
 * defaults (24000, 250) -- termgfx_termio.h no longer exposes these as public
 * macros the way door/termgfx_termio.h once did, so the test keeps its own copy
 * rather than guess a literal. */
#define TERMGFX_AUDIO_RATE 24000
#define TERMGFX_CHUNK_MS   250

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

/* Count non-overlapping occurrences of `needle` in `hay`. Used to pin actual
 * PCM delivery: `A;Volume` (the caps handshake) and `A;Queue` (a shipped
 * chunk) both contain the substring "A;", so "was any A; seen" cannot tell
 * the two apart -- only counting the chunk-specific marker can. */
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
	static char out[65536];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* (a) the burst solicits the audio caps query ... */
	assert(strstr(out, "libsndfile") != NULL);
	/* ... and NOT the Opus query, which is two-stage: it can only be asked
	 * once the terminal has confirmed a digital tier, because it queries a
	 * FORMAT that only a libsndfile-having terminal could decode. */
	assert(strstr(out, "32;100") == NULL);

	/* Inject a digital-audio caps reply (ESC[=7;100;1n) plus a JXL "no"
	 * (ESC[=1;0n), which latches the graphics settle flag so no test eats
	 * TERMGFX_GFX_SETTLE_MS of real time. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		termgfx_termio_pump();
	}

	/* (b) a confirmed digital tier IS audio. The caps answer is already
	 * latched, so this must not wait out the settle window; the Opus reply
	 * never comes, which is the historical assume-yes (termgfx/audio.h:41-51). */
	assert(termgfx_termio_audio_available() == 1);

	/* (c) the caps reply triggers the Opus query. */
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(strstr(out, "32;100") != NULL);

	/* Streamed PCM reaches the wire as audio APCs on a digital terminal. */
	{
		static int16_t pcm[TERMGFX_AUDIO_RATE * 2];   /* 1s of stereo, non-silent */
		int            i;

		for (i = 0; i < TERMGFX_AUDIO_RATE; i++) {
			pcm[2 * i]     = (int16_t)(i * 37);
			pcm[2 * i + 1] = (int16_t)(i * -37);
		}
		termgfx_termio_audio_stream(pcm, TERMGFX_AUDIO_RATE);
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);
		/* A 1s feed closes 1000/TERMGFX_CHUNK_MS chunks: the cushion is held
		 * then released, so they should all ship as A;Queue APCs by the
		 * end of the feed. Derived from the chunk length rather than
		 * asserted as a literal -- this read ">= 7" against the 10 that a
		 * 100ms chunk produced, which a 250ms chunk (4 per second) cannot
		 * reach however correctly the module behaves. Count the actual
		 * marker rather than "any A;": the caps handshake above already put
		 * an A;Volume on the wire before a single PCM sample existed, so a
		 * bare "A;" check passes even if termgfx_stream_feed() queued
		 * nothing at all. One chunk of slack absorbs boundary effects while
		 * staying impossible to satisfy from the handshake alone. */
		assert(count_occurrences(out, "A;Queue") >= (1000 / TERMGFX_CHUNK_MS) - 1);
	}

	/* A tone-only terminal (libsndfile absent) is NOT audio for our
	 * purposes: A;Load is a no-op there, so a Queue would play an empty
	 * slot. Covered in its own binary -- see test_termgfx_termio_audio_tone.c. */

	printf("TERMGFX_TERMIO_AUDIO OK\n");
	return 0;
}
