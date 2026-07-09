/*
 * test_kitty.c -- drive syncduke_input_pump() with the kitty keyboard protocol's
 * actual byte sequences (as emitted by Contour) and assert the decoded press/release
 * scancodes, native turn, press-only gating, ctrl shortcuts, and numpad folding.
 *
 * Companion to test_keymap.c (which covers the legacy byte path).  Build + run:
 *
 *   cc -I../Game/src -I../../termgfx -o /tmp/test_kitty test_kitty.c \
 *      ../syncduke_input.c ../syncduke_door.c \
 *      ../../termgfx/caps.c ../../termgfx/keymode.c && /tmp/test_kitty -s1
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "../syncduke.h"
#include "audio_mgr.h"
#include "keyboard.h"

/* syncduke_io.c functions the pump references, not linked here -- stub them. */
uint32_t syncduke_rtt(void) { return 0; }   /* low latency -> native turn */
void syncduke_pace_ack(void) { }
void syncduke_stats_toggle(void) { }
void syncduke_depth_cycle(void) { }
static int  g_userlist_reqs;                                           /* count Ctrl-U routing hits */
void syncduke_node_userlist_request(void) { g_userlist_reqs++; }
static int g_tier_cycles;                                              /* count F4/tier-cycle hits */
void syncduke_tier_cycle(void) { g_tier_cycles++; }
void syncduke_out_put(const void *b, size_t l) { (void)b; (void)l; }   /* kitty-flag push: not exercised */
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


/* termgfx audio the pump now touches (music-key path) -- stub out, no audio in the test. */
termgfx_audio_t *sd_audio;
void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *b, int n) { (void)m; (void)b; (void)n; }
int  termgfx_audio_tier(const termgfx_audio_t *m) { (void)m; return -1; }
void sd_music_pending_retry(void) { }

extern volatile int syncduke_pitch_step;
extern volatile int syncduke_help_request;

static int          fails, wfd;
static void feed(const char *s) { if (write(wfd, s, strlen(s)) < 0) { } }
static void chk(const char *n, int got, int want)
{
	printf("  %-26s got=%-5d want=%-5d %s\n", n, got, want, got == want ? "ok" : (fails++, "FAIL"));
}

int main(void)
{
	int  pp[2];
	char b;
	int  wsc, spc, ent;

	if (pipe(pp)) { perror("pipe"); return 2; }
	wfd = pp[1];

	feed("\x1b[?1u");                                    /* progressive-flags reply -> negotiate */
	syncduke_input_pump(pp[0], 0, 1);
	chk("kitty negotiated", syncduke_kitty_active(), 1);

	b = 'w'; wsc = syncduke_map_key(&b, 1, 1);
	feed("\x1b[119u");        syncduke_input_pump(pp[0], 1, 1);   /* w press  */
	chk("w down", syncduke_input_pop_raw(), wsc);
	feed("\x1b[119;1:3u");    syncduke_input_pump(pp[0], 2, 1);   /* w release */
	chk("w up", syncduke_input_pop_raw(), wsc | 0x80);

	b = ' '; spc = syncduke_map_key(&b, 1, 1);
	feed("\x1b[32u");         syncduke_input_pump(pp[0], 3, 1);   /* space -> fire */
	chk("space fire down", syncduke_input_pop_raw(), spc);
	feed("\x1b[32;1:3u");     syncduke_input_pump(pp[0], 4, 1);
	chk("space fire up", syncduke_input_pop_raw(), spc | 0x80);

	feed("\x1b[A");           syncduke_input_pump(pp[0], 5, 1);   /* up arrow press  */
	chk("up down", syncduke_input_pop_raw(), sc_UpArrow);
	feed("\x1b[1;1:3A");      syncduke_input_pump(pp[0], 6, 1);   /* up arrow release */
	chk("up up", syncduke_input_pop_raw(), sc_UpArrow | 0x80);

	feed("\x1b[C");           syncduke_input_pump(pp[0], 7, 1);   /* right arrow -> NATIVE turn key */
	chk("right down (native)", syncduke_input_pop_raw(), sc_RightArrow);
	feed("\x1b[1;1:3C");      syncduke_input_pump(pp[0], 8, 1);
	chk("right up (native)", syncduke_input_pop_raw(), sc_RightArrow | 0x80);

	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[5~");          syncduke_input_pump(pp[0], 9, 1);   /* PgUp -> Look_Up (sc_kpad_9), held */
	chk("PgUp -> Look_Up down", syncduke_input_pop_raw(), sc_kpad_9);
	feed("\x1b[5;1:3~");      syncduke_input_pump(pp[0], 10, 1);  /* PgUp release -> key-up (auto-centers) */
	chk("PgUp -> Look_Up up", syncduke_input_pop_raw(), sc_kpad_9 | 0x80);

	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[115;5u");      syncduke_input_pump(pp[0], 11, 1);  /* Ctrl-S (stats) -> no game scancode */
	chk("Ctrl-S no rawq", syncduke_input_has_raw(), 0);

	/* Ctrl-U (0x15) = who's-online: door-level, never a Duke scancode, routes to the request fn. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[117;5u");      syncduke_input_pump(pp[0], 33, 1);   /* kitty Ctrl-U */
	chk("Ctrl-U no rawq", syncduke_input_has_raw(), 0);
	chk("Ctrl-U userlist req", g_userlist_reqs, 1);

	b = '\r'; ent = syncduke_map_key(&b, 1, 0);                   /* numpad Enter in a menu (gameplay=0) */
	feed("\x1b[57414u");      syncduke_input_pump(pp[0], 12, 0);  /* KP_ENTER (PUA 57414) -> Enter */
	chk("numpad Enter down", syncduke_input_pop_raw(), ent);
	feed("\x1b[57414;1:3u");  syncduke_input_pump(pp[0], 13, 0);
	chk("numpad Enter up", syncduke_input_pop_raw(), ent | 0x80);

	/* Function keys, as Contour encodes them under kitty (F1=CSI P, F2=CSI Q, F3=13~,
	 * F4=CSI S, F5=15~, F6=17~); door actions: F1=help, F4=tier, F2/F3/F5/F6=Duke save-load. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	syncduke_help_request = 0;
	feed("\x1b[P");           syncduke_input_pump(pp[0], 14, 1);  /* F1 -> help */
	chk("F1 help", syncduke_help_request, 1);
	feed("\x1b[1;1:3P");      syncduke_input_pump(pp[0], 15, 1);  /* F1 release -> ignored */
	feed("\x1b[Q");           syncduke_input_pump(pp[0], 16, 1);  /* F2 -> sc_F2 */
	chk("F2 down", syncduke_input_pop_raw(), sc_F2);
	feed("\x1b[13~");         syncduke_input_pump(pp[0], 17, 1);  /* F3 -> sc_F3 */
	chk("F3 down", syncduke_input_pop_raw(), sc_F3);
	{
		int before = g_tier_cycles;
		feed("\x1b[S");       syncduke_input_pump(pp[0], 18, 1);  /* F4 -> tier cycle */
		chk("F4 tier cycle", g_tier_cycles, before + 1);
		feed("\x1b[1;1:3S");  syncduke_input_pump(pp[0], 19, 1);  /* F4 release -> NOT a 2nd cycle */
		chk("F4 release no cycle", g_tier_cycles, before + 1);
	}
	feed("\x1b[15~");         syncduke_input_pump(pp[0], 20, 1);  /* F5 -> sc_F5 */
	chk("F5 down", syncduke_input_pop_raw(), sc_F5);
	feed("\x1b[17~");         syncduke_input_pump(pp[0], 21, 1);  /* F6 -> sc_F6 */
	chk("F6 down", syncduke_input_pop_raw(), sc_F6);

	{
		int before = g_tier_cycles;
		feed("\x1b[100;5u");  syncduke_input_pump(pp[0], 22, 1);  /* Ctrl-D = F4 alias -> tier cycle */
		chk("Ctrl-D tier cycle", g_tier_cycles, before + 1);
	}

	/* Home/End: in a MENU jump to first/last item (sc_Home/sc_End); in gameplay under native key-up
	 * they're a HELD Aim (the original's Aim_Up/Aim_Down), not Center_View. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[H");           syncduke_input_pump(pp[0], 23, 0);  /* Home in menu -> first item */
	chk("Home (menu) first", syncduke_input_pop_raw(), sc_Home);
	feed("\x1b[F");           syncduke_input_pump(pp[0], 24, 0);  /* End in menu -> last item */
	chk("End (menu) last", syncduke_input_pop_raw(), sc_End);
	feed("\x1b[H");           syncduke_input_pump(pp[0], 25, 1);  /* Home in gameplay -> Aim_Up (sc_kpad_7), held */
	chk("Home (gameplay) Aim_Up down", syncduke_input_pop_raw(), sc_kpad_7);

	/* Gameplay view controls: keypad PUA KP5=57404 -> Center_View, KP0=57399 / KP.=57409 ->
	 * Look_Left/Right; Insert(2~) / Delete(3~) -> Look_Left/Right (held, key-up honored). */
	syncduke_input_reset();                                      /* clear held[] (Home above left kpad_7 "down") */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57404u");      syncduke_input_pump(pp[0], 26, 1);  /* KP5 -> Center_View */
	chk("KP5 -> Center", syncduke_input_pop_raw(), sc_kpad_5);
	feed("\x1b[57399u");      syncduke_input_pump(pp[0], 27, 1);  /* KP0 -> Look_Left */
	chk("KP0 -> Look_L", syncduke_input_pop_raw(), sc_kpad_0);
	feed("\x1b[57409u");      syncduke_input_pump(pp[0], 28, 1);  /* KP. -> Look_Right */
	chk("KP. -> Look_R", syncduke_input_pop_raw(), sc_kpad_Period);
	feed("\x1b[2~");          syncduke_input_pump(pp[0], 29, 1);  /* Insert -> NOT mapped (Shift-Insert paste) */
	chk("Insert: no rawq", syncduke_input_has_raw(), 0);
	feed("\x1b[3~");          syncduke_input_pump(pp[0], 31, 1);  /* Delete -> Look_Right */
	chk("Delete -> Look_R", syncduke_input_pop_raw(), sc_Delete);
	/* In a MENU the keypad still types digits (KP5 -> '5'). */
	{ char b5 = '5'; feed("\x1b[57404u"); syncduke_input_pump(pp[0], 32, 0);
	  chk("KP5 (menu) -> '5'", syncduke_input_pop_raw(), syncduke_map_key(&b5, 1, 0)); }

	/* --- NumLock OFF: foot (and other kitty terminals) report the numpad by its FUNCTION
	 *     codepoint (KP_UP/DOWN/LEFT/RIGHT=57419/20/17/18, KP_HOME/PGUP/END/PGDN=57423/21/24/22,
	 *     KP_INSERT/DELETE=57425/26, KP_BEGIN via CSI E) instead of the NumLock-ON digit PUA.
	 *     Must behave IDENTICALLY to NumLock ON. (Ground truth: ~/kitty_capture.log, foot 1.62.2.) */
	syncduke_input_reset();
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57419;1:1u"); syncduke_input_pump(pp[0], 40, 1);   /* numpad-8 = KP_UP    -> forward */
	chk("KP_UP -> UpArrow",     syncduke_input_pop_raw(), sc_UpArrow);
	feed("\x1b[57420;1:1u"); syncduke_input_pump(pp[0], 41, 1);   /* numpad-2 = KP_DOWN  -> back */
	chk("KP_DOWN -> DownArrow", syncduke_input_pop_raw(), sc_DownArrow);
	syncduke_input_reset();
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57417;1:1u"); syncduke_input_pump(pp[0], 42, 1);   /* numpad-4 = KP_LEFT  -> turn left */
	chk("KP_LEFT -> LeftArrow", syncduke_input_pop_raw(), sc_LeftArrow);
	syncduke_input_reset();
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57418;1:1u"); syncduke_input_pump(pp[0], 43, 1);   /* numpad-6 = KP_RIGHT -> turn right */
	chk("KP_RIGHT -> RightArrow", syncduke_input_pop_raw(), sc_RightArrow);
	syncduke_input_reset();
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57425;1:1u"); syncduke_input_pump(pp[0], 44, 1);   /* numpad-0 = KP_INSERT -> Look_Left */
	chk("KP_INSERT -> kpad_0",  syncduke_input_pop_raw(), sc_kpad_0);
	feed("\x1b[57426;1:1u"); syncduke_input_pump(pp[0], 45, 1);   /* numpad-. = KP_DELETE -> Look_Right */
	chk("KP_DELETE -> kpad_Period", syncduke_input_pop_raw(), sc_kpad_Period);
	feed("\x1b[E");          syncduke_input_pump(pp[0], 46, 1);   /* numpad-5 = KP_BEGIN (CSI E) -> Center */
	chk("KP_BEGIN(CSI E) -> kpad_5", syncduke_input_pop_raw(), sc_kpad_5);
	/* In a MENU the NumLock-OFF nav cluster does its NAV function like the main keys:
	 * numpad Home/End (KP_HOME/KP_END) jump to the first/last item (sc_Home/sc_End). */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[57423;1:1u"); syncduke_input_pump(pp[0], 47, 0);   /* numpad-7 = KP_HOME -> first item */
	chk("KP_HOME (menu) -> first", syncduke_input_pop_raw(), sc_Home);
	feed("\x1b[57424;1:1u"); syncduke_input_pump(pp[0], 48, 0);   /* numpad-1 = KP_END  -> last item */
	chk("KP_END (menu) -> last",  syncduke_input_pop_raw(), sc_End);
	/* In GAMEPLAY the same physical key is still the digit-folded view control (KP_HOME -> '7'),
	 * unchanged and matching the evdev path. */
	{ char b7 = '7'; feed("\x1b[57423;1:1u"); syncduke_input_pump(pp[0], 49, 1);
	  chk("KP_HOME (gameplay) -> '7'", syncduke_input_pop_raw(), syncduke_map_key(&b7, 1, 1)); }

	printf(fails ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", fails);
	return fails != 0;
}
