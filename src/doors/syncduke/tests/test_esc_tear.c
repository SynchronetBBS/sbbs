/*
 * test_esc_tear.c -- an escape sequence must survive arriving in pieces.
 *
 * syncduke_input_pump() runs once per presented frame, so in a slow tier (sextant
 * measured 7-30fps) the gap between pumps exceeds SYNCDUKE_ESC_MS.  If the pending
 * lone-ESC timer is judged before the socket is read, the tail of a split sequence
 * loses the race even when it is already queued: the game is handed an Escape
 * (which opens a menu) plus the rest of the sequence as literal keys, and the door
 * action the sequence encoded -- F4's tier cycle here -- never happens.
 *
 * Companion to test_kitty.c / test_keymap.c.  Build + run:
 *
 *   cc -I../Game/src -I../../termgfx -o /tmp/test_esc_tear test_esc_tear.c \
 *      ../syncduke_input.c ../../termgfx/caps.c ../../termgfx/keymode.c \
 *      ../../termgfx/sixel.c && /tmp/test_esc_tear
 *
 * syncduke_door.c is NOT linked: it now reaches the door32/idle/config layers, which
 * would drag in xpdev for a key test. Its handful of entry points are stubbed below.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include "../syncduke.h"
#include "audio_mgr.h"
#include "keyboard.h"

/* syncduke_io.c / syncduke_door.c functions the pump references, not linked here. */
uint32_t syncduke_rtt(void) { return 0; }
void syncduke_pace_ack(void) { }
void syncduke_stats_toggle(void) { }
void syncduke_depth_cycle(void) { }
void syncduke_node_userlist_request(void) { }
static int g_tier_cycles;                                              /* count F4/tier-cycle hits */
void syncduke_tier_cycle(void) { g_tier_cycles++; }
void syncduke_out_put(const void *b, size_t l) { (void)b; (void)l; }
void syncduke_hsteer(int *c, int *h) {
	if (c)
		*c = 40; if (h)
		*h = 40;
}
void syncduke_log(const char *f, ...) { (void)f; }
int  syncduke_node_composing(void) { return 0; }
void syncduke_node_compose_key(int c) { (void)c; }
void syncduke_node_page_request(void) { }
void syncduke_term_restore(void) { }
int  syncduke_door_socket(void) { return -1; }        /* dev mode: EOF is not a hangup */
void syncduke_hangup(const char *r) { (void)r; }
int  syncduke_idle_wake(void) { return 0; }
int  syncduke_idle_check(void) { return 0; }
void syncduke_set_sdm_probed(int v) { (void)v; }
void syncduke_set_cterm_ver(int v) { (void)v; }

/* termgfx pieces the pump touches that are not worth linking for a key test. */
termgfx_audio_t *sd_audio;
void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *b, int n) { (void)m; (void)b; (void)n; }
int  termgfx_audio_tier(const termgfx_audio_t *m) { (void)m; return -1; }
void termgfx_audio_set_blob_ok(termgfx_audio_t *m, int v) { (void)m; (void)v; }
void sd_music_pending_retry(void) { }
int  termgfx_term_parse_status(const uint8_t *b, int n) { (void)b; (void)n; return -1; }

/* One 7fps sextant frame -- comfortably past SYNCDUKE_ESC_MS (50). */
#define FRAME_US 140000

static int fails, wfd;
static void feed(const char *s) { if (write(wfd, s, strlen(s)) < 0) { } }
static void chk(const char *n, int got, int want)
{
	printf("  %-32s got=%-5d want=%-5d %s\n", n, got, want,
	       got == want ? "ok" : (fails++, "FAIL"));
}

int main(void)
{
	int pp[2], frame = 0, fl;

	if (pipe(pp)) { perror("pipe"); return 2; }
	wfd = pp[1];
	if ((fl = fcntl(pp[0], F_GETFL, 0)) != -1)   /* as syncduke_input_fd() leaves the real one */
		fcntl(pp[0], F_SETFL, fl | O_NONBLOCK);

	/* F4 as SS3 (ESC O S), whole: the door cycles the graphics tier and the game
	 * sees nothing. */
	feed("\x1bOS");
	syncduke_input_pump(pp[0], frame++, 1);
	chk("whole F4: tier cycled", g_tier_cycles, 1);
	chk("whole F4: no game keys", syncduke_input_has_raw(), 0);

	/* A genuinely lone ESC is still the Escape key once its window lapses -- the menu
	 * key must not wait for the next keystroke. */
	feed("\x1b");
	syncduke_input_pump(pp[0], frame++, 1);
	chk("lone ESC: not yet", syncduke_input_has_raw(), 0);
	usleep(FRAME_US);
	syncduke_input_pump(pp[0], frame++, 1);
	chk("lone ESC: Escape", syncduke_input_pop_raw(), sc_Escape);

	/* Same F4, torn: its ESC lands in one pump and the tail in the next, a frame later.
	 * Still one tier cycle, still no keys to the game. */
	g_tier_cycles = 0;
	feed("\x1b");
	syncduke_input_pump(pp[0], frame++, 1);
	usleep(FRAME_US);
	feed("OS");
	syncduke_input_pump(pp[0], frame++, 1);
	chk("torn F4: tier cycled", g_tier_cycles, 1);
	chk("torn F4: no game keys", syncduke_input_has_raw(), 0);

	printf("%s\n", fails ? "FAILURES" : "all ok");
	return fails ? 1 : 0;
}
