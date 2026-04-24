/*
 * Shell-dead degraded mode.  Runs after the interactive shell (ssh_chan)
 * closes while the SFTP channel and transfer queue are still alive.
 * Blocks the terminal loop until either:
 *
 *   - The queue drains (auto-exit)
 *   - The user picks "Hang up now" / presses Esc (abandon)
 *
 * Control is via a small modal list offering those two choices, plus
 * the queue screen.  The queue keeps running in its worker threads
 * throughout.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <genwrap.h>
#include <uifc.h>

#include "bbslist.h"
#include "ciolib.h"
#include "sftp_degraded.h"
#include "sftp_queue.h"
#include "sftp_queue_screen.h"
#include "uifcinit.h"

struct ciolib_screen *cp437_savescrn(void);

void
sftp_degraded_run(struct bbslist *bbs)
{
	if (!sftp_queue_has_work())
		return;

	struct ciolib_screen *savscrn = cp437_savescrn();
	init_uifc(true, true);

	static const char *const opts[] = {
		"Open queue screen",
		"Hang up now",
		NULL
	};
	int cur = 0;
	int bar = 0;
	bool first = true;

	while (sftp_queue_has_work()) {
		uifc_winmode_t mode = WIN_MID | WIN_SAV | WIN_ACT | WIN_DYN | WIN_EXTKEYS;
		if (!first)
			mode |= WIN_NODRAW;
		first = false;
		uifc.helpbuf =
		    "`Shell Disconnected`\n\n"
		    "The interactive shell on this BBS closed, but SFTP\n"
		    "transfers are still running on the same SSH session.\n"
		    "SyncTERM will disconnect automatically once the queue\n"
		    "drains.\n\n"
		    "~ Choices ~\n"
		    "  `Open queue screen`  Watch progress and cancel jobs.\n"
		    "  `Hang up now`        Abandon remaining transfers.\n"
		    "  `Esc`                Same as Hang up.\n\n"
		    "Queued (but not yet active) jobs are saved to the\n"
		    "per-BBS cache and will resume the next time you connect.\n";

		int rc = uifc.list(mode, 0, 0, 0, &cur, &bar,
		    "Shell Disconnected — SFTP transfers active",
		    (char **)opts);
		if (rc == -1) {
			/* Esc — abandon. */
			break;
		}
		if (rc < -1) {
			SLEEP(100);
			continue;
		}
		/* Selection. */
		if ((rc & MSK_OFF) == 0) {
			sftp_queue_screen_run(bbs);
			/* Queue screen did its own save/restore, but did
			 * its own init_uifc reset.  Force a redraw so our
			 * overlay comes back. */
			first = true;
		}
		else {
			/* Hang up now. */
			break;
		}
	}

	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}
