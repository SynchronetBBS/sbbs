/* Regression test for the strand-on-static-screen defect (F5 Sky control
 * panel BLACK until the mouse moves; toggling speech leaves a half-erased
 * red X until the mouse moves): termgfx_termio_present() can defer a frame at the
 * DSR-ack pacing gate or the backpressure gate WITHOUT sending it and
 * WITHOUT updating g_last, so on a static screen (no further updateScreen()
 * calls) the deferred frame just sits there forever -- present() is only
 * called from the engine's own redraw, which a static panel never triggers
 * again. termgfx_termio_tick() must retain and retry a deferred frame so it flushes
 * on the NEXT poll once the gate clears, with no further engine-side
 * present() call needed (the video twin of the M4 audio "static-screen
 * flush" fix).
 *
 * Also covers the follow-up non-convergence defect: a retry whose retained
 * frame turns out byte-identical to g_last_fb takes the whole-frame dedupe
 * early-return, which must still clear g_present_pending or termgfx_termio_tick()
 * re-invokes termgfx_termio_present() every poll forever on an idle static screen.
 *
 * Separate binary from test_termgfx_termio.c because termgfx_termio keeps file-static
 * session state with no reset. cc'd + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "termgfx_termio.h"

/* TERMGFX_TEST-only seams: termgfx_termio_test_set_inflight() forces the DSR-ack pacing
 * gate to defer (as if g_auto_depth in-flight frames were already
 * unacknowledged) without needing a real multi-frame DSR exchange, and
 * termgfx_termio_test_present_pending() exposes whether a present() call retained a
 * frame instead of sending it. Neither exists on the shipped door (TERMGFX_TEST
 * is never defined by build.sh). */
int termgfx_termio_test_set_inflight(int n);
int termgfx_termio_test_present_pending(void);

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
	static char    out[262144];
	static uint8_t idx[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
	static uint8_t pal[768];
	char           fdarg[32];
	char *         argv[3];
	int            n;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_present_pending";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Get past every startup gate: sixel-capable DA1 + CTDA, char-grid CPR,
	 * and the exact pixel canvas report (same bootstrap as
	 * test_termgfx_termio_canvas.c) -- once this lands, present() reaches the real
	 * pacing/backpressure gates instead of the startup grace. */
	{
		const char *r = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c"
		                 "\x1b[24;80R" "\x1b[4;400;640t";
		assert(send(sv[0], r, strlen(r), 0) > 0);
	}
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);

	memset(idx, 5, sizeof idx);
	memset(pal, 0x30, sizeof pal);

	/* A normal present establishes g_have_last and drains the wire, so the
	 * deferred frame below is unambiguously the ONLY thing being tested. */
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Force the DSR-ack pacing gate to defer: g_inflight >= g_auto_depth
	 * (default depth 3), well inside TERMGFX_PACE_DEADLINE_MS so the unstick
	 * deadline does not fire and mask the gate. */
	assert(termgfx_termio_test_set_inflight(3) == 1);

	/* A changed frame now hits "gated-inflight" and must be RETAINED, not
	 * sent, and not silently dropped either. */
	idx[0] = 9;
	termgfx_termio_present(idx, pal);
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n == 0);                                /* nothing emitted: still gated */
	assert(termgfx_termio_test_present_pending() == 1);     /* frame retained for retry */

	/* Clear the gate (as a real DSR ack would) and let termgfx_termio_tick() retry
	 * the retained frame with no further termgfx_termio_present() call from the
	 * engine -- exactly what a static screen after F5/speech-toggle needs. */
	assert(termgfx_termio_test_set_inflight(0) == 1);
	termgfx_termio_tick();
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n > 0);
	assert(strstr(out, "\x1bP") != NULL);           /* the retried frame's sixel DCS */
	assert(termgfx_termio_test_present_pending() == 0);     /* cleared once sent */

	/* A second tick with nothing pending must be a true no-op: no bytes,
	 * no busy-loop hazard from re-sending a frame that already went out. */
	termgfx_termio_tick();
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n == 0);

	/* Non-convergence regression: a RETRY whose retained frame turns out
	 * byte-identical to g_last_fb must still clear g_present_pending, even
	 * though it takes the whole-frame DEDUPE early-return instead of ever
	 * reaching a commit. g_last_fb right now is exactly the idx/pal the
	 * retry above just sent (idx[0] == 9), so gating and re-presenting THAT
	 * SAME frame reproduces the review's repro path (gate A defers frame A;
	 * a byte-identical frame B overwrites the retain; gate clears; the
	 * retry hits dedupe) without needing a second distinct frame. Before
	 * the fix, g_present_pending's clear sat AFTER the dedupe return, so it
	 * was never reached here and termgfx_termio_tick() would re-invoke
	 * termgfx_termio_present() -- a 64000-byte memcmp, plus a stats redraw + flush
	 * if the bar is on -- every poll forever. */
	assert(termgfx_termio_test_set_inflight(3) == 1);
	termgfx_termio_present(idx, pal);                       /* idx/pal unchanged since last send */
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n == 0);                                 /* still gated */
	assert(termgfx_termio_test_present_pending() == 1);      /* retained for retry */

	assert(termgfx_termio_test_set_inflight(0) == 1);        /* clear the gate */
	termgfx_termio_tick();                                   /* retry -> now hits dedupe, not a commit */
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n == 0);                                  /* dedupe: nothing to send */
	assert(termgfx_termio_test_present_pending() == 0);       /* MUST converge -- this is the fix */

	/* Prove it actually converged, not just cleared once: a further tick
	 * with nothing pending does no work (termgfx_termio_tick() is a no-op guarded
	 * by g_present_pending) and emits nothing -- no busy-loop. */
	termgfx_termio_tick();
	termgfx_termio_flush();
	n = drain(sv[0], out, sizeof out);
	assert(n == 0);
	assert(termgfx_termio_test_present_pending() == 0);

	printf("TERMGFX_TERMIO_PRESENT_PENDING OK\n");
	return 0;
}
