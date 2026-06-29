/*
 * test_keymap.c -- unit test for syncduke_map_key() (terminal byte/seq -> scancode).
 *
 * syncduke_input.c + syncduke_door.c are engine-independent, so this links them directly
 * with no Duke/SDL (plus termgfx/caps.c, which syncduke_input.c uses to parse the JXL
 * cap-probe reply). Build+run:
 *   cc -I../Game/src -I../../termgfx -o /tmp/test_keymap test_keymap.c \
 *      ../syncduke_input.c ../syncduke_door.c ../../termgfx/caps.c && /tmp/test_keymap
 */

#include <stdio.h>
#include <string.h>
#include "../syncduke.h"
#include "keyboard.h"

/* syncduke_input.c calls these (DSR pacing ack, Ctrl-S stats toggle) -- defined in
 * syncduke_io.c, which the keymap test doesn't link. Stub them; not exercised here. */
void syncduke_pace_ack(void) { }
void syncduke_stats_toggle(void) { }
void syncduke_depth_cycle(void) { }
void syncduke_tier_cycle(void) { }
void syncduke_hsteer(int *center_col, int *half_cols) { if (center_col) *center_col = 40; if (half_cols) *half_cols = 40; }
/* syncduke_door.c's hangup() logs via syncduke_log() (in syncduke_log.c, not linked here). */
void syncduke_log(const char *fmt, ...) { (void)fmt; }

static int fails;
#define GP 1   /* gameplay */
#define MN 0   /* menu / text-entry */

static void expect(const char *what, const char *seq, int len, int gp, int want)
{
	int got = syncduke_map_key(seq, len, gp);
	if (got != want) {
		printf("FAIL %-16s want %3d got %3d\n", what, want, got);
		fails++;
	} else {
		printf("ok   %-16s -> %3d\n", what, got);
	}
}

int main(void)
{
	/* universal (same in both modes) */
	expect("ESC",         "\x1b", 1, GP, sc_Escape);
	expect("Enter(CR)",   "\r",   1, GP, sc_Return);
	expect("Enter(LF)",   "\n",   1, GP, sc_Return);
	expect("Tab",         "\t",   1, GP, sc_Tab);
	expect("Backspace",   "\x7f", 1, GP, sc_BackSpace);
	expect("Up(CSI)",     "\x1b[A", 3, GP, sc_UpArrow);
	expect("Down(CSI)",   "\x1b[B", 3, GP, sc_DownArrow);
	expect("Right(CSI)",  "\x1b[C", 3, GP, sc_RightArrow);
	expect("Left(CSI)",   "\x1b[D", 3, GP, sc_LeftArrow);
	expect("Up(SS3)",     "\x1bOA", 3, GP, sc_UpArrow);

	/* gameplay action layer (SyncDOOM-style) */
	expect("Fire(Space)", " ",    1, GP, sc_LeftControl);
	expect("Forward(W)",  "w",    1, GP, sc_UpArrow);
	expect("Back(S)",     "s",    1, GP, sc_DownArrow);
	expect("StrafeL(A)",  "a",    1, GP, sc_Comma);
	expect("StrafeR(D)",  "d",    1, GP, sc_Period);
	expect("Use(E)",      "e",    1, GP, sc_Space);
	expect("Jump(Q)",     "q",    1, GP, sc_A);
	expect("AutoRun(R)",  "r",    1, GP, sc_CapsLock);
	expect("Crouch(Z)",   "z",    1, GP, sc_Z);     /* falls through to literal */
	expect("Weapon3",     "3",    1, GP, sc_3);
	expect("Holo(H)",     "h",    1, GP, sc_H);     /* inventory letter, literal */

	/* menu / text-entry: action layer OFF -> letters & space literal */
	expect("menu Space",  " ",    1, MN, sc_Space);
	expect("menu w",      "w",    1, MN, sc_W);
	expect("menu a",      "a",    1, MN, sc_A);
	expect("menu Up",     "\x1b[A", 3, MN, sc_UpArrow);   /* nav still works */
	expect("menu Enter",  "\r",   1, MN, sc_Return);

	expect("unknown",     "~",    1, GP, -1);

	printf(fails ? "\n%d FAILED\n" : "\nALL PASS\n", fails);
	return fails ? 1 : 0;
}
