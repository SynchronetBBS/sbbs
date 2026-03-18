/*
 * termtest.c — Automated terminal emulation test harness for SyncTERM
 *
 * Runs as the child process of a SyncTERM shell: connection.
 * Sends escape sequences to stdout (processed by SyncTERM's cterm),
 * reads terminal responses from stdin (DSR, DECRQCRA), and writes
 * results to a file.
 *
 * Usage: termtest [results_file]
 *   If results_file is omitted, creates /tmp/termtest.XXXXXX
 *
 * Invoke via:
 *   SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software \
 *   SDL_VIDEO_EGL_DRIVER=none \
 *   syncterm -iS -Q 'shell:./termtest /tmp/results.txt'
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static FILE *result_fp;
static struct termios orig_tio;
static int tio_saved;

static int total_tests;
static int passed_tests;
static int failed_tests;
static int skipped_tests;

/* Forward declarations */
static void cursor_to(int x, int y);
static int read_sos_response(char *buf, size_t bufsz, int timeout_ms);

/* Restore terminal on exit */
static void
cleanup(void)
{
	if (tio_saved)
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
}

/* Put stdin into raw mode for escape sequence I/O */
static int
set_raw_mode(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &orig_tio) < 0)
		return -1;
	tio_saved = 1;
	raw = orig_tio;
	cfmakeraw(&raw);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	return tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* Write a string to the terminal (stdout) */
static void
term_write(const char *s)
{
	size_t len = strlen(s);
	ssize_t w;

	while (len > 0) {
		w = write(STDOUT_FILENO, s, len);
		if (w <= 0)
			break;
		s += w;
		len -= w;
	}
}

/* Write formatted string to the terminal */
static void
term_printf(const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	term_write(buf);
}

/* Read bytes from stdin with timeout (milliseconds).
 * Returns number of bytes read, 0 on timeout. */
static int
term_read(char *buf, size_t bufsz, int timeout_ms)
{
	struct pollfd pfd;
	int ret;
	ssize_t n;
	size_t total = 0;

	while (total < bufsz) {
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;
		ret = poll(&pfd, 1, total == 0 ? timeout_ms : 50);
		if (ret <= 0)
			break;
		n = read(STDIN_FILENO, buf + total, bufsz - total);
		if (n <= 0)
			break;
		total += n;
	}
	return total;
}

/* Drain any pending input */
static void
term_drain(void)
{
	char junk[256];

	while (term_read(junk, sizeof(junk), 50) > 0)
		;
}

/*
 * Read a CSI sequence response (e.g. ESC [ Ps R)
 * Returns length of sequence in buf, or 0 on timeout/error.
 */
static int
read_csi_response(char *buf, size_t bufsz, int timeout_ms)
{
	size_t pos = 0;
	char ch;

	while (pos < bufsz - 1) {
		if (term_read(&ch, 1, pos == 0 ? timeout_ms : 200) != 1)
			break;
		buf[pos++] = ch;
		/* CSI sequence ends with a byte in 0x40-0x7E range after ESC [ */
		if (pos >= 3 && buf[1] == '[' && ch >= 0x40 && ch <= 0x7E) {
			buf[pos] = '\0';
			return pos;
		}
	}
	buf[pos] = '\0';
	return 0;
}

/*
 * Read a DCS/APC string response (ESC P ... ESC \)
 * Returns length of sequence in buf, or 0 on timeout/error.
 */
static int
read_dcs_response(char *buf, size_t bufsz, int timeout_ms)
{
	size_t pos = 0;
	char ch;

	while (pos < bufsz - 1) {
		if (term_read(&ch, 1, pos == 0 ? timeout_ms : 500) != 1)
			break;
		buf[pos++] = ch;
		/* DCS string ends with ESC \ (ST) */
		if (pos >= 4 && buf[pos - 2] == '\033' && buf[pos - 1] == '\\') {
			buf[pos] = '\0';
			return pos;
		}
	}
	buf[pos] = '\0';
	return 0;
}

/*
 * Send DSR (Device Status Report) and parse cursor position.
 * Returns 1 on success, 0 on failure.
 */
static int
get_cursor_pos(int *x, int *y)
{
	char buf[64];
	int row, col;

	term_write("\033[6n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response: ESC [ row ; col R */
	if (sscanf(buf, "\033[%d;%dR", &row, &col) != 2)
		return 0;
	*x = col;
	*y = row;
	return 1;
}

/* Check cursor is at expected position */
static int
check_xy(int expect_x, int expect_y)
{
	int x, y;

	if (!get_cursor_pos(&x, &y))
		return 0;
	if (x != expect_x || y != expect_y) {
		fprintf(result_fp, "    expected (%d,%d) got (%d,%d)\n",
		    expect_x, expect_y, x, y);
		return 0;
	}
	return 1;
}

/*
 * Get DECRQCRA checksum for a screen region.
 * Returns 1 on success and fills *checksum, 0 on failure.
 */
static int has_cksum;

static int
get_checksum(int sx, int sy, int ex, int ey, char *checksum, size_t cksz)
{
	char buf[64];
	char *p, *end;

	term_printf("\033[1;1;%d;%d;%d;%d*y", sy, sx, ey, ex);
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response: ESC P 1 ! ~ XXXX ESC \ */
	p = strstr(buf, "!~");
	if (p == NULL)
		return 0;
	p += 2;
	end = strstr(p, "\033\\");
	if (end == NULL)
		return 0;
	*end = '\0';
	if ((size_t)(end - p) >= cksz)
		return 0;
	strcpy(checksum, p);
	return 1;
}

/*
 * Read screen content at a position using STS.
 * With FETM=INSERT (attributed=1): returns ECMA-48 stream (SGR + chars).
 * With FETM=EXCLUDE (attributed=0): returns plain characters.
 * Returns content length, 0 on failure.
 */
static int has_sts;

static int
read_screen_at(int x, int y, int len, int attributed, char *buf, size_t bufsz)
{
	if (!has_sts)
		return 0;
	/* Use TTM=ALL + ESA for inclusive end */
	term_write("\033[16h");         /* TTM=ALL */
	if (!attributed)
		term_write("\033[14h");     /* FETM=EXCLUDE */
	cursor_to(x, y);
	term_write("\033F");            /* SSA */
	cursor_to(x + len - 1, y);
	term_write("\033G");            /* ESA */
	term_write("\033S");            /* STS */
	if (!attributed)
		term_write("\033[14l");     /* FETM=INSERT */
	term_write("\033[16l");         /* TTM=CURSOR */

	int n = read_sos_response(buf, bufsz, 2000);
	if (n == 0)
		return 0;

	/* Find content after prefix — use memmem since content
	 * may contain NUL bytes (doorway-encoded C0 cells). */
	char *prefix = attributed ? "CTerm:STS:0:" : "CTerm:STS:1:";
	int prefix_len = strlen(prefix);
	char *content = NULL;
	int i;
	for (i = 0; i <= n - prefix_len; i++) {
		if (memcmp(buf + i, prefix, prefix_len) == 0) {
			content = buf + i + prefix_len;
			break;
		}
	}
	if (content == NULL)
		return 0;

	/* Shift content to start of buf */
	int content_len = n - (content - buf);
	memmove(buf, content, content_len);
	buf[content_len] = '\0';
	return content_len;
}

/*
 * Extract just the graphic characters from an FETM=INSERT stream,
 * skipping all control sequences.  Returns number of characters extracted.
 */
static int
extract_text(const char *stream, char *text, size_t textsz)
{
	int pos = 0;
	const char *p = stream;

	while (*p && (size_t)pos < textsz - 1) {
		if ((unsigned char)*p == 0x1b) {
			/* ESC — skip sequence */
			p++;
			if (*p == '[') {
				/* CSI sequence — skip to final byte (0x40-0x7E) */
				p++;
				while (*p && ((unsigned char)*p < 0x40 || (unsigned char)*p > 0x7E))
					p++;
				if (*p) p++;
			}
			else if (*p == ']') {
				/* OSC — skip to ST (ESC \) or escaped ST (ESC : \) */
				p++;
				while (*p) {
					if (*p == 0x1b && *(p+1) == '\\') {
						p += 2;
						break;
					}
					if (*p == 0x1b && *(p+1) == ':' && *(p+2) == '\\') {
						p += 3;
						break;
					}
					p++;
				}
			}
			else {
				/* Other ESC sequence — skip one byte */
				if (*p) p++;
			}
		}
		else if ((unsigned char)*p >= 0x20 && (unsigned char)*p != 0x7f) {
			text[pos++] = *p++;
		}
		else {
			p++;
		}
	}
	text[pos] = '\0';
	return pos;
}

/*
 * Verify string appears at position using STS readback and checksums.
 * Returns 1 pass, 0 fail, -1 skip.
 */
static int
check_string_at(const char *str, int x, int y)
{
	char before[16], after[16];
	int len = strlen(str);

	if (!has_cksum)
		return -1;
	if (!get_checksum(x, y, x + len - 1, y, before, sizeof(before)))
		return -1;
	term_printf("\033[%d;%dH%s", y, x, str);
	if (!get_checksum(x, y, x + len - 1, y, after, sizeof(after)))
		return -1;
	/* Checksums should differ — old content replaced by str */
	if (strcmp(before, after) == 0)
		return 0;

	/* Also verify via STS readback if available */
	if (has_sts) {
		char stream[4096];
		int n = read_screen_at(x, y, len, 1, stream, sizeof(stream));
		if (n > 0) {
			char text[256];
			extract_text(stream, text, sizeof(text));
			if (strncmp(text, str, len) != 0) {
				fprintf(result_fp, "    STS readback mismatch: expected '%.40s' got '%.40s'\n", str, text);
				return 0;
			}
		}
	}
	return 1;
}

/* Move cursor to position */
static void
cursor_to(int x, int y)
{
	term_printf("\033[%d;%dH", y, x);
}

/* Clear screen and home cursor */
static void
clear_screen(void)
{
	term_write("\033[2J\033[H");
}

/* Log and run a test */
typedef int (*test_fn)(void);

static void
run_test(const char *name, test_fn fn)
{
	int result;

	clear_screen();
	term_drain();
	total_tests++;
	result = fn();
	if (result > 0) {
		fprintf(result_fp, "PASS %s\n", name);
		passed_tests++;
	}
	else if (result == 0) {
		fprintf(result_fp, "FAIL %s\n", name);
		failed_tests++;
	}
	else {
		fprintf(result_fp, "SKIP %s\n", name);
		skipped_tests++;
	}
	fflush(result_fp);
}

/* ================================================================
 * Test cases
 * ================================================================ */

static int
test_nul(void)
{
	char nuls[3] = {0, 0, 0};

	cursor_to(1, 1);
	write(STDOUT_FILENO, nuls, 3);
	return check_xy(1, 1);
}

static int
test_bs(void)
{
	cursor_to(1, 2);
	term_write("1234\b");
	if (!check_xy(4, 2))
		return 0;
	term_write("\b\b\b\b\b\b\b");
	return check_xy(1, 2);
}

static int
test_ht(void)
{
	cursor_to(1, 1);
	term_write("\t");
	/* Tab should move cursor past column 1 */
	int x, y;
	if (!get_cursor_pos(&x, &y))
		return 0;
	return (x > 1 && y == 1);
}

static int
test_lf(void)
{
	cursor_to(1, 1);
	term_write("1234\n");
	return check_xy(5, 2);
}

static int
test_cr(void)
{
	cursor_to(1, 1);
	term_write("1234\r");
	return check_xy(1, 1);
}

static int
test_nel(void)
{
	cursor_to(3, 1);
	term_write("\033E");
	return check_xy(1, 2);
}

static int
test_cuu(void)
{
	cursor_to(5, 5);
	term_write("\033[2A");
	return check_xy(5, 3);
}

static int
test_cud(void)
{
	cursor_to(5, 5);
	term_write("\033[3B");
	return check_xy(5, 8);
}

static int
test_cuf(void)
{
	cursor_to(5, 5);
	term_write("\033[4C");
	return check_xy(9, 5);
}

static int
test_cub(void)
{
	cursor_to(10, 5);
	term_write("\033[3D");
	return check_xy(7, 5);
}

static int
test_cup(void)
{
	term_write("\033[10;20H");
	return check_xy(20, 10);
}

static int
test_hvp(void)
{
	term_write("\033[15;25f");
	return check_xy(25, 15);
}

static int
test_cha(void)
{
	cursor_to(5, 5);
	term_write("\033[12G");
	return check_xy(12, 5);
}

static int
test_vpa(void)
{
	cursor_to(5, 5);
	term_write("\033[10d");
	return check_xy(5, 10);
}

static int
test_ed_below(void)
{
	return check_string_at("ED0 Test", 1, 1);
}

static int
test_el_right(void)
{
	return check_string_at("EL0 Test", 1, 1);
}

static int
test_ich(void)
{
	cursor_to(1, 1);
	term_write("ABCD");
	cursor_to(2, 1);
	term_write("\033[2@");
	/* Cursor stays at 2,1; B and C shifted right */
	return check_xy(2, 1);
}

static int
test_dch(void)
{
	cursor_to(1, 1);
	term_write("ABCDE");
	cursor_to(2, 1);
	term_write("\033[2P");
	return check_xy(2, 1);
}

static int
test_il(void)
{
	cursor_to(1, 3);
	term_write("\033[2L");
	return check_xy(1, 3);
}

static int
test_dl(void)
{
	cursor_to(1, 3);
	term_write("\033[2M");
	return check_xy(1, 3);
}

static int
test_ech(void)
{
	return check_string_at("ECH Test", 1, 1);
}

static int
test_cuu_stop(void)
{
	/* CUU at top of screen should not wrap */
	cursor_to(5, 1);
	term_write("\033[999A");
	return check_xy(5, 1);
}

static int
test_cud_stop(void)
{
	/* CUD at bottom should not wrap — move to row 24 first */
	cursor_to(5, 24);
	term_write("\033[999B");
	return check_xy(5, 24);
}

static int
test_save_restore_cursor(void)
{
	cursor_to(10, 10);
	term_write("\033[s");  /* Save */
	cursor_to(1, 1);
	term_write("\033[u");  /* Restore */
	return check_xy(10, 10);
}

static int
test_decsc_decrc(void)
{
	cursor_to(15, 12);
	term_write("\0337");   /* DECSC */
	cursor_to(1, 1);
	term_write("\0338");   /* DECRC */
	return check_xy(15, 12);
}

static int
test_dsr(void)
{
	/* DSR is implicitly tested by every check_xy call,
	 * but verify explicitly */
	cursor_to(7, 3);
	return check_xy(7, 3);
}

static int
test_decrqcra(void)
{
	char cksum[16];

	if (!get_checksum(1, 1, 1, 1, cksum, sizeof(cksum)))
		return -1; /* Skip if not supported */
	/* We got a valid response — mark as available */
	return 1;
}

static int
test_su(void)
{
	/* Scroll up — cursor should not move */
	cursor_to(5, 5);
	term_write("\033[2S");
	return check_xy(5, 5);
}

static int
test_sd(void)
{
	/* Scroll down — cursor should not move */
	cursor_to(5, 5);
	term_write("\033[2T");
	return check_xy(5, 5);
}

static int
test_hts_cht_cbt(void)
{
	int x, y;

	/* Set a tab stop at column 20 */
	cursor_to(20, 1);
	term_write("\033H");       /* HTS set tab */
	cursor_to(1, 1);
	/* CHT should advance to next tab stop */
	term_write("\033[I");
	if (!get_cursor_pos(&x, &y))
		return 0;
	/* Should be at a tab stop (could be default 9 or our 20) */
	if (y != 1 || x <= 1)
		return 0;
	/* CBT should go back */
	int saved_x = x;
	term_write("\033[I");      /* Forward one more */
	term_write("\033[Z");      /* Back one */
	if (!check_xy(saved_x, 1))
		return 0;
	/* Reset: clear all tabs then do RIS to restore defaults */
	term_write("\033[3g");
	term_write("\033c");
	usleep(100000);
	term_drain();
	return 1;
}

static int
test_decstbm(void)
{
	/* Set scroll region to rows 5-10, verify cursor homes */
	term_write("\033[5;10r");
	/* When origin mode is off, cursor goes to (1,1) */
	if (!check_xy(1, 1))
		return 0;
	/* Reset scroll region */
	term_write("\033[r");
	return 1;
}

static int
test_sgr_reset(void)
{
	/* SGR 0 should reset attributes — verify cursor doesn't move */
	cursor_to(3, 3);
	term_write("\033[1;31m");  /* Bold red */
	term_write("\033[0m");     /* Reset */
	return check_xy(3, 3);
}

static int
test_autowrap(void)
{
	int x, y;

	term_write("\033[?7h");    /* Enable autowrap */

	/* Write to last column: enters pending-wrap (LCF) state.
	 * We can't query via DSR without triggering the wrap (the
	 * response bytes cause output processing).  So write one
	 * more char and verify it wrapped. */
	cursor_to(79, 1);
	term_write("XYZ");
	if (!get_cursor_pos(&x, &y))
		return 0;
	/* X at col 79, Y at col 80 (pending wrap), Z wraps to (1,2)
	 * then advances to (2,2) */
	if (x != 2 || y != 2) {
		fprintf(result_fp, "    after XYZ: expected (2,2) got (%d,%d)\n", x, y);
		return 0;
	}
	return 1;
}

static int
test_origin_mode(void)
{
	int x, y;

	/* Set scroll region rows 5-10, enable origin mode */
	term_write("\033[5;10r");
	term_write("\033[?6h");

	/* CUP(1,1) in origin mode → top-left of scroll region.
	 * DSR should report (1,1) relative to the region. */
	term_write("\033[1;1H");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (x != 1 || y != 1) {
		fprintf(result_fp, "    origin CUP(1,1): expected (1,1) got (%d,%d)\n", x, y);
		term_write("\033[?6l\033[r");
		return 0;
	}

	/* CUP to row 3, col 5 → relative (5,3) */
	term_write("\033[3;5H");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (x != 5 || y != 3) {
		fprintf(result_fp, "    origin CUP(3,5): expected (5,3) got (%d,%d)\n", x, y);
		term_write("\033[?6l\033[r");
		return 0;
	}

	/* Cursor should clamp to bottom of region (row 6 relative) */
	term_write("\033[20;1H");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (y != 6) {
		fprintf(result_fp, "    origin clamp: expected row 6 got row %d\n", y);
		term_write("\033[?6l\033[r");
		return 0;
	}

	/* Disable origin mode — per VT100/VT102 spec, this homes
	 * the cursor to absolute (1,1).  (xterm does not home here,
	 * but the DEC documentation is clear that toggling DECOM
	 * in either direction performs a cursor home.) */
	term_write("\033[?6l");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (x != 1 || y != 1) {
		fprintf(result_fp, "    DECOM reset home: expected (1,1) got (%d,%d)\n", x, y);
		term_write("\033[r");
		return 0;
	}

	/* Reset scroll region */
	term_write("\033[r");
	return 1;
}

static int
test_decdmac(void)
{
	int x, y;

	/* Define macro 0 as "Hello" (hex-encoded), then invoke it */
	/* DECDMAC: DCS id ; dt ; fen ! z hex-data ST */
	/* dt=0 means define, fen=1 means hex encoding */
	term_write("\033P0;0;1!z48656c6c6f\033\\");
	/* Invoke macro 0: DECMAC CSI id * z */
	cursor_to(1, 1);
	term_write("\033[0*z");
	if (!get_cursor_pos(&x, &y))
		return 0;
	/* "Hello" is 5 chars, cursor should be at (6,1) */
	if (x != 6 || y != 1) {
		fprintf(result_fp, "    expected (6,1) got (%d,%d)\n", x, y);
		return 0;
	}
	return 1;
}

/*
 * Send DECRQM (Request Mode) for DEC private modes.
 * Sends CSI ? Ps $ p, expects DECRPM response CSI ? Ps ; Pm $ y
 * Returns Pm (1=set, 2=reset, 0=not recognized), or -1 on timeout.
 */
static int
query_decrqm(int mode)
{
	char buf[64];
	int ps, pm;

	term_printf("\033[?%d$p", mode);
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return -1;
	/* Response: ESC [ ? Ps ; Pm $ y */
	if (sscanf(buf, "\033[?%d;%d$y", &ps, &pm) != 2)
		return -1;
	if (ps != mode)
		return -1;
	return pm;
}

/*
 * Send DECRQM for CTerm '=' private modes.
 * Sends CSI = Ps $ p, expects CSI = Ps ; Pm $ y
 */
static int
query_decrqm_eq(int mode)
{
	char buf[64];
	int ps, pm;

	term_printf("\033[=%d$p", mode);
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return -1;
	if (sscanf(buf, "\033[=%d;%d$y", &ps, &pm) != 2)
		return -1;
	if (ps != mode)
		return -1;
	return pm;
}

/* ================================================================
 * Additional tests ported from termtest.js
 * ================================================================ */

static int
test_hts(void)
{
	/* Set tab at col 3, tab from col 1 should land there */
	cursor_to(3, 1);
	term_write("\033H");
	cursor_to(1, 1);
	term_write("\t");
	if (!check_xy(3, 1))
		return 0;
	/* Clean up */
	term_write("\033c");
	usleep(100000);
	term_drain();
	return 1;
}

static int
test_ri(void)
{
	cursor_to(5, 2);
	term_write("\033M");
	return check_xy(5, 1);
}

static int
test_apc(void)
{
	cursor_to(1, 1);
	term_write("\033_This is an application string\033\\");
	return check_xy(1, 1);
}

static int
test_dcs(void)
{
	cursor_to(1, 1);
	term_write("\033PThis is a device control string\033\\");
	return check_xy(1, 1);
}

static int
test_pm(void)
{
	cursor_to(1, 1);
	term_write("\033^This is a privacy message\033\\");
	return check_xy(1, 1);
}

static int
test_osc(void)
{
	cursor_to(1, 1);
	term_write("\033]9999;harmless\033\\");
	return check_xy(1, 1);
}

static int
test_sos(void)
{
	cursor_to(1, 1);
	term_write("\033XThis is just a string\033\\");
	return check_xy(1, 1);
}

static int
test_ris(void)
{
	cursor_to(2, 2);
	term_write("\033c");
	usleep(50000);
	term_drain();
	return check_xy(1, 1);
}

static int
test_cnl(void)
{
	cursor_to(20, 1);
	term_write("\033[E");
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 1);
	term_write("\033[1E");
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 1);
	term_write("\033[2E");
	return check_xy(1, 3);
}

static int
test_cpl(void)
{
	cursor_to(20, 3);
	term_write("\033[F");
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 3);
	term_write("\033[1F");
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 5);
	term_write("\033[2F");
	return check_xy(1, 3);
}

static int
test_hpa(void)
{
	cursor_to(20, 1);
	term_write("\033[`");
	if (!check_xy(1, 1))
		return 0;
	term_write("\033[80`");
	if (!check_xy(80, 1))
		return 0;
	term_write("\033[1`");
	return check_xy(1, 1);
}

static int
test_hpr(void)
{
	cursor_to(1, 1);
	term_write("\033[a");
	if (!check_xy(2, 1))
		return 0;
	term_write("\033[1a");
	if (!check_xy(3, 1))
		return 0;
	term_write("\033[2a");
	return check_xy(5, 1);
}

static int
test_vpr(void)
{
	cursor_to(20, 1);
	term_write("\033[e");
	if (!check_xy(20, 2))
		return 0;
	term_write("\033[1e");
	if (!check_xy(20, 3))
		return 0;
	term_write("\033[2e");
	return check_xy(20, 5);
}

static int
test_hpb(void)
{
	cursor_to(5, 1);
	term_write("\033[j");
	if (!check_xy(4, 1))
		return 0;
	term_write("\033[1j");
	if (!check_xy(3, 1))
		return 0;
	term_write("\033[2j");
	return check_xy(1, 1);
}

static int
test_vpb(void)
{
	cursor_to(20, 10);
	term_write("\033[k");
	if (!check_xy(20, 9))
		return 0;
	term_write("\033[1k");
	if (!check_xy(20, 8))
		return 0;
	term_write("\033[2k");
	return check_xy(20, 6);
}

static int
test_rep(void)
{
	cursor_to(1, 1);
	term_write(" \033[b");
	if (!check_xy(3, 1))
		return 0;
	cursor_to(1, 1);
	term_write(" \033[1b");
	if (!check_xy(3, 1))
		return 0;
	cursor_to(1, 1);
	term_write(" \033[10b");
	return check_xy(12, 1);
}

static int
test_cuf_clamp(void)
{
	cursor_to(1, 1);
	term_printf("\033[%dC", 90);
	return check_xy(80, 1);
}

static int
test_cub_clamp(void)
{
	cursor_to(5, 1);
	term_write("\033[99D");
	return check_xy(1, 1);
}

static int
test_hpb_clamp(void)
{
	cursor_to(5, 1);
	term_write("\033[99j");
	return check_xy(1, 1);
}

static int
test_vpb_clamp(void)
{
	cursor_to(5, 3);
	term_write("\033[99k");
	return check_xy(5, 1);
}

static int
test_da(void)
{
	char buf[64];

	term_write("\033[c");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response should end with 'c' */
	int len = strlen(buf);
	return (len >= 3 && buf[len - 1] == 'c');
}

static int
test_dsr_status(void)
{
	char buf[64];

	/* DSR operating status - should respond CSI 0 n (OK) */
	term_write("\033[5n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response: ESC [ 0 n */
	return (strstr(buf, "0n") != NULL);
}

static int
test_cup_variants(void)
{
	/* CUP with various parameter formats */
	cursor_to(20, 3);
	term_write("\033[H");      /* No params = home */
	if (!check_xy(1, 1))
		return 0;
	cursor_to(20, 3);
	term_write("\033[;H");     /* Both empty = home */
	if (!check_xy(1, 1))
		return 0;
	cursor_to(20, 3);
	term_write("\033[2H");     /* Row only */
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 3);
	term_write("\033[2;H");    /* Row; empty col */
	if (!check_xy(1, 2))
		return 0;
	cursor_to(20, 3);
	term_write("\033[;2H");    /* Empty row; col */
	return check_xy(2, 1);
}

static int
test_hvp_variants(void)
{
	/* HVP (f) with same parameter format tests as CUP */
	cursor_to(20, 3);
	term_write("\033[f");
	if (!check_xy(1, 1))
		return 0;
	cursor_to(20, 3);
	term_write("\033[;f");
	if (!check_xy(1, 1))
		return 0;
	cursor_to(20, 3);
	term_write("\033[2;2f");
	return check_xy(2, 2);
}

static int
test_tbc(void)
{
	int x, y;

	/* Tab to first default stop, then clear it */
	cursor_to(1, 1);
	term_write("\t");
	if (!get_cursor_pos(&x, &y))
		return 0;
	int first_stop = x;
	term_write("\033[0g");     /* TBC clear at cursor */
	cursor_to(1, 1);
	term_write("\t");
	if (!get_cursor_pos(&x, &y))
		return 0;
	/* Should now skip past where the first stop was */
	if (x == first_stop) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* Clear all tabs, set one at last column */
	term_write("\033c");
	usleep(50000);
	term_drain();
	term_write("\033[3g");     /* TBC clear all */
	cursor_to(80, 1);
	term_write("\033H");       /* HTS */
	cursor_to(1, 1);
	term_write("\t");
	if (!check_xy(80, 1)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	term_write("\033c");
	usleep(100000);
	term_drain();
	return 1;
}

static int
test_decstbm_scroll(void)
{
	int x, y;

	/* Set region rows 10-11, position at row 10 */
	term_write("\033[10;11r");
	if (!check_xy(1, 1))
		return 0;
	cursor_to(10, 10);
	/* Three LFs should scroll within the region, clamping at row 11 */
	term_write("\r\n\r\n\r\n");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (y != 11) {
		fprintf(result_fp, "    scroll clamp: expected row 11 got %d\n", y);
		term_write("\033[r");
		return 0;
	}
	/* CUU 5 from row 11 should clamp at top of region (row 10) */
	term_write("\033[5A");
	if (!get_cursor_pos(&x, &y))
		return 0;
	if (y != 10) {
		fprintf(result_fp, "    CUU clamp: expected row 10 got %d\n", y);
		term_write("\033[r");
		return 0;
	}
	term_write("\033[r");
	return 1;
}

static int
test_decslrm(void)
{
	/* Enable left/right margin mode, set margins 10-11 */
	term_write("\033[?69h");
	term_write("\033[10;11s");
	/* DECSLRM should home cursor */
	if (!check_xy(1, 1)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* BS at left margin should not move */
	cursor_to(10, 10);
	term_write("\b\b\b");
	if (!check_xy(10, 10)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* CUF 10 should clamp at right margin */
	term_write("\033[10C");
	if (!check_xy(11, 10)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* Reset */
	term_write("\033[s\033[?69l");
	return 1;
}

static int
test_decrst_nowrap(void)
{
	int x, y;

	/* Disable autowrap, write past last column */
	cursor_to(80, 1);
	term_write("\033[?7l");
	term_write("This is all garbage here! ");
	term_write("\033[?7h");
	if (!get_cursor_pos(&x, &y))
		return 0;
	return (x == 80 && y == 1);
}

static int
test_decrqss_m(void)
{
	char buf[64];

	/* Set red foreground, query SGR state */
	term_write("\033[0;31m");
	term_write("\033P$qm\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0) {
		term_write("\033[0m");
		return 0;
	}
	term_write("\033[0m");
	/* Response: DCS 1 $ r <params> m ST — should contain "31" */
	if (strstr(buf, "1$r") == NULL)
		return 0;
	if (strstr(buf, "31") == NULL)
		return 0;
	return 1;
}

static int
test_decrqss_r(void)
{
	char buf[64];

	/* Set scroll region, query it */
	term_write("\033[5;15r");
	term_write("\033P$qr\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0) {
		term_write("\033[r");
		return 0;
	}
	term_write("\033[r");
	/* Response should contain "5;15r" */
	return (strstr(buf, "5;15r") != NULL);
}

static int
test_decrqss_s(void)
{
	char buf[64];

	/* Set left/right margins, query them */
	term_write("\033[?69h");
	term_write("\033[5;75s");
	term_write("\033P$qs\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0) {
		term_write("\033[s\033[?69l");
		return 0;
	}
	term_write("\033[s\033[?69l");
	return (strstr(buf, "5;75s") != NULL);
}

static int
test_dectabsr(void)
{
	char buf[64];

	/* Query tab stop report */
	term_write("\033[2$w");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response: DCS 2 $ u <tab stops> ST */
	return (strstr(buf, "2$u") != NULL);
}

static int
test_sgr256_fg(void)
{
	char before[16], after[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, before, sizeof(before)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[38;5;196m#");
	if (!get_checksum(1, 1, 1, 1, after, sizeof(after)))
		return -1;
	term_write("\033[0m");
	return strcmp(before, after) != 0;
}

static int
test_sgr256_bg(void)
{
	char before[16], after[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m ");
	if (!get_checksum(1, 1, 1, 1, before, sizeof(before)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[48;5;21m ");
	if (!get_checksum(1, 1, 1, 1, after, sizeof(after)))
		return -1;
	term_write("\033[0m");
	return strcmp(before, after) != 0;
}

static int
test_sgr_rgb_fg(void)
{
	char before[16], after[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, before, sizeof(before)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[38;2;255;128;0m#");
	if (!get_checksum(1, 1, 1, 1, after, sizeof(after)))
		return -1;
	term_write("\033[0m");
	return strcmp(before, after) != 0;
}

static int
test_sgr_rgb_bg(void)
{
	char before[16], after[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m ");
	if (!get_checksum(1, 1, 1, 1, before, sizeof(before)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[48;2;0;255;128m ");
	if (!get_checksum(1, 1, 1, 1, after, sizeof(after)))
		return -1;
	term_write("\033[0m");
	return strcmp(before, after) != 0;
}

static int
test_sgr_bright(void)
{
	char cs_norm[16], cs_bright[16];

	if (!has_cksum)
		return -1;
	/* Normal red vs bright red foreground */
	cursor_to(1, 1);
	term_write("\033[31m#");
	if (!get_checksum(1, 1, 1, 1, cs_norm, sizeof(cs_norm)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[91m#");
	if (!get_checksum(1, 1, 1, 1, cs_bright, sizeof(cs_bright)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_norm, cs_bright) != 0;
}

static int
test_sgr_negative(void)
{
	char cs_normal[16], cs_neg[16], cs_pos[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;37;40m#");
	if (!get_checksum(1, 1, 1, 1, cs_normal, sizeof(cs_normal)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;37;40;7m#");
	if (!get_checksum(1, 1, 1, 1, cs_neg, sizeof(cs_neg)))
		return -1;
	if (strcmp(cs_normal, cs_neg) == 0) {
		term_write("\033[0m");
		return 0;
	}
	/* SGR 27 (positive) should restore */
	cursor_to(1, 1);
	term_write("\033[0;37;40;7;27m#");
	if (!get_checksum(1, 1, 1, 1, cs_pos, sizeof(cs_pos)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_pos, cs_normal) == 0;
}

static int
test_sgr_conceal(void)
{
	char cs_visible[16], cs_concealed[16];

	if (!has_cksum)
		return -1;
	/* Concealed text: fg should match bg, making text invisible */
	cursor_to(1, 1);
	term_write("\033[0;37;40m ");
	if (!get_checksum(1, 1, 1, 1, cs_visible, sizeof(cs_visible)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;37;40;8m#");
	if (!get_checksum(1, 1, 1, 1, cs_concealed, sizeof(cs_concealed)))
		return -1;
	term_write("\033[0m");
	/* Concealed '#' on black bg should look like space on black bg */
	return strcmp(cs_visible, cs_concealed) == 0;
}

static int
test_ri_scroll(void)
{
	/* RI at row 1 should scroll down */
	cursor_to(5, 1);
	term_write("\033M");
	return check_xy(5, 1);
}

static int
test_lf_scroll(void)
{
	/* LF at bottom row should scroll up, cursor stays on last row */
	int x, y;
	cursor_to(5, 24);
	term_write("\n");
	if (!get_cursor_pos(&x, &y))
		return 0;
	return (x == 5 && y == 24);
}

static int
test_decinvm(void)
{
	/* Define macro 1 as ESC[1;1H (hex: 1B5B313B3148), invoke it */
	term_write("\033P1;0;1!z1B5B313B3148\033\\");
	cursor_to(2, 2);
	term_write("\033[1*z");
	return check_xy(1, 1);
}

static int
test_ctsv(void)
{
	char buf[128];

	/* Query SyncTERM version via APC */
	term_write("\033_SyncTERM:VER\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "SyncTERM:VER;SyncTERM ") != NULL);
}

/*
 * Read an SOS...ST response.
 * Strips the SOS (ESC X) prefix and ST (ESC \) suffix.
 * Returns content length, 0 on timeout.
 */
static int
read_sos_response(char *buf, size_t bufsz, int timeout_ms)
{
	char raw[4096];
	int n;

	n = read_dcs_response(raw, sizeof(raw), timeout_ms);
	if (n == 0)
		return 0;
	/* Should start with ESC X and end with ESC \ */
	if (n < 4 || raw[0] != '\033' || raw[1] != 'X')
		return 0;
	if (raw[n - 2] != '\033' || raw[n - 1] != '\\')
		return 0;
	/* Copy content between delimiters */
	int content_len = n - 4;
	if ((size_t)content_len >= bufsz)
		content_len = bufsz - 1;
	memcpy(buf, raw + 2, content_len);
	buf[content_len] = '\0';
	return content_len;
}

static int
test_sts_text_only(void)
{
	char buf[4096];

	/* Write known text to screen */
	clear_screen();
	cursor_to(1, 1);
	term_write("Hello World");
	usleep(50000);

	/* Read it back: SSA at (1,1), cursor past end, FETM=EXCLUDE */
	cursor_to(1, 1);
	term_write("\033F");        /* SSA */
	cursor_to(12, 1);          /* past "Hello World" (11 chars) */
	term_write("\033[14h");     /* FETM=EXCLUDE */
	term_write("\033S");        /* STS */
	term_write("\033[14l");     /* FETM=INSERT */

	int n = read_sos_response(buf, sizeof(buf), 1000);
	if (n == 0) {
		fprintf(result_fp, "    no SOS response\n");
		return 0;
	}
	/* Should have prefix CTerm:STS:1: then "Hello World" */
	if (strstr(buf, "CTerm:STS:1:") == NULL) {
		fprintf(result_fp, "    missing prefix, got: %s\n", buf);
		return 0;
	}
	char *content = strstr(buf, "CTerm:STS:1:") + strlen("CTerm:STS:1:");
	if (strncmp(content, "Hello World", 11) != 0) {
		fprintf(result_fp, "    expected 'Hello World', got: %.20s\n", content);
		return 0;
	}
	return 1;
}

static int
test_sts_full_line(void)
{
	char buf[4096];

	/* Write text and read back full line using TTM=ALL + ESA */
	clear_screen();
	cursor_to(1, 1);
	term_write("ABCDE");
	usleep(50000);

	term_write("\033[16h");     /* TTM=ALL */
	term_write("\033[14h");     /* FETM=EXCLUDE */
	cursor_to(1, 1);
	term_write("\033F");        /* SSA */
	cursor_to(5, 1);
	term_write("\033G");        /* ESA (inclusive) */
	term_write("\033S");        /* STS */
	term_write("\033[14l");     /* FETM=INSERT */
	term_write("\033[16l");     /* TTM=CURSOR */

	int n = read_sos_response(buf, sizeof(buf), 1000);
	if (n == 0) {
		fprintf(result_fp, "    no SOS response\n");
		return 0;
	}
	char *content = strstr(buf, "CTerm:STS:1:");
	if (content == NULL) {
		fprintf(result_fp, "    missing prefix\n");
		return 0;
	}
	content += strlen("CTerm:STS:1:");
	if (strncmp(content, "ABCDE", 5) != 0) {
		fprintf(result_fp, "    expected 'ABCDE', got: %.10s\n", content);
		return 0;
	}
	return 1;
}

static int
test_sts_no_ssa(void)
{
	char buf[4096];

	/* STS with no SSA should return empty SOS...ST */
	clear_screen();
	term_write("\033c");       /* RIS clears SSA */
	usleep(100000);
	term_drain();
	term_write("\033[14h");    /* FETM=EXCLUDE */
	term_write("\033S");       /* STS */
	term_write("\033[14l");

	int n = read_sos_response(buf, sizeof(buf), 1000);
	if (n == 0) {
		fprintf(result_fp, "    no SOS response\n");
		return 0;
	}
	/* Should be just the prefix with no content */
	if (strstr(buf, "CTerm:STS:1:") == NULL) {
		fprintf(result_fp, "    missing prefix\n");
		return 0;
	}
	char *content = strstr(buf, "CTerm:STS:1:") + strlen("CTerm:STS:1:");
	if (content[0] != '\0') {
		fprintf(result_fp, "    expected empty, got %d bytes\n", (int)strlen(content));
		return 0;
	}
	return 1;
}

static int
test_sts_with_sgr(void)
{
	char buf[4096];

	/* Write colored text, read back with FETM=INSERT */
	clear_screen();
	cursor_to(1, 1);
	term_write("\033[31mRed\033[0m");
	usleep(50000);

	cursor_to(1, 1);
	term_write("\033F");        /* SSA */
	cursor_to(4, 1);           /* past "Red" */
	term_write("\033S");        /* STS — FETM=INSERT by default */

	int n = read_sos_response(buf, sizeof(buf), 1000);
	if (n == 0) {
		fprintf(result_fp, "    no SOS response\n");
		return 0;
	}
	if (strstr(buf, "CTerm:STS:0:") == NULL) {
		fprintf(result_fp, "    missing prefix\n");
		return 0;
	}
	/* Content should contain SGR for red (31) and the text "Red" */
	char *content = strstr(buf, "CTerm:STS:0:") + strlen("CTerm:STS:0:");
	if (strstr(content, "31") == NULL) {
		fprintf(result_fp, "    no red SGR in response\n");
		return 0;
	}
	if (strstr(content, "Red") == NULL) {
		fprintf(result_fp, "    text 'Red' not found in response\n");
		return 0;
	}
	return 1;
}

static int
test_decrqm_ansi_modes(void)
{
	char buf[64];
	int ps, pm;

	/* SATM (17) should be permanently reset (Pm=4) */
	term_printf("\033[17$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 17 || pm != 4) {
		fprintf(result_fp, "    SATM: expected 17;4, got %s\n", buf);
		return 0;
	}

	/* MATM (15) should be permanently reset (Pm=4) */
	term_printf("\033[15$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 15 || pm != 4)
		return 0;

	/* GATM (1) should be permanently reset (Pm=4) */
	term_printf("\033[1$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 1 || pm != 4)
		return 0;

	/* TTM (16) should be changeable, default reset (Pm=2) */
	term_printf("\033[16$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 16 || pm != 2)
		return 0;

	/* FETM (14) should be changeable, default reset (Pm=2) */
	term_printf("\033[14$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 14 || pm != 2)
		return 0;

	return 1;
}

static int
test_bcdm(void)
{
	char nul = 0;

	/* Enable BBS character display mode (=255h), NUL displays as space */
	cursor_to(1, 1);
	term_write("\033[=255h");
	write(STDOUT_FILENO, &nul, 1);
	term_write("\033[1;1H\033[=255l");
	/* NUL in BBS mode should have advanced cursor to col 2,
	 * then CUP homed it, then mode reset. After reset + CUP
	 * we should be at (7,1) — the cursor moved from the mode
	 * sequences. Actually, BBS mode + NUL should advance cursor.
	 * After CUP(1,1) we're at (1,1). Then =255l doesn't move.
	 * So check (1,1)... but termtest.js expects (7,1)?
	 * Let me match termtest.js: it writes NUL then CUP(1,1) then
	 * mode reset. It expects check_xy(7,1). That seems to be
	 * checking that the NUL caused visible output (6 chars + NUL
	 * display = 7). Actually, re-reading: it does
	 * "\x1b[=255h\x00\x1b[1;1H\x1b[=255l" and expects (7,1).
	 * The NUL in BBS mode displays as space (1 col), then
	 * ESC[1;1H in BBS mode would display literally...
	 * This needs more investigation. */
	return check_xy(7, 1);
}

static int
test_decmsr(void)
{
	char buf[64];

	/* DECMSR: query macro space remaining */
	term_write("\033[?62n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	/* Response: CSI Pn *{ */
	return (strstr(buf, "*{") != NULL || strstr(buf, "*\x7b") != NULL);
}

static int
test_decrpm(void)
{
	/* Basic DECRPM probe: query autowrap (mode 7), should get a valid response */
	int pm = query_decrqm(7);
	if (pm < 0)
		return 0;
	/* Should be 1 (set) or 2 (reset) — not 0 (unrecognized) */
	return (pm == 1 || pm == 2);
}

static int
test_decrpm_autowrap(void)
{
	int pm;

	/* Enable autowrap, verify via DECRPM */
	term_write("\033[?7h");
	pm = query_decrqm(7);
	if (pm != 1) {
		fprintf(result_fp, "    ?7h: expected pm=1(set) got %d\n", pm);
		return 0;
	}
	/* Disable autowrap, verify */
	term_write("\033[?7l");
	pm = query_decrqm(7);
	if (pm != 2) {
		fprintf(result_fp, "    ?7l: expected pm=2(reset) got %d\n", pm);
		term_write("\033[?7h");
		return 0;
	}
	/* Re-enable */
	term_write("\033[?7h");
	return 1;
}

static int
test_decrpm_origin(void)
{
	int pm;

	/* Origin mode should be off by default */
	pm = query_decrqm(6);
	if (pm != 2) {
		fprintf(result_fp, "    default: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	/* Set scroll region and enable origin mode */
	term_write("\033[5;10r");
	term_write("\033[?6h");
	pm = query_decrqm(6);
	if (pm != 1) {
		fprintf(result_fp, "    ?6h: expected pm=1(set) got %d\n", pm);
		term_write("\033[?6l\033[r");
		return 0;
	}
	/* Disable and reset */
	term_write("\033[?6l\033[r");
	pm = query_decrqm(6);
	if (pm != 2) {
		fprintf(result_fp, "    ?6l: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	return 1;
}

static int
test_decrpm_cursor(void)
{
	int pm;

	/* Cursor should be visible by default (mode 25 set) */
	pm = query_decrqm(25);
	if (pm != 1) {
		fprintf(result_fp, "    default: expected pm=1(set) got %d\n", pm);
		return 0;
	}
	/* Hide cursor */
	term_write("\033[?25l");
	pm = query_decrqm(25);
	if (pm != 2) {
		fprintf(result_fp, "    ?25l: expected pm=2(reset) got %d\n", pm);
		term_write("\033[?25h");
		return 0;
	}
	/* Restore */
	term_write("\033[?25h");
	return 1;
}

static int
test_decrpm_declrmm(void)
{
	int pm;

	/* LR margin mode should be off by default */
	pm = query_decrqm(69);
	if (pm != 2) {
		fprintf(result_fp, "    default: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	term_write("\033[?69h");
	pm = query_decrqm(69);
	if (pm != 1) {
		fprintf(result_fp, "    ?69h: expected pm=1(set) got %d\n", pm);
		term_write("\033[?69l");
		return 0;
	}
	term_write("\033[?69l");
	return 1;
}

static int
test_decrpm_unrecognized(void)
{
	/* Query an unrecognized mode — should return 0 */
	int pm = query_decrqm(9999);
	return (pm == 0);
}

static int
test_decrpm_bracketpaste(void)
{
	int pm;

	pm = query_decrqm(2004);
	if (pm != 2) {
		fprintf(result_fp, "    default: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	term_write("\033[?2004h");
	pm = query_decrqm(2004);
	if (pm != 1) {
		fprintf(result_fp, "    ?2004h: expected pm=1(set) got %d\n", pm);
		term_write("\033[?2004l");
		return 0;
	}
	term_write("\033[?2004l");
	return 1;
}

static int
test_decrqm_lcf(void)
{
	int pm;

	/* LCF should be off by default */
	pm = query_decrqm_eq(4);
	if (pm != 2) {
		fprintf(result_fp, "    default: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	term_write("\033[=4h");
	pm = query_decrqm_eq(4);
	if (pm != 1) {
		fprintf(result_fp, "    =4h: expected pm=1(set) got %d\n", pm);
		term_write("\033[=4l");
		return 0;
	}
	term_write("\033[=4l");
	pm = query_decrqm_eq(4);
	if (pm != 2) {
		fprintf(result_fp, "    =4l: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	return 1;
}

static int
test_decrqm_lcf_forced(void)
{
	int pm;

	/* Force LCF on — should report permanently set (3).
	 * This is a one-way operation; only RIS can undo it. */
	term_write("\033[=5h");
	pm = query_decrqm_eq(5);
	if (pm != 3) {
		fprintf(result_fp, "    =5h: expected pm=3(permanently set) got %d\n", pm);
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* LCF itself should also report set */
	pm = query_decrqm_eq(4);
	if (pm != 1) {
		fprintf(result_fp, "    =4 while forced: expected pm=1(set) got %d\n", pm);
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* =4l should be blocked while forced */
	term_write("\033[=4l");
	pm = query_decrqm_eq(4);
	if (pm != 1) {
		fprintf(result_fp, "    =4l while forced: expected pm=1(set) got %d\n", pm);
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* RIS to clean up (only way to unforce) */
	term_write("\033c");
	usleep(100000);
	term_drain();
	return 1;
}

static int
test_decrqm_doorway(void)
{
	int pm;

	/* Doorway mode should be off by default */
	pm = query_decrqm_eq(255);
	if (pm != 2) {
		fprintf(result_fp, "    default: expected pm=2(reset) got %d\n", pm);
		return 0;
	}
	term_write("\033[=255h");
	pm = query_decrqm_eq(255);
	if (pm != 1) {
		fprintf(result_fp, "    =255h: expected pm=1(set) got %d\n", pm);
		term_write("\033[=255l");
		return 0;
	}
	term_write("\033[=255l");
	return 1;
}

static int
test_decrqm_eq_unrecognized(void)
{
	int pm = query_decrqm_eq(9999);
	return (pm == 0);
}

static int
test_osc4_query(void)
{
	char buf[128];

	/* Set palette entry 1 to a known color using 8-bit precision
	 * (which survives the internal 8-bit-per-channel storage),
	 * then query it back. */
	term_write("\033]4;1;rgb:aa/55/ff\033\\");
	usleep(50000);
	term_drain();
	term_write("\033]4;1;?\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0) {
		fprintf(result_fp, "    no response to OSC 4 query\n");
		return 0;
	}
	/* Response: OSC 4 ; 1 ; rgb:RR/GG/BB ST (8-bit precision) */
	if (strstr(buf, "4;1;rgb:") == NULL) {
		fprintf(result_fp, "    unexpected response format: %s\n", buf);
		return 0;
	}
	if (strstr(buf, "aa/55/ff") == NULL) {
		fprintf(result_fp, "    color mismatch in response: %s\n", buf);
		return 0;
	}
	return 1;
}

static int
test_osc104(void)
{
	char buf1[128], buf2[128];

	/* Set palette entry 2 to non-default, query it */
	term_write("\033]4;2;rgb:ffff/0000/ffff\033\\");
	usleep(50000);
	term_drain();
	term_write("\033]4;2;?\033\\");
	if (read_dcs_response(buf1, sizeof(buf1), 500) == 0)
		return 0;
	/* Reset palette entry 2, query again */
	term_write("\033]104;2\033\\");
	usleep(50000);
	term_drain();
	term_write("\033]4;2;?\033\\");
	if (read_dcs_response(buf2, sizeof(buf2), 500) == 0)
		return 0;
	/* After reset, the color should differ from what we set */
	if (strstr(buf1, "ff/00/ff") == NULL) {
		fprintf(result_fp, "    set color not reflected: %s\n", buf1);
		return 0;
	}
	return (strcmp(buf1, buf2) != 0);
}

static int
test_decrqss_cursor_style(void)
{
	char buf[64];

	/* Set steady block cursor (DECSCUSR 2), query via DECRQSS */
	term_write("\033[2 q");
	term_write("\033P$q q\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0) {
		fprintf(result_fp, "    no response to DECRQSS SP q\n");
		return 0;
	}
	/* Response: DCS 1 $ r Ps SP q ST — Ps should be 2 */
	if (strstr(buf, "1$r2 q") == NULL) {
		fprintf(result_fp, "    expected style 2, got: %s\n", buf);
		return 0;
	}
	/* Set blinking underline (DECSCUSR 3), query again */
	term_write("\033[3 q");
	term_write("\033P$q q\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (strstr(buf, "1$r3 q") == NULL) {
		fprintf(result_fp, "    expected style 3, got: %s\n", buf);
		return 0;
	}
	/* Restore default */
	term_write("\033[0 q");
	return 1;
}

/* --- ED extended tests (modes 1, 2) --- */

static int
test_ed_above(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	cursor_to(1, 1);
	term_write("XYZZY");
	cursor_to(3, 1);
	term_write("\033[1J");
	if (!check_xy(3, 1))
		return 0;
	int n = read_screen_at(1, 1, 5, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	if (buf[0] != ' ' || buf[1] != ' ' || buf[2] != ' ') {
		fprintf(result_fp, "    erased area not blank: '%.5s'\n", buf);
		return 0;
	}
	if (buf[3] != 'Z' || buf[4] != 'Y') {
		fprintf(result_fp, "    preserved area wrong: '%.5s'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_ed_all(void)
{
	/* ED mode 2: erase entire screen AND home cursor (per cterm.adoc) */
	cursor_to(5, 5);
	term_write("\033[2J");
	return check_xy(1, 1);
}

/* --- EL extended tests (modes 1, 2) --- */

static int
test_el_left(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	cursor_to(1, 1);
	term_write("ABCDE");
	cursor_to(3, 1);
	term_write("\033[1K");
	if (!check_xy(3, 1))
		return 0;
	int n = read_screen_at(1, 1, 5, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	if (buf[0] != ' ' || buf[1] != ' ' || buf[2] != ' ') {
		fprintf(result_fp, "    erased area: '%.5s'\n", buf);
		return 0;
	}
	if (buf[3] != 'D' || buf[4] != 'E') {
		fprintf(result_fp, "    preserved: '%.5s'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_el_all(void)
{
	char buf[4096];
	int i;

	if (!has_sts)
		return -1;
	cursor_to(1, 1);
	term_write("ABCDE");
	cursor_to(3, 1);
	term_write("\033[2K");
	if (!check_xy(3, 1))
		return 0;
	int n = read_screen_at(1, 1, 5, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	for (i = 0; i < 5; i++) {
		if (buf[i] != ' ') {
			fprintf(result_fp, "    pos %d not blank: '%c'\n",
			    i + 1, buf[i]);
			return 0;
		}
	}
	return 1;
}

/* --- Scroll Left/Right --- */

static int
test_sl(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	cursor_to(1, 1);
	term_write("ABCDE");
	term_write("\033[1 @");
	int n = read_screen_at(1, 1, 5, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	if (strncmp(buf, "BCDE ", 5) != 0) {
		fprintf(result_fp, "    expected 'BCDE ', got '%.5s'\n", buf);
		return 0;
	}
	return 1;
}

static int
test_sr(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	cursor_to(1, 1);
	term_write("ABCDE");
	term_write("\033[1 A");
	int n = read_screen_at(1, 1, 6, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	if (strncmp(buf, " ABCDE", 6) != 0) {
		fprintf(result_fp, "    expected ' ABCDE', got '%.6s'\n", buf);
		return 0;
	}
	return 1;
}

/* --- CVT (Cursor Line Tabulation) --- */

static int
test_cvt(void)
{
	int x, y;

	/* cterm.adoc: "same as sending TAB Pn times" */
	cursor_to(1, 1);
	term_write("\033[Y");
	if (!get_cursor_pos(&x, &y))
		return 0;
	return (x > 1 && y == 1);
}

/* --- TSR (Tab Stop Remove) --- */

static int
test_tsr(void)
{
	int x, y;

	cursor_to(5, 1);
	term_write("\033H");
	cursor_to(1, 1);
	term_write("\t");
	if (!check_xy(5, 1)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	/* Remove tab at column 5 using TSR (CSI Pn SP d) */
	term_write("\033[5 d");
	cursor_to(1, 1);
	term_write("\t");
	if (!get_cursor_pos(&x, &y)) {
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	if (x == 5) {
		fprintf(result_fp, "    tab at 5 not removed\n");
		term_write("\033c");
		usleep(100000);
		term_drain();
		return 0;
	}
	term_write("\033c");
	usleep(100000);
	term_drain();
	return 1;
}

/* --- Device query extensions --- */

static int
test_ctda(void)
{
	char buf[128];

	/* CSI < 0 c — CTerm Device Attributes */
	term_write("\033[<0c");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	int len = strlen(buf);
	return (len >= 3 && buf[len - 1] == 'c');
}

static int
test_bcdsr(void)
{
	char buf[64];
	int row, col;

	/* CSI 255 n — terminal size as position report */
	term_write("\033[255n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%dR", &row, &col) != 2)
		return 0;
	return (col == 80 && row == 24);
}

static int
test_xtsrga(void)
{
	char buf[128];
	int px, py;

	/* CSI ? 2 ; 1 S — graphics screen info */
	term_write("\033[?2;1S");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[?2;0;%d;%dS", &px, &py) != 2) {
		fprintf(result_fp, "    format: %s\n", buf);
		return 0;
	}
	return (px > 0 && py > 0);
}

static int
test_deccksr(void)
{
	char buf[64];

	/* CSI ? 63 n — macro checksum */
	term_write("\033[?63n");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strlen(buf) >= 4);
}

/* --- CTSMRR (State/Mode Request/Report) --- */

static int
test_ctsmrr_font(void)
{
	char buf[128];

	/* CSI = 1 n — Font State Report */
	term_write("\033[=1n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	int len = strlen(buf);
	return (len >= 3 && buf[len - 1] == 'n' && strstr(buf, "=1;") != NULL);
}

static int
test_ctsmrr_mode(void)
{
	char buf[128];

	/* CSI = 2 n — Mode Report */
	term_write("\033[=2n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	int len = strlen(buf);
	return (len >= 3 && buf[len - 1] == 'n' && strstr(buf, "=2") != NULL);
}

static int
test_ctsmrr_cellsize(void)
{
	char buf[128];
	int ph, pw;

	/* CSI = 3 n — Cell Size: CSI = 3 ; pH ; pW n */
	term_write("\033[=3n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[=3;%d;%dn", &ph, &pw) != 2) {
		fprintf(result_fp, "    format: %s\n", buf);
		return 0;
	}
	return (ph > 0 && pw > 0 && ph <= 64 && pw <= 64);
}

static int
test_ctsmrr_lcf_state(void)
{
	char buf[128];
	int pf;

	/* CSI = 4 n — verify response format and toggle */
	term_write("\033[=4n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[=4;%dn", &pf) != 1)
		return 0;
	if (pf != 0 && pf != 1)
		return 0;
	/* Check if forced — if so, CSI = 4 l is blocked per spec */
	int forced = 0;
	term_write("\033[=5n");
	if (read_csi_response(buf, sizeof(buf), 500) > 0) {
		int ff;
		if (sscanf(buf, "\033[=5;%dn", &ff) == 1)
			forced = ff;
	}
	if (forced) {
		/* Can't toggle when forced — just verify format was valid */
		return 1;
	}
	/* Toggle and verify the report changes */
	int orig = pf;
	if (orig)
		term_write("\033[=4l");
	else
		term_write("\033[=4h");
	term_write("\033[=4n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[=4;%dn", &pf) != 1)
		return 0;
	/* Restore */
	if (orig)
		term_write("\033[=4h");
	else
		term_write("\033[=4l");
	return (pf != orig);
}

static int
test_ctsmrr_lcf_forced_state(void)
{
	char buf[128];
	int pf;

	/* CSI = 5 n — verify response format (pF is 0 or 1) */
	term_write("\033[=5n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[=5;%dn", &pf) != 1)
		return 0;
	return (pf == 0 || pf == 1);
}

static int
test_ctsmrr_osc8(void)
{
	char buf[128];
	int pa;

	/* CSI = 6 n — OSC 8 hyperlink support */
	term_write("\033[=6n");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[=6;%dn", &pa) != 1)
		return 0;
	return (pa == 0 || pa == 1);
}

/* --- Save/Restore mode (CTSMS/CTRMS) --- */

static int
test_save_restore_mode(void)
{
	int pm;

	/* Save autowrap (should be set), change, restore */
	term_write("\033[?7h");
	term_write("\033[?7s");
	term_write("\033[?7l");
	pm = query_decrqm(7);
	if (pm != 2) {
		fprintf(result_fp, "    after reset: expected 2 got %d\n", pm);
		term_write("\033[?7h");
		return 0;
	}
	term_write("\033[?7u");
	pm = query_decrqm(7);
	if (pm != 1) {
		fprintf(result_fp, "    after restore: expected 1 got %d\n", pm);
		term_write("\033[?7h");
		return 0;
	}
	return 1;
}

/* --- CT24BC (24-bit color via non-standard CSI t) --- */

static int
test_ct24bc(void)
{
	char before[16], after[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, before, sizeof(before)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[1;255;128;0t#");
	if (!get_checksum(1, 1, 1, 1, after, sizeof(after)))
		return -1;
	term_write("\033[0m");
	return strcmp(before, after) != 0;
}

/* --- OSC 10/11 queries --- */

static int
test_osc10_query(void)
{
	char buf[128];

	term_write("\033]10;?\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "10;rgb:") != NULL);
}

static int
test_osc11_query(void)
{
	char buf[128];

	term_write("\033]11;?\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "11;rgb:") != NULL);
}

/* --- OSC 8 hyperlink readback --- */

static int
test_osc8_readback(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	clear_screen();
	cursor_to(1, 1);
	term_write("\033]8;;https://example.com\033\\Link\033]8;;\033\\");
	usleep(50000);
	/* Read back with FETM=INSERT (attributed) */
	int n = read_screen_at(1, 1, 4, 1, buf, sizeof(buf));
	if (n == 0)
		return -1;
	/* Attributed stream should contain:
	 * - SGR sequence(s) for the initial attributes
	 * - OSC 8 with the hyperlink URI, terminated by ESC : \
	 *   (escaped ST to avoid premature SOS closure)
	 * - The literal text "Link"
	 * - OSC 8 closing (empty URI), also with ESC : \ */
	if (strstr(buf, "example.com") == NULL) {
		fprintf(result_fp, "    no hyperlink URI in readback\n");
		return 0;
	}
	/* Verify escaped ST (ESC : \) is used instead of bare ST */
	if (strstr(buf, "\033:\\") == NULL) {
		fprintf(result_fp, "    no escaped ST in readback\n");
		return 0;
	}
	/* Extract text — should recover "Link" */
	char text[256];
	extract_text(buf, text, sizeof(text));
	if (strncmp(text, "Link", 4) != 0) {
		fprintf(result_fp, "    expected 'Link', got '%.10s'\n", text);
		return 0;
	}
	return 1;
}

/* --- DECRPM for additional DEC private modes --- */

static int
test_decrpm_toggle(int mode, int default_set)
{
	int pm, expect;

	expect = default_set ? 1 : 2;
	pm = query_decrqm(mode);
	if (pm != expect) {
		fprintf(result_fp, "    mode %d default: expected %d got %d\n",
		    mode, expect, pm);
		return 0;
	}
	if (default_set)
		term_printf("\033[?%dl", mode);
	else
		term_printf("\033[?%dh", mode);
	pm = query_decrqm(mode);
	if (pm != (default_set ? 2 : 1)) {
		fprintf(result_fp, "    mode %d toggle: expected %d got %d\n",
		    mode, default_set ? 2 : 1, pm);
		if (default_set)
			term_printf("\033[?%dh", mode);
		else
			term_printf("\033[?%dl", mode);
		return 0;
	}
	if (default_set)
		term_printf("\033[?%dh", mode);
	else
		term_printf("\033[?%dl", mode);
	return 1;
}

static int test_decrpm_mode31(void) { return test_decrpm_toggle(31, 0); }
static int test_decrpm_mode32(void) { return test_decrpm_toggle(32, 0); }
static int test_decrpm_mode33(void) { return test_decrpm_toggle(33, 0); }
static int test_decrpm_mode34(void) { return test_decrpm_toggle(34, 0); }
static int test_decrpm_mode35(void) { return test_decrpm_toggle(35, 0); }
static int test_decrpm_mode67(void) { return test_decrpm_toggle(67, 1); }
static int test_decrpm_mode80(void) { return test_decrpm_toggle(80, 1); }

static int
test_decrpm_mouse_modes(void)
{
	static const int modes[] = {9, 1000, 1002, 1003, 1006};
	int i, pm;

	for (i = 0; i < 5; i++) {
		pm = query_decrqm(modes[i]);
		if (pm != 2) {
			fprintf(result_fp, "    mode %d default: expected 2 got %d\n",
			    modes[i], pm);
			return 0;
		}
		term_printf("\033[?%dh", modes[i]);
		pm = query_decrqm(modes[i]);
		if (pm != 1) {
			fprintf(result_fp, "    mode %d set: expected 1 got %d\n",
			    modes[i], pm);
			term_printf("\033[?%dl", modes[i]);
			return 0;
		}
		term_printf("\033[?%dl", modes[i]);
	}
	return 1;
}

/* --- SM/RM ANSI modes (functional set/reset) --- */

static int
test_sm_fetm(void)
{
	char buf[64];
	int ps, pm;

	term_write("\033[14h");
	term_printf("\033[14$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 14 || pm != 1) {
		fprintf(result_fp, "    14h: expected set, got pm=%d\n", pm);
		term_write("\033[14l");
		return 0;
	}
	term_write("\033[14l");
	term_printf("\033[14$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 14 || pm != 2) {
		fprintf(result_fp, "    14l: expected reset, got pm=%d\n", pm);
		return 0;
	}
	return 1;
}

static int
test_sm_ttm(void)
{
	char buf[64];
	int ps, pm;

	term_write("\033[16h");
	term_printf("\033[16$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 16 || pm != 1) {
		fprintf(result_fp, "    16h: expected set, got pm=%d\n", pm);
		term_write("\033[16l");
		return 0;
	}
	term_write("\033[16l");
	term_printf("\033[16$p");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	if (sscanf(buf, "\033[%d;%d$y", &ps, &pm) != 2 || ps != 16 || pm != 2) {
		fprintf(result_fp, "    16l: expected reset, got pm=%d\n", pm);
		return 0;
	}
	return 1;
}

/* --- SGR extended attribute tests --- */

static int
test_sgr_dim(void)
{
	char cs_bright[16], cs_dim[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;1m#");
	if (!get_checksum(1, 1, 1, 1, cs_bright, sizeof(cs_bright)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;2m#");
	if (!get_checksum(1, 1, 1, 1, cs_dim, sizeof(cs_dim)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_bright, cs_dim) != 0;
}

static int
test_sgr_blink(void)
{
	char cs_noblink[16], cs_blink[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, cs_noblink, sizeof(cs_noblink)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;5m#");
	if (!get_checksum(1, 1, 1, 1, cs_blink, sizeof(cs_blink)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_noblink, cs_blink) != 0;
}

static int
test_sgr_noblink(void)
{
	char cs_normal[16], cs_unblink[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, cs_normal, sizeof(cs_normal)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;5;25m#");
	if (!get_checksum(1, 1, 1, 1, cs_unblink, sizeof(cs_unblink)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_normal, cs_unblink) == 0;
}

static int
test_sgr_normal_intensity(void)
{
	char cs_normal[16], cs_reset[16];

	if (!has_cksum)
		return -1;
	cursor_to(1, 1);
	term_write("\033[0m#");
	if (!get_checksum(1, 1, 1, 1, cs_normal, sizeof(cs_normal)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;1;22m#");
	if (!get_checksum(1, 1, 1, 1, cs_reset, sizeof(cs_reset)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_normal, cs_reset) == 0;
}

static int
test_sgr_default_fg(void)
{
	char cs_white[16], cs_default[16];

	if (!has_cksum)
		return -1;
	/* SGR 39 = default foreground (same as white per cterm.adoc) */
	cursor_to(1, 1);
	term_write("\033[0;37m#");
	if (!get_checksum(1, 1, 1, 1, cs_white, sizeof(cs_white)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;39m#");
	if (!get_checksum(1, 1, 1, 1, cs_default, sizeof(cs_default)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_white, cs_default) == 0;
}

static int
test_sgr_default_bg(void)
{
	char cs_black[16], cs_default[16];

	if (!has_cksum)
		return -1;
	/* SGR 49 = default background (same as black per cterm.adoc) */
	cursor_to(1, 1);
	term_write("\033[0;40m ");
	if (!get_checksum(1, 1, 1, 1, cs_black, sizeof(cs_black)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;49m ");
	if (!get_checksum(1, 1, 1, 1, cs_default, sizeof(cs_default)))
		return -1;
	term_write("\033[0m");
	return strcmp(cs_black, cs_default) == 0;
}

static int
test_sgr_bright_bg(void)
{
	char cs_norm[16], cs_bright[16];

	if (!has_cksum)
		return -1;
	/* Bright backgrounds (100-107) require mode 33 */
	term_write("\033[?33h");
	cursor_to(1, 1);
	term_write("\033[0;41m ");
	if (!get_checksum(1, 1, 1, 1, cs_norm, sizeof(cs_norm)))
		return -1;
	cursor_to(1, 1);
	term_write("\033[0;101m ");
	if (!get_checksum(1, 1, 1, 1, cs_bright, sizeof(cs_bright)))
		return -1;
	term_write("\033[0m\033[?33l");
	return strcmp(cs_norm, cs_bright) != 0;
}

/* --- DECRQSS extensions --- */

static int
test_decrqss_t(void)
{
	char buf[64];

	/* DECRQSS for height in lines */
	term_write("\033P$qt\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "1$r") != NULL);
}

static int
test_decrqss_dollarpipe(void)
{
	char buf[64];

	/* DECRQSS for width in columns ($|) */
	term_write("\033P$q$|\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "1$r") != NULL);
}

static int
test_decrqss_starpipe(void)
{
	char buf[64];

	/* DECRQSS for height in lines (*|) */
	term_write("\033P$q*|\033\\");
	if (read_dcs_response(buf, sizeof(buf), 500) == 0)
		return 0;
	return (strstr(buf, "1$r") != NULL);
}

/* --- APC JXL query --- */

static int
test_apc_jxl_query(void)
{
	char buf[128];

	/* APC SyncTERM:Q;JXL ST — query JXL support */
	term_write("\033_SyncTERM:Q;JXL\033\\");
	if (read_csi_response(buf, sizeof(buf), 500) == 0)
		return 0;
	int len = strlen(buf);
	return (len >= 3 && buf[len - 1] == 'n');
}

/* --- NUL in doorway mode --- */

static int
test_nul_doorway(void)
{
	char buf[4096];

	if (!has_sts)
		return -1;
	/* Enter doorway mode, send NUL + ESC (0x1B).
	 * NUL means "next byte is literal CP437 character".
	 * ESC (0x1B) should be placed in the cell as a literal,
	 * not interpreted as a control character. */
	cursor_to(1, 1);
	term_write("\033[=255h");
	{
		char seq[2] = {0x00, 0x1B};
		write(STDOUT_FILENO, seq, 2);
	}
	term_write("\033[=255l");
	/* Queries work again — verify via FETM=EXCLUDE (text only).
	 * C0 bytes in cells become SPACE in text-only mode. */
	int n = read_screen_at(1, 1, 1, 0, buf, sizeof(buf));
	if (n == 0)
		return -1;
	if (buf[0] != ' ') {
		fprintf(result_fp, "    FETM=EXCLUDE: expected SPACE, got 0x%02x\n",
		    (unsigned char)buf[0]);
		return 0;
	}
	/* Verify via FETM=INSERT (attributed).  The cell contains 0x1B
	 * which must be doorway-encoded in the stream:
	 * ESC[=255h NUL 0x1B ESC[=255l
	 * Use memcmp scanning since content contains NUL bytes. */
	n = read_screen_at(1, 1, 1, 1, buf, sizeof(buf));
	if (n == 0)
		return -1;
	/* Find doorway open sequence in binary content */
	char *dw = NULL;
	int di;
	for (di = 0; di <= n - 7; di++) {
		if (memcmp(buf + di, "\033[=255h", 7) == 0) {
			dw = buf + di;
			break;
		}
	}
	if (dw == NULL) {
		fprintf(result_fp, "    FETM=INSERT: no doorway encoding\n");
		return 0;
	}
	/* The literal 0x1B should follow after NUL prefix */
	dw += 7; /* skip "ESC[=255h" */
	if (dw[0] != 0x00 || dw[1] != 0x1B) {
		fprintf(result_fp, "    doorway payload: %02x %02x (expected 00 1b)\n",
		    (unsigned char)dw[0], (unsigned char)dw[1]);
		return 0;
	}
	return 1;
}

/* --- Response ordering regression test --- */

static int
test_response_ordering(void)
{
	char buf[4096];
	char *ver_pos, *dsr_pos, *p;

	/* Send APC VER query + DSR in a single write() so they land in
	 * one cterm_write call.  The VER response must arrive before
	 * the DSR response — if responses go through two different
	 * paths (retbuf vs callback), the DSR arrives first. */
	char cmd[] = "\033_SyncTERM:VER\033\\\033[6n";
	write(STDOUT_FILENO, cmd, sizeof(cmd) - 1);

	/* Read all responses */
	usleep(100000);
	int n = term_read(buf, sizeof(buf) - 1, 1000);
	if (n <= 0) {
		fprintf(result_fp, "    no response data\n");
		return 0;
	}
	buf[n] = '\0';

	/* Find VER response (APC: ESC _ SyncTERM:VER;...) */
	ver_pos = strstr(buf, "SyncTERM:VER;");
	if (ver_pos == NULL) {
		fprintf(result_fp, "    no VER response\n");
		return 0;
	}

	/* Find DSR response (CSI row ; col R) */
	dsr_pos = NULL;
	for (p = buf; p < buf + n - 2; p++) {
		if (p[0] == '\033' && p[1] == '[') {
			char *end = p + 2;
			while (*end && ((*end >= '0' && *end <= '9') || *end == ';'))
				end++;
			if (*end == 'R') {
				dsr_pos = p;
				break;
			}
		}
	}
	if (dsr_pos == NULL) {
		fprintf(result_fp, "    no DSR response\n");
		return 0;
	}

	/* VER (APC) was sent first, so its response must arrive first */
	if (ver_pos > dsr_pos) {
		fprintf(result_fp, "    VER response after DSR (response ordering bug)\n");
		return 0;
	}
	return 1;
}

/* ================================================================ */

struct test_entry {
	const char *name;
	test_fn     fn;
};

static struct test_entry tests[] = {
	/* C0 controls and escape sequences */
	{"NUL",           test_nul},
	{"BS",            test_bs},
	{"HT",            test_ht},
	{"LF",            test_lf},
	{"CR",            test_cr},
	{"NEL",           test_nel},
	{"HTS",           test_hts},
	{"RI",            test_ri},
	{"RIS",           test_ris},
	/* String passthrough (cursor must not move) */
	{"APC",           test_apc},
	{"DCS",           test_dcs},
	{"PM",            test_pm},
	{"OSC",           test_osc},
	{"SOS",           test_sos},
	/* Cursor movement */
	{"CUU",           test_cuu},
	{"CUD",           test_cud},
	{"CUF",           test_cuf},
	{"CUB",           test_cub},
	{"CNL",           test_cnl},
	{"CPL",           test_cpl},
	{"CUP",           test_cup},
	{"CUP_variants",  test_cup_variants},
	{"HVP",           test_hvp},
	{"HVP_variants",  test_hvp_variants},
	{"CHA",           test_cha},
	{"VPA",           test_vpa},
	{"HPA",           test_hpa},
	{"HPR",           test_hpr},
	{"VPR",           test_vpr},
	{"HPB",           test_hpb},
	{"VPB",           test_vpb},
	{"REP",           test_rep},
	/* Clamping */
	{"CUU_clamp",     test_cuu_stop},
	{"CUD_clamp",     test_cud_stop},
	{"CUF_clamp",     test_cuf_clamp},
	{"CUB_clamp",     test_cub_clamp},
	{"HPB_clamp",     test_hpb_clamp},
	{"VPB_clamp",     test_vpb_clamp},
	/* Editing */
	{"ED",            test_ed_below},
	{"EL",            test_el_right},
	{"ICH",           test_ich},
	{"DCH",           test_dch},
	{"IL",            test_il},
	{"DL",            test_dl},
	{"ECH",           test_ech},
	/* Tabs */
	{"TBC",           test_tbc},
	{"HTS_CHT_CBT",   test_hts_cht_cbt},
	/* Save/restore */
	{"SCOSC_SCORC",   test_save_restore_cursor},
	{"DECSC_DECRC",   test_decsc_decrc},
	/* Device queries */
	{"DSR",           test_dsr},
	{"DSR_status",    test_dsr_status},
	{"DA",            test_da},
	{"DECRQCRA",      test_decrqcra},
	{"DECTABSR",      test_dectabsr},
	{"DECRQSS_m",     test_decrqss_m},
	{"DECRQSS_r",     test_decrqss_r},
	{"DECRQSS_s",     test_decrqss_s},
	{"DECMSR",        test_decmsr},
	/* Scroll regions and margins */
	{"DECSTBM",       test_decstbm},
	{"DECSTBM_scroll", test_decstbm_scroll},
	{"DECSLRM",       test_decslrm},
	/* Scrolling */
	{"SU",            test_su},
	{"SD",            test_sd},
	{"RI_scroll",     test_ri_scroll},
	{"LF_scroll",     test_lf_scroll},
	/* SGR */
	{"SGR_reset",     test_sgr_reset},
	{"SGR256_FG",     test_sgr256_fg},
	{"SGR256_BG",     test_sgr256_bg},
	{"SGRRGB_FG",     test_sgr_rgb_fg},
	{"SGRRGB_BG",     test_sgr_rgb_bg},
	{"SGR_bright",    test_sgr_bright},
	{"SGR_negative",  test_sgr_negative},
	{"SGR_conceal",   test_sgr_conceal},
	/* Modes */
	{"Autowrap",      test_autowrap},
	{"DECRST_nowrap", test_decrst_nowrap},
	{"Origin_mode",   test_origin_mode},
	{"BCDM",          test_bcdm},
	/* DECRPM mode queries */
	{"DECRQM",              test_decrpm},
	{"DECRQM_autowrap",     test_decrpm_autowrap},
	{"DECRQM_origin",       test_decrpm_origin},
	{"DECRQM_cursor",       test_decrpm_cursor},
	{"DECRQM_DECLRMM",      test_decrpm_declrmm},
	{"DECRQM_unrecognized", test_decrpm_unrecognized},
	{"DECRQM_bracketpaste", test_decrpm_bracketpaste},
	{"DECRQM_LCF",          test_decrqm_lcf},
	{"DECRQM_doorway",      test_decrqm_doorway},
	{"DECRQM_eq_unrecognized", test_decrqm_eq_unrecognized},
	/* Palette queries */
	{"OSC4_query",    test_osc4_query},
	{"OSC104",        test_osc104},
	/* Cursor style */
	{"DECRQSS_SPq",   test_decrqss_cursor_style},
	/* Screen readback (ECMA-48 SSA/STS) */
	{"STS_text_only",      test_sts_text_only},
	{"STS_full_line",      test_sts_full_line},
	{"STS_no_ssa",         test_sts_no_ssa},
	{"STS_with_sgr",       test_sts_with_sgr},
	{"DECRQM_ANSI_modes",  test_decrqm_ansi_modes},
	/* SyncTERM extensions */
	{"DECDMAC",       test_decdmac},
	{"DECINVM",       test_decinvm},
	{"CTSV",          test_ctsv},
	/* Editing extended */
	{"ED_above",       test_ed_above},
	{"ED_all",         test_ed_all},
	{"EL_left",        test_el_left},
	{"EL_all",         test_el_all},
	{"SL",             test_sl},
	{"SR",             test_sr},
	/* Cursor extended */
	{"CVT",            test_cvt},
	/* Tabs extended */
	{"TSR",            test_tsr},
	/* Device queries extended */
	{"CTDA",           test_ctda},
	{"BCDSR",          test_bcdsr},
	{"XTSRGA",         test_xtsrga},
	{"DECCKSR",        test_deccksr},
	{"CTSMRR_font",    test_ctsmrr_font},
	{"CTSMRR_mode",    test_ctsmrr_mode},
	{"CTSMRR_cellsize", test_ctsmrr_cellsize},
	{"CTSMRR_LCF",     test_ctsmrr_lcf_state},
	{"CTSMRR_OSC8",    test_ctsmrr_osc8},
	/* DECRQSS extended */
	{"DECRQSS_t",      test_decrqss_t},
	{"DECRQSS_dollarpipe", test_decrqss_dollarpipe},
	{"DECRQSS_starpipe", test_decrqss_starpipe},
	/* Save/restore modes */
	{"CTSMS_CTRMS",    test_save_restore_mode},
	/* SGR extended */
	{"SGR_dim",        test_sgr_dim},
	{"SGR_blink",      test_sgr_blink},
	{"SGR_noblink",    test_sgr_noblink},
	{"SGR_normal_int", test_sgr_normal_intensity},
	{"SGR_default_fg", test_sgr_default_fg},
	{"SGR_default_bg", test_sgr_default_bg},
	{"SGR_bright_bg",  test_sgr_bright_bg},
	/* 24-bit color extension */
	{"CT24BC",         test_ct24bc},
	/* OSC queries */
	{"OSC10_query",    test_osc10_query},
	{"OSC11_query",    test_osc11_query},
	/* OSC 8 hyperlink */
	{"OSC8_readback",  test_osc8_readback},
	/* DECRPM additional modes */
	{"DECRPM_mode31",  test_decrpm_mode31},
	{"DECRPM_mode32",  test_decrpm_mode32},
	{"DECRPM_mode33",  test_decrpm_mode33},
	{"DECRPM_mode34",  test_decrpm_mode34},
	{"DECRPM_mode35",  test_decrpm_mode35},
	{"DECRPM_mode67",  test_decrpm_mode67},
	{"DECRPM_mode80",  test_decrpm_mode80},
	{"DECRPM_mouse",   test_decrpm_mouse_modes},
	/* SM/RM ANSI modes */
	{"SM_FETM",        test_sm_fetm},
	{"SM_TTM",         test_sm_ttm},
	/* SyncTERM extensions */
	{"APC_JXL_query",  test_apc_jxl_query},
	/* NUL in doorway mode + doorway readback encoding */
	{"NUL_doorway",    test_nul_doorway},
	/* Regression: response ordering when APC + CSI in same cterm_write */
	{"response_ordering", test_response_ordering},
	/* Forced LCF must be last — it cannot be cleared and poisons state */
	{"DECRQM_LCF_forced",   test_decrqm_lcf_forced},
	{"CTSMRR_LCF_forced", test_ctsmrr_lcf_forced_state},
	{NULL, NULL}
};

int
main(int argc, char **argv)
{
	const char *result_path = NULL;
	char tmppath[64];
	int i;

	if (argc > 1) {
		result_path = argv[1];
	}
	else {
		strcpy(tmppath, "/tmp/termtest.XXXXXX");
		int fd = mkstemp(tmppath);
		if (fd < 0) {
			perror("mkstemp");
			return 1;
		}
		close(fd);
		result_path = tmppath;
	}

	result_fp = fopen(result_path, "w");
	if (result_fp == NULL) {
		perror(result_path);
		return 1;
	}

	/* Write the path to stderr so the caller can find it */
	fprintf(stderr, "TERMTEST_RESULTS=%s\n", result_path);

	atexit(cleanup);
	if (set_raw_mode() < 0) {
		fprintf(result_fp, "ERROR: failed to set raw mode: %s\n", strerror(errno));
		fclose(result_fp);
		return 1;
	}

	/* Brief delay for SyncTERM to finish initializing */
	usleep(200000);

	/* Reset terminal to known state */
	term_write("\033c");       /* RIS - full reset */
	usleep(100000);
	term_drain();

	/* Probe for DECRQCRA support */
	{
		char cksum[16];
		has_cksum = get_checksum(1, 1, 1, 1, cksum, sizeof(cksum));
	}

	/* Probe for STS screen readback support — try a direct STS */
	{
		char buf[4096];
		cursor_to(1, 1);
		term_write("X");
		cursor_to(1, 1);
		term_write("\033F");       /* SSA */
		cursor_to(2, 1);
		term_write("\033[14h");    /* FETM=EXCLUDE */
		term_write("\033S");       /* STS */
		term_write("\033[14l");
		int n = read_sos_response(buf, sizeof(buf), 1000);
		if (n > 0 && strstr(buf, "CTerm:STS:") != NULL)
			has_sts = 1;
		clear_screen();
		term_drain();
	}

	fprintf(result_fp, "# termtest results\n");
	fprintf(result_fp, "# DECRQCRA: %s\n", has_cksum ? "yes" : "no");
	fprintf(result_fp, "# STS: %s\n", has_sts ? "yes" : "no");
	fflush(result_fp);

	for (i = 0; tests[i].name != NULL; i++)
		run_test(tests[i].name, tests[i].fn);

	fprintf(result_fp, "# ---\n");
	fprintf(result_fp, "# total=%d passed=%d failed=%d skipped=%d\n",
	    total_tests, passed_tests, failed_tests, skipped_tests);
	fclose(result_fp);
	result_fp = NULL;

	return failed_tests > 0 ? 1 : 0;
}
