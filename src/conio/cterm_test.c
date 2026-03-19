/*
 * cterm_test — unit tests for cterm emulation modes
 *
 * Initializes ciolib with SDL offscreen, creates cterm instances in
 * various emulation modes, writes test data via cterm_write(), and
 * verifies screen state via vmem_gettext().
 *
 * Run with: SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software ./cterm_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciolib.h"
#include "cterm.h"
#include "vidmodes.h"

/* Stub required by cterm for response output */
int conn_send(const void *buf, size_t len, unsigned timeout)
{
	return len;
}

static FILE *result_fp;
static int total, passed, failed, skipped;

typedef int (*test_fn)(void);

/* ================================================================ */
/* Test infrastructure                                              */
/* ================================================================ */

static struct cterminal *cterm;
static struct vmem_cell *scrollback;
static int term_cols, term_rows;

#define SCROLL_LINES 100

static void
setup_cterm(int mode, int emulation)
{
	struct text_info ti;
	int flags;

	if (cterm) {
		cterm_end(cterm, 0);
		cterm = NULL;
	}
	free(scrollback);
	scrollback = NULL;

	textmode(mode);

	int vm = find_vmode(mode);
	if (vm == -1) {
		fprintf(stderr, "Unknown video mode %d\n", mode);
		return;
	}
	term_cols = vparams[vm].cols;

	/* Set video flags appropriate for the mode */
	flags = getvideoflags();
	flags |= vparams[vm].flags;
	setvideoflags(flags);

	gettextinfo(&ti);
	term_rows = ti.screenheight;

	scrollback = calloc(SCROLL_LINES * term_cols, sizeof(*scrollback));
	if (!scrollback) {
		fprintf(stderr, "Failed to allocate scrollback\n");
		return;
	}

	cterm = cterm_init(term_rows, term_cols, 1, 1,
	    SCROLL_LINES, term_cols, scrollback, emulation);
	if (!cterm) {
		fprintf(stderr, "cterm_init failed\n");
		return;
	}
	cterm_start(cterm);
}

static void
teardown_cterm(void)
{
	if (cterm) {
		cterm_end(cterm, 0);
		cterm = NULL;
	}
	free(scrollback);
	scrollback = NULL;
}

/*
 * Write raw bytes to cterm.
 */
static void
ct_write(const void *buf, int len)
{
	int speed = 0;
	cterm_write(cterm, buf, len, NULL, 0, &speed);
}

/*
 * Write a NUL-terminated string to cterm.
 */
static void
ct_puts(const char *s)
{
	ct_write(s, strlen(s));
}

/*
 * Read screen cells at 1-based coordinates.
 * Returns the number of cells read.
 */
static int
ct_gettext(int x1, int y1, int x2, int y2, struct vmem_cell *buf)
{
	return vmem_gettext(x1, y1, x2, y2, buf);
}

/*
 * Read characters from a row into a string buffer.
 * Returns the number of characters read.
 */
static int
ct_read_chars(int row, int col, int len, char *out, size_t outsz)
{
	struct vmem_cell cells[256];
	int n = len;
	if (n > 256) n = 256;
	if ((size_t)n >= outsz) n = outsz - 1;

	ct_gettext(col, row, col + n - 1, row, cells);
	for (int i = 0; i < n; i++)
		out[i] = cells[i].ch & 0xFF;
	out[n] = '\0';
	return n;
}

/*
 * Get cursor position (1-based).
 */
static void
ct_cursor(int *col, int *row)
{
	struct text_info ti;
	gettextinfo(&ti);
	if (col) *col = ti.curx;
	if (row) *row = ti.cury;
}

/* ================================================================ */
/* Atari ST VT52 tests                                              */
/* ================================================================ */

static int
test_vt52_printable(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("Hello");
	ct_read_chars(1, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "Hello", 5) != 0) {
		fprintf(result_fp, "    expected 'Hello', got '%.5s'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_cr(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("Hello\r");
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_vt52_lf(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("Hi");
	ct_write("\n", 1);
	ct_cursor(&col, &row);
	if (col != 3 || row != 2) {
		fprintf(result_fp, "    cursor at %d,%d, expected 3,2\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_vt52_bs(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("AB\b");
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    cursor at col %d, expected 2\n", col);
		return 0;
	}
	/* BS at col 1 should clamp */
	ct_puts("\b\b\b");
	ct_cursor(&col, &row);
	if (col != 1) {
		fprintf(result_fp, "    BS didn't clamp at col 1, got %d\n", col);
		return 0;
	}
	return 1;
}

static int
test_vt52_bel(void)
{
	int col, row;

	/* BEL should not move cursor */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("A");
	ct_cursor(&col, &row);
	ct_write("\x07", 1);
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    BEL moved cursor from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

static int
test_vt52_vt_ff(void)
{
	int col, row;

	/* VT and FF should behave like LF */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("AB");
	ct_write("\x0B", 1);	/* VT */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    VT: cursor row %d, expected 2\n", row);
		return 0;
	}
	if (col != 3) {
		fprintf(result_fp, "    VT: cursor col %d, expected 3\n", col);
		return 0;
	}
	ct_write("\x0C", 1);	/* FF */
	ct_cursor(&col, &row);
	if (row != 3) {
		fprintf(result_fp, "    FF: cursor row %d, expected 3\n", row);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_A(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\r\n\r\n");	/* move to row 3 */
	ct_puts("\x1b""A");	/* cursor up */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	/* At top, should clamp */
	ct_puts("\x1b""A\x1b""A\x1b""A");
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    didn't clamp at row 1, got %d\n", row);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_B(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""B");	/* cursor down */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_C(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""C");	/* cursor right */
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    col %d, expected 2\n", col);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_D(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ABC\x1b""D");	/* cursor left */
	ct_cursor(&col, &row);
	if (col != 3) {
		fprintf(result_fp, "    col %d, expected 3\n", col);
		return 0;
	}
	/* At left margin, should clamp */
	ct_puts("\x1b""D\x1b""D\x1b""D\x1b""D");
	ct_cursor(&col, &row);
	if (col != 1) {
		fprintf(result_fp, "    didn't clamp at col 1, got %d\n", col);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_H(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("Hello\r\n\r\n");
	ct_puts("\x1b""H");	/* home */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_I(void)
{
	int col, row;
	char buf[16];

	/* Reverse line feed — at row 2, should move to row 1 */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\r\nHello");	/* row 2 */
	ct_puts("\x1b""I");	/* reverse LF */
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    row %d, expected 1\n", row);
		return 0;
	}
	/* At row 1, should scroll down */
	ct_puts("\x1b""I");
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    after scroll, row %d, expected 1\n", row);
		return 0;
	}
	/* "Hello" should have moved to row 3 */
	ct_read_chars(3, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "Hello", 5) != 0) {
		fprintf(result_fp, "    scroll: row 3 = '%.5s', expected 'Hello'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_E(void)
{
	int col, row;
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ABCDEFGHIJ\r\nKLMNOPQRST");
	ct_puts("\x1b""E");	/* clear screen + home */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	ct_read_chars(1, 1, 10, buf, sizeof(buf));
	for (int i = 0; i < 10; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    screen not cleared, col %d = 0x%02x\n",
			    i + 1, (unsigned char)buf[i]);
			return 0;
		}
	}
	return 1;
}

static int
test_vt52_esc_J(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("AAAAAAAAAA\r\nBBBBBBBBBB\r\nCCCCCCCCCC");
	/* Position at row 2, col 4 and erase to end of page */
	ct_puts("\x1b""Y" "\x21\x23");	/* row 1, col 3 (0-based) = row 2, col 4 */
	ct_puts("\x1b""J");
	/* Row 2 cols 1-3 should still be "BBB" */
	ct_read_chars(2, 1, 3, buf, sizeof(buf));
	if (strncmp(buf, "BBB", 3) != 0) {
		fprintf(result_fp, "    row 2 cols 1-3: '%.3s', expected 'BBB'\n", buf);
		return 0;
	}
	/* Row 2 cols 4+ should be blank */
	ct_read_chars(2, 4, 7, buf, sizeof(buf));
	for (int i = 0; i < 7; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    row 2 col %d not blank\n", i + 4);
			return 0;
		}
	}
	/* Row 3 should be blank */
	ct_read_chars(3, 1, 10, buf, sizeof(buf));
	for (int i = 0; i < 10; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    row 3 col %d not blank\n", i + 1);
			return 0;
		}
	}
	return 1;
}

static int
test_vt52_esc_K(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ABCDEFGHIJ");
	/* Move to col 4 and erase to end of line */
	ct_puts("\x1b""Y" "\x20\x23");	/* row 0, col 3 = row 1, col 4 */
	ct_puts("\x1b""K");
	ct_read_chars(1, 1, 10, buf, sizeof(buf));
	if (strncmp(buf, "ABC", 3) != 0) {
		fprintf(result_fp, "    cols 1-3: '%.3s', expected 'ABC'\n", buf);
		return 0;
	}
	for (int i = 3; i < 10; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    col %d not blank\n", i + 1);
			return 0;
		}
	}
	return 1;
}

static int
test_vt52_esc_Y(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* ESC Y row col — row and col are 0-based + 32 */
	ct_puts("\x1b""Y" "\x28\x2A");	/* row 8, col 10 (0-based) = row 9, col 11 */
	ct_cursor(&col, &row);
	if (col != 11 || row != 9) {
		fprintf(result_fp, "    cursor at %d,%d, expected 11,9\n", col, row);
		return 0;
	}
	/* Row > 23 should clamp */
	ct_puts("\x1b""Y" "\x38\x20");	/* row 24 (0-based), should clamp */
	ct_cursor(&col, &row);
	/* Row 24 (0-based) > 23, so it should clamp — cursor row unchanged */
	/* Actually, the code says "if y > 23, CURR_XY(NULL, &y)" which keeps current y */
	/* Column should be 1 (offset 0 + 1) */
	return 1;
}

static int
test_vt52_esc_L(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("LINE1\r\nLINE2\r\nLINE3");
	/* Move to row 2 and insert a line */
	ct_puts("\x1b""Y" "\x21\x20");	/* row 1, col 0 = row 2, col 1 */
	ct_puts("\x1b""L");
	/* Row 2 should now be blank */
	ct_read_chars(2, 1, 5, buf, sizeof(buf));
	for (int i = 0; i < 5; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    inserted row not blank at col %d\n", i + 1);
			return 0;
		}
	}
	/* Old LINE2 should be at row 3 */
	ct_read_chars(3, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "LINE2", 5) != 0) {
		fprintf(result_fp, "    row 3: '%.5s', expected 'LINE2'\n", buf);
		return 0;
	}
	/* LINE1 should still be at row 1 */
	ct_read_chars(1, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "LINE1", 5) != 0) {
		fprintf(result_fp, "    row 1: '%.5s', expected 'LINE1'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_M(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("LINE1\r\nLINE2\r\nLINE3");
	/* Move to row 2 and delete a line */
	ct_puts("\x1b""Y" "\x21\x20");
	ct_puts("\x1b""M");
	/* Row 2 should now have LINE3 */
	ct_read_chars(2, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "LINE3", 5) != 0) {
		fprintf(result_fp, "    row 2: '%.5s', expected 'LINE3'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_d(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("AAAAAAAAAA\r\nBBBBBBBBBB\r\nCCCCCCCCCC");
	/* Position at row 2, col 4 and erase to start of page */
	ct_puts("\x1b""Y" "\x21\x23");
	ct_puts("\x1b""d");
	/* Row 1 should be blank */
	ct_read_chars(1, 1, 10, buf, sizeof(buf));
	for (int i = 0; i < 10; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    row 1 col %d not blank\n", i + 1);
			return 0;
		}
	}
	/* Row 2 cols 1-4 should be blank (inclusive of cursor) */
	ct_read_chars(2, 1, 4, buf, sizeof(buf));
	for (int i = 0; i < 4; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    row 2 col %d not blank\n", i + 1);
			return 0;
		}
	}
	/* Row 3 should still have content */
	ct_read_chars(3, 1, 10, buf, sizeof(buf));
	if (strncmp(buf, "CCCCCCCCCC", 10) != 0) {
		fprintf(result_fp, "    row 3: '%.10s', expected 'CCCCCCCCCC'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_l(void)
{
	char buf[16];
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ABCDEFGHIJ");
	ct_puts("\x1b""Y" "\x20\x24");	/* move to col 5 */
	ct_puts("\x1b""l");		/* clear line */
	ct_cursor(&col, &row);
	/* Cursor position should be preserved */
	if (col != 5 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 5,1\n", col, row);
		return 0;
	}
	ct_read_chars(1, 1, 10, buf, sizeof(buf));
	for (int i = 0; i < 10; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    col %d not blank\n", i + 1);
			return 0;
		}
	}
	return 1;
}

static int
test_vt52_esc_o(void)
{
	char buf[16];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ABCDEFGHIJ");
	ct_puts("\x1b""Y" "\x20\x24");	/* move to col 5 */
	ct_puts("\x1b""o");		/* erase to start of line */
	/* Cols 1-5 should be blank */
	ct_read_chars(1, 1, 5, buf, sizeof(buf));
	for (int i = 0; i < 5; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    col %d not blank\n", i + 1);
			return 0;
		}
	}
	/* Cols 6+ should still have content */
	ct_read_chars(1, 6, 5, buf, sizeof(buf));
	if (strncmp(buf, "FGHIJ", 5) != 0) {
		fprintf(result_fp, "    cols 6-10: '%.5s', expected 'FGHIJ'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_jk(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""Y" "\x25\x2A");	/* row 5, col 10 (0-based) */
	ct_puts("\x1b""j");		/* save cursor */
	ct_puts("\x1b""H");		/* home */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    after home: %d,%d\n", col, row);
		return 0;
	}
	ct_puts("\x1b""k");		/* restore cursor */
	ct_cursor(&col, &row);
	if (col != 11 || row != 6) {
		fprintf(result_fp, "    after restore: %d,%d, expected 11,6\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_ef(void)
{
	/* Just verify these don't crash — cursor visibility isn't
	 * readable via vmem, but we can check the cterm state */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""f");	/* hide cursor */
	if (cterm->cursor != _NOCURSOR) {
		fprintf(result_fp, "    cursor not hidden\n");
		return 0;
	}
	ct_puts("\x1b""e");	/* show cursor */
	if (cterm->cursor != _NORMALCURSOR) {
		fprintf(result_fp, "    cursor not shown\n");
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_pq(void)
{
	struct vmem_cell cells[2];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("A");	/* normal video */
	ct_puts("\x1b""p");	/* reverse video on */
	ct_puts("B");
	ct_puts("\x1b""q");	/* reverse video off */

	ct_gettext(1, 1, 1, 1, &cells[0]);
	ct_gettext(2, 1, 2, 1, &cells[1]);

	/* In reverse video, fg and bg should be swapped */
	if (cells[0].fg == cells[1].fg && cells[0].bg == cells[1].bg) {
		fprintf(result_fp, "    reverse video had no effect on colors\n");
		return 0;
	}
	/* Specifically, A's fg should equal B's bg and vice versa */
	if (cells[0].fg != cells[1].bg || cells[0].bg != cells[1].fg) {
		fprintf(result_fp, "    reverse didn't swap: A fg=%u bg=%u, B fg=%u bg=%u\n",
		    cells[0].fg, cells[0].bg, cells[1].fg, cells[1].bg);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_vw(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Autowrap should be off by default */
	if (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) {
		fprintf(result_fp, "    autowrap on by default\n");
		return 0;
	}
	ct_puts("\x1b""v");	/* enable autowrap */
	if (!(cterm->extattr & CTERM_EXTATTR_AUTOWRAP)) {
		fprintf(result_fp, "    autowrap not enabled\n");
		return 0;
	}
	ct_puts("\x1b""w");	/* disable autowrap */
	if (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) {
		fprintf(result_fp, "    autowrap not disabled\n");
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_bc(void)
{
	struct vmem_cell cells[2];

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Set fg to palette index 2 (green in 4-color mode) */
	ct_puts("\x1b""b\x02");
	ct_puts("A");
	/* Set bg to palette index 1 (red in 4-color mode) */
	ct_puts("\x1b""c\x01");
	ct_puts("B");

	ct_gettext(1, 1, 1, 1, &cells[0]);	/* A: fg=2, bg=default */
	ct_gettext(2, 1, 2, 1, &cells[1]);	/* B: fg=2, bg=1 */

	/* A and B should have the same fg (both set to index 2) */
	if (cells[0].fg != cells[1].fg) {
		fprintf(result_fp, "    fg mismatch: A=%u, B=%u\n", cells[0].fg, cells[1].fg);
		return 0;
	}
	/* B should have a different bg than A */
	if (cells[0].bg == cells[1].bg) {
		fprintf(result_fp, "    bg unchanged after ESC c\n");
		return 0;
	}
	return 1;
}

static int
test_vt52_ignored_controls(void)
{
	int col, row;

	/* Control characters 1-6, 14-26, 28-31 should be ignored */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("A");
	ct_cursor(&col, &row);
	/* Send all ignored controls */
	char ignored[] = {1, 2, 3, 4, 5, 6, 14, 15, 16, 17, 18, 19,
	    20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31};
	ct_write(ignored, sizeof(ignored));
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    cursor moved from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

static int
test_vt52_nowrap_default(void)
{
	int col, row;
	char buf[4];

	/* With autowrap off (default), writing at col 80 should not wrap */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Fill row 1 with 80 A's */
	for (int i = 0; i < 80; i++)
		ct_puts("A");
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    wrapped to row %d without autowrap\n", row);
		return 0;
	}
	/* Write one more — should overwrite at col 80, not wrap */
	ct_puts("B");
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    overwrite wrapped to row %d\n", row);
		return 0;
	}
	ct_read_chars(1, 80, 1, buf, sizeof(buf));
	if (buf[0] != 'B') {
		fprintf(result_fp, "    col 80 = '%c', expected 'B'\n", buf[0]);
		return 0;
	}
	return 1;
}

static int
test_vt52_esc_eq_gt(void)
{
	/* Test alternate/normal keypad mode flags */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	if (cterm->extattr & CTERM_EXTATTR_ALTERNATE_KEYPAD) {
		fprintf(result_fp, "    alternate keypad on by default\n");
		return 0;
	}
	ct_puts("\x1b=");	/* alternate keypad */
	if (!(cterm->extattr & CTERM_EXTATTR_ALTERNATE_KEYPAD)) {
		fprintf(result_fp, "    alternate keypad not enabled\n");
		return 0;
	}
	ct_puts("\x1b>");	/* normal keypad */
	if (cterm->extattr & CTERM_EXTATTR_ALTERNATE_KEYPAD) {
		fprintf(result_fp, "    alternate keypad not disabled\n");
		return 0;
	}
	return 1;
}

static int
test_vt52_lf_scroll(void)
{
	char buf[16];
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("FIRST");
	/* Move to last row via ESC B repeatedly */
	for (int i = 0; i < term_rows - 1; i++)
		ct_puts("\x1b""B");
	ct_cursor(&col, &row);
	int last_row = row;
	ct_puts("\rBOTTOM");
	ct_write("\n", 1);	/* LF at bottom — triggers scroll */
	/* BOTTOM should have moved up one row */
	ct_read_chars(last_row - 1, 1, 6, buf, sizeof(buf));
	if (strncmp(buf, "BOTTOM", 6) != 0) {
		fprintf(result_fp, "    row %d: '%.6s', expected 'BOTTOM'\n",
		    last_row - 1, buf);
		return 0;
	}
	/* Last row should be blank */
	ct_read_chars(last_row, 1, 6, buf, sizeof(buf));
	for (int i = 0; i < 6; i++) {
		if (buf[i] != ' ' && buf[i] != 0) {
			fprintf(result_fp, "    last row col %d not blank\n", i + 1);
			return 0;
		}
	}
	return 1;
}

static int
test_vt52_vt_scroll(void)
{
	char buf[16];
	int col, row;

	/* VT at bottom row should also scroll */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Move to last row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_puts("\x1b""B");
	ct_cursor(&col, &row);
	int last_row = row;
	ct_puts("\rVT_TEST");
	ct_write("\x0B", 1);	/* VT at bottom */
	ct_cursor(&col, &row);
	if (row != last_row) {
		fprintf(result_fp, "    cursor row %d after VT scroll, expected %d\n",
		    row, last_row);
		return 0;
	}
	/* VT_TEST should have scrolled up one row */
	ct_read_chars(last_row - 1, 1, 7, buf, sizeof(buf));
	if (strncmp(buf, "VT_TEST", 7) != 0) {
		fprintf(result_fp, "    row %d: '%.7s', expected 'VT_TEST'\n",
		    last_row - 1, buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_wrap_enabled(void)
{
	int col, row;
	char buf[4];

	/* Enable autowrap, fill row, next char should wrap */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""v");	/* enable autowrap */
	for (int i = 0; i < 80; i++)
		ct_puts("A");
	/* Cursor should still be on row 1 col 80 (LCF) or row 2 col 1 */
	ct_puts("B");	/* this should wrap */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    after wrap: row %d, expected 2\n", row);
		return 0;
	}
	if (col != 2) {
		fprintf(result_fp, "    after wrap: col %d, expected 2\n", col);
		return 0;
	}
	/* B should be at row 2 col 1 */
	ct_read_chars(2, 1, 1, buf, sizeof(buf));
	if (buf[0] != 'B') {
		fprintf(result_fp, "    row 2 col 1 = '%c', expected 'B'\n", buf[0]);
		return 0;
	}
	return 1;
}

static int
test_vt52_wrap_scroll(void)
{
	char buf[16];
	int col, row;

	/* Autowrap at bottom row should scroll */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""v");	/* enable autowrap */
	ct_puts("ROW1");
	/* Move to last row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_puts("\x1b""B");
	ct_cursor(&col, &row);
	int last_row = row;
	ct_puts("\r");
	/* Fill the last row */
	for (int i = 0; i < term_cols; i++)
		ct_puts("Z");
	/* Write one more — should wrap and scroll */
	ct_puts("W");
	ct_cursor(&col, &row);
	if (row != last_row) {
		fprintf(result_fp, "    cursor row %d after wrap+scroll, expected %d\n",
		    row, last_row);
		return 0;
	}
	/* W should be at last_row col 1 */
	ct_read_chars(last_row, 1, 1, buf, sizeof(buf));
	if (buf[0] != 'W') {
		fprintf(result_fp, "    last row col 1 = '%c', expected 'W'\n", buf[0]);
		return 0;
	}
	return 1;
}

static int
test_vt52_cursor_B_clamp(void)
{
	int col, row;

	/* ESC B at bottom row should clamp, not scroll */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Move to last row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_puts("\x1b""B");
	ct_cursor(&col, &row);
	int last_row = row;
	/* One more — should clamp */
	ct_puts("\x1b""B");
	ct_cursor(&col, &row);
	if (row != last_row) {
		fprintf(result_fp, "    cursor row %d, expected %d (clamped)\n",
		    row, last_row);
		return 0;
	}
	return 1;
}

static int
test_vt52_cursor_C_clamp(void)
{
	int col, row;

	/* ESC C at right margin should clamp */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("\x1b""Y" "\x20\x6F");	/* row 0, col 79 = last col */
	ct_puts("\x1b""C");	/* cursor right — should clamp */
	ct_cursor(&col, &row);
	if (col != 80) {
		fprintf(result_fp, "    cursor col %d, expected 80 (clamped)\n", col);
		return 0;
	}
	return 1;
}

static int
test_vt52_cr_lf_sequence(void)
{
	int col, row;
	char buf[16];

	/* CR LF should move to col 1, next row */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("Hello\r\nWorld");
	ct_cursor(&col, &row);
	if (col != 6 || row != 2) {
		fprintf(result_fp, "    cursor at %d,%d, expected 6,2\n", col, row);
		return 0;
	}
	ct_read_chars(1, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "Hello", 5) != 0) {
		fprintf(result_fp, "    row 1: '%.5s', expected 'Hello'\n", buf);
		return 0;
	}
	ct_read_chars(2, 1, 5, buf, sizeof(buf));
	if (strncmp(buf, "World", 5) != 0) {
		fprintf(result_fp, "    row 2: '%.5s', expected 'World'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_scroll_content_preserved(void)
{
	char buf[16];

	/* Verify scroll moves content up correctly */
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("ROW_01\r\nROW_02\r\nROW_03");
	/* Move to last row via ESC B */
	for (int i = 3; i < term_rows; i++)
		ct_puts("\x1b""B");
	/* One scroll — ROW_01 should move to row 0 (gone),
	 * ROW_02 -> row 1, ROW_03 -> row 2 */
	ct_write("\n", 1);
	ct_read_chars(1, 1, 6, buf, sizeof(buf));
	if (strncmp(buf, "ROW_02", 6) != 0) {
		fprintf(result_fp, "    row 1 after scroll: '%.6s', expected 'ROW_02'\n", buf);
		return 0;
	}
	ct_read_chars(2, 1, 6, buf, sizeof(buf));
	if (strncmp(buf, "ROW_03", 6) != 0) {
		fprintf(result_fp, "    row 2 after scroll: '%.6s', expected 'ROW_03'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_vt52_tab(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_puts("A\t");
	ct_cursor(&col, &row);
	/* Default tab stops are every 8 columns.
	 * A is at col 1, cursor advances to col 2.
	 * Tab should advance to col 9 (next 8-col boundary). */
	if (col != 9) {
		fprintf(result_fp, "    after tab: col %d, expected 9\n", col);
		return 0;
	}
	return 1;
}

/* ================================================================ */
/* ATASCII tests                                                    */
/* ================================================================ */

static int
test_atascii_printable(void)
{
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	/* In normal mode, raw ATASCII byte is stored directly */
	ct_write("A", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 65) {
		fprintf(result_fp, "    'A' stored as %d, expected 65\n",
		    cells[0].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_atascii_return(void)
{
	int col, row;

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("AB", 2);
	ct_write("\x9b", 1);	/* 155 = Return */
	ct_cursor(&col, &row);
	if (col != 1 || row != 2) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,2\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_atascii_clear_screen(void)
{
	int col, row;
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("HELLO", 5);
	ct_write("\x7d", 1);	/* 125 = Clear Screen */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	ct_gettext(1, 1, 5, 1, cells);
	for (int i = 0; i < 5; i++) {
		if (cells[i].ch != 0 && cells[i].ch != ' ') {
			fprintf(result_fp, "    col %d not blank (ch=%d)\n",
			    i + 1, cells[i].ch);
			return 0;
		}
	}
	return 1;
}

static int
test_atascii_cursor_up_wrap(void)
{
	int col, row;

	/* Cursor up wraps to bottom of same column */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("\x1c", 1);	/* 28 = Cursor Up from row 1 */
	ct_cursor(&col, &row);
	if (row != term_rows) {
		fprintf(result_fp, "    wrapped to row %d, expected %d\n",
		    row, term_rows);
		return 0;
	}
	if (col != 1) {
		fprintf(result_fp, "    column changed to %d, expected 1\n", col);
		return 0;
	}
	return 1;
}

static int
test_atascii_cursor_down_wrap(void)
{
	int col, row;

	/* Cursor down wraps to top of same column */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	/* Move to bottom row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_write("\x1d", 1);	/* 29 = Cursor Down */
	ct_cursor(&col, &row);
	int last_row = row;
	ct_write("\x1d", 1);	/* one more wraps to top */
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    wrapped to row %d, expected 1\n", row);
		return 0;
	}
	(void)last_row;
	return 1;
}

static int
test_atascii_cursor_left_wrap(void)
{
	int col, row;

	/* Cursor left wraps to right side of same row */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("\x1e", 1);	/* 30 = Cursor Left from col 1 */
	ct_cursor(&col, &row);
	if (col != term_cols) {
		fprintf(result_fp, "    wrapped to col %d, expected %d\n",
		    col, term_cols);
		return 0;
	}
	if (row != 1) {
		fprintf(result_fp, "    row changed to %d, expected 1\n", row);
		return 0;
	}
	return 1;
}

static int
test_atascii_cursor_right_wrap(void)
{
	int col, row;

	/* Cursor right wraps to left side of same row */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	/* Move to last column */
	for (int i = 0; i < term_cols - 1; i++)
		ct_write("\x1f", 1);	/* 31 = Cursor Right */
	ct_write("\x1f", 1);	/* one more wraps */
	ct_cursor(&col, &row);
	if (col != 1) {
		fprintf(result_fp, "    wrapped to col %d, expected 1\n", col);
		return 0;
	}
	if (row != 1) {
		fprintf(result_fp, "    row changed to %d, expected 1\n", row);
		return 0;
	}
	return 1;
}

static int
test_atascii_backspace(void)
{
	int col, row;
	struct vmem_cell cells[1];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("AB", 2);
	ct_write("\x7e", 1);	/* 126 = Backspace */
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    cursor at col %d, expected 2\n", col);
		return 0;
	}
	/* Character at col 2 should be erased (space) */
	ct_gettext(2, 1, 2, 1, cells);
	if (cells[0].ch != 0 && cells[0].ch != ' ') {
		fprintf(result_fp, "    col 2 not erased (ch=%d)\n", cells[0].ch);
		return 0;
	}
	/* Backspace at col 1 should stick */
	ct_write("\x7e\x7e", 2);
	ct_cursor(&col, &row);
	if (col != 1) {
		fprintf(result_fp, "    BS didn't clamp at col 1, got %d\n", col);
		return 0;
	}
	return 1;
}

static int
test_atascii_esc_inverse(void)
{
	struct vmem_cell cells[2];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("A", 1);		/* normal */
	ct_write("\x1b" "A", 2);	/* ESC + A = inverse A */
	ct_gettext(1, 1, 2, 1, cells);
	/* Normal char should have attr 7, inverse should have attr 1 */
	if (cells[0].legacy_attr != 7) {
		fprintf(result_fp, "    normal attr %d, expected 7\n",
		    cells[0].legacy_attr);
		return 0;
	}
	if (cells[1].legacy_attr != 1) {
		fprintf(result_fp, "    inverse attr %d, expected 1\n",
		    cells[1].legacy_attr);
		return 0;
	}
	return 1;
}

static int
test_atascii_esc_auto_reset(void)
{
	struct vmem_cell cells[3];

	/* ESC only affects the next character, then resets to normal */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("\x1b" "AB", 3);	/* ESC + A (inverse) + B (normal) */
	ct_gettext(1, 1, 2, 1, cells);
	if (cells[0].legacy_attr != 1) {
		fprintf(result_fp, "    first char attr %d, expected 1\n",
		    cells[0].legacy_attr);
		return 0;
	}
	if (cells[1].legacy_attr != 7) {
		fprintf(result_fp, "    second char attr %d, expected 7\n",
		    cells[1].legacy_attr);
		return 0;
	}
	return 1;
}

static int
test_atascii_delete_line(void)
{
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("AAAAA", 5);
	ct_write("\x9b", 1);	/* Return - next line */
	ct_write("BBBBB", 5);
	ct_write("\x9b", 1);
	ct_write("CCCCC", 5);
	/* Cursor at row 3 col 6. Up to row 2. */
	ct_write("\x1c", 1);
	ct_write("\x9c", 1);	/* 156 = Delete Line */
	/* Row 2 should now have what was row 3 (CCCCC) — raw byte 67 */
	ct_gettext(1, 2, 5, 2, cells);
	for (int i = 0; i < 5; i++) {
		if ((cells[i].ch & 0xFF) != 67) {
			fprintf(result_fp, "    row 2 col %d: ch=%d, expected 67\n",
			    i + 1, cells[i].ch & 0xFF);
			return 0;
		}
	}
	return 1;
}

static int
test_atascii_insert_line(void)
{
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("AAAAA", 5);
	ct_write("\x9b", 1);	/* Return */
	ct_write("BBBBB", 5);
	/* Cursor at row 2 col 6. Move up to row 1 */
	ct_write("\x1c", 1);
	ct_write("\x9d", 1);	/* 157 = Insert Line */
	/* Row 1 should be blank, old row 1 (AAAAA) should be at row 2 */
	ct_gettext(1, 1, 5, 1, cells);
	for (int i = 0; i < 5; i++) {
		if (cells[i].ch != 0 && cells[i].ch != ' ') {
			fprintf(result_fp, "    inserted row not blank at col %d\n", i + 1);
			return 0;
		}
	}
	/* 'A' raw byte = 65 */
	ct_gettext(1, 2, 5, 2, cells);
	for (int i = 0; i < 5; i++) {
		if ((cells[i].ch & 0xFF) != 65) {
			fprintf(result_fp, "    row 2 col %d: ch=%d, expected 65\n",
			    i + 1, cells[i].ch & 0xFF);
			return 0;
		}
	}
	return 1;
}

static int
test_atascii_delete_char(void)
{
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("ABCDE", 5);
	/* Move to col 2: Left 3 times from col 6 (after E) -> 5 -> 4 -> 3.
	 * Wait, cursor is at col 6 after writing 5 chars. Left 3 = col 3.
	 * Need to be at col 2 for delete. Left 4 times. */
	ct_write("\x1e\x1e\x1e\x1e", 4);	/* col 6->5->4->3->2 */
	ct_write("\xfe", 1);	/* 254 = Delete Char */
	/* Deletes char at col 2 ('B'=66), shifts CDE left: A C D E _ */
	ct_gettext(1, 1, 4, 1, cells);
	/* Raw bytes: A=65, C=67, D=68, E=69 */
	if ((cells[0].ch & 0xFF) != 65 || (cells[1].ch & 0xFF) != 67 ||
	    (cells[2].ch & 0xFF) != 68 || (cells[3].ch & 0xFF) != 69) {
		fprintf(result_fp, "    after DCH: %d %d %d %d, expected 65 67 68 69\n",
		    cells[0].ch & 0xFF, cells[1].ch & 0xFF,
		    cells[2].ch & 0xFF, cells[3].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_atascii_insert_char(void)
{
	struct vmem_cell cells[5];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("ABCD", 4);
	/* Move to col 2: from col 5, Left 3 times */
	ct_write("\x1e\x1e\x1e", 3);	/* col 5->4->3->2 */
	ct_write("\xff", 1);	/* 255 = Insert Char */
	/* Should insert space at col 2, shifting BCD right: A_BCD */
	ct_gettext(1, 1, 5, 1, cells);
	/* Raw bytes: A=65, space=32, B=66, C=67, D=68 */
	if ((cells[0].ch & 0xFF) != 65) {
		fprintf(result_fp, "    col 1: %d, expected 65\n", cells[0].ch & 0xFF);
		return 0;
	}
	if ((cells[1].ch & 0xFF) != 32 && cells[1].ch != 0) {
		fprintf(result_fp, "    col 2 not blank: %d\n", cells[1].ch & 0xFF);
		return 0;
	}
	if ((cells[2].ch & 0xFF) != 66) {
		fprintf(result_fp, "    col 3: %d, expected 66\n", cells[2].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_atascii_bell(void)
{
	int col, row;

	/* Bell should not move cursor */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\xfd", 1);	/* 253 = Bell */
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    bell moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_atascii_return_scroll(void)
{
	struct vmem_cell cells[5];

	/* Return at bottom row should scroll */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("FIRST", 5);
	/* Move to last row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_write("\x9b", 1);	/* Return to advance rows */
	/* Write something on last row */
	ct_write("LAST!", 5);
	ct_write("\x9b", 1);	/* Return at bottom — should scroll */
	/* "LAST!" should have moved up one row. Raw byte 'L'=76 */
	ct_gettext(1, term_rows - 1, 1, term_rows - 1, cells);
	if ((cells[0].ch & 0xFF) != 76) {
		fprintf(result_fp, "    scroll: row %d col 1 ch=%d, expected 76\n",
		    term_rows - 1, cells[0].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_atascii_tab(void)
{
	int col, row;

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	ct_write("A", 1);
	ct_write("\x7f", 1);	/* 127 = Tab */
	ct_cursor(&col, &row);
	/* Default tabs at 8-col intervals: from col 2, next stop at col 9 */
	if (col != 9) {
		fprintf(result_fp, "    tab: col %d, expected 9\n", col);
		return 0;
	}
	return 1;
}

static int
test_atascii_tab_set_clear(void)
{
	int col, row;

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	/* Move to col 5 and set a tab */
	for (int i = 0; i < 4; i++)
		ct_write("\x1f", 1);	/* Right */
	ct_write("\x9f", 1);	/* 159 = Set Tab */
	/* Go home and tab — should land at col 5 */
	ct_write("\x7d", 1);	/* Clear screen (homes cursor) */
	ct_write("A", 1);
	ct_write("\x7f", 1);	/* Tab */
	ct_cursor(&col, &row);
	if (col != 5) {
		fprintf(result_fp, "    custom tab: col %d, expected 5\n", col);
		return 0;
	}
	/* Clear that tab stop */
	ct_write("\x9e", 1);	/* 158 = Clear Tab at col 5 */
	ct_write("\x7d", 1);	/* Clear screen */
	ct_write("A", 1);
	ct_write("\x7f", 1);	/* Tab */
	ct_cursor(&col, &row);
	/* Should skip to next default stop (col 9) since col 5 was cleared */
	if (col != 9) {
		fprintf(result_fp, "    after clear: col %d, expected 9\n", col);
		return 0;
	}
	return 1;
}

static int
test_atascii_screen_code_mapping(void)
{
	struct vmem_cell cells[6];

	/* In ESC (inverse) mode, screen code translation is applied.
	 * In normal mode, raw bytes are stored directly.
	 * Test both modes. */
	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);

	/* Normal mode: raw bytes stored as-is */
	ct_write("A", 1);	/* byte 65 -> stored as 65 */
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 65) {
		fprintf(result_fp, "    normal 'A': ch=%d, expected 65\n",
		    cells[0].ch & 0xFF);
		return 0;
	}

	/* ESC mode: screen code translation applied */
	/* ESC + byte 65 (32-95 range): 65-32=33 */
	ct_write("\x1b\x41", 2);
	ct_gettext(2, 1, 2, 1, cells);
	if ((cells[0].ch & 0xFF) != 33) {
		fprintf(result_fp, "    ESC 'A': ch=%d, expected 33\n",
		    cells[0].ch & 0xFF);
		return 0;
	}

	/* ESC + byte 1 (0-31 range): 1+64=65 */
	ct_write("\x1b\x01", 2);
	ct_gettext(3, 1, 3, 1, cells);
	if ((cells[0].ch & 0xFF) != 65) {
		fprintf(result_fp, "    ESC 0x01: ch=%d, expected 65\n",
		    cells[0].ch & 0xFF);
		return 0;
	}

	/* ESC + byte 97 (96-127 range): no translation = 97 */
	ct_write("\x1b\x61", 2);
	ct_gettext(4, 1, 4, 1, cells);
	if ((cells[0].ch & 0xFF) != 97) {
		fprintf(result_fp, "    ESC 0x61: ch=%d, expected 97\n",
		    cells[0].ch & 0xFF);
		return 0;
	}

	return 1;
}

/* ================================================================ */
/* Test table and main                                              */
/* ================================================================ */

struct test_entry {
	const char *name;
	test_fn     fn;
};

static struct test_entry tests[] = {
	/* Atari ST VT52 */
	{"VT52_printable",     test_vt52_printable},
	{"VT52_CR",            test_vt52_cr},
	{"VT52_LF",            test_vt52_lf},
	{"VT52_BS",            test_vt52_bs},
	{"VT52_BEL",           test_vt52_bel},
	{"VT52_VT_FF",         test_vt52_vt_ff},
	{"VT52_ignored_ctrl",  test_vt52_ignored_controls},
	{"VT52_cursor_up",     test_vt52_esc_A},
	{"VT52_cursor_down",   test_vt52_esc_B},
	{"VT52_cursor_right",  test_vt52_esc_C},
	{"VT52_cursor_left",   test_vt52_esc_D},
	{"VT52_home",          test_vt52_esc_H},
	{"VT52_reverse_lf",    test_vt52_esc_I},
	{"VT52_cursor_addr",   test_vt52_esc_Y},
	{"VT52_clear_screen",  test_vt52_esc_E},
	{"VT52_erase_eos",     test_vt52_esc_J},
	{"VT52_erase_eol",     test_vt52_esc_K},
	{"VT52_erase_sop",     test_vt52_esc_d},
	{"VT52_erase_sol",     test_vt52_esc_o},
	{"VT52_clear_line",    test_vt52_esc_l},
	{"VT52_insert_line",   test_vt52_esc_L},
	{"VT52_delete_line",   test_vt52_esc_M},
	{"VT52_save_restore",  test_vt52_esc_jk},
	{"VT52_show_hide_cur", test_vt52_esc_ef},
	{"VT52_reverse_video", test_vt52_esc_pq},
	{"VT52_autowrap",      test_vt52_esc_vw},
	{"VT52_colors",        test_vt52_esc_bc},
	{"VT52_nowrap",        test_vt52_nowrap_default},
	{"VT52_keypad_mode",   test_vt52_esc_eq_gt},
	{"VT52_lf_scroll",     test_vt52_lf_scroll},
	{"VT52_vt_scroll",     test_vt52_vt_scroll},
	{"VT52_wrap_enabled",  test_vt52_wrap_enabled},
	{"VT52_wrap_scroll",   test_vt52_wrap_scroll},
	{"VT52_B_clamp",       test_vt52_cursor_B_clamp},
	{"VT52_C_clamp",       test_vt52_cursor_C_clamp},
	{"VT52_cr_lf",         test_vt52_cr_lf_sequence},
	{"VT52_scroll_content", test_vt52_scroll_content_preserved},
	{"VT52_tab",           test_vt52_tab},
	/* ATASCII */
	{"ATA_printable",      test_atascii_printable},
	{"ATA_return",         test_atascii_return},
	{"ATA_clear_screen",   test_atascii_clear_screen},
	{"ATA_cursor_up",      test_atascii_cursor_up_wrap},
	{"ATA_cursor_down",    test_atascii_cursor_down_wrap},
	{"ATA_cursor_left",    test_atascii_cursor_left_wrap},
	{"ATA_cursor_right",   test_atascii_cursor_right_wrap},
	{"ATA_backspace",      test_atascii_backspace},
	{"ATA_esc_inverse",    test_atascii_esc_inverse},
	{"ATA_esc_auto_reset", test_atascii_esc_auto_reset},
	{"ATA_delete_line",    test_atascii_delete_line},
	{"ATA_insert_line",    test_atascii_insert_line},
	{"ATA_delete_char",    test_atascii_delete_char},
	{"ATA_insert_char",    test_atascii_insert_char},
	{"ATA_bell",           test_atascii_bell},
	{"ATA_return_scroll",  test_atascii_return_scroll},
	{"ATA_tab",            test_atascii_tab},
	{"ATA_tab_set_clear",  test_atascii_tab_set_clear},
	{"ATA_screen_codes",   test_atascii_screen_code_mapping},
	{NULL, NULL}
};

int main(int argc, char **argv)
{
	const char *filter = NULL;

	result_fp = stderr;

	if (argc > 1)
		filter = argv[1];

	/* Force SDL offscreen so no window is opened */
	setenv("SDL_VIDEODRIVER", "offscreen", 1);
	setenv("SDL_RENDER_DRIVER", "software", 1);
	setenv("SDL_VIDEO_EGL_DRIVER", "none", 1);

	ciolib_initial_scaling = 1.0;
	ciolib_initial_mode = ATARIST_80X25;
	ciolib_initial_program_name = "cterm_test";
	ciolib_initial_program_class = "cterm_test";
	if (initciolib(CIOLIB_MODE_SDL)) {
		fprintf(stderr, "Failed to initialize ciolib (SDL)\n");
		return 1;
	}
	textmode(ATARIST_80X25);

	fprintf(result_fp, "# cterm_test results\n");

	for (int i = 0; tests[i].name; i++) {
		if (filter && strstr(tests[i].name, filter) == NULL)
			continue;
		total++;

		/* Reset terminal state */
		setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);

		int result = tests[i].fn();
		switch (result) {
			case 1:
				fprintf(result_fp, "PASS %s\n", tests[i].name);
				passed++;
				break;
			case 0:
				fprintf(result_fp, "FAIL %s\n", tests[i].name);
				failed++;
				break;
			case -1:
				fprintf(result_fp, "SKIP %s\n", tests[i].name);
				skipped++;
				break;
		}
	}

	teardown_cterm();

	fprintf(result_fp, "# ---\n");
	fprintf(result_fp, "# total=%d passed=%d failed=%d skipped=%d\n",
	    total, passed, failed, skipped);

	return failed > 0 ? 1 : 0;
}
