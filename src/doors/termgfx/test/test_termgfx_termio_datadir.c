/* Regression test for the Talkie/Floppy data-set selection helper
 * (termgfx_select_datadir(), M5 Task 1): audio -> talkie, no-audio -> floppy,
 * falling back to whichever variant is actually present, then to a flat
 * base dir. Pure directory-stat helper -- no session, no socket needed, so
 * this is a standalone binary (mirrors test_termgfx_termio_canvas.c's convention).
 * cc'd + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "termgfx_termio.h"

/* termgfx_select_datadir() is not part of the public termgfx_termio.h API (it
 * kept its original name across the move) -- declared here the same way the
 * TERMGFX_TEST-only seams are declared locally in the other test_termgfx_termio_*.c
 * files. */
const char *termgfx_select_datadir(const char *base, int audio, char *buf, size_t bufsz);

static void mkd(const char *p) { mkdir(p, 0777); }

int main(void)
{
	char tmpl[] = "/tmp/termgfx_dd_XXXXXX";
	char *base = mkdtemp(tmpl);
	assert(base);
	char t[512], f[512], buf[600];
	snprintf(t, sizeof t, "%s/talkie", base);
	snprintf(f, sizeof f, "%s/floppy", base);

	/* both present: audio -> talkie, no-audio -> floppy */
	mkd(t); mkd(f);
	assert(strcmp(termgfx_select_datadir(base, 1, buf, sizeof buf), t) == 0);
	assert(strcmp(termgfx_select_datadir(base, 0, buf, sizeof buf), f) == 0);

	/* only talkie present: no-audio falls back to talkie */
	rmdir(f);
	assert(strcmp(termgfx_select_datadir(base, 0, buf, sizeof buf), t) == 0);

	/* neither present: falls back to base itself (flat --path) */
	rmdir(t);
	assert(strcmp(termgfx_select_datadir(base, 1, buf, sizeof buf), base) == 0);

	rmdir(base);
	printf("TERMGFX_DATADIR OK\n");
	return 0;
}
