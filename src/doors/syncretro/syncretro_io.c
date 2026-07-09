/* syncretro_io.c -- terminal enter/probe/leave + the present path.
 *
 * Mirrors ../syncmoo1/syncmoo1_io.c: SyncRetro owns its present function
 * because termgfx is a compositional toolkit (encode + geometry + pace), not a
 * single present() call. See DESIGN.md sec 6. Stub -- M1 fills these in.
 */
#include "syncretro.h"

#include "term.h"        /* termgfx_term_enter/probe/leave */
#include "sixel.h"       /* sixel encode */
#include "geometry.h"    /* termgfx_geom_fit / _center */
#include "pace.h"        /* termgfx_aimd_update / _rtt_sample */
#include "caps.h"        /* capability parse */
#ifdef WITH_JXL
#include "jxl.h"         /* termgfx_jxl_encode */
#endif

#include <stdio.h>
#include <stdlib.h>

static int g_sock = -1;

int sr_io_init(int sockfd)
{
	g_sock = sockfd;
	/* TODO(M1): emit termgfx_term_enter, then the capability + geometry probe
	 * (termgfx_term_probe / termgfx_query_jxl); replies arrive via
	 * sr_io_feed_reply() as syncretro_input pumps the socket. Block (with a
	 * deadline) until geometry is known or the probe times out. */
	return 0;
}

void sr_io_present(const uint8_t *rgb, int w, int h)
{
	/* TODO(M1):
	 *   1. termgfx_geom_fit()/_center() the w x h frame to the probed terminal
	 *      geometry (cell + pixel dims from the enter/probe handshake).
	 *   2. encode via the active tier: WITH_JXL -> termgfx_jxl_encode(), else
	 *      sixel; text tier as the no-graphics fallback.
	 *   3. gate on termgfx_aimd_update()/_rtt_sample() so we only write when the
	 *      pipe has capacity -- frames the pipe can't take are dropped
	 *      (DESIGN.md sec 3). SYNCRETRO_SIXELOUT capture mode overwrites a file
	 *      with one self-contained frame instead of writing to the socket. */
	(void)rgb; (void)w; (void)h;
}

void sr_io_leave(void)
{
	/* TODO(M1): emit termgfx_term_leave (+ status_off) to restore the BBS
	 * terminal. Must run on every exit path (main.c's single cleanup). */
}

void sr_io_feed_reply(const uint8_t *buf, int len)
{
	/* TODO(M1): feed probe/DA/geometry replies to termgfx_term_parse_status()
	 * and termgfx_caps_parse_jxl() to finalize tier + geometry. No-op-safe. */
	(void)buf; (void)len;
}
