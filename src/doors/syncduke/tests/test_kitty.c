/*
 * test_kitty.c -- drive syncduke_input_pump() with the kitty keyboard protocol's
 * actual byte sequences (as emitted by Contour) and assert the decoded press/release
 * scancodes, native turn, press-only gating, ctrl shortcuts, and numpad folding.
 *
 * Companion to test_keymap.c (which covers the legacy byte path).  Build + run:
 *
 *   cc -I../Game/src -I../../termgfx -o /tmp/test_kitty test_kitty.c \
 *      ../syncduke_input.c ../syncduke_door.c ../../termgfx/caps.c && /tmp/test_kitty
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "../syncduke.h"
#include "keyboard.h"

/* syncduke_io.c functions the pump references, not linked here -- stub them. */
void syncduke_pace_ack(void) { }
void syncduke_stats_toggle(void) { }
void syncduke_depth_cycle(void) { }
static int g_tier_cycles;                                              /* count F4/tier-cycle hits */
void syncduke_tier_cycle(void) { g_tier_cycles++; }
void syncduke_out_put(const void *b, size_t l) { (void)b; (void)l; }   /* kitty-flag push: not exercised */
void syncduke_hsteer(int *c, int *h) { if (c) *c = 40; if (h) *h = 40; }
void syncduke_log(const char *f, ...) { (void)f; }

extern volatile int syncduke_pitch_step;
extern volatile int syncduke_help_request;

static int fails, wfd;
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

	syncduke_pitch_step = 0;
	feed("\x1b[5~");          syncduke_input_pump(pp[0], 9, 1);   /* PgUp -> one look notch */
	chk("PgUp notch=1", syncduke_pitch_step, 1);
	feed("\x1b[5;1:3~");      syncduke_input_pump(pp[0], 10, 1);  /* PgUp release -> IGNORED (no 2nd) */
	chk("PgUp release no notch", syncduke_pitch_step, 1);

	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[115;5u");      syncduke_input_pump(pp[0], 11, 1);  /* Ctrl-S (stats) -> no game scancode */
	chk("Ctrl-S no rawq", syncduke_input_has_raw(), 0);

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

	/* Home/End: in a MENU jump to first/last item (sc_Home/sc_End); in gameplay, Center_View. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[H");           syncduke_input_pump(pp[0], 23, 0);  /* Home in menu -> first item */
	chk("Home (menu) first", syncduke_input_pop_raw(), sc_Home);
	feed("\x1b[F");           syncduke_input_pump(pp[0], 24, 0);  /* End in menu -> last item */
	chk("End (menu) last", syncduke_input_pop_raw(), sc_End);
	feed("\x1b[H");           syncduke_input_pump(pp[0], 25, 1);  /* Home in gameplay -> Center_View */
	chk("Home (gameplay) center", syncduke_input_pop_raw(), sc_kpad_5);

	printf(fails ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", fails);
	return fails != 0;
}
