/* test_term_sdm.c -- which DECSDM (mode 80) sequence asks a given terminal to draw
 * its sixels at the text CURSOR.
 *
 * Getting this backwards is not a cosmetic bug. Under the wrong sequence cterm
 * ignores the cursor and anchors every image at the screen origin, so a centered
 * frame lands hard against the top-left corner and -- worse -- a dirty-rect
 * patch, which is positioned by nothing but the ESC[r;cH in front of it, lands
 * there too, over and over, one per frame. That is GitLab #1214, reported
 * against SyncRetro on SyncTERM 1.8.
 *
 * The boundary is the whole content of this test: cterm reversed the mode's
 * set/reset sense in revision 1.328 (commit 117de27530, 2026-06-28, "Reverse
 * DECSDM meaning"), and SyncTERM 1.8 -- the release players actually run --
 * ships 1.327, one revision below it. So both answers are live in the field
 * simultaneously and the table cannot be collapsed to a constant.
 *
 * Copyright(C) 2026 Rob Swindell / termgfx.  GPL-2.0.
 */
#include "term.h"
#include "caps.h"
#include "sixel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

#define AT_CURSOR_IS(ver, want) \
		CHECK(strcmp(termgfx_term_sixel_at_cursor(ver), (want)) == 0)

int main(void)
{
	/* Below the reversal: ?80h is what asks for the cursor. 1327 is not an
	 * arbitrary sample -- it is exactly what SyncTERM 1.8 ships. */
	AT_CURSOR_IS(1327, "\x1b[?80h");
	AT_CURSOR_IS(1207, "\x1b[?80h");   /* CTDA landed here; the oldest peer we identify */
	AT_CURSOR_IS(1, "\x1b[?80h");

	/* At and above it, ?80l does -- the DEC-documented sense. */
	AT_CURSOR_IS(TERMGFX_CTERM_VER_SDM, "\x1b[?80l");
	AT_CURSOR_IS(1329, "\x1b[?80l");
	AT_CURSOR_IS(1332, "\x1b[?80l");

	/* The boundary itself, spelled out both ways round. */
	AT_CURSOR_IS(TERMGFX_CTERM_VER_SDM - 1, "\x1b[?80h");
	CHECK(TERMGFX_CTERM_VER_SDM == 1328);

	/* Not cterm, or it has not answered yet: term_enter has already sent ?80l
	 * on the DEC/xterm/foot reading, and re-sending it must not disturb a
	 * terminal that was right all along. An UNKNOWN peer must never be handed
	 * the legacy sequence -- that would break every non-cterm sixel terminal to
	 * suit one that has not identified itself. */
	AT_CURSOR_IS(0, "\x1b[?80l");
	AT_CURSOR_IS(-1, "\x1b[?80l");

	/* term_enter's own mode-80 sequence has to agree with the unknown-peer answer,
	 * since that is the claim a door makes when it seeds its "already sent"
	 * state from term_enter and only corrects on a difference. */
	CHECK(strstr(termgfx_term_enter, termgfx_term_sixel_at_cursor(0)) != NULL);

	/* And term_leave must carry NO mode-80 sequence of its own. Nothing needs one on
	 * the way out -- draw-at-cursor, which the correction above has already
	 * established, is every terminal's default -- and the ?80h baked in here
	 * before is what left a current SyncTERM origin-anchored after every door
	 * exit, since that sequence means the opposite from cterm 1.328 on. */
	CHECK(strstr(termgfx_term_leave, "\x1b[?80") == NULL);

	/* --- the BEHAVIOURAL verdict ------------------------------------------
	 * Six cursor rows: [0..2] measured under ?80h, [3..5] under ?80l, each half
	 * being baseline / after a pan=1 sliver / after a pan=2 sliver. A half whose
	 * rows do not move is one where the terminal drew at the screen origin and
	 * ignored the cursor; the half that moves is the draw-at-cursor one. */
	{
		/* Genuine VT340, and cterm >= 1.328: ?80l draws at the cursor. */
		static const int modern[] = { 1, 1, 1,  1, 4, 10 };
		/* The VT340 MANUAL's (backwards) reading, and cterm <= 1.327. */
		static const int legacy[] = { 1, 4, 10,  1, 1, 1 };
		/* No mode 80 at all: it drew at the cursor under both. */
		static const int nomode[] = { 1, 4, 10,  1, 4, 10 };
		/* Never at the cursor under either -- nothing to choose between them. */
		static const int neither[] = { 1, 1, 1,  1, 1, 1 };
		/* Draws at the cursor under ?80l but ignores the pan: equal advances. */
		static const int nopan[] = { 1, 1, 1,  1, 4, 7 };

		CHECK(termgfx_sixel_sdm_verdict(modern, 6) == 0);
		CHECK(termgfx_sixel_sdm_verdict(legacy, 6) == 1);
		CHECK(termgfx_sixel_sdm_verdict(nomode, 6) == -1);
		CHECK(termgfx_sixel_sdm_verdict(neither, 6) == -1);

		/* Short of a full set, no verdict -- never a guess. */
		CHECK(termgfx_sixel_sdm_verdict(modern, 5) == -1);
		CHECK(termgfx_sixel_sdm_verdict(NULL, 6) == -1);

		/* The pan answer is read off whichever half actually drew at the cursor,
		 * so it survives the door having guessed the mode wrong -- which is the
		 * whole reason the probe now runs both halves. */
		CHECK(termgfx_sixel_vscale_verdict(modern, 6) == 1);
		CHECK(termgfx_sixel_vscale_verdict(legacy, 6) == 1);
		CHECK(termgfx_sixel_vscale_verdict(nopan, 6) == 0);
		CHECK(termgfx_sixel_vscale_verdict(neither, 6) == 0);
		CHECK(termgfx_sixel_vscale_verdict(modern, 5) == -1);
	}

	/* --- precedence: measurement outranks the revision --------------------
	 * The revision is an inference, the probe is an observation. A terminal that
	 * reports no version at all (0 below) can only be served by the probe, and
	 * that is the case the version gate cannot reach: the VT340 manual documents
	 * mode 80 backwards, so a terminal built from it behaves like cterm 1.327
	 * while saying nothing about itself. */
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(1, 0), "\x1b[?80h") == 0);
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(0, 0), "\x1b[?80l") == 0);
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(1, 1332), "\x1b[?80h") == 0);
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(0, 1327), "\x1b[?80l") == 0);

	/* Not measured: fall back to the revision, unchanged. */
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(-1, 1327), "\x1b[?80h") == 0);
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(-1, 1332), "\x1b[?80l") == 0);
	CHECK(strcmp(termgfx_term_sixel_at_cursor_probed(-1, 0), "\x1b[?80l") == 0);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
	return failures != 0;
}
