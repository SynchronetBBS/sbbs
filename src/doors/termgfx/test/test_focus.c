/* Unit tests for focus.c -- DECSET 1004 focus events.
 *
 * The properties that matter are all about what happens when the terminal
 * says NOTHING: SyncTERM has no mode 1004, so every door linking this has
 * to be fully playable with no event ever arriving.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "focus.h"

static void test_a_silent_terminal_is_focused(void)
{
	termgfx_focus_t f;

	/* SyncTERM never answers. If the initial state were anything but
	 * focused, mouse look would be dead there rather than merely
	 * unimproved. */
	termgfx_focus_init(&f);
	assert(termgfx_focus_have(&f) == 1);
	assert(termgfx_focus_reported(&f) == 0);
	assert(termgfx_focus_take_lost(&f) == 0);
}

static void test_a_zeroed_tracker_is_focused_too(void)
{
	termgfx_focus_t f;

	/* Doors memset their terminal state, and one that forgot the
	 * initializer must not end up with mouse look disabled on every
	 * terminal that never reports focus. That is the whole reason the
	 * stored sense is inverted. */
	memset(&f, 0, sizeof(f));
	assert(termgfx_focus_have(&f) == 1);
	assert(termgfx_focus_reported(&f) == 0);
}

static void test_the_two_events_move_the_state(void)
{
	termgfx_focus_t f;

	termgfx_focus_init(&f);
	assert(termgfx_focus_on_csi(&f, "", 0, 'O') == 1);
	assert(termgfx_focus_have(&f) == 0);
	assert(termgfx_focus_reported(&f) == 1);

	assert(termgfx_focus_on_csi(&f, "", 0, 'I') == 1);
	assert(termgfx_focus_have(&f) == 1);
}

static void test_the_lost_edge_latches_until_taken(void)
{
	termgfx_focus_t f;

	/* Alt-tab away and back between two polls: the release edges for
	 * whatever was held went missing while the window was not listening,
	 * so the door still needs to hear about it. */
	termgfx_focus_init(&f);
	termgfx_focus_on_csi(&f, "", 0, 'O');
	termgfx_focus_on_csi(&f, "", 0, 'I');
	assert(termgfx_focus_have(&f) == 1);
	assert(termgfx_focus_take_lost(&f) == 1);
	assert(termgfx_focus_take_lost(&f) == 0);   /* collected once */
}

static void test_only_a_bare_final_is_a_focus_event(void)
{
	termgfx_focus_t f;

	termgfx_focus_init(&f);
	/* Anything carrying parameters is somebody else's sequence -- a
	 * reply the door asked for, or a key in one of the legacy
	 * functional forms. Claiming those is how CSI S became an F4 and
	 * ate SyncTERM's graphics reply. */
	assert(termgfx_focus_on_csi(&f, "?1004", 5, 'O') == 0);
	assert(termgfx_focus_on_csi(&f, "1;1:3", 5, 'I') == 0);
	assert(termgfx_focus_on_csi(&f, "2", 1, 'O') == 0);
	assert(termgfx_focus_have(&f) == 1);
	assert(termgfx_focus_reported(&f) == 0);

	/* ...and neither is a different final. */
	assert(termgfx_focus_on_csi(&f, "", 0, 'S') == 0);
	assert(termgfx_focus_on_csi(&f, "", 0, 'P') == 0);
	assert(termgfx_focus_on_csi(&f, "", 0, 'R') == 0);
	assert(termgfx_focus_have(&f) == 1);
}

static void test_the_strings_are_the_documented_ones(void)
{
	assert(strcmp(termgfx_focus_enable,  "\x1b[?1004h") == 0);
	assert(strcmp(termgfx_focus_restore, "\x1b[?1004l") == 0);
}

static void test_nothing_at_all_is_safe(void)
{
	/* Doors call the accessors from drawing code that does not check,
	 * and the answer for "no tracker" has to be the permissive one. */
	assert(termgfx_focus_have(NULL) == 1);
	assert(termgfx_focus_reported(NULL) == 0);
	assert(termgfx_focus_take_lost(NULL) == 0);
	assert(termgfx_focus_on_csi(NULL, "", 0, 'O') == 0);
	termgfx_focus_init(NULL);
}

int main(void)
{
	test_a_silent_terminal_is_focused();
	test_a_zeroed_tracker_is_focused_too();
	test_the_two_events_move_the_state();
	test_the_lost_edge_latches_until_taken();
	test_only_a_bare_final_is_a_focus_event();
	test_the_strings_are_the_documented_ones();
	test_nothing_at_all_is_safe();
	printf("test_focus: OK\n");
	return 0;
}
