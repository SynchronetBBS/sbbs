/* test_termgfx_termio_audio_backlog.c -- audio reports ITS OWN share of the FIFO,
 * and the FIFO stays strictly ordered.
 *
 * THE DEFECT THIS PINS. Beneath a Steel Sky's comic intro dropped 702 of 4025
 * audio chunks (~70 seconds of dialogue) while gameplay -- at a HIGHER byte
 * rate -- dropped none. termgfx/audio_stream.c drops a chunk once the door's
 * backlog() reports over 48KB on two consecutive chunk boundaries, a rule
 * meant to spot a link that cannot carry AUDIO; the door reported g_out_len,
 * the depth of the stage it SHARES with video, and the comic's palette storm
 * puts 24KB-average full frames (142 of them over 48KB, the largest 82KB) in
 * there. One big frame tripped an audio rule, so audio dropped itself to make
 * room for video's bursts.
 *
 * So the assertions below are all one idea: with a large video frame stuck in
 * the stage, the number audio is shown must be AUDIO's, and must stay exact as
 * the wire takes the queue apart a piece at a time.
 *
 * Ordering is pinned at the wire rather than by inspecting state, because
 * ordering is what a previous fix for this defect got wrong: it wrote audio
 * ahead of staged video at "safe boundaries" and spliced an A;LoadBlob into an
 * open sixel DCS. Strict FIFO makes that unrepresentable, and check_nesting()
 * below is what says so about the actual bytes.
 *
 * Its own binary because termgfx_termio keeps file-static session state with no reset,
 * so a fresh probe/stream sequence needs a fresh process. cc'd + run by
 * unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

/* Mirrors termgfx_termio.c's own internal TERMGFX_AUDIO_RATE/TERMGFX_CHUNK_MS/
 * TERMGFX_PREBUFFER_CHUNKS defaults (24000, 250, 3) -- termgfx_termio.h no longer
 * exposes these as public macros the way door/termgfx_termio.h once did, so the test
 * keeps its own copy rather than guess a literal. */
#define TERMGFX_AUDIO_RATE       24000
#define TERMGFX_CHUNK_MS         250
#define TERMGFX_PREBUFFER_CHUNKS 3

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Every byte the door has put on the wire this session, in order: the ordering
 * assertions need the whole stream, not a window of it. */
static unsigned char g_wire[4 << 20];
static size_t        g_wire_n;

/* Take up to `cap` bytes off the terminal's end, appending to g_wire. Returns
 * how many moved -- the partial-flush assertions steer by that number, so a
 * short read is data, not a failure. */
static size_t sip(int fd, size_t cap)
{
	unsigned char buf[65536];
	ssize_t       n;
	size_t        got = 0;

	while (got < cap) {
		size_t want = cap - got;

		if (want > sizeof buf)
			want = sizeof buf;
		n = recv(fd, buf, want, MSG_DONTWAIT);
		if (n <= 0)
			break;
		assert(g_wire_n + (size_t)n <= sizeof g_wire);
		memcpy(g_wire + g_wire_n, buf, (size_t)n);
		g_wire_n += (size_t)n;
		got      += (size_t)n;
	}
	return got;
}

/* Drain the terminal's end until it stops giving, flushing the door in between
 * so a stage held back by EAGAIN gets its chance to move. */
static void drain_all(int fd)
{
	int i;

	for (i = 0; i < 200; i++) {
		size_t got = sip(fd, sizeof g_wire);

		termgfx_termio_flush();
		if (got == 0 && termgfx_termio_out_backlog() == 0)
			return;
	}
	assert(!"drain_all: door would not empty");
}

/* Feed `chunks` worth of non-silent stereo PCM in one call.
 * Non-silent matters: the module ships silence from a cached name without
 * encoding, so a silent feed would never exercise the drop rule at all.
 *
 * Sized off TERMGFX_PREBUFFER_CHUNKS with headroom: the cushion is what the
 * largest feed below has to clear, so the buffer has to follow it rather than
 * pin a literal that a later cushion change would silently overrun.
 *
 * A chunk's frame count comes from TERMGFX_AUDIO_RATE and TERMGFX_CHUNK_MS, never from
 * a literal: this file spelled it 2205 ("100ms at 22050Hz") and kept doing so
 * after the mixer moved to 24000, which quietly made every "chunk" here 92ms of
 * a 100ms chunk. It survived only because the assertions below are >= bounds
 * rather than exact counts. Derive it and it cannot rot again. */
#define BACKLOG_CHUNK_FRAMES ((TERMGFX_AUDIO_RATE * TERMGFX_CHUNK_MS) / 1000)
#define BACKLOG_MAX_CHUNKS (TERMGFX_PREBUFFER_CHUNKS + 4)
static void feed_chunks(int chunks)
{
	static int16_t pcm[BACKLOG_CHUNK_FRAMES * 2 * BACKLOG_MAX_CHUNKS];
	int            frames = BACKLOG_CHUNK_FRAMES * chunks;
	int            i;

	assert(chunks <= BACKLOG_MAX_CHUNKS);
	for (i = 0; i < frames; i++) {
		pcm[2 * i]     = (int16_t)(i * 37);
		pcm[2 * i + 1] = (int16_t)(i * -37);
	}
	termgfx_termio_audio_stream(pcm, (size_t)frames);
}

/* Stage one frame shaped like the ones that caused the defect: comfortably
 * over the module's 48KB rule (~75KB here, against the comic's 24KB average
 * and 82KB worst) but nowhere near the 256KB stage, so it parks in the FIFO
 * instead of overrunning it. `seed` keeps it from being deduped against the
 * last one.
 *
 * The size is steered by the color count, not by noise alone. test_termgfx_termio.c's
 * frame -- full 256-index noise over a single flat palette -- is built to
 * OVERRUN the stage (it asserts the stage-full guard engages), because sixel
 * encodes one pass per color and 256 identical colors means 256 passes. That
 * frame is useless here: it pins g_out_len at the cap, out_put() then refuses
 * the audio outright, and the test would measure the door's drop path instead
 * of the backlog accounting it is about. 16 colors over a genuinely distinct
 * palette lands in the window this test needs, and is closer to what a real
 * ScummVM frame does anyway. */
static void present_big(uint8_t seed)
{
	static uint8_t idx[320 * 200];
	static uint8_t pal[768];
	int            i;

	for (i = 0; i < (int)sizeof idx; i++)
		idx[i] = (uint8_t)((i ^ 0x5a) & 0x0f);   /* noisy: defeats sixel RLE */
	for (i = 0; i < 256; i++) {                  /* distinct: no two indices merge */
		pal[i * 3]     = (uint8_t)(i * 7);
		pal[i * 3 + 1] = (uint8_t)(i * 13);
		pal[i * 3 + 2] = (uint8_t)(i * 29);
	}
	idx[0] = (uint8_t)(seed & 0x0f);
	termgfx_termio_present(idx, pal);
}

/* Count non-overlapping occurrences of a byte string in the capture. Not
 * strstr(): the wire is binary (a JXL blob may carry NULs), so the capture is
 * never treated as a C string. */
static int wire_has(const char *needle, size_t len)
{
	size_t i;
	int    n = 0;

	for (i = 0; len > 0 && i + len <= g_wire_n; i++) {
		if (memcmp(g_wire + i, needle, len) == 0) {
			n++;
			i += len - 1;
		}
	}
	return n;
}

/* Walk the captured wire and fail if any DCS/APC opens inside an open one.
 *
 * Legal by construction: every sequence the door emits is ESC-framed, and no
 * DCS/APC payload it produces (sixel, base64 JXL, base64 Ogg) can contain an
 * ESC, so an introducer seen while a sequence is open is not payload -- it is
 * one sequence spliced into another, which is precisely the corruption the
 * reverted audio-priority attempt shipped. ESC P = DCS, ESC _ = APC, ESC \ =
 * ST. CSI (ESC [) is short, self-terminating and never nests, so it is not
 * tracked.
 */
static void check_nesting(void)
{
	size_t i;
	int    in_seq = 0;
	size_t where  = 0;

	for (i = 0; i + 1 < g_wire_n; i++) {
		if (g_wire[i] != 0x1b)
			continue;
		if (g_wire[i + 1] == '\\') {
			in_seq = 0;
			i++;
			continue;
		}
		if (g_wire[i + 1] == 'P' || g_wire[i + 1] == '_') {
			if (in_seq) {
				fprintf(stderr, "FAIL: ESC %c at %zu opened inside the"
				        " sequence opened at %zu\n",
				        g_wire[i + 1], i, where);
				assert(!"nested DCS/APC on the wire");
			}
			in_seq = 1;
			where  = i;
			i++;
		}
	}
}

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	int         bufsz = 8192;
	size_t      video_backlog;
	size_t      with_audio;
	size_t      audio_bytes;
	char        errpath[] = "/tmp/test_termgfx_termio_audio_backlog_err.XXXXXX";
	int         errfd;
	FILE       *errf;
	static char errbuf[65536];
	size_t      errn;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	/* Small socket buffers so a single frame outruns the wire and parks in the
	 * stage: that parked frame IS the defect's trigger, and without it every
	 * flush would succeed and there would be no backlog to misreport. */
	setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsz, sizeof bufsz);
	setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof bufsz);

	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio_backlog";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	drain_all(sv[0]);

	/* Sixel-capable DA1 + CTerm 1.330, the digital-audio caps reply, and a JXL
	 * "no" -- the last both latches the graphics settle flag (so nothing here
	 * waits on it in real time) and keeps the present path on sixel, whose DCS
	 * is what check_nesting() watches audio not splice into. */
	{
		const char *replies = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c"
		                      "\x1b[=7;100;1n" "\x1b[=1;0n" "\x1b[24;80R";
		/* ESC[4;h;wt (test_termgfx_termio.c:98 uses the same report shape). 1280x800 is
		 * chosen with present_big()'s palette to put a frame at ~75KB: over the
		 * module's 48KB rule -- without which there would be no misreport to
		 * catch -- and well under the 256KB stage, so it parks rather than
		 * overruns. test_termgfx_termio.c reports 4096x2560 instead precisely because
		 * it WANTS the overrun; see present_big(). */
		const char *canvas_reply = "\x1b[4;800;1280t";

		assert(send(sv[0], replies, strlen(replies), 0) > 0);
		termgfx_termio_pump();
		assert(send(sv[0], canvas_reply, strlen(canvas_reply), 0) > 0);
		termgfx_termio_pump();
	}
	assert(termgfx_termio_audio_available() == 1);
	drain_all(sv[0]);
	assert(termgfx_termio_audio_backlog() == 0);   /* the probe's own bytes have left */

	/* ---- (1) a big video frame is not audio's backlog ---- */

	present_big(1);
	video_backlog = termgfx_termio_out_backlog();
	/* The frame really is parked, and really is over the module's 48KB rule --
	 * otherwise everything below would pass for the wrong reason. */
	assert(video_backlog > 48 * 1024);
	/* ...and it parked without overrunning the stage. If a future change to the
	 * encoder or the canvas fit pushes this to the 256KB cap, out_put() starts
	 * refusing the audio outright and this test quietly stops testing the
	 * backlog accounting at all -- so say so here rather than let it rot into a
	 * test of the drop path. */
	assert(termgfx_termio_frames_dropped() == 0);
	assert(termgfx_termio_hung_up() == 0);   /* EAGAIN is backpressure, not an error */

	/* THE FIX, in one line. Before it, this returned g_out_len -- the whole
	 * stage -- and every one of those bytes was video's. */
	assert(termgfx_termio_audio_backlog() == 0);

	/* ---- (2) audio's share is exact, and only audio's ---- */

	/* A full cushion's worth: below TERMGFX_PREBUFFER_CHUNKS the module is still in
	 * PRIME and holds every chunk internally, so it would stage nothing and
	 * there would be no audio in the FIFO to measure. Completing the cushion
	 * releases all of them back to back -- which is also the largest single
	 * burst of audio this door can ever put, so it is the right one to test the
	 * accounting with. */
	feed_chunks(TERMGFX_PREBUFFER_CHUNKS);
	with_audio  = termgfx_termio_out_backlog();
	audio_bytes = with_audio - video_backlog;
	assert(audio_bytes > 0);                              /* audio did stage */
	assert(termgfx_termio_audio_backlog() == audio_bytes);        /* ...and exactly that much */
	assert(termgfx_termio_audio_dropped() == 0);                  /* the door kept all of it */

	/* ---- (3) exact across PARTIAL flushes ---- */

	/* Take the parked frame apart a bite at a time. Every one of these flushes
	 * writes only PART of the stage, and while the wire is still working
	 * through the video at the front of the FIFO, audio's share must not move
	 * by a single byte -- the flush is attributing every written byte to video,
	 * which is whose it is.
	 *
	 * This is the assertion that says the accounting is by STREAM OFFSET and
	 * not by stage depth. A backlog derived from how much is staged would sag
	 * on every one of these iterations, because the stage is shrinking and
	 * audio's share of it is not.
	 *
	 * Stop while the video still has a comfortable margin left, so "the wire
	 * has not reached the audio yet" stays unambiguous. */
	{
		int i;

		for (i = 0; i < 200 && termgfx_termio_out_backlog() > audio_bytes + 8192; i++) {
			sip(sv[0], 8192);
			termgfx_termio_flush();
			assert(termgfx_termio_audio_backlog() == audio_bytes);
		}
		assert(termgfx_termio_out_backlog() < with_audio);   /* the wire really did take video */
		assert(termgfx_termio_out_backlog() > audio_bytes);  /* ...and has not reached audio */
		assert(termgfx_termio_audio_backlog() == audio_bytes);
	}

	/* Now take the queue apart in small bites all the way down. Audio's
	 * backlog must fall monotonically, never exceed the stage's own depth, and
	 * land exactly on zero -- an off-by-anything in the pruning shows up here. */
	{
		size_t prev = audio_bytes;
		int    i;

		for (i = 0; i < 400 && termgfx_termio_out_backlog() != 0; i++) {
			size_t now;

			sip(sv[0], 1500);   /* deliberately not a chunk multiple */
			termgfx_termio_flush();
			now = termgfx_termio_audio_backlog();
			assert(now <= prev);                       /* monotone: nothing re-queues */
			assert(now <= termgfx_termio_out_backlog());       /* audio is a SHARE of the stage */
			prev = now;
		}
		assert(termgfx_termio_out_backlog() == 0);
		assert(termgfx_termio_audio_backlog() == 0);           /* fully drained == nothing pending */
	}

	/* ---- (4) interleaved staging keeps its coordinates ---- */

	/* video, audio, video, audio -- with the stage backpressured throughout, so
	 * the ring holds several live segments with video between them. Audio's
	 * share must still be exactly the sum of the audio runs. */
	{
		size_t v1, a1, v2, a2;

		present_big(2);
		v1 = termgfx_termio_out_backlog();
		feed_chunks(3);
		a1 = termgfx_termio_out_backlog() - v1;
		assert(termgfx_termio_audio_backlog() == a1);

		present_big(3);
		v2 = termgfx_termio_out_backlog();
		feed_chunks(3);
		a2 = termgfx_termio_out_backlog() - v2;
		assert(termgfx_termio_audio_backlog() == a1 + a2);
		assert(termgfx_termio_audio_dropped() == 0);
	}

	/* ---- (5) the wire itself: strict FIFO, nothing spliced ---- */

	drain_all(sv[0]);
	assert(termgfx_termio_audio_backlog() == 0);
	check_nesting();

	/* An audio APC and a sixel DCS both actually reached the wire, so (5) was
	 * a real test of interleaving and not of an empty capture. */
	assert(wire_has("\x1b_", 2) > 0);   /* APCs went out (audio's carrier) */
	assert(wire_has("A;", 2) > 0);      /* ...and they are the audio ones */
	assert(wire_has("\x1bP", 2) > 0);   /* ...alongside sixel DCSes */

	/* ---- (6) the module dropped nothing ---- */

	/* The whole point: with the fix, a parked 48KB+ frame never reads as audio
	 * congestion, so the two-strike rule never fires. termgfx_stream_destroy()
	 * prints its telemetry only when a counter is nonzero, so capture stderr
	 * around termgfx_termio_audio_stop() -- which is what tears the stream down -- and
	 * assert on what it says. Before the fix this read "N drop(s)" with N > 0.
	 */
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

	assert(strstr(errbuf, "chunks") != NULL);      /* the stream really ran */
	assert(strstr(errbuf, "0 drop(s)") != NULL);   /* ...and never dropped */
	assert(termgfx_termio_audio_dropped() == 0);

	printf("TERMGFX_TERMIO_AUDIO_BACKLOG OK\n");
	return 0;
}
