/*
 * test_evdev.c -- drive syncduke_input_pump() with SyncTERM's physical key event
 * reports (CSI = <evdev-code>[;...] K press / k release) and assert the decoded
 * press/release scancodes, native turn, look notches, modifier folding (Shift/Ctrl),
 * function keys, keypad folding, and multi-code reports.
 *
 * Companion to test_kitty.c (kitty protocol) and test_keymap.c (legacy byte path).
 * Build + run:
 *
 *   cc -I../Game/src -I../../termgfx -o /tmp/test_evdev test_evdev.c \
 *      ../syncduke_input.c ../syncduke_door.c \
 *      ../../termgfx/caps.c ../../termgfx/keymode.c && /tmp/test_evdev -s1
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "../syncduke.h"
#include "audio_mgr.h"
#include "keyboard.h"

/* The audio manager the pump feeds its cap-probe replies to; not exercised here. */
termgfx_audio_t *sd_audio;
void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *b, int n) { (void)m; (void)b; (void)n; }
int  termgfx_audio_tier(const termgfx_audio_t *m) { (void)m; return -1; }
void sd_music_pending_retry(void) { }

/* syncduke_io.c functions the pump references, not linked here -- stub them. */
static uint32_t g_test_rtt;                  /* drive native (0) vs synthetic (>120) turn */
uint32_t syncduke_rtt(void) { return g_test_rtt; }
void syncduke_pace_ack(void) { }
void syncduke_stats_toggle(void) { }
void syncduke_depth_cycle(void) { }
void syncduke_node_userlist_request(void) { }
static int g_tier_cycles;
void syncduke_tier_cycle(void) { g_tier_cycles++; }
void syncduke_out_put(const void *b, size_t l) { (void)b; (void)l; }   /* enable-mode push: not exercised */
void syncduke_hsteer(int *c, int *h) {
	if (c)
		*c = 40; if (h)
		*h = 40;
}
void syncduke_log(const char *f, ...) { (void)f; }

/* syncduke_node.c's page-compose hooks the pump routes keys through -- stub them. */
int  syncduke_node_composing(void) { return 0; }
void syncduke_node_compose_key(int c) { (void)c; }
void syncduke_node_page_request(void) { }
void syncduke_term_restore(void) { }   /* hangup path's terminal restore: no terminal here */


extern volatile int syncduke_pitch_step;
extern volatile int syncduke_help_request;

static int          fails, wfd;
static void feed(const char *s) { if (write(wfd, s, strlen(s)) < 0) { } }
static void chk(const char *n, int got, int want)
{
	printf("  %-28s got=%-5d want=%-5d %s\n", n, got, want, got == want ? "ok" : (fails++, "FAIL"));
}

int main(void)
{
	int  pp[2];
	char b;
	int  wsc, spc, ent;

	if (pipe(pp)) { perror("pipe"); return 2; }
	wfd = pp[1];

	/* CTDA reply advertising cap 8 (physical key reports) -> negotiate evdev. */
	feed("\x1b[<8c");
	syncduke_input_pump(pp[0], 0, 1);
	chk("evdev negotiated", syncduke_evdev_active(), 1);
	chk("kitty NOT active", syncduke_kitty_active(), 0);

	/* Settle window: SyncTERM resyncs keys held at enable time as PRESS reports (the key still
	 * held from selecting the door).  Those must be DROPPED so they can't skip Duke's opening. */
	feed("\x1b[=28K");        syncduke_input_pump(pp[0], 0, 1);   /* simulated resync of a held key */
	chk("resync press dropped", syncduke_input_has_raw(), 0);
	usleep(600000);                                              /* > SYNCDUKE_EVDEV_SETTLE_MS (500) */

	/* W (evdev 17) -> forward, with a true release edge (hold-to-move). */
	b = 'w'; wsc = syncduke_map_key(&b, 1, 1);
	feed("\x1b[=17K");        syncduke_input_pump(pp[0], 1, 1);
	chk("W down -> forward", syncduke_input_pop_raw(), wsc);
	feed("\x1b[=17k");        syncduke_input_pump(pp[0], 2, 1);
	chk("W up", syncduke_input_pop_raw(), wsc | 0x80);

	/* Space (evdev 57) -> fire. */
	b = ' '; spc = syncduke_map_key(&b, 1, 1);
	feed("\x1b[=57K");        syncduke_input_pump(pp[0], 3, 1);
	chk("Space down -> fire", syncduke_input_pop_raw(), spc);
	feed("\x1b[=57k");        syncduke_input_pump(pp[0], 4, 1);
	chk("Space up", syncduke_input_pop_raw(), spc | 0x80);

	/* Arrow keys come as evdev nav codes (UP 103, RIGHT 106), not ESC[A. */
	feed("\x1b[=103K");       syncduke_input_pump(pp[0], 5, 1);
	chk("Up down", syncduke_input_pop_raw(), sc_UpArrow);
	feed("\x1b[=103k");       syncduke_input_pump(pp[0], 6, 1);
	chk("Up up", syncduke_input_pop_raw(), sc_UpArrow | 0x80);
	feed("\x1b[=106K");       syncduke_input_pump(pp[0], 7, 1);   /* RIGHT -> native turn key (low RTT) */
	chk("Right down (native)", syncduke_input_pop_raw(), sc_RightArrow);
	feed("\x1b[=106k");       syncduke_input_pump(pp[0], 8, 1);
	chk("Right up (native)", syncduke_input_pop_raw(), sc_RightArrow | 0x80);

	/* PageUp (104) -> Look_Up: drive the engine's real Look key (sc_kpad_9), held with real key-up
	 * so it auto-centers on release just like the original game. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[=104K");       syncduke_input_pump(pp[0], 9, 1);    /* press  -> key-down */
	chk("PgUp -> Look_Up down", syncduke_input_pop_raw(), sc_kpad_9);
	feed("\x1b[=104k");       syncduke_input_pump(pp[0], 10, 1);   /* release -> key-up */
	chk("PgUp -> Look_Up up", syncduke_input_pop_raw(), sc_kpad_9 | 0x80);

	/* Ctrl is its OWN evdev key (LEFTCTRL 29): held, delivers nothing; then S (31) folds
	 * to Ctrl-S (stats toggle) -> no game scancode. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[=29K");        syncduke_input_pump(pp[0], 11, 1);   /* Ctrl down */
	chk("Ctrl down no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[=31K");        syncduke_input_pump(pp[0], 12, 1);   /* S with Ctrl -> Ctrl-S */
	chk("Ctrl-S no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[=31k\x1b[=29k"); syncduke_input_pump(pp[0], 13, 1); /* release S then Ctrl */
	chk("Ctrl-S release no rawq", syncduke_input_has_raw(), 0);

	/* Shift = Run + Strafe (held): asserts BOTH sc_LeftShift (Run) and sc_LeftAlt (Strafe), internal
	 * scancodes (never a physical Alt to SyncTERM).  The Shift bit ALSO folds the next menu letter to
	 * upper. */
	feed("\x1b[=42K");        syncduke_input_pump(pp[0], 14, 1);   /* Shift down -> Run + Strafe */
	chk("Shift down -> Run", syncduke_input_pop_raw(), sc_LeftShift);
	chk("Shift down -> Strafe", syncduke_input_pop_raw(), sc_LeftAlt);
	feed("\x1b[=42k");        syncduke_input_pump(pp[0], 16, 1);   /* Shift up -> release both */
	chk("Shift up -> Run up", syncduke_input_pop_raw(), sc_LeftShift | 0x80);
	chk("Shift up -> Strafe up", syncduke_input_pop_raw(), sc_LeftAlt | 0x80);
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* Menu: Shift folds the next letter to upper (its bit); drain the run+strafe scancodes first. */
	feed("\x1b[=42K");        syncduke_input_pump(pp[0], 14, 0);   /* Shift down (menu) */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();     /* drain Run+Strafe */
	feed("\x1b[=30K");        syncduke_input_pump(pp[0], 15, 0);   /* A (menu): folds to upper 'A' */
	{ char bb = 'A'; chk("Shift+A (menu) -> A", syncduke_input_pop_raw(), syncduke_map_key(&bb, 1, 0)); }
	feed("\x1b[=30k\x1b[=42k"); syncduke_input_pump(pp[0], 16, 0);
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* GAMEPLAY: Shift keeps letters lowercase so the action remap still fires (Shift+W -> forward). */
	feed("\x1b[=42K");        syncduke_input_pump(pp[0], 16, 1);   /* Shift down */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();     /* drain Run+Strafe */
	{ char bw = 'w'; feed("\x1b[=17K"); syncduke_input_pump(pp[0], 16, 1);
	  chk("Shift+W (game) -> forward", syncduke_input_pop_raw(), syncduke_map_key(&bw, 1, 1)); }
	feed("\x1b[=17k\x1b[=42k"); syncduke_input_pump(pp[0], 16, 1);
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* Alt is NOT forwarded (would collide with SyncTERM Alt+arrow resize / Alt+letter): no scancode. */
	feed("\x1b[=56K");        syncduke_input_pump(pp[0], 16, 1);   /* Alt down -> no scancode */
	chk("Alt down: no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[=56k");        syncduke_input_pump(pp[0], 16, 1);
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* Numpad arrows 8/2/4/6 = the main arrows: move/turn in gameplay AND navigate menus. */
	feed("\x1b[=72K");        syncduke_input_pump(pp[0], 16, 1);   /* KP8 -> forward */
	chk("KP8 (game) -> forward", syncduke_input_pop_raw(), sc_UpArrow);
	feed("\x1b[=80K");        syncduke_input_pump(pp[0], 16, 1);   /* KP2 -> back */
	chk("KP2 (game) -> back", syncduke_input_pop_raw(), sc_DownArrow);
	feed("\x1b[=75K");        syncduke_input_pump(pp[0], 16, 1);   /* KP4 -> turn left */
	chk("KP4 (game) -> turn L", syncduke_input_pop_raw(), sc_LeftArrow);
	feed("\x1b[=77K");        syncduke_input_pump(pp[0], 16, 1);   /* KP6 -> turn right */
	chk("KP6 (game) -> turn R", syncduke_input_pop_raw(), sc_RightArrow);
	syncduke_input_reset();                                       /* clear held arrows (no key-up fed) */
	feed("\x1b[=72K");        syncduke_input_pump(pp[0], 16, 0);   /* KP8 in a MENU -> still nav-up (sc_UpArrow) */
	chk("KP8 (menu) -> nav up", syncduke_input_pop_raw(), sc_UpArrow);
	syncduke_input_reset();

	/* GAMEPLAY view controls: KP5 = Center_View, KP0/Insert = Look_Left, KP./Delete = Look_Right --
	 * the engine's own keypad/Insert/Delete scancodes (not digits). */
	feed("\x1b[=76K");        syncduke_input_pump(pp[0], 16, 1);   /* KP5 -> Center_View */
	chk("KP5 (game) -> Center", syncduke_input_pop_raw(), sc_kpad_5);
	feed("\x1b[=82K");        syncduke_input_pump(pp[0], 16, 1);   /* KP0 -> Look_Left */
	chk("KP0 (game) -> Look_L", syncduke_input_pop_raw(), sc_kpad_0);
	feed("\x1b[=110K");       syncduke_input_pump(pp[0], 16, 1);   /* Insert -> NOT mapped (Shift-Insert paste) */
	chk("Insert (game): no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[=83K");        syncduke_input_pump(pp[0], 16, 1);   /* KP. -> Look_Right */
	chk("KP. (game) -> Look_R", syncduke_input_pop_raw(), sc_kpad_Period);
	feed("\x1b[=111K");       syncduke_input_pump(pp[0], 16, 1);   /* Delete -> Look_Right */
	chk("Delete (game) -> Look_R", syncduke_input_pop_raw(), sc_Delete);
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	/* In a MENU the keypad still types digits (KP5 -> '5'), so save-name entry is unaffected. */
	{ char b5 = '5'; feed("\x1b[=76K"); syncduke_input_pump(pp[0], 16, 0);
	  chk("KP5 (menu) -> '5'", syncduke_input_pop_raw(), syncduke_map_key(&b5, 1, 0)); }
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* Numpad Home/End (KP7=71, KP1=79) navigate MENUS like the dedicated Home/End keys -- first/last
	 * item -- instead of folding to a digit (SyncTERM sends the keypad scancode NumLock-agnostic).
	 * KP9/KP3 (PgUp/PgDn) do nothing in a menu, like the main PgUp/PgDn. */
	syncduke_input_reset();
	feed("\x1b[=71K");        syncduke_input_pump(pp[0], 16, 0);   /* KP7 in a MENU -> first item */
	chk("KP7 (menu) -> first", syncduke_input_pop_raw(), sc_Home);
	feed("\x1b[=79K");        syncduke_input_pump(pp[0], 16, 0);   /* KP1 in a MENU -> last item */
	chk("KP1 (menu) -> last", syncduke_input_pop_raw(), sc_End);
	feed("\x1b[=73K");        syncduke_input_pump(pp[0], 16, 0);   /* KP9 in a MENU -> nothing */
	chk("KP9 (menu): no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[=81K");        syncduke_input_pump(pp[0], 16, 0);   /* KP3 in a MENU -> nothing */
	chk("KP3 (menu): no rawq", syncduke_input_has_raw(), 0);
	/* GAMEPLAY unchanged: KP7 -> Aim_Up (sc_kpad_7), KP1 -> Aim_Down (sc_kpad_1). */
	syncduke_input_reset();
	feed("\x1b[=71K");        syncduke_input_pump(pp[0], 16, 1);
	chk("KP7 (game) -> Aim_Up", syncduke_input_pop_raw(), sc_kpad_7);
	syncduke_input_reset();
	feed("\x1b[=79K");        syncduke_input_pump(pp[0], 16, 1);
	chk("KP1 (game) -> Aim_Down", syncduke_input_pop_raw(), sc_kpad_1);
	syncduke_input_reset();
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();

	/* Function keys (evdev F1=59 .. F7=65): F1 help, F2/F3 save/load, F4 tier, F7 chase. */
	syncduke_help_request = 0;
	feed("\x1b[=59K");        syncduke_input_pump(pp[0], 17, 1);   /* F1 -> help */
	chk("F1 help", syncduke_help_request, 1);
	feed("\x1b[=60K");        syncduke_input_pump(pp[0], 18, 1);   /* F2 -> save */
	chk("F2 down", syncduke_input_pop_raw(), sc_F2);
	feed("\x1b[=61K");        syncduke_input_pump(pp[0], 19, 1);   /* F3 -> load */
	chk("F3 down", syncduke_input_pop_raw(), sc_F3);
	{
		int before = g_tier_cycles;
		feed("\x1b[=62K");    syncduke_input_pump(pp[0], 20, 1);   /* F4 -> tier cycle */
		chk("F4 tier cycle", g_tier_cycles, before + 1);
		feed("\x1b[=62k");    syncduke_input_pump(pp[0], 21, 1);   /* release -> NOT a 2nd cycle */
		chk("F4 release no cycle", g_tier_cycles, before + 1);
	}
	feed("\x1b[=65K");        syncduke_input_pump(pp[0], 22, 1);   /* F7 -> chase view */
	chk("F7 chase", syncduke_input_pop_raw(), sc_F7);

	/* Keypad folds to ASCII: KP_ENTER (96) -> Enter in a menu. */
	b = '\r'; ent = syncduke_map_key(&b, 1, 0);
	feed("\x1b[=96K");        syncduke_input_pump(pp[0], 23, 0);
	chk("KP Enter down", syncduke_input_pop_raw(), ent);

	/* Home (102): menu -> first item; gameplay -> Aim_Up = the engine's real Aim key (sc_kpad_7),
	 * held (holds position on release, unlike Look).  Center_View stays on KP5. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[=102K");       syncduke_input_pump(pp[0], 24, 0);
	chk("Home (menu) first", syncduke_input_pop_raw(), sc_Home);
	feed("\x1b[=102K");       syncduke_input_pump(pp[0], 25, 1);   /* press  -> Aim_Up down */
	chk("Home (gameplay) Aim_Up down", syncduke_input_pop_raw(), sc_kpad_7);
	feed("\x1b[=102k");       syncduke_input_pump(pp[0], 26, 1);   /* release -> up */
	chk("Home (gameplay) Aim_Up up", syncduke_input_pop_raw(), sc_kpad_7 | 0x80);

	/* Multiple key edges in one report: CSI = 17 ; 57 K -> both W and Space press. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[=17;57K");     syncduke_input_pump(pp[0], 26, 1);
	{ char bw = 'w'; chk("multi-report W", syncduke_input_pop_raw(), syncduke_map_key(&bw, 1, 1)); }
	{ char bs = ' '; chk("multi-report Space", syncduke_input_pop_raw(), syncduke_map_key(&bs, 1, 1)); }

	/* High latency (LAST -- the hysteresis latch is sticky): the turn key uses the SYNTHETIC constant
	 * rate (syncduke_key_turn), not the native scancode.  Release clears the rate.  300ms > _SYNTH so
	 * the latch flips to synthetic on the first check (last_switch starts 0, so the dwell allows it). */
	{
		extern volatile int syncduke_key_turn;
		g_test_rtt = 300;
		syncduke_input_reset();
		feed("\x1b[=106K");   syncduke_input_pump(pp[0], 30, 1);   /* RIGHT (hi RTT) -> synthetic */
		chk("hi-rtt turn: no scancode", syncduke_input_has_raw(), 0);
		chk("hi-rtt turn: synthetic rate", syncduke_key_turn > 0, 1);
		feed("\x1b[=106k");   syncduke_input_pump(pp[0], 30, 1);   /* release -> rate 0 */
		chk("hi-rtt turn release", syncduke_key_turn, 0);
		g_test_rtt = 0;
	}

	printf(fails ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", fails);
	return fails != 0;
}
