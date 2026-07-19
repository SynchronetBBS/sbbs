/* test_termgfx_termio_audio_underrun.c -- underrun re-primes rather than dribbling.
 *
 * `CSI = 7 ; <ch> ; 0 n` is CTerm's audio-channel-idle notification: our FIFO
 * on that channel drained. csi_final()'s 'n' case dispatches it to
 * termgfx_stream_underrun(), which re-arms the PRIME state so playback
 * rebuilds its cushion instead of dribbling one chunk at a time into an
 * empty FIFO (termgfx/audio_stream.c). This test proves that dispatch
 * actually happens and actually re-primes, rather than either being
 * silently absorbed or leaving the stream in RUN (which would ship each
 * chunk the instant it closes).
 *
 * It also injects an audio-CAPS-shaped reply ("ESC[=7;100;1n") right next
 * to the real underrun ("ESC[=7;2;0n") in the SAME buffer: both share the
 * "=7" prefix, and only the load-bearing `p[1] != 100` guard in csi_final()
 * keeps the former from being misread as an underrun on channel 100. A
 * test that injects only the real underrun would not exercise that guard
 * at all.
 *
 * Its own binary because termgfx_termio keeps file-static session state with no
 * reset, so a fresh probe/stream sequence needs a fresh process. cc'd + run
 * by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

/* Mirrors termgfx_termio.c's own internal SST_AUDIO_RATE/SST_CHUNK_MS/
 * SST_PREBUFFER_CHUNKS defaults (24000, 250, 3) -- termgfx_termio.h no longer
 * exposes these as public macros the way door/sst_io.h once did, so the test
 * keeps its own copy rather than guess a literal. */
#define SST_AUDIO_RATE       24000
#define SST_CHUNK_MS         250
#define SST_PREBUFFER_CHUNKS 3

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

/* Count non-overlapping occurrences of `needle` in `hay`. */
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

/* Feed `chunks` worth of non-silent stereo PCM in one call.
 * Sized off SST_PREBUFFER_CHUNKS with headroom: the cushion is what the
 * largest feed below has to clear, so the buffer has to follow it.
 *
 * A chunk is a DURATION (the door's SST_CHUNK_MS default, audio_stream_open()),
 * so its frame count is derived from BOTH the rate and that duration rather
 * than from the number they happen to produce today: the counts asserted below
 * are exact, so a literal here silently mis-feeds -- and did, twice. A
 * hardcoded 2205 stopped being one chunk when the mixer rate moved to 24000,
 * and the SST_AUDIO_RATE/10 that replaced it stopped being one chunk when the
 * chunk moved to 250ms. Both spellings baked in a value that was never this
 * test's to choose. */
#define UNDERRUN_CHUNK_FRAMES ((SST_AUDIO_RATE * SST_CHUNK_MS) / 1000)
#define UNDERRUN_MAX_CHUNKS (SST_PREBUFFER_CHUNKS + 4)
static void feed_chunks(int chunks)
{
	static int16_t pcm[UNDERRUN_CHUNK_FRAMES * 2 * UNDERRUN_MAX_CHUNKS];
	int            frames = UNDERRUN_CHUNK_FRAMES * chunks;
	int            i;

	assert(chunks <= UNDERRUN_MAX_CHUNKS);
	for (i = 0; i < frames; i++) {
		pcm[2 * i]     = (int16_t)(i * 37);
		pcm[2 * i + 1] = (int16_t)(i * -37);
	}
	termgfx_termio_audio_stream(pcm, (size_t)frames);
}

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	static char out[65536];
	char        errpath[] = "/tmp/test_termgfx_termio_audio_underrun_err.XXXXXX";
	int         errfd;
	FILE       *errf;
	static char errbuf[65536];
	size_t      errn;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio_underrun";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Land the digital-tier caps reply (plus a JXL "no" to latch the
	 * graphics settle flag) so the stream opens on the first PCM below. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		termgfx_termio_pump();
	}
	assert(termgfx_termio_audio_available() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Feed past the cushion and into RUN, with four chunks of headroom so RUN is
	 * unambiguous. Spelled off SST_PREBUFFER_CHUNKS, not as a literal: this read
	 * "12" when the cushion was 8, which silently became a buffer-overrunning
	 * demand for 12 chunks of a 7-chunk array the moment the cushion changed.
	 * This is the baseline the underrun must disturb: if the module silently
	 * ignored the underrun below, this count would be indistinguishable from
	 * what follows. */
	feed_chunks(UNDERRUN_MAX_CHUNKS);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(count_occurrences(out, "A;Queue") >= SST_PREBUFFER_CHUNKS);

	/* Inject the real underrun (channel 2) alongside a caps-shaped decoy
	 * (channel 100) in ONE buffer, so the guard has to do real work to
	 * keep them apart. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=7;2;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		termgfx_termio_pump();
	}
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(out[0] == '\0');   /* neither CSI produces a reply of its own */

	/* Re-primed: a single fresh chunk must NOT ship yet. If the underrun
	 * had been dropped (module stayed in RUN), this chunk would go out
	 * immediately as its own A;Queue -- that is exactly the dribble the
	 * re-prime exists to prevent. */
	feed_chunks(1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(count_occurrences(out, "A;Queue") == 0);

	/* Completing the cushion releases it as ONE batch: a fresh cushion
	 * release, not a dribble. Spelled against SST_PREBUFFER_CHUNKS rather than
	 * a literal, because the count IS the cushion -- when the comic-intro fix
	 * took it 3 -> 8 to cover a big frame's drain, a literal here would have
	 * had to be re-guessed instead of just following. */
	feed_chunks(SST_PREBUFFER_CHUNKS - 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);
	assert(count_occurrences(out, "A;Queue") == SST_PREBUFFER_CHUNKS);

	/* Confirm exactly one underrun was actually counted by the module
	 * (not zero -- silently absorbed; not two -- the channel-100 decoy
	 * misread as a second one). termgfx_stream_destroy() only prints this
	 * line at all if chunks/underruns/dropped is nonzero, so capture
	 * stderr around termgfx_termio_audio_stop(), which is what tears the stream
	 * down. */
	errfd = mkstemp(errpath);
	assert(errfd >= 0);
	close(errfd);
	errf = freopen(errpath, "w", stderr);
	assert(errf != NULL);
	termgfx_termio_audio_stop();
	fflush(stderr);
	errf = freopen("/dev/null", "w", stderr);   /* stop writing to errpath */
	(void)errf;

	errfd = open(errpath, O_RDONLY);
	assert(errfd >= 0);
	errn = (size_t)read(errfd, errbuf, sizeof errbuf - 1);
	close(errfd);
	unlink(errpath);
	errbuf[errn] = '\0';

	assert(strstr(errbuf, "1 underrun(s)") != NULL);
	assert(strstr(errbuf, "2 underrun(s)") == NULL);

	printf("SST_IO_AUDIO_UNDERRUN OK\n");
	return 0;
}
