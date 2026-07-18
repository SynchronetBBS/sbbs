/* mixertest.c */

/* Regression test for xp_mixer_pull()'s FIFO buffer draining.
 *
 * Guards the fix for the gap that used to leave the tail of a device-callback
 * pull silent whenever a queued buffer ended part way through the pull: a
 * channel fed a continuous stream chunked into back-to-back buffers (a game
 * door streaming Opus/PCM through SyncTERM's A;Queue) dropped up to one
 * callback of silence at every buffer boundary, heard as constant clicking.
 * The pull must now drain successive buffers into the same call, gaplessly,
 * while still mixing a crossfade overlay pair concurrently.
 *
 * Drives the mixer directly with no audio device, so it is fast and headless.
 * Returns the number of failed checks (0 == pass) for use as a CI exit code. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpbeep.h"

/* Internal to xpbeep.c (a device backend's callback pulls it); not in the
 * public header, so declare it here the same way the backends do. */
size_t xp_mixer_pull(int16_t *out, size_t frames);

static int failures = 0;

static void
check(const char *name, int ok)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
	if (!ok)
		failures++;
}

/* A buffer of `n` frames, every sample set to `v` (both channels).  The mixer
 * takes ownership and frees it. */
static int16_t *
make_buf(size_t n, int16_t v)
{
	int16_t *b = (int16_t *)malloc(n * XPBEEP_FRAMESIZE);
	size_t   i;

	for (i = 0; i < n; i++) {
		b[i * 2 + 0] = v;
		b[i * 2 + 1] = v;
	}
	return b;
}

/* Left-channel sample of frame `f` in a just-pulled buffer. */
#define LCH(out, f) ((out)[(f) * 2])

/* Two buffers, one pull straddling their boundary: the whole pull is filled. */
static void
test_gapless_boundary(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[150 * 2];
	size_t            got;

	printf("gapless_boundary: two 100-frame bufs, pull 150\n");
	xp_audio_append(h, make_buf(100, 10000), 100, NULL);
	xp_audio_append(h, make_buf(100, 10000), 100, NULL);
	memset(out, 0, sizeof out);
	got = xp_mixer_pull(out, 150);
	check("whole pull populated (max_n == 150)", got == 150);
	check("frame 50 (buf A) audible", LCH(out, 50) != 0);
	check("frame 125 (buf B, was the gap) audible", LCH(out, 125) != 0);
	xp_audio_close(h);
}

/* Three short buffers drained in a single pull, then the queue is empty. */
static void
test_multi_drain(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[150 * 2];
	int               i;

	printf("multi_drain: three 50-frame bufs, pull 150 empties all\n");
	for (i = 0; i < 3; i++)
		xp_audio_append(h, make_buf(50, 10000), 50, NULL);
	memset(out, 0, sizeof out);
	check("pull drains all three (max_n == 150)", xp_mixer_pull(out, 150) == 150);
	check("frame 25 (buf 0) audible", LCH(out, 25) != 0);
	check("frame 75 (buf 1) audible", LCH(out, 75) != 0);
	check("frame 125 (buf 2) audible", LCH(out, 125) != 0);
	memset(out, 0, sizeof out);
	check("queue now empty (max_n == 0, no crash)", xp_mixer_pull(out, 150) == 0);
	xp_audio_close(h);
}

/* The queue empties part way through a pull: the tail is silent, no overrun. */
static void
test_drain_to_empty(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[200 * 2];

	printf("drain_to_empty: two 50-frame bufs, pull 200\n");
	xp_audio_append(h, make_buf(50, 10000), 50, NULL);
	xp_audio_append(h, make_buf(50, 10000), 50, NULL);
	memset(out, 0x11, sizeof out);
	check("filled only what was queued (max_n == 100)", xp_mixer_pull(out, 200) == 100);
	check("frame 75 (buf 1) audible", LCH(out, 75) != 0);
	check("frame 150 (past end) silent", LCH(out, 150) == 0);
	xp_audio_close(h);
}

/* A buffer exactly the size of the pull hands off cleanly on the next pull. */
static void
test_exact_alignment(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[100 * 2];

	printf("exact_alignment: buf==pull, then successor next pull\n");
	xp_audio_append(h, make_buf(100, 10000), 100, NULL);
	xp_audio_append(h, make_buf(100, 8000), 100, NULL);
	memset(out, 0, sizeof out);
	check("pull 1 full (A)", xp_mixer_pull(out, 100) == 100 && LCH(out, 99) != 0);
	memset(out, 0, sizeof out);
	check("pull 2 full (B)", xp_mixer_pull(out, 100) == 100 && LCH(out, 50) != 0);
	xp_audio_close(h);
}

/* A buffer longer than the pull keeps playing without being dropped. */
static void
test_partial(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[500 * 2];

	printf("partial: buf 1000, pull 500 (no drop)\n");
	xp_audio_append(h, make_buf(1000, 10000), 1000, NULL);
	memset(out, 0, sizeof out);
	check("pull full, buffer continues", xp_mixer_pull(out, 500) == 500 && LCH(out, 499) != 0);
	xp_audio_close(h);
}

/* Consumed state must carry across many pulls, then a mid-pull boundary. */
static void
test_across_pulls(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	int16_t           out[30 * 2];
	int               p;

	printf("across_pulls: 100-frame buf over 30-frame pulls, then successor\n");
	xp_audio_append(h, make_buf(100, 10000), 100, NULL);
	xp_audio_append(h, make_buf(100, 8000), 100, NULL);
	for (p = 0; p < 3; p++) {
		memset(out, 0, sizeof out);
		check("pull full mid-buffer", xp_mixer_pull(out, 30) == 30 && LCH(out, 0) != 0);
	}
	/* frames 90..100 of buf 1, then buf 2 -- gapless across the boundary */
	memset(out, 0, sizeof out);
	check("straddling pull full", xp_mixer_pull(out, 30) == 30);
	check("straddling pull ends on successor", LCH(out, 25) != 0);
	xp_audio_close(h);
}

/* A crossfade overlay must still mix head (decaying) and next (rising)
 * CONCURRENTLY over the overlap -- the fix must not flatten this to a gap. */
static void
test_crossfade_preserved(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	xp_audio_opts_t   o;
	int16_t           out[80 * 2];

	printf("crossfade_preserved: A then B(crossfade 40) overlap mixes both\n");
	xp_audio_append(h, make_buf(80, 10000), 80, NULL);
	memset(&o, 0, sizeof o);
	o.crossfade      = 1;
	o.fade_in_frames = 40;
	xp_audio_append(h, make_buf(80, 10000), 80, &o);
	memset(out, 0, sizeof out);
	check("overlap span populated (max_n >= 80)", xp_mixer_pull(out, 80) >= 80);
	check("early (A) audible", LCH(out, 5) != 0);
	check("mid-overlap audible", LCH(out, 20) != 0);
	check("after overlap (B) audible", LCH(out, 60) != 0);
	xp_audio_close(h);
}

/* A plain buffer draining INTO a crossfade pair fills what it used to leave
 * silent -- the drain loop and overlay path composed. */
static void
test_drain_into_crossfade(void)
{
	xp_audio_handle_t h = xp_audio_open(0.0f, 0.0f);
	xp_audio_opts_t   o;
	int16_t           out[240 * 2];
	size_t            got;

	printf("drain_into_crossfade: [A plain][B overlay->C], pull 240\n");
	xp_audio_append(h, make_buf(50, 10000), 50, NULL);   /* A: plain */
	xp_audio_append(h, make_buf(80, 10000), 80, NULL);   /* B */
	memset(&o, 0, sizeof o);
	o.crossfade      = 1;
	o.fade_in_frames = 40;
	xp_audio_append(h, make_buf(80, 9000), 80, &o);      /* C crossfades over B */
	memset(out, 0, sizeof out);
	got = xp_mixer_pull(out, 240);
	check("A region audible", LCH(out, 25) != 0);
	check("B/C overlap audible", LCH(out, 60) != 0);
	check("C tail audible", LCH(out, 120) != 0);
	check("content spans A+overlap+C (max_n >= 130)", got >= 130);
	xp_audio_close(h);
}

int
main(void)
{
	test_gapless_boundary();
	test_multi_drain();
	test_drain_to_empty();
	test_exact_alignment();
	test_partial();
	test_across_pulls();
	test_crossfade_preserved();
	test_drain_into_crossfade();

	printf("\n%s (%d failure%s)\n", failures ? "MIXERTEST FAILED" : "mixertest ok",
	       failures, failures == 1 ? "" : "s");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* End of mixertest.c */
