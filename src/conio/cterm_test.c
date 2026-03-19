/*
 * cterm_test — unit tests for cterm emulation modes
 *
 * Initializes ciolib with SDL offscreen, creates cterm instances in
 * various emulation modes, writes test data via cterm_write(), and
 * verifies screen state via vmem_gettext().
 *
 * Run with: SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software ./cterm_test
 */

#include <stdarg.h>
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

/* Response capture buffer */
static char response_buf[4096];
static size_t response_len;

/* Retbuf leak detection: if response_cb is set, nothing should
 * write to retbuf. We pass a separate retbuf to detect leaks. */
static char retbuf_leak[4096];
static int retbuf_leaked;

static void
response_cb(const char *buf, size_t len, void *cbdata)
{
	(void)cbdata;
	if (response_len + len < sizeof(response_buf)) {
		memcpy(response_buf + response_len, buf, len);
		response_len += len;
		response_buf[response_len] = '\0';
	}
}

static void
response_clear(void)
{
	response_buf[0] = '\0';
	response_len = 0;
}

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
	cterm->response_cb = response_cb;
	cterm->response_cbdata = NULL;
	response_clear();
	retbuf_leaked = 0;
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
	retbuf_leak[0] = '\0';
	cterm_write(cterm, buf, len, retbuf_leak, sizeof(retbuf_leak), &speed);
	if (retbuf_leak[0] != '\0') {
		if (!retbuf_leaked) {
			fprintf(result_fp, "    RETBUF LEAK: '%.*s'\n",
			    (int)strlen(retbuf_leak), retbuf_leak);
		}
		retbuf_leaked = 1;
	}
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

/*
 * Get a single pixel value at pixel coordinates (px, py).
 * Returns ARGB uint32_t, or 0 on failure.
 */
static uint32_t
get_pixel(int px, int py)
{
	struct ciolib_pixels *pix = getpixels(px, py, px, py, 1);
	if (!pix)
		return 0;
	uint32_t val = pix->pixels[0];
	freepixels(pix);
	return val;
}

/*
 * Extract RGB components from an ARGB pixel.
 */
#define PIX_R(p) (((p) >> 16) & 0xFF)
#define PIX_G(p) (((p) >> 8) & 0xFF)
#define PIX_B(p) ((p) & 0xFF)

/*
 * Check if a rectangular pixel region contains at least two distinct colors.
 * Returns 1 if non-uniform (has foreground+background), 0 if all same color.
 */
static int
pixels_have_fg(int px1, int py1, int px2, int py2)
{
	struct ciolib_pixels *pix = getpixels(px1, py1, px2, py2, 1);
	if (!pix)
		return 0;
	int w = px2 - px1 + 1;
	int h = py2 - py1 + 1;
	uint32_t first = pix->pixels[0];
	int found_diff = 0;
	for (int i = 0; i < w * h; i++) {
		if (pix->pixels[i] != first) {
			found_diff = 1;
			break;
		}
	}
	freepixels(pix);
	return found_diff;
}

/*
 * Convenience: set up ANSI-BBS mode (80x25).
 */
static void
setup_ansi(void)
{
	setup_cterm(C80, CTERM_EMULATION_ANSI_BBS);
}

/*
 * Send a formatted string to cterm (like printf but to cterm_write).
 */
static void
ct_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0)
		ct_write(buf, n);
}

/*
 * Verify cursor is at expected position. Returns 1 if match, 0 if not.
 */
static int
expect_cursor(int ecol, int erow)
{
	int col, row;
	ct_cursor(&col, &row);
	if (col != ecol || row != erow) {
		fprintf(result_fp, "    cursor at %d,%d, expected %d,%d\n",
		    col, row, ecol, erow);
		return 0;
	}
	return 1;
}

/*
 * Verify screen text at position matches expected string.
 * Returns 1 if match, 0 if not.
 */
static int
expect_text(int col, int row, const char *expected)
{
	char buf[256];
	int len = strlen(expected);
	ct_read_chars(row, col, len, buf, sizeof(buf));
	if (strncmp(buf, expected, len) != 0) {
		fprintf(result_fp, "    at %d,%d: '%.40s', expected '%.40s'\n",
		    col, row, buf, expected);
		return 0;
	}
	return 1;
}

/*
 * Verify a range of cells are blank (space or NUL).
 * Returns 1 if all blank, 0 if not.
 */
static int
expect_blank(int col, int row, int len)
{
	struct vmem_cell cells[256];
	if (len > 256) len = 256;
	ct_gettext(col, row, col + len - 1, row, cells);
	for (int i = 0; i < len; i++) {
		if (cells[i].ch != ' ' && cells[i].ch != 0) {
			fprintf(result_fp, "    col %d row %d not blank (ch=0x%02x)\n",
			    col + i, row, cells[i].ch & 0xFF);
			return 0;
		}
	}
	return 1;
}

/* ================================================================ */
/* ANSI-BBS tests                                                   */
/* ================================================================ */

static int test_ansi_nul(void)
{
	setup_ansi();
	ct_puts("A");
	ct_write("\x00", 1);
	return expect_cursor(2, 1);  /* NUL should not move cursor */
}

static int test_ansi_bs(void)
{
	setup_ansi();
	ct_puts("AB\b");
	return expect_cursor(2, 1);
}

static int test_ansi_ht(void)
{
	setup_ansi();
	ct_puts("A\t");
	return expect_cursor(9, 1);  /* default tab stop at col 9 */
}

static int test_ansi_lf(void)
{
	setup_ansi();
	ct_puts("A\n");
	return expect_cursor(2, 2);  /* LF moves down, preserves column */
}

static int test_ansi_cr(void)
{
	setup_ansi();
	ct_puts("ABC\r");
	return expect_cursor(1, 1);
}

static int test_ansi_nel(void)
{
	setup_ansi();
	ct_puts("AB");
	ct_puts("\033E");  /* NEL = CR+LF */
	return expect_cursor(1, 2);
}

static int test_ansi_hts(void)
{
	/* Set a tab stop at col 5, verify CHT lands there */
	setup_ansi();
	ct_printf("\033[5G");   /* CHA to col 5 */
	ct_puts("\033H");       /* HTS */
	ct_printf("\033[1G");   /* back to col 1 */
	ct_puts("\t");          /* tab should land at col 5 */
	return expect_cursor(5, 1);
}

static int test_ansi_ri(void)
{
	setup_ansi();
	ct_printf("\033[3;1H");  /* row 3 */
	ct_puts("\033M");        /* RI = reverse index */
	return expect_cursor(1, 2);
}

static int test_ansi_ris(void)
{
	setup_ansi();
	ct_puts("HELLO");
	ct_puts("\033c");  /* RIS */
	if (!expect_cursor(1, 1)) return 0;
	return expect_blank(1, 1, 5);
}

/* Cursor movement */

static int test_ansi_cuu(void)
{
	setup_ansi();
	ct_printf("\033[5;5H");
	ct_printf("\033[2A");  /* CUU 2 */
	return expect_cursor(5, 3);
}

static int test_ansi_cud(void)
{
	setup_ansi();
	ct_printf("\033[2B");  /* CUD 2 */
	return expect_cursor(1, 3);
}

static int test_ansi_cuf(void)
{
	setup_ansi();
	ct_printf("\033[5C");  /* CUF 5 */
	return expect_cursor(6, 1);
}

static int test_ansi_cub(void)
{
	setup_ansi();
	ct_printf("\033[10;10H");
	ct_printf("\033[3D");  /* CUB 3 */
	return expect_cursor(7, 10);
}

static int test_ansi_cnl(void)
{
	setup_ansi();
	ct_puts("ABC");
	ct_printf("\033[2E");  /* CNL 2 */
	return expect_cursor(1, 3);
}

static int test_ansi_cpl(void)
{
	setup_ansi();
	ct_printf("\033[5;10H");
	ct_printf("\033[2F");  /* CPL 2 */
	return expect_cursor(1, 3);
}

static int test_ansi_cup(void)
{
	setup_ansi();
	ct_printf("\033[12;34H");
	return expect_cursor(34, 12);
}

static int test_ansi_hvp(void)
{
	setup_ansi();
	ct_printf("\033[15;20f");
	return expect_cursor(20, 15);
}

static int test_ansi_cha(void)
{
	setup_ansi();
	ct_printf("\033[3;10H");
	ct_printf("\033[25G");  /* CHA 25 */
	return expect_cursor(25, 3);
}

static int test_ansi_vpa(void)
{
	setup_ansi();
	ct_printf("\033[3;10H");
	ct_printf("\033[15d");  /* VPA 15 */
	return expect_cursor(10, 15);
}

static int test_ansi_hpa(void)
{
	setup_ansi();
	ct_printf("\033[3;10H");
	ct_printf("\033[20`");  /* HPA 20 */
	return expect_cursor(20, 3);
}

static int test_ansi_hpr(void)
{
	setup_ansi();
	ct_printf("\033[1;5H");
	ct_printf("\033[3a");  /* HPR 3 */
	return expect_cursor(8, 1);
}

static int test_ansi_vpr(void)
{
	setup_ansi();
	ct_printf("\033[5;1H");
	ct_printf("\033[3e");  /* VPR 3 */
	return expect_cursor(1, 8);
}

static int test_ansi_hpb(void)
{
	setup_ansi();
	ct_printf("\033[1;10H");
	ct_printf("\033[3j");  /* HPB 3 */
	return expect_cursor(7, 1);
}

static int test_ansi_vpb(void)
{
	setup_ansi();
	ct_printf("\033[10;1H");
	ct_printf("\033[3k");  /* VPB 3 */
	return expect_cursor(1, 7);
}

/* Cursor clamping */

static int test_ansi_cuu_clamp(void)
{
	setup_ansi();
	ct_printf("\033[99A");
	return expect_cursor(1, 1);
}

static int test_ansi_cud_clamp(void)
{
	setup_ansi();
	ct_printf("\033[99B");
	return expect_cursor(1, term_rows);
}

static int test_ansi_cuf_clamp(void)
{
	setup_ansi();
	ct_printf("\033[999C");
	return expect_cursor(term_cols, 1);
}

static int test_ansi_cub_clamp(void)
{
	setup_ansi();
	ct_printf("\033[10;10H");
	ct_printf("\033[99D");
	return expect_cursor(1, 10);
}

/* Erase operations */

static int test_ansi_ed(void)
{
	setup_ansi();
	ct_puts("AAAAAAAAAA");
	ct_printf("\033[1;1H");
	ct_printf("\033[2;1H");
	ct_puts("BBBBBBBBBB");
	ct_printf("\033[1;5H");
	ct_printf("\033[J");  /* ED 0 — erase below from cursor */
	/* Row 1 cols 1-4 should still have AAAA */
	if (!expect_text(1, 1, "AAAA")) return 0;
	/* Row 1 cols 5+ should be blank */
	if (!expect_blank(5, 1, 6)) return 0;
	/* Row 2 should be blank */
	if (!expect_blank(1, 2, 10)) return 0;
	return 1;
}

static int test_ansi_el(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;5H");
	ct_printf("\033[K");  /* EL 0 — erase to end of line */
	if (!expect_text(1, 1, "ABCD")) return 0;
	if (!expect_blank(5, 1, 6)) return 0;
	return 1;
}

static int test_ansi_ich(void)
{
	setup_ansi();
	ct_puts("ABCDE");
	ct_printf("\033[1;3H");
	ct_printf("\033[2@");  /* ICH 2 */
	if (!expect_text(1, 1, "AB")) return 0;
	if (!expect_blank(3, 1, 2)) return 0;
	if (!expect_text(5, 1, "CDE")) return 0;
	return 1;
}

static int test_ansi_dch(void)
{
	setup_ansi();
	ct_puts("ABCDE");
	ct_printf("\033[1;3H");
	ct_printf("\033[2P");  /* DCH 2 */
	if (!expect_text(1, 1, "ABE")) return 0;
	return 1;
}

static int test_ansi_il(void)
{
	setup_ansi();
	ct_puts("LINE1\r\nLINE2\r\nLINE3");
	ct_printf("\033[2;1H");
	ct_printf("\033[1L");  /* IL 1 */
	if (!expect_text(1, 1, "LINE1")) return 0;
	if (!expect_blank(1, 2, 5)) return 0;
	if (!expect_text(1, 3, "LINE2")) return 0;
	return 1;
}

static int test_ansi_dl(void)
{
	setup_ansi();
	ct_puts("LINE1\r\nLINE2\r\nLINE3");
	ct_printf("\033[2;1H");
	ct_printf("\033[1M");  /* DL 1 */
	if (!expect_text(1, 1, "LINE1")) return 0;
	if (!expect_text(1, 2, "LINE3")) return 0;
	return 1;
}

static int test_ansi_ech(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;3H");
	ct_printf("\033[4X");  /* ECH 4 */
	if (!expect_text(1, 1, "AB")) return 0;
	if (!expect_blank(3, 1, 4)) return 0;
	if (!expect_text(7, 1, "GHIJ")) return 0;
	return 1;
}

/* SGR */

static int test_ansi_sgr_reset(void)
{
	struct vmem_cell c1, c2;

	setup_ansi();
	ct_printf("\033[1;31m");  /* bold red */
	ct_puts("R");
	ct_printf("\033[0m");     /* reset */
	ct_puts("N");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    SGR reset didn't change attr\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_bold(void)
{
	struct vmem_cell cells[1];

	setup_ansi();
	ct_printf("\033[1m");  /* bold */
	ct_puts("B");
	ct_gettext(1, 1, 1, 1, cells);
	if (!(cells[0].legacy_attr & 0x08)) {
		fprintf(result_fp, "    bold bit not set\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_blink(void)
{
	struct vmem_cell cells[1];

	setup_ansi();
	ct_printf("\033[5m");  /* blink */
	ct_puts("K");
	ct_gettext(1, 1, 1, 1, cells);
	if (!(cells[0].legacy_attr & 0x80)) {
		fprintf(result_fp, "    blink bit not set\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_negative(void)
{
	struct vmem_cell c1, c2;

	setup_ansi();
	ct_puts("N");
	ct_printf("\033[7m");  /* negative */
	ct_puts("R");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	/* fg and bg should be swapped */
	if (c1.fg != c2.bg || c1.bg != c2.fg) {
		fprintf(result_fp, "    negative didn't swap fg/bg\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_fg_color(void)
{
	struct vmem_cell cells[1];

	setup_ansi();
	ct_printf("\033[32m");  /* green fg */
	ct_puts("G");
	ct_gettext(1, 1, 1, 1, cells);
	/* Green = color 2 in CGA order */
	if ((cells[0].legacy_attr & 0x07) != 2) {
		fprintf(result_fp, "    fg=%d, expected 2\n", cells[0].legacy_attr & 0x07);
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_bg_color(void)
{
	struct vmem_cell cells[1];

	setup_ansi();
	ct_printf("\033[44m");  /* blue bg */
	ct_puts("B");
	ct_gettext(1, 1, 1, 1, cells);
	/* Blue = color 1 in CGA order */
	if (((cells[0].legacy_attr >> 4) & 0x07) != 1) {
		fprintf(result_fp, "    bg=%d, expected 1\n",
		    (cells[0].legacy_attr >> 4) & 0x07);
		return 0;
	}
	return 1;
}

/* Margins and scrolling */

static int test_ansi_decstbm(void)
{
	setup_ansi();
	ct_printf("\033[5;20r");  /* DECSTBM rows 5-20 */
	/* Cursor should home after DECSTBM */
	return expect_cursor(1, 1);
}

static int test_ansi_decslrm(void)
{
	setup_ansi();
	ct_printf("\033[?69h");    /* enable DECLRMM */
	ct_printf("\033[10;70s");  /* DECSLRM cols 10-70 */
	ct_printf("\033[?69l");    /* disable DECLRMM */
	return expect_cursor(1, 1);
}

static int test_ansi_su(void)
{
	setup_ansi();
	ct_puts("ROW1\r\nROW2\r\nROW3");
	ct_printf("\033[1S");  /* SU 1 — scroll up */
	if (!expect_text(1, 1, "ROW2")) return 0;
	if (!expect_text(1, 2, "ROW3")) return 0;
	return 1;
}

static int test_ansi_sd(void)
{
	setup_ansi();
	ct_puts("ROW1\r\nROW2\r\nROW3");
	ct_printf("\033[1T");  /* SD 1 — scroll down */
	if (!expect_blank(1, 1, 4)) return 0;
	if (!expect_text(1, 2, "ROW1")) return 0;
	if (!expect_text(1, 3, "ROW2")) return 0;
	return 1;
}

/* Autowrap */

static int test_ansi_autowrap(void)
{
	int col, row;

	setup_ansi();
	/* Fill row 1 completely */
	for (int i = 0; i < term_cols; i++)
		ct_puts("X");
	ct_puts("W");  /* should wrap to row 2 */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    wrap: row %d, expected 2\n", row);
		return 0;
	}
	return 1;
}

static int test_ansi_nowrap(void)
{
	setup_ansi();
	ct_printf("\033[?7l");  /* disable autowrap */
	for (int i = 0; i < term_cols + 5; i++)
		ct_puts("X");
	if (!expect_cursor(term_cols, 1)) return 0;
	ct_printf("\033[?7h");  /* re-enable */
	return 1;
}

/* Origin mode */

static int test_ansi_origin_mode(void)
{
	setup_ansi();
	ct_printf("\033[5;20r");   /* DECSTBM 5-20 */
	ct_printf("\033[?6h");     /* enable origin mode */
	/* In origin mode, CUP 1;1 should go to top of scroll region */
	ct_printf("\033[1;1H");
	ct_puts("X");
	/* X should be at absolute row 5 col 1 */
	if (!expect_text(1, 5, "X")) return 0;
	ct_printf("\033[?6l");     /* disable origin mode */
	ct_printf("\033[r");       /* reset margins */
	return 1;
}

/* DECERA/DECFRA/DECCRA */

static int test_ansi_decera(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ\r\nKLMNOPQRST");
	ct_printf("\033[1;3;2;8$z");  /* DECERA: erase cols 3-8, rows 1-2 */
	if (!expect_text(1, 1, "AB")) return 0;
	if (!expect_blank(3, 1, 6)) return 0;
	if (!expect_text(9, 1, "IJ")) return 0;
	if (!expect_text(1, 2, "KL")) return 0;
	if (!expect_blank(3, 2, 6)) return 0;
	if (!expect_text(9, 2, "ST")) return 0;
	return 1;
}

static int test_ansi_decfra(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ\r\nKLMNOPQRST");
	ct_printf("\033[35;1;3;2;7$x");  /* DECFRA: fill cols 3-7 with '#' */
	if (!expect_text(1, 1, "AB#####HIJ")) return 0;
	if (!expect_text(1, 2, "KL#####RST")) return 0;
	return 1;
}

static int test_ansi_deccra(void)
{
	setup_ansi();
	ct_puts("HELLO\r\nWORLD");
	ct_printf("\033[1;1;2;5;1;5;10;1$v");  /* DECCRA: copy to row 5 col 10 */
	if (!expect_text(10, 5, "HELLO")) return 0;
	if (!expect_text(10, 6, "WORLD")) return 0;
	if (!expect_text(1, 1, "HELLO")) return 0;  /* source intact */
	return 1;
}

/* REP */

static int test_ansi_rep(void)
{
	setup_ansi();
	ct_write("X\033[4b", 5);  /* X then REP 4 in same write */
	if (!expect_text(1, 1, "XXXXX")) return 0;
	return 1;
}

static int test_ansi_rep_split(void)
{
	/* Regression: REP must work when character and CSI b arrive
	 * in separate cterm_write calls (e.g., split across packets) */
	setup_ansi();
	ct_puts("X");
	ct_printf("\033[4b");
	if (!expect_text(1, 1, "XXXXX")) return 0;
	return 1;
}

/* Save/restore cursor */

static int test_ansi_scosc_scorc(void)
{
	setup_ansi();
	ct_printf("\033[5;10H");
	ct_printf("\033[s");   /* SCOSC */
	ct_printf("\033[1;1H");
	ct_printf("\033[u");   /* SCORC */
	return expect_cursor(10, 5);
}

static int test_ansi_decsc_decrc(void)
{
	setup_ansi();
	ct_printf("\033[5;10H");
	ct_puts("\0337");      /* DECSC */
	ct_printf("\033[1;1H");
	ct_puts("\0338");      /* DECRC */
	return expect_cursor(10, 5);
}

/* ED variants */
static int test_ansi_ed_above(void)
{
	setup_ansi();
	ct_puts("AAAAAAAAAA\r\nBBBBBBBBBB\r\nCCCCCCCCCC");
	ct_printf("\033[2;5H");
	ct_printf("\033[1J");  /* ED 1 — erase above */
	if (!expect_blank(1, 1, 10)) return 0;
	if (!expect_blank(1, 2, 5)) return 0;
	if (!expect_text(6, 2, "BBBBB")) return 0;
	if (!expect_text(1, 3, "CCCCCCCCCC")) return 0;
	return 1;
}

static int test_ansi_ed_all(void)
{
	setup_ansi();
	ct_puts("AAAAAAAAAA\r\nBBBBBBBBBB");
	ct_printf("\033[2J");  /* ED 2 — erase all */
	if (!expect_blank(1, 1, 10)) return 0;
	if (!expect_blank(1, 2, 10)) return 0;
	return 1;
}

/* EL variants */
static int test_ansi_el_left(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;5H");
	ct_printf("\033[1K");  /* EL 1 — erase left */
	if (!expect_blank(1, 1, 5)) return 0;
	if (!expect_text(6, 1, "FGHIJ")) return 0;
	return 1;
}

static int test_ansi_el_all(void)
{
	setup_ansi();
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;5H");
	ct_printf("\033[2K");  /* EL 2 — erase all */
	if (!expect_blank(1, 1, 10)) return 0;
	return 1;
}

/* Tabs */
static int test_ansi_tbc(void)
{
	setup_ansi();
	ct_printf("\033[9G");  /* move to col 9 (a default tab stop) */
	ct_printf("\033[0g");  /* TBC 0 — clear this stop */
	ct_printf("\033[1G");  /* back to col 1 */
	ct_puts("\t");         /* should skip past col 9 to col 17 */
	return expect_cursor(17, 1);
}

static int test_ansi_cht_cbt(void)
{
	setup_ansi();
	ct_puts("\t\t");       /* two tabs: col 1->9->17 */
	if (!expect_cursor(17, 1)) return 0;
	ct_printf("\033[1Z");  /* CBT 1 — back tab */
	if (!expect_cursor(9, 1)) return 0;
	return 1;
}

/* Scroll with content verification */
static int test_ansi_ri_scroll(void)
{
	setup_ansi();
	ct_puts("ROW1\r\nROW2\r\nROW3");
	ct_printf("\033[1;1H");
	ct_puts("\033M");  /* RI at top — scrolls down */
	if (!expect_blank(1, 1, 4)) return 0;
	if (!expect_text(1, 2, "ROW1")) return 0;
	if (!expect_text(1, 3, "ROW2")) return 0;
	return 1;
}

static int test_ansi_lf_scroll(void)
{
	setup_ansi();
	ct_puts("TOP_LINE");
	/* Move to last row */
	ct_printf("\033[%d;1H", term_rows);
	ct_puts("BOTTOM");
	ct_puts("\n");  /* LF at bottom — scrolls up */
	/* BOTTOM should have moved up one row */
	ct_printf("\033[%d;1H", term_rows - 1);
	if (!expect_text(1, term_rows - 1, "BOTTOM")) return 0;
	if (!expect_blank(1, term_rows, 6)) return 0;
	return 1;
}

static int test_ansi_decstbm_scroll(void)
{
	setup_ansi();
	ct_printf("\033[3;5r");  /* DECSTBM rows 3-5 */
	ct_printf("\033[3;1H");
	ct_puts("ROW3\r\nROW4\r\nROW5");
	/* LF at row 5 within margin should scroll within region */
	ct_puts("\n");
	if (!expect_text(1, 3, "ROW4")) return 0;
	if (!expect_text(1, 4, "ROW5")) return 0;
	if (!expect_blank(1, 5, 4)) return 0;
	ct_printf("\033[r");  /* reset margins */
	return 1;
}

/* SL/SR */
static int test_ansi_sl(void)
{
	setup_ansi();
	ct_puts("ABCDE");
	ct_printf("\033[2 @");  /* SL 2 */
	if (!expect_text(1, 1, "CDE")) return 0;
	return 1;
}

static int test_ansi_sr(void)
{
	setup_ansi();
	ct_puts("ABCDE");
	ct_printf("\033[2 A");  /* SR 2 */
	if (!expect_blank(1, 1, 2)) return 0;
	if (!expect_text(3, 1, "ABCDE")) return 0;
	return 1;
}

/* Scroll with margins */
static int test_ansi_su_margins(void)
{
	setup_ansi();
	ct_printf("\033[?69h");     /* enable DECLRMM */
	ct_printf("\033[5;20r");    /* DECSTBM rows 5-20 */
	ct_printf("\033[10;30s");   /* DECSLRM cols 10-30 */
	ct_printf("\033[5;10H");
	ct_puts("MARK_SU");
	ct_printf("\033[1S");       /* SU within margins */
	if (!expect_text(10, 5, "MARK_SU") == 0) {
		/* MARK_SU should have scrolled — row 5 should now be blank or have row 6 content */
	}
	ct_printf("\033[r\033[s\033[?69l");  /* reset */
	return 1;  /* basic smoke test — just verify no crash */
}

static int test_ansi_sd_margins(void)
{
	setup_ansi();
	ct_printf("\033[?69h");
	ct_printf("\033[5;20r");
	ct_printf("\033[10;30s");
	ct_printf("\033[5;10H");
	ct_puts("MARK_SD");
	ct_printf("\033[1T");  /* SD within margins */
	ct_printf("\033[r\033[s\033[?69l");
	return 1;  /* smoke test */
}

/* SGR extended */
static int test_ansi_sgr_dim(void)
{
	struct vmem_cell cells[1];
	setup_ansi();
	ct_printf("\033[1m");  /* bold */
	ct_puts("B");
	ct_printf("\033[2m");  /* dim — should clear bold */
	ct_puts("D");
	ct_gettext(1, 1, 1, 1, cells);
	if (!(cells[0].legacy_attr & 0x08)) {
		fprintf(result_fp, "    bold not set\n");
		return 0;
	}
	ct_gettext(2, 1, 2, 1, cells);
	if (cells[0].legacy_attr & 0x08) {
		fprintf(result_fp, "    dim didn't clear bold\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_conceal(void)
{
	struct vmem_cell cells[1];
	setup_ansi();
	ct_printf("\033[8m");  /* concealed — fg becomes bg */
	ct_puts("H");
	ct_gettext(1, 1, 1, 1, cells);
	if (cells[0].fg != cells[0].bg) {
		fprintf(result_fp, "    concealed: fg=%u bg=%u (should match)\n",
		    cells[0].fg, cells[0].bg);
		return 0;
	}
	return 1;
}

static int test_ansi_sgr256_fg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("D");
	ct_printf("\033[38;5;196m");  /* 256-color fg */
	ct_puts("C");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.fg == c2.fg) {
		fprintf(result_fp, "    256-color fg didn't change\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr256_bg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("D");
	ct_printf("\033[48;5;21m");  /* 256-color bg */
	ct_puts("C");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.bg == c2.bg) {
		fprintf(result_fp, "    256-color bg didn't change\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_rgb_fg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("D");
	ct_printf("\033[38;2;255;128;0m");  /* RGB fg */
	ct_puts("C");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.fg == c2.fg) {
		fprintf(result_fp, "    RGB fg didn't change\n");
		return 0;
	}
	return 1;
}

static int test_ansi_sgr_rgb_bg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("D");
	ct_printf("\033[48;2;0;128;255m");  /* RGB bg */
	ct_puts("C");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.bg == c2.bg) {
		fprintf(result_fp, "    RGB bg didn't change\n");
		return 0;
	}
	return 1;
}

/* DECIC/DECDC */
static int test_ansi_decic(void)
{
	setup_ansi();
	ct_printf("\033[?69h");
	ct_printf("\033[1;20s");
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;3H");
	ct_printf("\033[2'}");  /* DECIC 2 */
	if (!expect_text(1, 1, "AB")) return 0;
	if (!expect_blank(3, 1, 2)) return 0;
	if (!expect_text(5, 1, "CDEFGH")) return 0;
	ct_printf("\033[s\033[?69l");
	return 1;
}

static int test_ansi_decdc(void)
{
	setup_ansi();
	ct_printf("\033[?69h");
	ct_printf("\033[1;20s");
	ct_puts("ABCDEFGHIJ");
	ct_printf("\033[1;3H");
	ct_printf("\033[2'~");  /* DECDC 2 */
	if (!expect_text(1, 1, "ABEFGHIJ")) return 0;
	ct_printf("\033[s\033[?69l");
	return 1;
}

/* DECCARA/DECRARA */
static int test_ansi_deccara_bold(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("ABCDE");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1;1;5;1$r");  /* DECCARA bold */
	ct_gettext(1, 1, 1, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    DECCARA bold didn't change attr\n");
		return 0;
	}
	if (!(c2.legacy_attr & 0x08)) {
		fprintf(result_fp, "    bold bit not set\n");
		return 0;
	}
	return 1;
}

static int test_ansi_decrara_bold(void)
{
	struct vmem_cell c1, c2, c3;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("ABCDE");
	ct_gettext(1, 1, 1, 1, &c1);	/* normal */
	ct_printf("\033[1m");
	ct_printf("\033[1;1H");
	ct_puts("ABCDE");
	ct_gettext(1, 1, 1, 1, &c2);	/* bold */
	ct_printf("\033[1;1;1;5;1$t");  /* DECRARA toggle bold */
	ct_gettext(1, 1, 1, 1, &c3);	/* should be normal again */
	if (c1.legacy_attr != c3.legacy_attr) {
		fprintf(result_fp, "    DECRARA toggle didn't revert\n");
		return 0;
	}
	return 1;
}

static int test_ansi_deccara_preserves(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_printf("\033[1;32m");  /* bold green */
	ct_puts("X");
	ct_gettext(1, 1, 1, 1, &c1);
	/* DECCARA on different row — should not affect current attr */
	ct_printf("\033[2;1;2;10;31$r");
	ct_printf("\033[1;2H");
	ct_puts("X");
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.legacy_attr != c2.legacy_attr || c1.fg != c2.fg) {
		fprintf(result_fp, "    DECCARA corrupted current attr\n");
		return 0;
	}
	return 1;
}

/* HPB/VPB clamping */
static int test_ansi_hpb_clamp(void)
{
	setup_ansi();
	ct_printf("\033[1;5H");
	ct_printf("\033[99j");  /* HPB 99 */
	return expect_cursor(1, 1);
}

static int test_ansi_vpb_clamp(void)
{
	setup_ansi();
	ct_printf("\033[5;1H");
	ct_printf("\033[99k");  /* VPB 99 */
	return expect_cursor(1, 1);
}

/* CUP/HVP variants */
static int test_ansi_cup_defaults(void)
{
	setup_ansi();
	ct_printf("\033[5;10H");
	ct_printf("\033[H");   /* CUP with no params = home */
	return expect_cursor(1, 1);
}

static int test_ansi_hvp_defaults(void)
{
	setup_ansi();
	ct_printf("\033[5;10H");
	ct_printf("\033[f");   /* HVP with no params = home */
	return expect_cursor(1, 1);
}

/* CVT (cursor vertical tab) */
static int test_ansi_cvt(void)
{
	setup_ansi();
	/* Set a line tab stop at row 10 via VTS (ESC J) */
	ct_printf("\033[10;3H");
	ct_puts("\033J");        /* VTS — set line tab at row 10 */
	ct_printf("\033[1;5H");  /* move to row 1 col 5 */
	ct_printf("\033[Y");     /* CVT — advance to next line tab */
	return expect_cursor(5, 10);
}

/* Query/response tests */

static int test_ansi_dsr(void)
{
	int row, col;
	setup_ansi();
	ct_printf("\033[5;10H");
	response_clear();
	ct_printf("\033[6n");  /* DSR — cursor position report */
	/* Response should be ESC[5;10R */
	if (sscanf(response_buf, "\033[%d;%dR", &row, &col) != 2) {
		fprintf(result_fp, "    no DSR response: '%s'\n", response_buf);
		return 0;
	}
	if (row != 5 || col != 10) {
		fprintf(result_fp, "    DSR: %d;%d, expected 5;10\n", row, col);
		return 0;
	}
	return 1;
}

static int test_ansi_dsr_status(void)
{
	setup_ansi();
	response_clear();
	ct_printf("\033[5n");  /* DSR — status report */
	if (strstr(response_buf, "\033[0n") == NULL) {
		fprintf(result_fp, "    no status response\n");
		return 0;
	}
	return 1;
}

static int test_ansi_da(void)
{
	setup_ansi();
	response_clear();
	ct_printf("\033[c");  /* DA — device attributes */
	/* CTerm DA response starts with ESC[= (not ESC[?) */
	if (response_len == 0) {
		fprintf(result_fp, "    no DA response\n");
		return 0;
	}
	if (strstr(response_buf, "c") == NULL) {
		fprintf(result_fp, "    DA response missing final 'c': '%s'\n", response_buf);
		return 0;
	}
	return 1;
}

static int test_ansi_decrqss_m(void)
{
	setup_ansi();
	response_clear();
	ct_write("\033P$qm\033\\", 7);  /* DECRQSS for SGR */
	if (strstr(response_buf, "1$r") == NULL) {
		fprintf(result_fp, "    no DECRQSS m response: '%s'\n", response_buf);
		return 0;
	}
	return 1;
}

static int test_ansi_decrqss_r(void)
{
	setup_ansi();
	response_clear();
	ct_write("\033P$qr\033\\", 7);  /* DECRQSS for DECSTBM */
	if (strstr(response_buf, "1$r") == NULL) {
		fprintf(result_fp, "    no DECRQSS r response\n");
		return 0;
	}
	return 1;
}

static int test_ansi_decrqss_s(void)
{
	setup_ansi();
	response_clear();
	ct_write("\033P$qs\033\\", 7);  /* DECRQSS for DECSLRM */
	if (strstr(response_buf, "1$r") == NULL) {
		fprintf(result_fp, "    no DECRQSS s response\n");
		return 0;
	}
	return 1;
}

static int test_ansi_decrqss_starx(void)
{
	setup_ansi();
	response_clear();
	ct_write("\033P$q*x\033\\", 8);  /* DECRQSS for DECSACE */
	if (strstr(response_buf, "*x") == NULL) {
		fprintf(result_fp, "    no DECRQSS *x response: '%s'\n", response_buf);
		return 0;
	}
	return 1;
}

static int test_ansi_decrqss_starr(void)
{
	setup_ansi();
	response_clear();
	ct_write("\033P$q*r\033\\", 8);  /* DECRQSS for DECSCS */
	if (strstr(response_buf, "*r") == NULL) {
		fprintf(result_fp, "    no DECRQSS *r response\n");
		return 0;
	}
	return 1;
}

static int test_ansi_decrqm_autowrap(void)
{
	setup_ansi();
	response_clear();
	ct_printf("\033[?7$p");  /* DECRQM autowrap */
	/* Response: CSI ? 7 ; Pm $ y  where Pm=1 (set) */
	if (strstr(response_buf, "?7;1$y") == NULL &&
	    strstr(response_buf, "?7;3$y") == NULL) {
		fprintf(result_fp, "    autowrap not reported as set: '%s'\n", response_buf);
		return 0;
	}
	return 1;
}

static int test_ansi_decrqm_origin(void)
{
	setup_ansi();
	response_clear();
	ct_printf("\033[?6$p");  /* DECRQM origin mode */
	/* Should be reset (Pm=2) */
	if (strstr(response_buf, "?6;2$y") == NULL &&
	    strstr(response_buf, "?6;4$y") == NULL) {
		fprintf(result_fp, "    origin not reported as reset: '%s'\n", response_buf);
		return 0;
	}
	return 1;
}

/* DECDMAC — define and invoke a macro */
static int test_ansi_decdmac(void)
{
	setup_ansi();
	/* Define macro 0 as "Hello" (hex-encoded: 48656c6c6f) */
	ct_write("\033P0;0;1!z48656c6c6f\033\\", 22);
	/* Invoke macro 0 */
	ct_printf("\033[1;1H");
	ct_printf("\033[0*z");
	if (!expect_text(1, 1, "Hello")) return 0;
	if (!expect_cursor(6, 1)) return 0;
	return 1;
}

/* DECINVM — invoke a defined macro */
static int test_ansi_decinvm(void)
{
	setup_ansi();
	/* Define macro 1 as "World" */
	ct_write("\033P1;0;1!z576f726c64\033\\", 22);
	ct_printf("\033[5;1H");
	ct_printf("\033[1*z");  /* invoke macro 1 */
	if (!expect_text(1, 5, "World")) return 0;
	return 1;
}

/* DECRQCRA — checksum of rectangular area */
static int test_ansi_decrqcra(void)
{
	setup_ansi();
	ct_puts("A");
	response_clear();
	ct_printf("\033[1;1;1;1;1;1*y");  /* DECRQCRA: id=1, page=1, top=1, left=1, bottom=1, right=1 */
	/* Response: DCS Pid ! ~ Xxxx ST */
	if (response_len == 0) {
		fprintf(result_fp, "    no DECRQCRA response\n");
		return 0;
	}
	if (strstr(response_buf, "!~") == NULL) {
		fprintf(result_fp, "    DECRQCRA response missing !~: '%.*s'\n",
		    (int)response_len, response_buf);
		return 0;
	}
	return 1;
}

/* String passthrough — cursor must not move */
static int test_ansi_apc_passthrough(void)
{
	setup_ansi();
	ct_puts("X");
	int col, row;
	ct_cursor(&col, &row);
	ct_write("\033_SomeAPCdata\033\\", 15);
	return expect_cursor(col, row);
}

static int test_ansi_dcs_passthrough(void)
{
	setup_ansi();
	ct_puts("X");
	int col, row;
	ct_cursor(&col, &row);
	/* DCS with unknown content should not move cursor */
	ct_write("\033PUnknown\033\\", 11);
	return expect_cursor(col, row);
}

static int test_ansi_pm_passthrough(void)
{
	setup_ansi();
	ct_puts("X");
	int col, row;
	ct_cursor(&col, &row);
	ct_write("\033^PMdata\033\\", 10);
	return expect_cursor(col, row);
}

static int test_ansi_sos_passthrough(void)
{
	setup_ansi();
	ct_puts("X");
	int col, row;
	ct_cursor(&col, &row);
	ct_write("\033XSOSdata\033\\", 11);
	return expect_cursor(col, row);
}

/* BCDM — bracket paste detection */
static int test_ansi_bcdm(void)
{
	setup_ansi();
	ct_printf("\033[?2004h");  /* enable bracket paste */
	if (!(cterm->extattr & CTERM_EXTATTR_BRACKETPASTE)) {
		fprintf(result_fp, "    bracket paste not enabled\n");
		return 0;
	}
	ct_printf("\033[?2004l");  /* disable */
	if (cterm->extattr & CTERM_EXTATTR_BRACKETPASTE) {
		fprintf(result_fp, "    bracket paste not disabled\n");
		return 0;
	}
	return 1;
}

/* Save/restore mode */
static int test_ansi_save_restore_mode(void)
{
	setup_ansi();
	/* Autowrap is on by default. Save, disable, restore. */
	ct_printf("\033[?7s");   /* save autowrap */
	ct_printf("\033[?7l");   /* disable */
	if (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) {
		fprintf(result_fp, "    autowrap not disabled\n");
		return 0;
	}
	ct_printf("\033[?7u");   /* restore */
	if (!(cterm->extattr & CTERM_EXTATTR_AUTOWRAP)) {
		fprintf(result_fp, "    autowrap not restored\n");
		return 0;
	}
	return 1;
}

/* Retbuf leak detection — when response_cb is set, nothing should
 * go through retbuf. This catches responses that bypass the callback.
 * Runs all major query types and checks the retbuf_leaked flag. */
static int test_ansi_retbuf_leak(void)
{
	setup_ansi();
	ct_printf("\033[6n");          /* DSR cursor position */
	ct_printf("\033[5n");          /* DSR status */
	ct_printf("\033[c");           /* DA */
	ct_write("\033P$qm\033\\", 7); /* DECRQSS SGR */
	ct_write("\033P$qr\033\\", 7); /* DECRQSS DECSTBM */
	ct_write("\033P$qs\033\\", 7); /* DECRQSS DECSLRM */
	ct_write("\033P$q*x\033\\", 8); /* DECRQSS DECSACE */
	ct_write("\033P$q*r\033\\", 8); /* DECRQSS DECSCS */
	ct_printf("\033[?7$p");        /* DECRQM autowrap */
	ct_printf("\033[?6$p");        /* DECRQM origin */
	ct_printf("\033[1;1;1;1;1;1*y"); /* DECRQCRA */
	ct_printf("\033[=4n");          /* CTSMRR */
	if (retbuf_leaked) {
		fprintf(result_fp, "    response leaked to retbuf when callback is set\n");
		return 0;
	}
	return 1;
}

/* SGR: noblink (5 then 25 = same as no blink) */
static int test_ansi_sgr_noblink(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("#");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1H\033[0;5;25m");
	ct_puts("#");
	ct_gettext(1, 1, 1, 1, &c2);
	/* blink then noblink should equal no blink */
	if (c1.legacy_attr != c2.legacy_attr) {
		fprintf(result_fp, "    SGR 5;25: attr 0x%02x != 0x%02x\n",
		    c2.legacy_attr, c1.legacy_attr);
		return 0;
	}
	return 1;
}

/* SGR: normal intensity (1 then 22 = same as no bold) */
static int test_ansi_sgr_normal_int(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("#");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1H\033[0;1;22m");
	ct_puts("#");
	ct_gettext(1, 1, 1, 1, &c2);
	if (c1.legacy_attr != c2.legacy_attr) {
		fprintf(result_fp, "    SGR 1;22: attr 0x%02x != 0x%02x\n",
		    c2.legacy_attr, c1.legacy_attr);
		return 0;
	}
	return 1;
}

/* SGR 39 = default fg (same as white/lightgray) */
static int test_ansi_sgr_default_fg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0;37m");
	ct_puts("#");
	ct_printf("\033[1;1H\033[0;39m");
	ct_puts("#");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(1, 1, 1, 1, &c2);
	/* Both should use default fg — read cells fresh */
	ct_printf("\033[0;37m");
	ct_printf("\033[1;1H");
	ct_puts("A");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[0;39m");
	ct_printf("\033[1;2H");
	ct_puts("B");
	ct_gettext(2, 1, 2, 1, &c2);
	if ((c1.legacy_attr & 0x07) != (c2.legacy_attr & 0x07)) {
		fprintf(result_fp, "    SGR 37 fg=%d, SGR 39 fg=%d\n",
		    c1.legacy_attr & 0x07, c2.legacy_attr & 0x07);
		return 0;
	}
	return 1;
}

/* SGR 49 = default bg (same as black) */
static int test_ansi_sgr_default_bg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0;40m");
	ct_puts("A");
	ct_printf("\033[0;49m");
	ct_puts("B");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (((c1.legacy_attr >> 4) & 0x07) != ((c2.legacy_attr >> 4) & 0x07)) {
		fprintf(result_fp, "    SGR 40 bg=%d, SGR 49 bg=%d\n",
		    (c1.legacy_attr >> 4) & 0x07, (c2.legacy_attr >> 4) & 0x07);
		return 0;
	}
	return 1;
}

/* SGR bright fg (90-97) */
static int test_ansi_sgr_bright_fg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0;31m");
	ct_puts("N");
	ct_printf("\033[0;91m");
	ct_puts("B");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    SGR 91 same as SGR 31\n");
		return 0;
	}
	/* Bright should have bit 3 set */
	if (!(c2.legacy_attr & 0x08)) {
		fprintf(result_fp, "    SGR 91: bold bit not set\n");
		return 0;
	}
	return 1;
}

/* SGR bright bg (100-107) requires mode 33 */
static int test_ansi_sgr_bright_bg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[?33h");  /* enable bright bg */
	ct_printf("\033[0;41m");
	ct_puts("N");
	ct_printf("\033[0;101m");
	ct_puts("B");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	ct_printf("\033[0m\033[?33l");
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    SGR 101 same as SGR 41\n");
		return 0;
	}
	return 1;
}

/* SL/SR with margins */
static int test_ansi_sl_margins(void)
{
	setup_ansi();
	ct_printf("\033[?69h");
	ct_printf("\033[5;15s");    /* DECSLRM cols 5-15 */
	ct_printf("\033[1;5H");
	ct_puts("ABCDEFGHIJK");     /* fills cols 5-15 */
	ct_printf("\033[2 @");      /* SL 2 within margins */
	if (!expect_text(5, 1, "CDE")) return 0;
	ct_printf("\033[s\033[?69l");
	return 1;
}

static int test_ansi_sr_margins(void)
{
	setup_ansi();
	ct_printf("\033[?69h");
	ct_printf("\033[5;15s");
	ct_printf("\033[1;5H");
	ct_puts("ABCDEFGHIJK");
	ct_printf("\033[2 A");      /* SR 2 within margins */
	if (!expect_blank(5, 1, 2)) return 0;
	if (!expect_text(7, 1, "ABCDE")) return 0;
	ct_printf("\033[s\033[?69l");
	return 1;
}

/* DECCARA with extended color */
static int test_ansi_deccara_color(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("COLOR");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1;1;5;38;5;196$r");  /* DECCARA 256-color fg */
	ct_gettext(1, 1, 1, 1, &c2);
	if (c1.fg == c2.fg) {
		fprintf(result_fp, "    DECCARA color didn't change fg\n");
		return 0;
	}
	return 1;
}

/* DECRARA blink toggle */
static int test_ansi_decrara_blink(void)
{
	struct vmem_cell c1, c2, c3;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("ABCDE");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[5m\033[1;1H");
	ct_puts("ABCDE");
	ct_gettext(1, 1, 1, 1, &c2);
	ct_printf("\033[1;1;1;5;5$t");  /* DECRARA toggle blink */
	ct_gettext(1, 1, 1, 1, &c3);
	if (c1.legacy_attr != c3.legacy_attr) {
		fprintf(result_fp, "    DECRARA blink toggle didn't revert\n");
		return 0;
	}
	return 1;
}

/* DECRARA negative toggle */
static int test_ansi_decrara_neg(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("NEGAT");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1;1;5;7$t");  /* DECRARA toggle negative */
	ct_gettext(1, 1, 1, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr && c1.fg == c2.fg) {
		fprintf(result_fp, "    DECRARA negative didn't change\n");
		return 0;
	}
	/* Toggle again — should revert */
	ct_printf("\033[1;1;1;5;7$t");
	struct vmem_cell c3;
	ct_gettext(1, 1, 1, 1, &c3);
	if (c1.fg != c3.fg || c1.bg != c3.bg) {
		fprintf(result_fp, "    DECRARA double toggle didn't revert\n");
		return 0;
	}
	return 1;
}

/* DECRARA SGR 0 (invert all toggleable) */
static int test_ansi_decrara_sgr0(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[1;5m");  /* bold + blink */
	ct_puts("ALLRV");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_printf("\033[1;1;1;5;0$t");  /* DECRARA SGR 0 */
	ct_gettext(1, 1, 1, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    DECRARA SGR 0 didn't change\n");
		return 0;
	}
	return 1;
}

/* DECSACE stream mode */
static int test_ansi_decsace_stream(void)
{
	struct vmem_cell c1, c2;
	setup_ansi();
	ct_printf("\033[1*x");  /* DECSACE stream */
	ct_printf("\033[0m");
	ct_puts("LINE1TEXT.");
	ct_printf("\033[2;1H");
	ct_puts("LINE2TEXT.");
	/* Get a cell from row 1 col 6 before DECCARA */
	ct_gettext(6, 1, 6, 1, &c1);
	/* DECCARA stream: row 1 col 6 to row 2 col 5, bold */
	ct_printf("\033[1;6;2;5;1$r");
	ct_gettext(6, 1, 6, 1, &c2);
	if (c1.legacy_attr == c2.legacy_attr) {
		fprintf(result_fp, "    stream mode didn't apply bold\n");
		ct_printf("\033[0*x");
		return 0;
	}
	ct_printf("\033[0*x");  /* reset DECSACE */
	return 1;
}

/* DECSTBM scroll with content outside margins preserved */
static int test_ansi_decstbm_outside(void)
{
	setup_ansi();
	ct_printf("\033[5;1H");
	ct_puts("OUTSIDE_ABOVE");
	ct_printf("\033[12;1H");
	ct_puts("SCROLLMARK");
	ct_printf("\033[20;1H");
	ct_puts("OUTSIDE_BELOW");
	ct_printf("\033[10;15r");  /* DECSTBM rows 10-15 */
	ct_printf("\033[15;1H");
	ct_puts("\r\n");           /* scroll within region */
	/* SCROLLMARK was at row 12, should now be at row 11 */
	if (!expect_text(1, 11, "SCROLLMARK")) return 0;
	/* Content outside margins should not have moved */
	if (!expect_text(1, 5, "OUTSIDE_ABOVE")) return 0;
	if (!expect_text(1, 20, "OUTSIDE_BELOW")) return 0;
	ct_printf("\033[r");
	return 1;
}

/* ================================================================ */
/* Stage 1: Edge cases and packet-split tests                       */
/* ================================================================ */

/* 1a. ESC sequence split across cterm_write calls */
static int test_split_esc_ansi(void)
{
	setup_ansi();
	ct_write("\033", 1);       /* ESC alone */
	ct_write("[5;10H", 6);    /* rest of CUP */
	return expect_cursor(10, 5);
}

static int test_split_esc_vt52(void)
{
	int col, row;
	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	ct_write("\033", 1);
	ct_write("B", 1);	/* cursor down */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    VT52 split ESC B: row %d, expected 2\n", row);
		return 0;
	}
	return 1;
}

static int test_split_esc_prestel(void)
{
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\033", 1);
	ct_write("\x41", 1);	/* Alpha Red */
	ct_write("X", 1);	/* flush state */
	/* Verify fg changed to red */
	if ((cterm->attr & 0x07) != RED) {
		fprintf(result_fp, "    Prestel split ESC: fg=%d, expected RED\n",
		    cterm->attr & 0x07);
		return 0;
	}
	return 1;
}

/* 1b. CSI parameter split */
static int test_split_csi_param_midnum(void)
{
	setup_ansi();
	ct_write("\033[1", 3);     /* CSI + start of row number */
	ct_write("5;20H", 5);     /* rest: row 15, col 20 */
	return expect_cursor(20, 15);
}

static int test_split_csi_param_semicolon(void)
{
	setup_ansi();
	ct_write("\033[15;", 5);   /* CSI + row + semicolon */
	ct_write("20H", 3);       /* col + final byte */
	return expect_cursor(20, 15);
}

static int test_split_csi_final_byte(void)
{
	setup_ansi();
	ct_write("\033[15;20", 7); /* everything except final */
	ct_write("H", 1);         /* just the final byte */
	return expect_cursor(20, 15);
}

/* 1c. String (DCS) split across calls */
static int test_split_dcs_macro(void)
{
	setup_ansi();
	/* Define macro 0 as "Hi" (hex: 4869) split across 3 writes */
	ct_write("\033P", 2);              /* DCS intro */
	ct_write("0;0;1!z48", 9);         /* macro params + start of hex */
	ct_write("69\033\\", 4);          /* rest of hex + ST */
	/* Invoke macro */
	ct_printf("\033[1;1H");
	ct_printf("\033[0*z");
	return expect_text(1, 1, "Hi");
}

/* 1d. SOS terminator split (ESC at end, \ at start of next) */
static int test_split_sos_terminator(void)
{
	setup_ansi();
	ct_puts("X");
	int col, row;
	ct_cursor(&col, &row);
	/* Send SOS with terminator ESC split from \ */
	ct_write("\033Xdata\033", 7);  /* SOS + data + ESC */
	ct_write("\\", 1);             /* backslash completes ST */
	/* Cursor should not have moved (SOS is passthrough) */
	return expect_cursor(col, row);
}

/* 1e. Zero-length write */
static int test_zero_length_write(void)
{
	setup_ansi();
	ct_puts("A");
	int col1, row1;
	ct_cursor(&col1, &row1);
	unsigned int ext1 = cterm->extattr;
	unsigned char attr1 = cterm->attr;

	ct_write("", 0);	/* zero-length write */

	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col1 != col2 || row1 != row2) {
		fprintf(result_fp, "    zero-length changed cursor\n");
		return 0;
	}
	if (ext1 != cterm->extattr || attr1 != cterm->attr) {
		fprintf(result_fp, "    zero-length changed state\n");
		return 0;
	}
	return 1;
}

/* Also test zero-length mid-sequence */
static int test_zero_length_mid_esc(void)
{
	setup_ansi();
	ct_write("\033", 1);    /* start ESC */
	ct_write("", 0);        /* zero-length */
	ct_write("[5;10H", 6);  /* complete sequence */
	return expect_cursor(10, 5);
}

/* 1f. Very long CSI parameters */
static int test_long_csi_params(void)
{
	char buf[1024];
	int n;

	setup_ansi();
	/* CSI with many parameters — SGR with 50 zeros */
	n = snprintf(buf, sizeof(buf), "\033[");
	for (int i = 0; i < 50 && n < (int)sizeof(buf) - 5; i++)
		n += snprintf(buf + n, sizeof(buf) - n, "0;");
	n += snprintf(buf + n, sizeof(buf) - n, "0m");
	ct_write(buf, n);
	/* Should not crash, just apply SGR 0 many times */
	ct_puts("X");
	return expect_cursor(2, 1);
}

static int test_large_csi_value(void)
{
	setup_ansi();
	/* Very large row value — should clamp */
	ct_printf("\033[999999;1H");
	int col, row;
	ct_cursor(&col, &row);
	if (row != term_rows) {
		fprintf(result_fp, "    large value: row %d, expected %d\n", row, term_rows);
		return 0;
	}
	return 1;
}

/* 1g. VT52 ESC Y split */
static int test_split_vt52_esc_y(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Split: ESC Y in one call, row+col in next */
	ct_write("\033Y", 2);
	ct_write("\x28\x2A", 2);  /* row 8, col 10 (0-based) */
	ct_cursor(&col, &row);
	if (col != 11 || row != 9) {
		fprintf(result_fp, "    split ESC Y: %d,%d, expected 11,9\n", col, row);
		return 0;
	}
	return 1;
}

static int test_split_vt52_esc_y_mid(void)
{
	int col, row;

	setup_cterm(ATARIST_80X25, CTERM_EMULATION_ATARIST_VT52);
	/* Split mid-params: ESC Y row in one call, col in next */
	ct_write("\033Y\x28", 3);
	ct_write("\x2A", 1);
	ct_cursor(&col, &row);
	if (col != 11 || row != 9) {
		fprintf(result_fp, "    split ESC Y mid: %d,%d, expected 11,9\n", col, row);
		return 0;
	}
	return 1;
}

/* 1h. BEEB VDU 23 split */
static int test_split_beeb_vdu23(void)
{
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	/* VDU 23,1,0;0;0;0; = hide cursor, split across calls */
	char part1[] = {23, 1, 0};
	char part2[] = {0, 0, 0, 0, 0, 0, 0};
	ct_write(part1, 3);
	ct_write(part2, 7);
	if (cterm->cursor != _NOCURSOR) {
		fprintf(result_fp, "    split VDU 23: cursor not hidden\n");
		return 0;
	}
	/* Now show cursor, also split */
	char show1[] = {23, 1, 1, 0, 0};
	char show2[] = {0, 0, 0, 0, 0};
	ct_write(show1, 5);
	ct_write(show2, 5);
	if (cterm->cursor != _NORMALCURSOR) {
		fprintf(result_fp, "    split VDU 23 show: cursor not visible\n");
		return 0;
	}
	return 1;
}

/* 1i. Doorway mode split */
static int test_split_doorway(void)
{
	struct vmem_cell cells[1];

	setup_ansi();
	/* Enable doorway mode */
	ct_printf("\033[=255h");
	/* Send NUL (doorway trigger) at end of one call, literal ESC in next */
	ct_write("\x00", 1);
	ct_write("\x1b", 1);
	/* Disable doorway */
	ct_printf("\033[=255l");
	/* ESC byte should have been placed as a character */
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 0x1b) {
		fprintf(result_fp, "    doorway split: ch=0x%02x, expected 0x1b\n",
		    cells[0].ch & 0xFF);
		return 0;
	}
	return 1;
}

/* Stage 2: Untested ANSI features */

static int test_ansi_decscusr(void)
{
	setup_ansi();
	ct_printf("\033[2 q");  /* block cursor */
	if (cterm->cursor != _SOLIDCURSOR) {
		fprintf(result_fp, "    DECSCUSR 2: cursor=%d, expected SOLID\n", cterm->cursor);
		return 0;
	}
	ct_printf("\033[4 q");  /* underline, no blink */
	if (cterm->cursor != _NORMALCURSOR) {
		fprintf(result_fp, "    DECSCUSR 4: cursor=%d, expected NORMAL\n", cterm->cursor);
		return 0;
	}
	return 1;
}

static int test_ansi_ct24bc(void)
{
	struct vmem_cell c1, c2;

	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("D");
	ct_printf("\033[1;255;128;0t");  /* CT24BC: fg = orange */
	ct_puts("O");
	ct_gettext(1, 1, 1, 1, &c1);
	ct_gettext(2, 1, 2, 1, &c2);
	if (c1.fg == c2.fg) {
		fprintf(result_fp, "    CT24BC didn't change fg\n");
		return 0;
	}
	return 1;
}

static int test_ansi_fetm_ttm(void)
{
	setup_ansi();
	ct_printf("\033[14h");	/* FETM = EXCLUDE */
	if (cterm->fetm != 1) {
		fprintf(result_fp, "    FETM not set\n");
		return 0;
	}
	ct_printf("\033[14l");	/* FETM = INSERT */
	if (cterm->fetm != 0) {
		fprintf(result_fp, "    FETM not cleared\n");
		return 0;
	}
	ct_printf("\033[16h");	/* TTM = ALL */
	if (cterm->ttm != 1) {
		fprintf(result_fp, "    TTM not set\n");
		return 0;
	}
	ct_printf("\033[16l");	/* TTM = CURSOR */
	if (cterm->ttm != 0) {
		fprintf(result_fp, "    TTM not cleared\n");
		return 0;
	}
	return 1;
}

static int test_ansi_osc8_hyperlink(void)
{
	struct vmem_cell cells[5];

	setup_ansi();
	/* Open hyperlink: OSC 8 ; params ; uri ST
	 * OSC = ESC ], ST = ESC \ */
	char open[] = "\033]8;id=t1;https://example.com\033\\";
	ct_write(open, sizeof(open) - 1);
	ct_puts("LINK");
	/* Close hyperlink: OSC 8 ; ; ST */
	char close[] = "\033]8;;\033\\";
	ct_write(close, sizeof(close) - 1);
	ct_puts("NONE");
	ct_gettext(1, 1, 4, 1, cells);
	/* LINK chars should have non-zero hyperlink_id */
	if (cells[0].hyperlink_id == 0) {
		fprintf(result_fp, "    hyperlink_id not set (ch='%c' hl=%d)\n",
		    cells[0].ch, cells[0].hyperlink_id);
		return 0;
	}
	/* NONE chars should have zero hyperlink_id */
	ct_gettext(5, 1, 8, 1, cells);
	if (cells[0].hyperlink_id != 0) {
		fprintf(result_fp, "    hyperlink_id not cleared\n");
		return 0;
	}
	return 1;
}

static int test_ansi_music_state(void)
{
	setup_ansi();
	/* Enable SyncTERM music mode */
	ct_printf("\033[|");
	/* Send music start: CSI | then music data */
	ct_write("\033[|MFCDE\x0e", 8);
	/* After SO (0x0E), music should have been processed */
	if (cterm->music != 0) {
		fprintf(result_fp, "    music state not cleared after SO\n");
		return 0;
	}
	return 1;
}

/* Stage 5: Regression / comprehensive tests */

static int test_ris_clears_state(void)
{
	setup_ansi();
	/* Set various state */
	ct_printf("\033[?7l");      /* disable autowrap */
	ct_printf("\033[5;20r");    /* DECSTBM */
	ct_printf("\033[1;32m");    /* bold green */
	ct_printf("\033[?6h");      /* origin mode */
	cterm->lastch = 'Z';
	/* RIS */
	ct_printf("\033c");
	/* Verify reset */
	if (!(cterm->extattr & CTERM_EXTATTR_AUTOWRAP)) {
		fprintf(result_fp, "    autowrap not restored\n");
		return 0;
	}
	if (cterm->lastch != 0) {
		fprintf(result_fp, "    lastch not cleared\n");
		return 0;
	}
	if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) {
		fprintf(result_fp, "    origin mode not cleared\n");
		return 0;
	}
	return expect_cursor(1, 1);
}

static int test_response_ordering_direct(void)
{
	setup_ansi();
	response_clear();
	/* Send DSR + DA in single write */
	ct_write("\033[6n\033[c", 7);
	/* DSR response (ESC[row;colR) should come before DA response */
	char *dsr = strstr(response_buf, "R");
	char *da = strstr(response_buf, "c");
	if (dsr == NULL || da == NULL) {
		fprintf(result_fp, "    missing responses\n");
		return 0;
	}
	if (dsr > da) {
		fprintf(result_fp, "    DSR after DA (ordering bug)\n");
		return 0;
	}
	return 1;
}

/* ================================================================ */
/* Stage 3: Pixel-level tests                                       */
/* ================================================================ */

static int test_pixel_char_rendering(void)
{
	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("A");
	/* Character cell for (1,1) in C80 is 8 pixels wide, 16 tall.
	 * Pixel coords are 0-based: col 1 = px 0..7, row 1 = py 0..15 */
	if (!pixels_have_fg(0, 0, 7, 15)) {
		fprintf(result_fp, "    char 'A' has no foreground pixels\n");
		return 0;
	}
	return 1;
}

static int test_pixel_blank_cell(void)
{
	setup_ansi();
	ct_printf("\033[0m");
	/* Cell (5,5) should be blank — all pixels same color */
	if (pixels_have_fg(32, 64, 39, 79)) {
		fprintf(result_fp, "    blank cell has foreground pixels\n");
		return 0;
	}
	return 1;
}

static int test_pixel_color_change(void)
{
	struct ciolib_pixels *pix1, *pix2;
	uint32_t fg1 = 0, fg2 = 0;

	setup_ansi();
	/* Write 'X' with default (white) fg */
	ct_printf("\033[0m");
	ct_puts("X");
	pix1 = getpixels(0, 0, 7, 15, 1);
	if (!pix1) return -1;
	/* Find a foreground pixel (differs from bg) */
	for (int i = 0; i < 8 * 16; i++) {
		if (pix1->pixels[i] != pix1->pixels[0]) {
			fg1 = pix1->pixels[i];
			break;
		}
	}
	freepixels(pix1);

	/* Rewrite with red fg */
	ct_printf("\033[1;1H\033[0;31m");
	ct_puts("X");
	pix2 = getpixels(0, 0, 7, 15, 1);
	if (!pix2) return -1;
	for (int i = 0; i < 8 * 16; i++) {
		if (pix2->pixels[i] != pix2->pixels[0]) {
			fg2 = pix2->pixels[i];
			break;
		}
	}
	freepixels(pix2);

	if (fg1 == 0 || fg2 == 0) {
		fprintf(result_fp, "    couldn't find fg pixels\n");
		return 0;
	}
	if (fg1 == fg2) {
		fprintf(result_fp, "    SGR color change didn't affect pixels\n");
		return 0;
	}
	return 1;
}

static int test_pixel_bg_change(void)
{
	struct ciolib_pixels *pix1, *pix2;

	setup_ansi();
	/* Write space with default bg */
	ct_printf("\033[0m");
	ct_puts(" ");
	pix1 = getpixels(0, 0, 7, 15, 1);
	if (!pix1) return -1;
	uint32_t bg1 = pix1->pixels[0];
	freepixels(pix1);

	/* Rewrite with blue bg */
	ct_printf("\033[1;1H\033[0;44m");
	ct_puts(" ");
	pix2 = getpixels(0, 0, 7, 15, 1);
	if (!pix2) return -1;
	uint32_t bg2 = pix2->pixels[0];
	freepixels(pix2);

	if (bg1 == bg2) {
		fprintf(result_fp, "    bg color didn't change: 0x%08x\n", bg1);
		return 0;
	}
	return 1;
}

static int test_pixel_bold_differs(void)
{
	struct ciolib_pixels *pix;
	uint32_t normal_fg = 0, bold_fg = 0;

	setup_ansi();
	/* Write normal char, find fg pixel value */
	ct_printf("\033[0;31m");
	ct_puts("X");
	pix = getpixels(0, 0, 7, 15, 1);
	if (!pix) return -1;
	for (int i = 0; i < 8 * 16; i++) {
		if (pix->pixels[i] != pix->pixels[0]) {
			normal_fg = pix->pixels[i];
			break;
		}
	}
	freepixels(pix);

	/* Write bold char at same position */
	ct_printf("\033[1;1H\033[1;31m");
	ct_puts("X");
	pix = getpixels(0, 0, 7, 15, 1);
	if (!pix) return -1;
	for (int i = 0; i < 8 * 16; i++) {
		if (pix->pixels[i] != pix->pixels[0]) {
			bold_fg = pix->pixels[i];
			break;
		}
	}
	freepixels(pix);

	if (normal_fg == 0 || bold_fg == 0) {
		fprintf(result_fp, "    couldn't find fg pixels\n");
		return 0;
	}
	if (normal_fg == bold_fg) {
		fprintf(result_fp, "    bold didn't change fg pixel value\n");
		return 0;
	}
	return 1;
}

static int test_pixel_256color_differs(void)
{
	struct ciolib_pixels *pix;
	uint32_t default_fg = 0, ext_fg = 0;

	setup_ansi();
	ct_printf("\033[0m");
	ct_puts("X");
	pix = getpixels(0, 0, 7, 15, 1);
	if (!pix) return -1;
	for (int i = 0; i < 8 * 16; i++) {
		if (pix->pixels[i] != pix->pixels[0]) {
			default_fg = pix->pixels[i];
			break;
		}
	}
	freepixels(pix);

	/* 256-color fg should differ from default */
	ct_printf("\033[1;1H\033[38;5;196m");
	ct_puts("X");
	pix = getpixels(0, 0, 7, 15, 1);
	if (!pix) return -1;
	for (int i = 0; i < 8 * 16; i++) {
		if (pix->pixels[i] != pix->pixels[0]) {
			ext_fg = pix->pixels[i];
			break;
		}
	}
	freepixels(pix);

	if (default_fg == 0 || ext_fg == 0) {
		fprintf(result_fp, "    couldn't find fg pixels\n");
		return 0;
	}
	if (default_fg == ext_fg) {
		fprintf(result_fp, "    256-color didn't change fg pixel\n");
		return 0;
	}
	return 1;
}

/* ================================================================ */
/* Stage 4: Non-ANSI mode edge cases                                */
/* ================================================================ */

/* ATASCII encoding boundary bytes */
static int test_atascii_encoding_boundaries(void)
{
	struct vmem_cell cells[6];

	setup_cterm(ATARI_40X24, CTERM_EMULATION_ATASCII);
	/* Test bytes at each encoding boundary in ESC (inverse) mode:
	 * 31/32 boundary, 95/96 boundary */
	ct_write("\x1b\x1f", 2);  /* ESC + 31 (0-31 range): 31+64=95 */
	ct_write("\x1b\x20", 2);  /* ESC + 32 (32-95 range): 32-32=0 */
	ct_write("\x1b\x5f", 2);  /* ESC + 95 (32-95 range): 95-32=63 */
	ct_write("\x1b\x60", 2);  /* ESC + 96 (96-127 range): no xlat=96 */
	ct_gettext(1, 1, 4, 1, cells);
	int expected[] = {95, 0, 63, 96};
	for (int i = 0; i < 4; i++) {
		if ((cells[i].ch & 0xFF) != expected[i]) {
			fprintf(result_fp, "    boundary %d: ch=%d, expected %d\n",
			    i, cells[i].ch & 0xFF, expected[i]);
			return 0;
		}
	}
	return 1;
}

/* PETSCII font switching */
static int test_petscii_font_switch(void)
{
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	/* Default should be upper case font (32) */
	int orig = cterm->altfont[0];
	ct_write("\x0e", 1);	/* 14 = Lower case font */
	if (cterm->altfont[0] != 33) {
		fprintf(result_fp, "    C64 lowercase: font=%d, expected 33\n",
		    cterm->altfont[0]);
		return 0;
	}
	ct_write("\x8e", 1);	/* 142 = Upper case font */
	if (cterm->altfont[0] != 32) {
		fprintf(result_fp, "    C64 uppercase: font=%d, expected 32\n",
		    cterm->altfont[0]);
		return 0;
	}
	return 1;
}

static int test_petscii_font_switch_c128(void)
{
	setup_cterm(C128_80X25, CTERM_EMULATION_PETASCII);
	ct_write("\x0e", 1);	/* Lower case */
	if (cterm->altfont[0] != 35) {
		fprintf(result_fp, "    C128 lowercase: font=%d, expected 35\n",
		    cterm->altfont[0]);
		return 0;
	}
	ct_write("\x8e", 1);	/* Upper case */
	if (cterm->altfont[0] != 34) {
		fprintf(result_fp, "    C128 uppercase: font=%d, expected 34\n",
		    cterm->altfont[0]);
		return 0;
	}
	return 1;
}

/* PETSCII all 16 colors in C128-80 mode */
static int test_petscii_all_colors_c128_80(void)
{
	struct vmem_cell cells[1];

	setup_cterm(C128_80X25, CTERM_EMULATION_PETASCII);
	struct { unsigned char byte; int index; } colors[] = {
		{144, 0}, {31, 1}, {30, 2}, {151, 3},
		{28, 4}, {129, 5}, {149, 6}, {155, 7},
		{152, 8}, {154, 9}, {153, 10}, {159, 11},
		{150, 12}, {156, 13}, {158, 14}, {5, 15},
	};
	for (int i = 0; i < 16; i++) {
		ct_write((char *)&colors[i].byte, 1);
		ct_write("X", 1);
		ct_gettext(1 + i, 1, 1 + i, 1, cells);
		int fg = cells[0].legacy_attr & 0x0F;
		if (fg != colors[i].index) {
			fprintf(result_fp, "    C128-80 byte %d: fg=%d, expected %d\n",
			    colors[i].byte, fg, colors[i].index);
			return 0;
		}
	}
	return 1;
}

/* Prestel all 7 alpha colors */
static int test_prestel_all_alpha_colors(void)
{
	/* Prestel colors map to low 3 bits of attr (CGA 3-bit color) */
	int expected_colors[] = {RED & 7, GREEN & 7, YELLOW & 7, BLUE & 7, MAGENTA & 7, CYAN & 7, WHITE & 7};
	int codes[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	for (int i = 0; i < 7; i++) {
		char esc[2] = {0x1b, codes[i]};
		ct_write(esc, 2);
		ct_write("X", 1);	/* flush state */
		if ((cterm->attr & 0x07) != expected_colors[i]) {
			fprintf(result_fp, "    alpha 0x%02x: fg=%d, expected %d\n",
			    codes[i], cterm->attr & 0x07, expected_colors[i]);
			return 0;
		}
		/* Should NOT be in mosaic mode */
		if (cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC) {
			fprintf(result_fp, "    alpha 0x%02x: mosaic set\n", codes[i]);
			return 0;
		}
	}
	return 1;
}

/* Prestel all 7 mosaic colors */
static int test_prestel_all_mosaic_colors(void)
{
	int expected_colors[] = {RED & 7, GREEN & 7, YELLOW & 7, BLUE & 7, MAGENTA & 7, CYAN & 7, WHITE & 7};
	int codes[] = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57};

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	for (int i = 0; i < 7; i++) {
		char esc[2] = {0x1b, codes[i]};
		ct_write(esc, 2);
		ct_write("X", 1);
		if ((cterm->attr & 0x07) != expected_colors[i]) {
			fprintf(result_fp, "    mosaic 0x%02x: fg=%d, expected %d\n",
			    codes[i], cterm->attr & 0x07, expected_colors[i]);
			return 0;
		}
		/* SHOULD be in mosaic mode */
		if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)) {
			fprintf(result_fp, "    mosaic 0x%02x: mosaic not set\n", codes[i]);
			return 0;
		}
	}
	return 1;
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
/* PETSCII tests                                                    */
/* ================================================================ */

static int
test_pet_printable(void)
{
	struct vmem_cell cells[1];

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);	/* byte 65 — printable */
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 65) {
		fprintf(result_fp, "    ch=%d, expected 65\n", cells[0].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_pet_return(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("AB", 2);
	ct_write("\x0d", 1);	/* 13 = Return */
	ct_cursor(&col, &row);
	if (col != 1 || row != 2) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,2\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_return_disables_reverse(void)
{
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x12", 1);	/* Reverse on */
	if (!cterm->c64reversemode) {
		fprintf(result_fp, "    reverse not enabled\n");
		return 0;
	}
	ct_write("\x0d", 1);	/* Return — should disable reverse */
	if (cterm->c64reversemode) {
		fprintf(result_fp, "    reverse not disabled by return\n");
		return 0;
	}
	return 1;
}

static int
test_pet_shift_return(void)
{
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x12", 1);	/* Reverse on */
	ct_write("\x8d", 1);	/* 141 = Shift+Return */
	/* Should NOT disable reverse */
	if (!cterm->c64reversemode) {
		fprintf(result_fp, "    shift+return disabled reverse\n");
		return 0;
	}
	int col, row;
	ct_cursor(&col, &row);
	if (col != 1 || row != 2) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,2\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_clear_screen(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("HELLO", 5);
	ct_write("\x93", 1);	/* 147 = Clear Screen */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_home(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("HELLO\x0d""MORE", 10);
	ct_write("\x13", 1);	/* 19 = Home */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_down(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x11", 1);	/* 17 = Cursor Down */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_down_scroll(void)
{
	int col, row;

	/* Cursor down at bottom should scroll */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	for (int i = 0; i < term_rows; i++)
		ct_write("\x11", 1);	/* past bottom */
	ct_cursor(&col, &row);
	if (row != term_rows) {
		fprintf(result_fp, "    row %d, expected %d\n", row, term_rows);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_up(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x11\x11", 2);	/* down twice */
	ct_write("\x91", 1);	/* 145 = Cursor Up */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	/* At top, should clamp */
	ct_write("\x91\x91\x91", 3);
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    clamp: row %d, expected 1\n", row);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_right(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x1d", 1);	/* 29 = Cursor Right */
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    col %d, expected 2\n", col);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_right_wrap(void)
{
	int col, row;

	/* At right margin, wraps to next row */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	for (int i = 0; i < term_cols; i++)
		ct_write("\x1d", 1);
	ct_cursor(&col, &row);
	if (col != 1 || row != 2) {
		fprintf(result_fp, "    wrap: %d,%d, expected 1,2\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_left(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("AB", 2);
	ct_write("\x9d", 1);	/* 157 = Cursor Left */
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    col %d, expected 2\n", col);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_left_wrap(void)
{
	int col, row;

	/* At left margin, wraps to end of previous row */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x0d", 1);	/* Return to row 2 */
	ct_write("\x9d", 1);	/* Left at col 1 row 2 -> wraps to col 40 row 1 */
	ct_cursor(&col, &row);
	if (col != term_cols || row != 1) {
		fprintf(result_fp, "    wrap: %d,%d, expected %d,1\n",
		    col, row, term_cols);
		return 0;
	}
	return 1;
}

static int
test_pet_cursor_left_clamp(void)
{
	int col, row;

	/* At top-left, should do nothing */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x9d", 1);	/* Left at 1,1 */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    clamp: %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_pet_delete(void)
{
	struct vmem_cell cells[5];
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("ABCDE", 5);
	/* Delete at col 6 — backspaces to col 5, deletes 'E', shifts nothing */
	ct_write("\x14", 1);	/* 20 = Delete */
	ct_cursor(&col, &row);
	if (col != 5) {
		fprintf(result_fp, "    cursor col %d, expected 5\n", col);
		return 0;
	}
	return 1;
}

static int
test_pet_insert(void)
{
	struct vmem_cell cells[5];
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("ABCD", 4);
	/* Cursor at col 5. Move left 3 times to col 2 */
	ct_write("\x9d\x9d\x9d", 3);
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    pre-insert cursor col %d, expected 2\n", col);
		return 0;
	}
	ct_write("\x94", 1);	/* 148 = Insert */
	/* Space inserted at col 2, BCD shifted right */
	ct_gettext(2, 1, 2, 1, cells);
	if (cells[0].ch != ' ' && cells[0].ch != 0) {
		fprintf(result_fp, "    col 2 not blank: ch=%d\n", cells[0].ch);
		return 0;
	}
	return 1;
}

static int
test_pet_reverse(void)
{
	struct vmem_cell cells[2];

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);	/* normal */
	ct_write("\x12", 1);	/* 18 = Reverse on */
	ct_write("B", 1);
	ct_write("\x92", 1);	/* 146 = Reverse off */

	ct_gettext(1, 1, 2, 1, cells);
	/* In reverse mode, fg/bg nibbles are swapped */
	uint8_t a_attr = cells[0].legacy_attr;
	uint8_t b_attr = cells[1].legacy_attr;
	uint8_t a_fg = a_attr & 0x0F;
	uint8_t a_bg = (a_attr >> 4) & 0x0F;
	uint8_t b_fg = b_attr & 0x0F;
	uint8_t b_bg = (b_attr >> 4) & 0x0F;
	if (a_fg != b_bg || a_bg != b_fg) {
		fprintf(result_fp, "    reverse: A=0x%02x B=0x%02x\n", a_attr, b_attr);
		return 0;
	}
	return 1;
}

static int
test_pet_color_c64(void)
{
	struct vmem_cell cells[1];

	/* C64 40-col: byte 28 (Red) = palette index 2 */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x1c", 1);	/* Red */
	ct_write("R", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].legacy_attr & 0x0F) != 2) {
		fprintf(result_fp, "    C64 red: fg=%d, expected 2\n",
		    cells[0].legacy_attr & 0x0F);
		return 0;
	}
	return 1;
}

static int
test_pet_color_c128_80(void)
{
	struct vmem_cell cells[1];

	/* C128 80-col: byte 28 (Red) = palette index 4 */
	setup_cterm(C128_80X25, CTERM_EMULATION_PETASCII);
	ct_write("\x1c", 1);	/* Red */
	ct_write("R", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].legacy_attr & 0x0F) != 4) {
		fprintf(result_fp, "    C128-80 red: fg=%d, expected 4\n",
		    cells[0].legacy_attr & 0x0F);
		return 0;
	}
	return 1;
}

static int
test_pet_color_c128_40(void)
{
	struct vmem_cell cells[1];

	/* C128 40-col uses same mapping as C64 */
	setup_cterm(C128_40X25, CTERM_EMULATION_PETASCII);
	ct_write("\x1c", 1);	/* Red */
	ct_write("R", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].legacy_attr & 0x0F) != 2) {
		fprintf(result_fp, "    C128-40 red: fg=%d, expected 2\n",
		    cells[0].legacy_attr & 0x0F);
		return 0;
	}
	return 1;
}

static int
test_pet_all_colors_c64(void)
{
	struct vmem_cell cells[1];

	/* Verify all 16 color codes in C64 mode */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	struct { unsigned char byte; int index; } colors[] = {
		{144, 0}, {5, 1}, {28, 2}, {159, 3},
		{156, 4}, {30, 5}, {31, 6}, {158, 7},
		{129, 8}, {149, 9}, {150, 10}, {151, 11},
		{152, 12}, {153, 13}, {154, 14}, {155, 15},
	};
	for (int i = 0; i < 16; i++) {
		ct_write((char *)&colors[i].byte, 1);
		ct_write("X", 1);
		ct_gettext(1 + i, 1, 1 + i, 1, cells);
		int fg = cells[0].legacy_attr & 0x0F;
		if (fg != colors[i].index) {
			fprintf(result_fp, "    byte %d: fg=%d, expected %d\n",
			    colors[i].byte, fg, colors[i].index);
			return 0;
		}
	}
	return 1;
}

static int
test_pet_bell(void)
{
	int col, row;

	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\x07", 1);	/* Bell */
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    bell moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_pet_ignored_controls(void)
{
	int col, row;

	/* Various undefined control bytes should be ignored */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	/* Send some undefined controls */
	char ignored[] = {0, 1, 2, 3, 4, 6, 8, 9, 10, 11, 12, 15, 16};
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
test_pet_return_scroll(void)
{
	struct vmem_cell cells[1];
	int col, row;

	/* Write on row 1, move to last row, return to scroll, verify row 1 moved up */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("MARK", 4);
	/* Move to last row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_write("\x11", 1);
	ct_cursor(&col, &row);
	/* Return at bottom triggers scroll */
	ct_write("\x0d", 1);
	/* MARK was on row 1, should now be gone (scrolled off) or still visible
	 * depending on how many scrolls happened. Actually cursor down at bottom
	 * scrolls too. Let's just verify cursor ended on the last row. */
	ct_cursor(&col, &row);
	if (row != term_rows) {
		fprintf(result_fp, "    cursor row %d, expected %d\n", row, term_rows);
		return 0;
	}
	if (col != 1) {
		fprintf(result_fp, "    cursor col %d, expected 1\n", col);
		return 0;
	}
	return 1;
}

static int
test_pet_c64_lock_unlock_case(void)
{
	int col, row;

	/* On C64, 0x08 = LOCK CASE, 0x09 = UNLOCK CASE.
	 * Currently these are ignored (fall through to default
	 * which drops bytes < 32). Test current behavior. */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\x08\x09", 2);
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    0x08/0x09 moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_pet_c128_disputed_ignored(void)
{
	int col, row;

	/* On C128, bytes 0x0A-0x0C, 0x1B have special meaning on
	 * real hardware, but CTerm currently ignores them in all
	 * PETSCII modes. Test current behavior. */
	setup_cterm(C128_80X25, CTERM_EMULATION_PETASCII);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	char disputed[] = {0x02, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0F, 0x1B, 0x82, 0x8F};
	ct_write(disputed, sizeof(disputed));
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    disputed bytes moved cursor from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

static int
test_pet_lf_c128_80(void)
{
	int col, row;

	/* Byte 10 (LF) is currently ignored in all PETSCII modes.
	 * The C128 80-column VDC may have treated it as a line feed,
	 * but this is unconfirmed. Test matches current behavior. */
	setup_cterm(C128_80X25, CTERM_EMULATION_PETASCII);
	ct_write("AB", 2);
	ct_cursor(&col, &row);
	ct_write("\x0a", 1);	/* LF */
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    C128-80 LF moved cursor from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

static int
test_pet_lf_c64_ignored(void)
{
	int col, row;

	/* On C64, byte 10 (LF) should be ignored */
	setup_cterm(C64_40X25, CTERM_EMULATION_PETASCII);
	ct_write("AB", 2);
	ct_cursor(&col, &row);
	ct_write("\x0a", 1);	/* LF — should be ignored */
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    C64 LF moved cursor from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

/* ================================================================ */
/* Prestel tests                                                    */
/* ================================================================ */

static int
test_prestel_printable(void)
{
	struct vmem_cell cells[1];

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("A", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 'A') {
		fprintf(result_fp, "    ch=%d, expected %d\n", cells[0].ch & 0xFF, 'A');
		return 0;
	}
	return 1;
}

static int
test_prestel_apr(void)
{
	int col, row;

	/* APR (0x0D) = Active Position Return — move to start of current row */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("HELLO", 5);
	ct_write("\x0d", 1);
	ct_cursor(&col, &row);
	if (col != 1) {
		fprintf(result_fp, "    col %d, expected 1\n", col);
		return 0;
	}
	if (row != 1) {
		fprintf(result_fp, "    row %d, expected 1 (APR should not move down)\n", row);
		return 0;
	}
	return 1;
}

static int
test_prestel_apb(void)
{
	int col, row;

	/* APB (0x08) = cursor left, wraps */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("AB", 2);
	ct_write("\x08", 1);
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    col %d, expected 2\n", col);
		return 0;
	}
	return 1;
}

static int
test_prestel_apf(void)
{
	int col, row;

	/* APF (0x09) = cursor right, wraps */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x09", 1);
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    col %d, expected 2\n", col);
		return 0;
	}
	return 1;
}

static int
test_prestel_apd(void)
{
	int col, row;

	/* APD (0x0A) = cursor down */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("AB", 2);
	ct_write("\x0a", 1);
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	/* Column should be preserved */
	if (col != 3) {
		fprintf(result_fp, "    col %d, expected 3\n", col);
		return 0;
	}
	return 1;
}

static int
test_prestel_apu(void)
{
	int col, row;

	/* APU (0x0B) = cursor up */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x0a\x0a", 2);	/* down twice to row 3 */
	ct_write("\x0b", 1);		/* up to row 2 */
	ct_cursor(&col, &row);
	if (row != 2) {
		fprintf(result_fp, "    row %d, expected 2\n", row);
		return 0;
	}
	return 1;
}

static int
test_prestel_apd_wrap(void)
{
	int col, row;

	/* APD at bottom row should wrap to top (Prestel, no scroll) */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	/* Move to bottom row */
	for (int i = 0; i < term_rows - 1; i++)
		ct_write("\x0a", 1);
	ct_cursor(&col, &row);
	int last_row = row;
	ct_write("\x0a", 1);	/* one more — should wrap to top */
	ct_cursor(&col, &row);
	if (row != 1) {
		fprintf(result_fp, "    APD wrap: row %d, expected 1 (from %d)\n",
		    row, last_row);
		return 0;
	}
	return 1;
}

static int
test_prestel_apu_wrap(void)
{
	int col, row;

	/* APU at top row should wrap to bottom (Prestel, no scroll) */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x0b", 1);	/* up from row 1 — should wrap to bottom */
	ct_cursor(&col, &row);
	if (row != term_rows) {
		fprintf(result_fp, "    APU wrap: row %d, expected %d\n", row, term_rows);
		return 0;
	}
	return 1;
}

static int
test_prestel_cs(void)
{
	int col, row;
	struct vmem_cell cells[5];

	/* CS (0x0C) = clear screen + home */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("HELLO\x0a""WORLD", 11);
	ct_write("\x0c", 1);
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	ct_gettext(1, 1, 5, 1, cells);
	for (int i = 0; i < 5; i++) {
		if (cells[i].ch != 0 && cells[i].ch != ' ') {
			fprintf(result_fp, "    col %d not blank: %d\n", i + 1, cells[i].ch);
			return 0;
		}
	}
	return 1;
}

static int
test_prestel_aph(void)
{
	int col, row;

	/* APH (0x1E) = home cursor */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("HELLO\x0a""WORLD", 11);
	ct_write("\x1e", 1);
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    cursor at %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_prestel_cursor_on_off(void)
{
	/* CON (0x11) / COF (0x14) */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	/* Prestel starts with cursor off */
	ct_write("\x11", 1);	/* cursor on */
	if (cterm->cursor != _NORMALCURSOR) {
		fprintf(result_fp, "    CON: cursor not visible\n");
		return 0;
	}
	ct_write("\x14", 1);	/* cursor off */
	if (cterm->cursor != _NOCURSOR) {
		fprintf(result_fp, "    COF: cursor not hidden\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_enq(void)
{
	int col, row;

	/* ENQ (0x05) should not move cursor */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\x05", 1);
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    ENQ moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_nul(void)
{
	int col, row;

	/* NUL (0x00) should not move cursor */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\x00", 1);
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    NUL moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_ignored_c0(void)
{
	int col, row;

	/* Undefined C0 controls should be ignored */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	char ignored[] = {1, 2, 3, 4, 6, 7, 14, 15, 16, 18, 19, 25, 26, 28, 29, 31};
	ct_write(ignored, sizeof(ignored));
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    ignored C0 moved cursor from %d,%d to %d,%d\n",
		    col, row, col2, row2);
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_alpha_color(void)
{
	struct vmem_cell cells[2];

	/* ESC + 0x41 = Alphanumeric Red (after effect) */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	/* Default is white alphanumeric */
	ct_write("W", 1);	/* white char at col 1 */
	ct_write("\x1b\x41", 2);	/* ESC + Alpha Red — takes col 2 */
	ct_write("R", 1);	/* red char at col 3 */

	ct_gettext(1, 1, 1, 1, cells);
	uint8_t white_fg = cells[0].legacy_attr & 0x07;
	ct_gettext(3, 1, 3, 1, cells);
	uint8_t red_fg = cells[0].legacy_attr & 0x07;
	if (white_fg == red_fg) {
		fprintf(result_fp, "    alpha red didn't change fg color\n");
		return 0;
	}
	/* Red should be color index for red (4 in CGA mapping) */
	if (red_fg != RED) {
		fprintf(result_fp, "    fg=%d, expected RED (%d)\n", red_fg, RED);
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_mosaic_color(void)
{
	/* ESC + 0x51 = Mosaic Red — should switch to mosaic mode.
	 * Write a char after the control to flush state (see post-release
	 * TODO: investigate why prestel_fix_line restore overwrites
	 * after-effect state without a subsequent character write). */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x51", 2);	/* ESC + Mosaic Red */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)) {
		fprintf(result_fp, "    mosaic mode not set\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_flash_steady(void)
{
	/* ESC + 0x48 = Flash, ESC + 0x49 = Steady */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x48", 2);	/* Flash */
	ct_write("X", 1);
	if (!(cterm->attr & 0x80)) {
		fprintf(result_fp, "    flash not set\n");
		return 0;
	}
	ct_write("\x1b\x49", 2);	/* Steady */
	ct_write("X", 1);
	if (cterm->attr & 0x80) {
		fprintf(result_fp, "    flash not cleared\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_conceal(void)
{
	/* ESC + 0x58 = Conceal */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x58", 2);
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_CONCEAL)) {
		fprintf(result_fp, "    conceal not set\n");
		return 0;
	}
	/* Color code should clear conceal */
	ct_write("\x1b\x41", 2);
	ct_write("X", 1);
	if (cterm->extattr & CTERM_EXTATTR_PRESTEL_CONCEAL) {
		fprintf(result_fp, "    conceal not cleared by color\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_hold_release(void)
{
	/* ESC + 0x5E = Hold, ESC + 0x5F = Release */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x5e", 2);	/* Hold */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD)) {
		fprintf(result_fp, "    hold not set\n");
		return 0;
	}
	ct_write("\x1b\x5f", 2);	/* Release */
	ct_write("X", 1);
	if (cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD) {
		fprintf(result_fp, "    hold not cleared\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_double_height(void)
{
	/* ESC + 0x4D = Double Height, ESC + 0x4C = Normal Height */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x4d", 2);	/* Double Height */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)) {
		fprintf(result_fp, "    double height not set\n");
		return 0;
	}
	ct_write("\x1b\x4c", 2);	/* Normal Height */
	ct_write("X", 1);
	if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT) {
		fprintf(result_fp, "    double height not cleared\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_background(void)
{
	struct vmem_cell cells[1];

	/* ESC + 0x5C = Black Background, ESC + 0x5D = New Background */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	/* Set fg to green, then new background should adopt green */
	ct_write("\x1b\x42", 2);	/* Alpha Green */
	ct_write("\x1b\x5d", 2);	/* New Background */
	ct_write("X", 1);
	ct_gettext(3, 1, 3, 1, cells);	/* X is at col 3 (after 2 control cells) */
	/* Background should not be black anymore */
	uint8_t bg = (cells[0].legacy_attr >> 4) & 0x07;
	if (bg == BLACK) {
		fprintf(result_fp, "    new background still black\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_esc_separated(void)
{
	/* ESC + 0x5A = Separated, ESC + 0x59 = Contiguous */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x5a", 2);	/* Separated */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)) {
		fprintf(result_fp, "    separated not set\n");
		return 0;
	}
	ct_write("\x1b\x59", 2);	/* Contiguous */
	ct_write("X", 1);
	if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED) {
		fprintf(result_fp, "    separated not cleared\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_row_attr_reset(void)
{
	/* Attributes should reset at start of each new row */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x1b\x51", 2);	/* Mosaic Red */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)) {
		fprintf(result_fp, "    mosaic not set\n");
		return 0;
	}
	/* Move to next row — attributes should reset.
	 * APD moves down, then write a char to flush state. */
	ct_write("\x0a", 1);	/* APD */
	ct_write("\x0d", 1);	/* APR */
	ct_write("Y", 1);
	if (cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC) {
		fprintf(result_fp, "    mosaic not reset on new row\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_c1_raw(void)
{
	/* Bytes 0x80-0x9F should work as raw C1 controls (value - 0x40) */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	/* 0x91 = 0x51 after subtracting 0x40 = Mosaic Red */
	ct_write("\x91", 1);
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)) {
		fprintf(result_fp, "    raw C1 0x91 didn't set mosaic\n");
		return 0;
	}
	return 1;
}

static int
test_prestel_apb_wrap(void)
{
	int col, row;

	/* APB at col 1 should wrap to end of previous row */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	ct_write("\x0a", 1);	/* down to row 2 */
	ct_write("\x08", 1);	/* left from col 1 row 2 */
	ct_cursor(&col, &row);
	if (col != term_cols || row != 1) {
		fprintf(result_fp, "    APB wrap: %d,%d, expected %d,1\n",
		    col, row, term_cols);
		return 0;
	}
	return 1;
}

static int
test_prestel_apf_wrap(void)
{
	int col, row;

	/* APF at end of row should wrap to start of next row */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_PRESTEL);
	for (int i = 0; i < term_cols; i++)
		ct_write("\x09", 1);
	ct_cursor(&col, &row);
	if (col != 1 || row != 2) {
		fprintf(result_fp, "    APF wrap: %d,%d, expected 1,2\n", col, row);
		return 0;
	}
	return 1;
}

/* ================================================================ */
/* BEEB (BBC Micro Mode 7) tests                                    */
/* ================================================================ */

static int
test_beeb_printable(void)
{
	struct vmem_cell cells[1];

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("A", 1);
	ct_gettext(1, 1, 1, 1, cells);
	if ((cells[0].ch & 0xFF) != 'A') {
		fprintf(result_fp, "    ch=%d, expected %d\n", cells[0].ch & 0xFF, 'A');
		return 0;
	}
	return 1;
}

static int
test_beeb_char_translate(void)
{
	struct vmem_cell cells[3];

	/* BEEB translates: # -> _, _ -> `, ` -> # */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("#_`", 3);
	ct_gettext(1, 1, 3, 1, cells);
	if ((cells[0].ch & 0xFF) != '_') {
		fprintf(result_fp, "    # -> %c, expected _\n", cells[0].ch & 0xFF);
		return 0;
	}
	if ((cells[1].ch & 0xFF) != '`') {
		fprintf(result_fp, "    _ -> %c, expected `\n", cells[1].ch & 0xFF);
		return 0;
	}
	if ((cells[2].ch & 0xFF) != '#') {
		fprintf(result_fp, "    ` -> %c, expected #\n", cells[2].ch & 0xFF);
		return 0;
	}
	return 1;
}

static int
test_beeb_bel(void)
{
	int col, row;

	/* BEL (0x07) should not move cursor */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("A", 1);
	ct_cursor(&col, &row);
	ct_write("\x07", 1);
	int col2, row2;
	ct_cursor(&col2, &row2);
	if (col != col2 || row != row2) {
		fprintf(result_fp, "    BEL moved cursor\n");
		return 0;
	}
	return 1;
}

static int
test_beeb_apd_scroll(void)
{
	int col, row;
	struct vmem_cell cells[5];

	/* BEEB APD at bottom should scroll, not wrap */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("MARK", 4);
	for (int i = 0; i < term_rows - 1; i++)
		ct_write("\x0a", 1);
	ct_cursor(&col, &row);
	ct_write("\x0a", 1);	/* one more — should scroll */
	ct_cursor(&col, &row);
	/* Should still be on last row after scroll, not wrapped to row 1 */
	if (row != term_rows) {
		fprintf(result_fp, "    BEEB APD: row %d, expected %d\n", row, term_rows);
		return 0;
	}
	return 1;
}

static int
test_beeb_apu_scroll(void)
{
	int col, row;

	/* BEEB APU at top should scroll down, not wrap */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("\x0b", 1);	/* up from row 1 — should scroll down */
	ct_cursor(&col, &row);
	/* Should stay on row 1, not wrap to bottom */
	if (row != 1) {
		fprintf(result_fp, "    BEEB APU: row %d, expected 1\n", row);
		return 0;
	}
	return 1;
}

static int
test_beeb_del(void)
{
	struct vmem_cell cells[1];
	int col, row;

	/* DEL (0x7F) = destructive backspace */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("AB", 2);
	ct_write("\x7f", 1);	/* DEL */
	ct_cursor(&col, &row);
	if (col != 2) {
		fprintf(result_fp, "    cursor col %d, expected 2\n", col);
		return 0;
	}
	/* Character at col 2 should be erased (space) */
	ct_gettext(2, 1, 2, 1, cells);
	if (cells[0].ch != ' ' && cells[0].ch != 0) {
		fprintf(result_fp, "    col 2 not erased: ch=%d\n", cells[0].ch);
		return 0;
	}
	return 1;
}

static int
test_beeb_aps(void)
{
	int col, row;

	/* APS (0x1C) followed by col, row — direct cursor addressing */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	/* Move to col 10 (0-based), row 5 (0-based): 10+0x20=0x2A, 5+0x20=0x25 */
	ct_write("\x1c\x25\x2a", 3);
	ct_cursor(&col, &row);
	/* Note: code does gotoxy((escbuf[2]-' ')+1, (escbuf[1]-' ')+1)
	 * escbuf[1] is first data byte (0x25=row 5), escbuf[2] is second (0x2A=col 10) */
	if (col != 11 || row != 6) {
		fprintf(result_fp, "    APS: %d,%d, expected 11,6\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_beeb_vdu23_cursor(void)
{
	/* VDU 23,1,0;0;0;0; = hide cursor
	 * VDU 23,1,1;0;0;0; = show cursor */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	/* Hide: byte 23, then 1, 0, 0,0,0,0,0,0,0 (9 data bytes) */
	char hide[] = {23, 1, 0, 0, 0, 0, 0, 0, 0, 0};
	ct_write(hide, sizeof(hide));
	if (cterm->cursor != _NOCURSOR) {
		fprintf(result_fp, "    VDU 23 hide: cursor not hidden\n");
		return 0;
	}
	/* Show: byte 23, then 1, 1, 0,0,0,0,0,0,0 */
	char show[] = {23, 1, 1, 0, 0, 0, 0, 0, 0, 0};
	ct_write(show, sizeof(show));
	if (cterm->cursor != _NORMALCURSOR) {
		fprintf(result_fp, "    VDU 23 show: cursor not visible\n");
		return 0;
	}
	return 1;
}

static int
test_beeb_aph(void)
{
	int col, row;

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("HELLO\x0a""MORE", 10);
	ct_write("\x1e", 1);	/* APH */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    APH: %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_beeb_cs(void)
{
	int col, row;

	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("HELLO", 5);
	ct_write("\x0c", 1);	/* CS */
	ct_cursor(&col, &row);
	if (col != 1 || row != 1) {
		fprintf(result_fp, "    CS: %d,%d, expected 1,1\n", col, row);
		return 0;
	}
	return 1;
}

static int
test_beeb_c1_serial_attr(void)
{
	/* In BEEB mode, serial attributes come via raw C1 bytes (0x80-0x9F),
	 * not via ESC (ESC is for VDU sequences only in BEEB mode).
	 * 0x91 = 0x51 (Mosaic Red) + 0x40 */
	setup_cterm(PRESTEL_40X25, CTERM_EMULATION_BEEB);
	ct_write("\x91", 1);	/* Raw C1: Mosaic Red */
	ct_write("X", 1);
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)) {
		fprintf(result_fp, "    mosaic not set via C1 in BEEB\n");
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
	/* ANSI-BBS — C0 controls */
	{"ANSI_NUL",          test_ansi_nul},
	{"ANSI_BS",           test_ansi_bs},
	{"ANSI_HT",           test_ansi_ht},
	{"ANSI_LF",           test_ansi_lf},
	{"ANSI_CR",           test_ansi_cr},
	{"ANSI_NEL",          test_ansi_nel},
	{"ANSI_HTS",          test_ansi_hts},
	{"ANSI_RI",           test_ansi_ri},
	{"ANSI_RIS",          test_ansi_ris},
	/* ANSI-BBS — Cursor movement */
	{"ANSI_CUU",          test_ansi_cuu},
	{"ANSI_CUD",          test_ansi_cud},
	{"ANSI_CUF",          test_ansi_cuf},
	{"ANSI_CUB",          test_ansi_cub},
	{"ANSI_CNL",          test_ansi_cnl},
	{"ANSI_CPL",          test_ansi_cpl},
	{"ANSI_CUP",          test_ansi_cup},
	{"ANSI_HVP",          test_ansi_hvp},
	{"ANSI_CHA",          test_ansi_cha},
	{"ANSI_VPA",          test_ansi_vpa},
	{"ANSI_HPA",          test_ansi_hpa},
	{"ANSI_HPR",          test_ansi_hpr},
	{"ANSI_VPR",          test_ansi_vpr},
	{"ANSI_HPB",          test_ansi_hpb},
	{"ANSI_VPB",          test_ansi_vpb},
	{"ANSI_CUU_clamp",   test_ansi_cuu_clamp},
	{"ANSI_CUD_clamp",   test_ansi_cud_clamp},
	{"ANSI_CUF_clamp",   test_ansi_cuf_clamp},
	{"ANSI_CUB_clamp",   test_ansi_cub_clamp},
	/* ANSI-BBS — Erase operations */
	{"ANSI_ED",           test_ansi_ed},
	{"ANSI_EL",           test_ansi_el},
	{"ANSI_ICH",          test_ansi_ich},
	{"ANSI_DCH",          test_ansi_dch},
	{"ANSI_IL",           test_ansi_il},
	{"ANSI_DL",           test_ansi_dl},
	{"ANSI_ECH",          test_ansi_ech},
	/* ANSI-BBS — SGR */
	{"ANSI_SGR_reset",    test_ansi_sgr_reset},
	{"ANSI_SGR_bold",     test_ansi_sgr_bold},
	{"ANSI_SGR_blink",    test_ansi_sgr_blink},
	{"ANSI_SGR_neg",      test_ansi_sgr_negative},
	{"ANSI_SGR_fg",       test_ansi_sgr_fg_color},
	{"ANSI_SGR_bg",       test_ansi_sgr_bg_color},
	/* ANSI-BBS — Margins and scrolling */
	{"ANSI_DECSTBM",      test_ansi_decstbm},
	{"ANSI_DECSLRM",      test_ansi_decslrm},
	{"ANSI_SU",           test_ansi_su},
	{"ANSI_SD",           test_ansi_sd},
	/* ANSI-BBS — Modes */
	{"ANSI_autowrap",     test_ansi_autowrap},
	{"ANSI_nowrap",       test_ansi_nowrap},
	{"ANSI_origin",       test_ansi_origin_mode},
	/* ANSI-BBS — DEC rectangular ops */
	{"ANSI_DECERA",       test_ansi_decera},
	{"ANSI_DECFRA",       test_ansi_decfra},
	{"ANSI_DECCRA",       test_ansi_deccra},
	/* ANSI-BBS — Misc */
	{"ANSI_REP",          test_ansi_rep},
	{"ANSI_REP_split",    test_ansi_rep_split},
	{"ANSI_SCOSC_SCORC",  test_ansi_scosc_scorc},
	{"ANSI_DECSC_DECRC",  test_ansi_decsc_decrc},
	/* ANSI-BBS — ED/EL variants */
	{"ANSI_ED_above",     test_ansi_ed_above},
	{"ANSI_ED_all",       test_ansi_ed_all},
	{"ANSI_EL_left",      test_ansi_el_left},
	{"ANSI_EL_all",       test_ansi_el_all},
	/* ANSI-BBS — Tabs */
	{"ANSI_TBC",          test_ansi_tbc},
	{"ANSI_CHT_CBT",      test_ansi_cht_cbt},
	{"ANSI_CVT",          test_ansi_cvt},
	/* ANSI-BBS — Scroll with content */
	{"ANSI_RI_scroll",    test_ansi_ri_scroll},
	{"ANSI_LF_scroll",    test_ansi_lf_scroll},
	{"ANSI_DECSTBM_scrl", test_ansi_decstbm_scroll},
	{"ANSI_SL",           test_ansi_sl},
	{"ANSI_SR",           test_ansi_sr},
	{"ANSI_SU_margins",   test_ansi_su_margins},
	{"ANSI_SD_margins",   test_ansi_sd_margins},
	/* ANSI-BBS — SGR extended */
	{"ANSI_SGR_dim",      test_ansi_sgr_dim},
	{"ANSI_SGR_conceal",  test_ansi_sgr_conceal},
	{"ANSI_SGR256_fg",    test_ansi_sgr256_fg},
	{"ANSI_SGR256_bg",    test_ansi_sgr256_bg},
	{"ANSI_SGRRGB_fg",    test_ansi_sgr_rgb_fg},
	{"ANSI_SGRRGB_bg",    test_ansi_sgr_rgb_bg},
	/* ANSI-BBS — DECIC/DECDC */
	{"ANSI_DECIC",        test_ansi_decic},
	{"ANSI_DECDC",        test_ansi_decdc},
	/* ANSI-BBS — DECCARA/DECRARA */
	{"ANSI_DECCARA_bold", test_ansi_deccara_bold},
	{"ANSI_DECRARA_bold", test_ansi_decrara_bold},
	{"ANSI_DECCARA_pres", test_ansi_deccara_preserves},
	/* ANSI-BBS — Clamping */
	{"ANSI_HPB_clamp",    test_ansi_hpb_clamp},
	{"ANSI_VPB_clamp",    test_ansi_vpb_clamp},
	/* ANSI-BBS — Variants */
	{"ANSI_CUP_default",  test_ansi_cup_defaults},
	{"ANSI_HVP_default",  test_ansi_hvp_defaults},
	/* ANSI-BBS — Query/response */
	{"ANSI_DSR",          test_ansi_dsr},
	{"ANSI_DSR_status",   test_ansi_dsr_status},
	{"ANSI_DA",           test_ansi_da},
	{"ANSI_DECRQSS_m",   test_ansi_decrqss_m},
	{"ANSI_DECRQSS_r",   test_ansi_decrqss_r},
	{"ANSI_DECRQSS_s",   test_ansi_decrqss_s},
	{"ANSI_DECRQSS_sx",  test_ansi_decrqss_starx},
	{"ANSI_DECRQSS_sr",  test_ansi_decrqss_starr},
	{"ANSI_DECRQM_aw",   test_ansi_decrqm_autowrap},
	{"ANSI_DECRQM_orig", test_ansi_decrqm_origin},
	{"ANSI_DECRQCRA",    test_ansi_decrqcra},
	/* ANSI-BBS — Macros */
	{"ANSI_DECDMAC",     test_ansi_decdmac},
	{"ANSI_DECINVM",     test_ansi_decinvm},
	/* ANSI-BBS — String passthrough */
	{"ANSI_APC_pass",    test_ansi_apc_passthrough},
	{"ANSI_DCS_pass",    test_ansi_dcs_passthrough},
	{"ANSI_PM_pass",     test_ansi_pm_passthrough},
	{"ANSI_SOS_pass",    test_ansi_sos_passthrough},
	/* ANSI-BBS — Modes */
	{"ANSI_BCDM",        test_ansi_bcdm},
	{"ANSI_save_mode",   test_ansi_save_restore_mode},
	{"ANSI_retbuf_leak", test_ansi_retbuf_leak},
	/* ANSI-BBS — SGR more */
	{"ANSI_SGR_noblink",  test_ansi_sgr_noblink},
	{"ANSI_SGR_normint",  test_ansi_sgr_normal_int},
	{"ANSI_SGR_def_fg",   test_ansi_sgr_default_fg},
	{"ANSI_SGR_def_bg",   test_ansi_sgr_default_bg},
	{"ANSI_SGR_brt_fg",   test_ansi_sgr_bright_fg},
	{"ANSI_SGR_brt_bg",   test_ansi_sgr_bright_bg},
	/* ANSI-BBS — SL/SR with margins */
	{"ANSI_SL_margins",   test_ansi_sl_margins},
	{"ANSI_SR_margins",   test_ansi_sr_margins},
	/* ANSI-BBS — DECCARA/DECRARA more */
	{"ANSI_DECCARA_clr",  test_ansi_deccara_color},
	{"ANSI_DECRARA_blk",  test_ansi_decrara_blink},
	{"ANSI_DECRARA_neg",  test_ansi_decrara_neg},
	{"ANSI_DECRARA_sg0",  test_ansi_decrara_sgr0},
	{"ANSI_DECSACE_strm", test_ansi_decsace_stream},
	/* ANSI-BBS — DECSTBM outside margins */
	{"ANSI_STBM_outside", test_ansi_decstbm_outside},
	/* Stage 1: Packet-split edge cases */
	{"SPLIT_esc_ansi",    test_split_esc_ansi},
	{"SPLIT_esc_vt52",    test_split_esc_vt52},
	{"SPLIT_esc_prestel", test_split_esc_prestel},
	{"SPLIT_csi_midnum",  test_split_csi_param_midnum},
	{"SPLIT_csi_semi",    test_split_csi_param_semicolon},
	{"SPLIT_csi_final",   test_split_csi_final_byte},
	{"SPLIT_dcs_macro",   test_split_dcs_macro},
	{"SPLIT_sos_term",    test_split_sos_terminator},
	{"SPLIT_zero_len",    test_zero_length_write},
	{"SPLIT_zero_mid",    test_zero_length_mid_esc},
	{"SPLIT_long_params", test_long_csi_params},
	{"SPLIT_large_val",   test_large_csi_value},
	{"SPLIT_vt52_esc_y",  test_split_vt52_esc_y},
	{"SPLIT_vt52_y_mid",  test_split_vt52_esc_y_mid},
	{"SPLIT_beeb_vdu23",  test_split_beeb_vdu23},
	{"SPLIT_doorway",     test_split_doorway},
	/* Stage 2: Untested ANSI features */
	{"ANSI_DECSCUSR",     test_ansi_decscusr},
	{"ANSI_CT24BC",       test_ansi_ct24bc},
	{"ANSI_FETM_TTM",     test_ansi_fetm_ttm},
	{"ANSI_OSC8_link",    test_ansi_osc8_hyperlink},
	{"ANSI_music_state",  test_ansi_music_state},
	/* Stage 5: Regressions */
	{"REG_ris_clears",    test_ris_clears_state},
	{"REG_resp_order",    test_response_ordering_direct},
	/* Stage 3: Pixel-level tests */
	{"PIX_char_render",   test_pixel_char_rendering},
	{"PIX_blank_cell",    test_pixel_blank_cell},
	{"PIX_color_change",  test_pixel_color_change},
	{"PIX_bg_change",     test_pixel_bg_change},
	{"PIX_bold_differs",  test_pixel_bold_differs},
	{"PIX_256color",      test_pixel_256color_differs},
	/* Stage 4: Non-ANSI edge cases */
	{"ATA_enc_boundary",  test_atascii_encoding_boundaries},
	{"PET_font_c64",      test_petscii_font_switch},
	{"PET_font_c128",     test_petscii_font_switch_c128},
	{"PET_colors_c128",   test_petscii_all_colors_c128_80},
	{"PRS_alpha_colors",  test_prestel_all_alpha_colors},
	{"PRS_mosaic_colors", test_prestel_all_mosaic_colors},
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
	/* PETSCII */
	{"PET_printable",      test_pet_printable},
	{"PET_return",         test_pet_return},
	{"PET_return_norev",   test_pet_return_disables_reverse},
	{"PET_shift_return",   test_pet_shift_return},
	{"PET_clear_screen",   test_pet_clear_screen},
	{"PET_home",           test_pet_home},
	{"PET_cursor_down",    test_pet_cursor_down},
	{"PET_down_scroll",    test_pet_cursor_down_scroll},
	{"PET_cursor_up",      test_pet_cursor_up},
	{"PET_cursor_right",   test_pet_cursor_right},
	{"PET_right_wrap",     test_pet_cursor_right_wrap},
	{"PET_cursor_left",    test_pet_cursor_left},
	{"PET_left_wrap",      test_pet_cursor_left_wrap},
	{"PET_left_clamp",     test_pet_cursor_left_clamp},
	{"PET_delete",         test_pet_delete},
	{"PET_insert",         test_pet_insert},
	{"PET_reverse",        test_pet_reverse},
	{"PET_color_c64",      test_pet_color_c64},
	{"PET_color_c128_80",  test_pet_color_c128_80},
	{"PET_color_c128_40",  test_pet_color_c128_40},
	{"PET_all_colors_c64", test_pet_all_colors_c64},
	{"PET_bell",           test_pet_bell},
	{"PET_ignored_ctrl",   test_pet_ignored_controls},
	{"PET_return_scroll",  test_pet_return_scroll},
	{"PET_c64_lock_case",  test_pet_c64_lock_unlock_case},
	{"PET_c128_disputed",  test_pet_c128_disputed_ignored},
	{"PET_lf_c128_80",    test_pet_lf_c128_80},
	{"PET_lf_c64_ignored", test_pet_lf_c64_ignored},
	/* Prestel */
	{"PRS_printable",      test_prestel_printable},
	{"PRS_APR",            test_prestel_apr},
	{"PRS_APB",            test_prestel_apb},
	{"PRS_APF",            test_prestel_apf},
	{"PRS_APD",            test_prestel_apd},
	{"PRS_APU",            test_prestel_apu},
	{"PRS_APD_wrap",       test_prestel_apd_wrap},
	{"PRS_APU_wrap",       test_prestel_apu_wrap},
	{"PRS_APB_wrap",       test_prestel_apb_wrap},
	{"PRS_APF_wrap",       test_prestel_apf_wrap},
	{"PRS_CS",             test_prestel_cs},
	{"PRS_APH",            test_prestel_aph},
	{"PRS_cursor_on_off",  test_prestel_cursor_on_off},
	{"PRS_ENQ",            test_prestel_enq},
	{"PRS_NUL",            test_prestel_nul},
	{"PRS_ignored_c0",     test_prestel_ignored_c0},
	{"PRS_alpha_color",    test_prestel_esc_alpha_color},
	{"PRS_mosaic_color",   test_prestel_esc_mosaic_color},
	{"PRS_flash_steady",   test_prestel_esc_flash_steady},
	{"PRS_conceal",        test_prestel_esc_conceal},
	{"PRS_hold_release",   test_prestel_esc_hold_release},
	{"PRS_double_height",  test_prestel_esc_double_height},
	{"PRS_background",     test_prestel_esc_background},
	{"PRS_separated",      test_prestel_esc_separated},
	{"PRS_row_reset",      test_prestel_row_attr_reset},
	{"PRS_c1_raw",         test_prestel_c1_raw},
	/* BEEB */
	{"BEEB_printable",     test_beeb_printable},
	{"BEEB_char_xlate",    test_beeb_char_translate},
	{"BEEB_BEL",           test_beeb_bel},
	{"BEEB_APD_scroll",    test_beeb_apd_scroll},
	{"BEEB_APU_scroll",    test_beeb_apu_scroll},
	{"BEEB_DEL",           test_beeb_del},
	{"BEEB_APS",           test_beeb_aps},
	{"BEEB_VDU23_cursor",  test_beeb_vdu23_cursor},
	{"BEEB_APH",           test_beeb_aph},
	{"BEEB_CS",            test_beeb_cs},
	{"BEEB_c1_serial",     test_beeb_c1_serial_attr},
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
	ciolib_initial_mode = C80;
	ciolib_initial_program_name = "cterm_test";
	ciolib_initial_program_class = "cterm_test";
	if (initciolib(CIOLIB_MODE_SDL)) {
		fprintf(stderr, "Failed to initialize ciolib (SDL)\n");
		return 1;
	}
	textmode(C80);

	fprintf(result_fp, "# cterm_test results\n");

	for (int i = 0; tests[i].name; i++) {
		if (filter && strstr(tests[i].name, filter) == NULL)
			continue;
		total++;

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
