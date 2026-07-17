/* test_sst_io_audio_static.c -- fed audio reaches the wire with NO present().
 *
 * THE DEFECT THIS PINS. Beneath a Steel Sky's comic intro has seconds-long
 * gaps in its dialogue, right from the first panel, while gameplay -- at a
 * HIGHER byte rate -- is flawless. A comic panel is a STILL: the game draws it
 * once and then talks over it for seconds, so updateScreen() presents nothing
 * and video_term.cpp's present path never runs.
 *
 * Two flush policies met there and left a hole between them:
 *
 *   - termgfx/audio_stream.c calls io.flush() in exactly TWO places -- when
 *     the PRIME cushion releases (audio_stream.c:416) and at stop
 *     (audio_stream.c:470). In steady RUN it only stages chunks via io.put()
 *     and leaves writing them to the door's main loop.
 *   - sst_io.c only flushed from sst_io_present() (sst_io.c:2350, 2627).
 *
 * So with nothing drawn, nothing flushed, and every chunk the mixer produced
 * sat in the 256KB stage unwritten. Its share of the stage
 * (sst_io_audio_backlog()) then climbed past the module's 48KB rule and the
 * module started dropping chunks -- concluding "this link cannot carry audio"
 * about a socket that was completely IDLE. The live trace is unambiguous: no
 * present() between 34s and 42s, and 71 drops all landing at once at 42s, the
 * first present after the gap (8s x 10 chunks/s ~= 71). Gameplay never showed
 * it because constant dirty-rect redraws mean constant flushes.
 *
 * So the assertion is the defect, stated directly: feed PCM, never present,
 * never flush by hand, and the terminal must still receive the audio. Every
 * sst_io_flush() call in the other audio tests hides this bug -- that is why
 * this test's defining feature is the calls it does NOT make.
 *
 * Its own binary because sst_io keeps file-static session state with no reset,
 * so a fresh probe/stream sequence needs a fresh process. cc'd + run by
 * unit_sst_io.sh.
 */
#include "sst_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Every byte the terminal received this session, in order. The terminal end
 * reading its socket is not the door flushing: this drain is the peer, and
 * calling it can never move a byte the door has not already written. */
static unsigned char g_wire[8 << 20];
static size_t        g_wire_n;

static size_t sip(int fd)
{
	unsigned char buf[65536];
	ssize_t       n;
	size_t        got = 0;

	while ((n = recv(fd, buf, sizeof buf, MSG_DONTWAIT)) > 0) {
		assert(g_wire_n + (size_t)n <= sizeof g_wire);
		memcpy(g_wire + g_wire_n, buf, (size_t)n);
		g_wire_n += (size_t)n;
		got      += (size_t)n;
	}
	return got;
}

/* Count non-overlapping occurrences of a byte string in the capture. Not
 * strstr(): the wire is binary (an Opus chunk carries NULs), so the capture is
 * never treated as a C string. */
static int count_bytes(const unsigned char *needle, size_t len)
{
	size_t i;
	int    n = 0;

	for (i = 0; i + len <= g_wire_n; i++) {
		if (memcmp(g_wire + i, needle, len) == 0) {
			n++;
			i += len - 1;
		}
	}
	return n;
}

/* One mixer tick's worth of non-silent PCM. Non-silent matters: the module
 * ships silence from a cached name without encoding, so a silent feed would
 * never put a chunk in the stage and the test would pass on an empty stream.
 *
 * Exactly one chunk, derived from SST_AUDIO_RATE and SST_CHUNK_MS, so `blocks`
 * below still reads as "chunks fed" whatever those two say. It was spelled
 * "2205" for "100ms at 22050Hz" and stayed that way through the move to 24000,
 * where it silently became 92ms of a 100ms chunk -- and the count asserted
 * below is a near-exact >= bound, so this feeding fractional chunks is how the
 * assertion breaks without the module doing anything wrong. */
#define STATIC_CHUNK_FRAMES ((SST_AUDIO_RATE * SST_CHUNK_MS) / 1000)
static void feed_one_chunk(int seq)
{
	static int16_t pcm[STATIC_CHUNK_FRAMES * 2];
	int            i;

	for (i = 0; i < STATIC_CHUNK_FRAMES; i++) {
		int s = seq * STATIC_CHUNK_FRAMES + i;

		pcm[2 * i]     = (int16_t)(s * 37);
		pcm[2 * i + 1] = (int16_t)(s * -37);
	}
	sst_io_audio_stream(pcm, STATIC_CHUNK_FRAMES);
}

int main(void)
{
	static const unsigned char queue[] = { 'A', ';', 'Q', 'u', 'e', 'u', 'e' };
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	int         i;
	int         queues;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_sst_io_audio_static";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(sst_io_init(2, argv) == 1);
	assert(sst_io_active() == 1);

	/* The ONE hand flush in this test, and it is startup, not the path under
	 * test: the capability burst has to reach the terminal before the terminal
	 * can answer it. Everything after this line stands or falls on the door
	 * flushing audio by itself. */
	sst_io_flush();
	sip(sv[0]);

	/* Digital-audio caps reply (ESC[=7;100;1n) plus a JXL "no" (ESC[=1;0n),
	 * which latches the graphics settle flag so nothing here waits out
	 * SST_GFX_SETTLE_MS in real time. No sixel DA1 and no canvas report: this
	 * session never presents, so the graphics tier is deliberately left
	 * uninteresting. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";

		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		sst_io_pump();
	}
	assert(sst_io_audio_available() == 1);

	/* 80 chunks == 8 seconds, the exact length of the still-panel gap in the
	 * trace (34s..42s) that produced 71 drops. Fed one chunk at a time, which
	 * is how the mixer really feeds it: audio_term.cpp's tick() pulls what the
	 * wall clock owes and hands that block straight to sst_io_audio_stream().
	 *
	 * NOT ONE sst_io_flush() and NOT ONE sst_io_present() in this loop. That
	 * is the whole test. sip() is the terminal reading its own socket, which
	 * cannot make the door write anything. */
	for (i = 0; i < 80; i++) {
		feed_one_chunk(i);
		sip(sv[0]);
	}

	/* The audio is ON THE WIRE. Before the fix this was 0: every chunk the
	 * module staged was still sitting in g_out, and the module had spent the
	 * back half of the loop dropping chunks because the backlog it was shown
	 * kept growing -- over an idle socket. Count the chunk-specific marker
	 * rather than "any A;": the caps handshake already put an A;Volume on the
	 * wire before a single PCM sample existed, so a bare "A;" check passes on
	 * a stream carrying no audio at all. */
	queues = count_bytes(queue, sizeof queue);
	printf("A;Queue on the wire with no present(): %d of 80 chunks fed\n", queues);
	/* 80 fed, 80 expected: the cushion holds the first SST_PREBUFFER_CHUNKS
	 * and then releases all of them, so by the end of the feed every closed
	 * chunk should have shipped. >=72 leaves the cushion's worth of slack for
	 * a chunk-boundary straggler while staying impossible to reach if the
	 * module ever starts dropping again -- 71 drops is what the defect cost,
	 * so anything near that number must fail here. */
	assert(queues >= 72);

	/* ...and the door itself refused nothing. A drop here would mean the stage
	 * filled, which with a flush per feed it cannot. */
	assert(sst_io_audio_dropped() == 0);
	/* Nothing is left holding: the door wrote what it staged rather than
	 * parking it for a present() that a still panel never makes. */
	assert(sst_io_audio_backlog() == 0);
	assert(sst_io_hung_up() == 0);

	printf("SST_IO_AUDIO_STATIC OK\n");
	return 0;
}
