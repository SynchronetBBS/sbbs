/* repro_fade.c -- headless reproduction of the M2 "jerky fade-in" defect.
 *
 * Drives sst_io_present() directly with a synthetic palette fade (a constant
 * index buffer, the palette ramping black -> target over N steps, presented
 * at ~60Hz) over a socketpair whose reader side plays a terminal with a
 * selectable DSR-ack policy:
 *
 *   prompt  -- ack every DSR immediately (an infinitely fast terminal:
 *              isolates the door's own pacing logic)
 *   none    -- never ack (total CPR loss: exposes the 750ms deadline floor)
 *   slow    -- ack each DSR only once the bytes queued ahead of it have
 *              "drained" at a fixed virtual link bandwidth (SYNCSCUMM_REPRO_BW
 *              bytes/ms, default 96) -- the realistic bandwidth-limited BBS
 *              case: a big full frame's CPR reply queues behind its own DCS
 *              bytes, so acks lag exactly in proportion to frame size.
 *
 * NOT part of the unit suite; a measurement tool. Set SYNCSCUMM_TRACE to
 * capture the per-present trace. Usage: repro_fade <prompt|none|slow> [steps]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sst_io.h"

#define FADE_STEPS_DEFAULT 60
#define STEP_HZ            60
#define MAX_PENDING        4096

static uint32_t now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

/* reader/terminal model ---------------------------------------------------- */
enum ack_mode { ACK_PROMPT, ACK_NONE, ACK_SLOW };

static int      g_peer;                 /* our (terminal) end of the socketpair */
static enum ack_mode g_mode;
static double   g_bw;                   /* virtual link bytes/ms (slow mode) */
static double   g_txclock;              /* virtual "all queued bytes sent by" time (ms) */
static uint32_t g_pending_due[MAX_PENDING]; /* due-times of DSR acks not yet released */
static int      g_pending_n;
static uint64_t g_bytes_seen;           /* total bytes the terminal received */
static int      g_dsr_seen;             /* total DSR queries observed */
static int      g_acks_sent;            /* total CPR replies emitted */

/* Send a CPR (cursor-position report) back to the door. */
static void send_cpr(void)
{
	const char *cpr = "\x1b[1;1R";
	(void)!write(g_peer, cpr, strlen(cpr));
	g_acks_sent++;
}

/* Drain everything the door has written to us, count it, find DSR queries
 * ("ESC [ 6 n"), and schedule/emit their acks per the mode. Then release any
 * scheduled (slow-mode) acks now due. */
static void reader_service(void)
{
	uint8_t buf[65536];
	ssize_t n;
	uint32_t t = now_ms();

	while ((n = recv(g_peer, buf, sizeof buf, MSG_DONTWAIT)) > 0) {
		int i;
		/* advance the virtual link clock by this chunk's transmit time */
		if (g_txclock < t)
			g_txclock = t;
		for (i = 0; i < n; i++) {
			g_bytes_seen++;
			if (g_bw > 0)
				g_txclock += 1.0 / g_bw;   /* one byte's worth of link time */
			/* detect the 4-byte DSR "ESC [ 6 n" ending at position i */
			if (i >= 3 && buf[i] == 'n' && buf[i - 1] == '6'
			    && buf[i - 2] == '[' && buf[i - 3] == 0x1b) {
				g_dsr_seen++;
				if (g_mode == ACK_PROMPT) {
					send_cpr();
				} else if (g_mode == ACK_SLOW && g_pending_n < MAX_PENDING) {
					/* due when all bytes through this DSR have drained */
					g_pending_due[g_pending_n++] = (uint32_t)g_txclock;
				}
				/* ACK_NONE: never reply */
			}
		}
	}

	/* release slow-mode acks that have come due */
	if (g_mode == ACK_SLOW) {
		int w = 0, i;
		for (i = 0; i < g_pending_n; i++) {
			if ((int32_t)(t - g_pending_due[i]) >= 0)
				send_cpr();
			else
				g_pending_due[w++] = g_pending_due[i];
		}
		g_pending_n = w;
	}
}

static void sleep_ms(uint32_t ms)
{
	struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}

int main(int argc, char **argv)
{
	int sv[2];
	char fdarg[32];
	int  steps = FADE_STEPS_DEFAULT;
	static uint8_t idx[SST_FB_W * SST_FB_H];
	static uint8_t pal[768];
	int  i, step;
	const char *bwenv = getenv("SYNCSCUMM_REPRO_BW");

	if (argc < 2) {
		fprintf(stderr, "usage: %s <prompt|none|slow> [steps]\n", argv[0]);
		return 2;
	}
	if (strcmp(argv[1], "prompt") == 0) g_mode = ACK_PROMPT;
	else if (strcmp(argv[1], "none") == 0) g_mode = ACK_NONE;
	else if (strcmp(argv[1], "slow") == 0) g_mode = ACK_SLOW;
	else { fprintf(stderr, "bad mode %s\n", argv[1]); return 2; }
	if (argc >= 3) steps = atoi(argv[2]);
	g_bw = bwenv ? atof(bwenv) : 96.0;   /* bytes/ms; 96 ~ 768 kbps */

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); return 1; }
	g_peer = sv[0];

	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	{
		char *av[] = { (char *)"repro_fade", fdarg, NULL };
		if (sst_io_init(2, av) != 1) { fprintf(stderr, "init failed\n"); return 1; }
	}
	sst_io_flush();
	reader_service();   /* swallow the probe burst */

	/* Capability replies: sixel-capable DA1 + SyncTERM CTDA (cterm 1.330) +
	 * an initial DSR ack, then an exact canvas size. Deliberately NOT the JXL
	 * reply, so sst_tier() stays on the sixel path (deterministic, no libjxl
	 * encode-time variance). A large-ish text-area canvas so a full frame is
	 * a realistically heavy sixel image (the crux of the fade defect). */
	{
		const char *caps = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c" "\x1b[1;1R";
		const char *canvas = "\x1b[4;800;1280t";   /* 1280x800 text area */
		(void)!write(g_peer, caps, strlen(caps));
		(void)!write(g_peer, canvas, strlen(canvas));
		sst_io_pump();
	}

	/* Constant index pattern (a horizontal-ish gradient of indices, so the
	 * sixel encode is a realistic non-trivial size, not a flat RLE freebie);
	 * the FADE is entirely in the palette. */
	for (i = 0; i < (int)sizeof idx; i++) {
		int x = i % SST_FB_W, y = i / SST_FB_W;
		idx[i] = (uint8_t)(((x >> 2) + (y >> 2)) & 0xff);
	}

	/* Palette ramp: target palette = a smooth spread; each step scales it by
	 * step/steps so step 0 is black and the last step is the full target. */
	for (step = 0; step <= steps; step++) {
		int c;
		uint32_t t0 = now_ms();
		for (c = 0; c < 256; c++) {
			int tr = (c * 7) & 0xff, tg = (c * 5) & 0xff, tb = (c * 3) & 0xff;
			pal[c * 3 + 0] = (uint8_t)(tr * step / steps);
			pal[c * 3 + 1] = (uint8_t)(tg * step / steps);
			pal[c * 3 + 2] = (uint8_t)(tb * step / steps);
		}
		sst_io_present(idx, pal);
		reader_service();
		sst_io_pump();
		/* hold ~1/60s per fade step, servicing acks so slow-mode releases fire */
		{
			uint32_t elapsed = now_ms() - t0, budget = 1000 / STEP_HZ;
			while (elapsed < budget) {
				sleep_ms(1);
				reader_service();
				sst_io_pump();
				elapsed = now_ms() - t0;
			}
		}
	}

	/* Post-fade drain: keep servicing for up to 2s so late acks/deadline
	 * unsticks that would deliver the final palette are captured in the trace. */
	{
		uint32_t start = now_ms();
		while (now_ms() - start < 2000) {
			sst_io_present(idx, pal);   /* engine would keep presenting the settled frame */
			reader_service();
			sst_io_pump();
			sleep_ms(5);
		}
	}

	sst_io_shutdown();
	close(g_peer);

	fprintf(stderr,
	        "mode=%s steps=%d bw=%.0fB/ms  bytes_to_term=%llu dsr_seen=%d acks_sent=%d "
	        "dropped=%u\n",
	        argv[1], steps, g_bw, (unsigned long long)g_bytes_seen,
	        g_dsr_seen, g_acks_sent, sst_io_frames_dropped());
	return 0;
}
