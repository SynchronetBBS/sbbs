/* Unit test for termgfx_termio: run the session against a socketpair, play the
 * terminal's side with canned replies, assert what the door emits and
 * what its parsers conclude.  cc'd + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "termgfx_termio.h"

/* Backstop for the backpressure case below: if termgfx_termio_present() ever spins
 * (the carried review-flag regressing), fail loudly instead of hanging the
 * test runner forever. */
static void alarm_handler(int sig)
{
	static const char msg[] = "FAIL: backpressure hang (present() did not return)\n";
	(void)sig;
	write(2, msg, sizeof(msg) - 1);
	_exit(1);
}

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
	int sv[2];
	static char out[262144];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	char fdarg[32];
	snprintf(fdarg, sizeof(fdarg), "-s%d", sv[1]);
	char *argv[] = { (char *)"test_termgfx_termio", fdarg, NULL };

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof(out));
	/* probe burst: enter, status-off, term probe, DA1+CTDA, JXL query */
	assert(strstr(out, "\x1b[?25l"));                 /* cursor hidden (term_enter) */
	assert(strstr(out, "\x1b[c"));                     /* DA1 */
	assert(strstr(out, "SyncTERM:Q;JXL"));             /* JXL query */

	/* play a sixel-capable DA1 reply + CTDA (cterm version 1.330, which is
	 * also >= TERMGFX_CTERM_VER_BLOB/1.329 -- the JXL wire-format test below
	 * rides the inline DrawJXLBlob path because of this same reply), then a
	 * DSR ack. Deliberately NOT feeding the JXL-capable reply yet: that's
	 * done further down, AFTER the sixel-tier backpressure/recovery tests
	 * below -- sst_tier() would otherwise pick JXL for every present() call
	 * from here on (in a build with WITH_JXL), breaking their sixel-DCS
	 * assertions. */
	const char *replies = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c" "\x1b[24;80R";
	assert(send(sv[0], replies, strlen(replies), 0) > 0);
	termgfx_termio_pump();
	assert(termgfx_termio_have_sixel() == 1);

	/* hotkeys: Ctrl-S toggles stats, q requests quit.
	 *
	 * Stats-bar immediate draw (M2 live-retest): fa3eb126f7 made the toggle
	 * draw (or erase) the bar's own bytes synchronously from the hotkey
	 * path, not wait for the next present() -- a static/deduped scene may
	 * not present again for a long time. Drain right here, with NO
	 * present() call in between, and look for the bar's actual wire bytes
	 * (not just the g_stats flag, which was already correct before that
	 * fix -- only the draw was missing). */
	assert(send(sv[0], "\x13", 1, 0) == 1);
	termgfx_termio_pump();
	assert(termgfx_termio_stats_visible() == 1);
	{
		int n = drain(sv[0], out, sizeof(out));
		assert(n > 0);
		assert(strstr(out, "\x1b[30;46m") != NULL);   /* bar's color attribute, unprompted */
	}

	assert(send(sv[0], "q", 1, 0) == 1);
	termgfx_termio_pump();
	assert(termgfx_termio_quit_requested() == 1);

	/* backpressure: present() must never hang, even when the peer stops
	 * draining and out_put()'s 256KB stage buffer can't be flushed. Force a
	 * single oversized frame (fake a large probed canvas, so the fitted
	 * sixel is well over 256KB by itself -- present()'s own inflight/
	 * "prior frame not flushed" gates would otherwise keep a REALISTIC
	 * frame's writes from ever reaching out_put()'s internal full-buffer
	 * path at all) against a shrunk-SO_RCVBUF socket that nothing drains. */
	{
		int rcvbuf = 1024;
		static uint8_t idx[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
		static uint8_t pal[768];
		int i;
		const char *canvas_reply = "\x1b[4;2560;4096t";   /* ESC[4;h;wt, exact
		                                                    * text-area pixels:
		                                                    * 4096x2560 (320:200
		                                                    * aspect exactly) so
		                                                    * the fit hits
		                                                    * SST_SCALE_MAX
		                                                    * (2048) wide. */

		setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
		assert(send(sv[0], canvas_reply, strlen(canvas_reply), 0) > 0);
		termgfx_termio_pump();

		for (i = 0; i < (int)sizeof idx; i++)
			idx[i] = (uint8_t)(i ^ 0x5a);   /* noisy: defeats sixel RLE, maximizing size */
		memset(pal, 0x40, sizeof pal);

		signal(SIGALRM, alarm_handler);
		alarm(5);
		for (i = 0; i < 3; i++) {
			idx[0] = (uint8_t)i;   /* keep it changing so it's never deduped */
			termgfx_termio_present(idx, pal);
		}
		alarm(0);

		assert(termgfx_termio_hung_up() == 0);        /* EAGAIN is backpressure, not an error */
		assert(termgfx_termio_frames_dropped() > 0);   /* the stage-full guard actually engaged */

		/* Drain the socket back to empty so out_put()'s stage buffer (still
		 * holding whatever this storm queued but couldn't flush) empties
		 * before the shutdown assertions below -- otherwise termgfx_termio_shutdown()'s
		 * own status-restore write finds the stage still full and gets
		 * silently dropped by the same guard just verified above.
		 *
		 * Stats-bar survival (M2 live-retest finding): g_stats has been ON
		 * since the hotkey test above and was never toggled off, so all 3
		 * oversized presents above tried to draw it. Before the fix, once
		 * out_put()'s stage-full guard pinned g_out_len at its 256KB cap,
		 * EVERY later out_put() call that present() made -- sst_stats_draw()
		 * included -- computed zero room and silently dropped, with no
		 * resync marker the way a dropped image gets (g_need_st/g_have_last).
		 * That starved the bar exactly during the big/heavy frames a player
		 * is most likely to have it open for. termgfx_termio_present() now queues the
		 * bar BEFORE the image so it wins the race for stage space; confirm
		 * its distinctive color attribute ("30;46", set nowhere else) shows
		 * up somewhere in this drain, not just the giant image bytes. */
		{
			int saw_stats_marker = 0;
			for (i = 0; i < 200; i++) {
				termgfx_termio_flush();
				if (drain(sv[0], out, sizeof(out)) > 0 && strstr(out, "\x1b[30;46m") != NULL)
					saw_stats_marker = 1;
			}
			assert(saw_stats_marker);
		}

		/* Recovery: a dropped frame must not get recorded as "what the
		 * client has" (review finding 1). Restore a normal-sized canvas
		 * (the 4096x2560 probe above only exists to force the drop) so this
		 * next frame is NOT itself dropped, present one changed frame, and
		 * confirm the door (a) closes the DCS the dropped frame may have
		 * left dangling by leading with the recovery ST before its own DCS,
		 * and (b) sends a FULL frame rather than diffing against the
		 * partial frame the client never fully received -- i.e. it must
		 * NOT take the dirty path (sst_dirty_sixel_present()'s per-box
		 * "\x1b7"/DECSC ... "\x1b8"/DECRC wrap is the tell; the full-frame
		 * path never emits it). */
		{
			unsigned before = termgfx_termio_frames_dropped();
			static uint8_t idx2[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
			static uint8_t pal2[768];
			const char *   small_canvas = "\x1b[4;400;640t";   /* back to a
			                                                    * canvas small
			                                                    * enough that
			                                                    * one frame
			                                                    * fits well
			                                                    * under the
			                                                    * 256KB stage */
			int n;

			assert(send(sv[0], small_canvas, strlen(small_canvas), 0) > 0);
			termgfx_termio_pump();

			memset(idx2, 7, sizeof idx2);   /* flat fill: compresses tiny,
			                                 * keeps the drained bytes free
			                                 * of embedded NULs for strstr() */
			memset(pal2, 0x20, sizeof pal2);

			termgfx_termio_present(idx2, pal2);
			termgfx_termio_flush();
			n = drain(sv[0], out, sizeof(out));
			assert(n > 2);

			/* (a) recovery ST leads, ahead of this frame's own DCS -- the
			 * stage was fully drained above, so it must be the very first
			 * bytes out. */
			assert(out[0] == 0x1b && out[1] == '\\');
			assert(strstr(out, "\x1bP") != NULL);          /* this frame's own DCS */

			/* (b) a full frame: raster attributes present, and none of the
			 * dirty path's per-box DECSC/DECRC wrap bytes. */
			assert(strstr(out, "\"1;1;") != NULL);          /* sixel raster attributes */
			assert(strstr(out, "\x1b" "7") == NULL);        /* not the dirty path's wrap */

			assert(termgfx_termio_frames_dropped() == before);      /* this present did not itself drop */
		}
	}

	/* JXL tier (Task 7): feed the JXL-capable reply now, after every sixel-
	 * tier assertion above has already run -- byte-at-a-time, to also cover
	 * the reply parsing when split across many separate reads/sends (the
	 * assertions above only ever exercised a single, whole-buffer send()). */
	{
		const char *jxl_reply = "\x1b[=1;1-n";
		size_t k;
		for (k = 0; k < strlen(jxl_reply); k++) {
			assert(send(sv[0], jxl_reply + k, 1, 0) == 1);
			termgfx_termio_pump();
		}
	}
	assert(termgfx_termio_jxl_supported() == 1);

#ifdef WITH_JXL
	/* Present a changed frame now that the door has confirmed both JXL
	 * decode support (g_jxl, via the reply just fed) and this build's own
	 * encoder (WITH_JXL, from unit_termgfx_termio.sh's -DWITH_JXL when libjxl was
	 * found): sst_tier() must switch to JXL, and the wire bytes must carry
	 * the SyncTERM APC introducer + DrawJXL verb (door_io.c:1938-1957
	 * door_emit_jxl() pattern) instead of a sixel DCS. The CTDA reply fed
	 * above (cterm version 1.330) is >= TERMGFX_CTERM_VER_BLOB (1.329), so
	 * this rides the inline DrawJXLBlob path, not cache-file Store+Draw. */
	{
		static uint8_t idx3[TERMGFX_TERMIO_FB_W * TERMGFX_TERMIO_FB_H];
		static uint8_t pal3[768];
		int            n;

		memset(idx3, 9, sizeof idx3);      /* flat fill: tiny JXL encode either way */
		memset(pal3, 0x55, sizeof pal3);

		termgfx_termio_present(idx3, pal3);
		termgfx_termio_flush();
		n = drain(sv[0], out, sizeof(out));
		assert(n > 2);

		assert(strstr(out, "\x1b_SyncTERM:C;DrawJXLBlob") != NULL);  /* APC blob + verb */
		assert(strstr(out, "\x1bP") == NULL);                       /* not a sixel DCS */
	}
#endif

	/* Stats-bar immediate erase (M2 live-retest), same synchronous-draw
	 * contract as the show above, now exercised AFTER a run of real
	 * present()s (backpressure storm, dirty-path recovery, and -- in a
	 * WITH_JXL build -- a JXL frame) rather than right at session start:
	 * toggling off must clear the bar's row on its own, with no present()
	 * call needed to paint over it. */
	{
		int n;

		assert(send(sv[0], "\x13", 1, 0) == 1);   /* Ctrl-S again: hide */
		termgfx_termio_pump();
		assert(termgfx_termio_stats_visible() == 0);
		n = drain(sv[0], out, sizeof(out));
		assert(n > 0);
		assert(strstr(out, "\x1b[K") != NULL);        /* erase-to-EOL on the bar's row */
		assert(strstr(out, "\x1b[30;46m") == NULL);   /* an erase, not a redraw */
	}

	/* shutdown, while the socketpair is still open: restores the status
	 * line (DECSSDT type 1 -- no DECRQSS reply was ever fed to this
	 * session, so g_status_type stays -1 and the default applies) ahead
	 * of term_leave. Must run BEFORE the close()+hangup test below --
	 * that closes sv[0], and this side can't drain anything the door
	 * writes after that. */
	termgfx_termio_shutdown();
	drain(sv[0], out, sizeof(out));
	assert(strstr(out, "\x1b[1$~"));                   /* status line restored (type 1) */
	assert(strstr(out, "\x1b[?25h"));                  /* cursor restored (term_leave) */

	/* hangup: closing our end must be detected by the next pump() as a
	 * peer EOF, even though termgfx_termio_shutdown() already ran above -- see
	 * termgfx_termio_pump()'s comment on why it isn't gated on g_active. */
	close(sv[0]);
	termgfx_termio_pump();
	assert(termgfx_termio_hung_up() == 1);

	/* idempotency: a second shutdown() (e.g. an atexit-style caller that
	 * doesn't track whether it already ran) must not crash. */
	termgfx_termio_shutdown();

	printf("SST_IO OK\n");
	return 0;
}
