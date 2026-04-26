/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Foreign-method bindings for the Wren scripting host.  These wrap
 * SyncTERM's conio / connection / cterm / bbslist / cache APIs and
 * expose hook-registration entry points; lifecycle and dispatch live
 * in wren_host.c. */

#include "wren_host_internal.h"
#include "wren_host.h"
#include "wren.h"

#include "ciolib.h"
#include "cterm.h"
#include "syncterm.h"
#include "bbslist.h"
#include "conn.h"
#include "term.h"
#include "utf8_codepages.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* term.c globals — emulation state and current bbs context. */
extern struct cterminal *cterm;

/* ----- Conio ------------------------------------------------------- */

static void
fn_Conio_putch(WrenVM *vm)
{
	int c = (int)wrenGetSlotDouble(vm, 1);
	putch(c);
}

static void
fn_Conio_cputs(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	for (int i = 0; i < len; i++)
		putch((unsigned char)s[i]);
}

static void
fn_Conio_gotoxy(WrenVM *vm)
{
	int x = (int)wrenGetSlotDouble(vm, 1);
	int y = (int)wrenGetSlotDouble(vm, 2);
	gotoxy(x, y);
}

static void
fn_Conio_wherex(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wherex());
}

static void
fn_Conio_wherey(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wherey());
}

static void
fn_Conio_textattr(WrenVM *vm)
{
	int a = (int)wrenGetSlotDouble(vm, 1);
	textattr(a);
}

static void
fn_Conio_screenSize(WrenVM *vm)
{
	struct text_info ti;
	gettextinfo(&ti);
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)ti.screenwidth);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)ti.screenheight);
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fn_Conio_ungetch(WrenVM *vm)
{
	int k = (int)wrenGetSlotDouble(vm, 1);
	ungetch(k);
}

static void
fn_Conio_getch(WrenVM *vm)
{
	/* ciolib_getch returns one raw byte at a time; assemble the 16-bit
	 * extended-key value here so scripts get the same encoding rip_getch
	 * exposes (e.g. Esc=0x011B, Ctrl+`=0x29E0).  CIO_KEY_LITERAL_E0
	 * decodes back to the literal 0xe0 byte. */
	int ch = getch();
	if (ch == 0 || ch == 0xe0) {
		int ch2 = getch();
		ch |= ch2 << 8;
		if (ch == CIO_KEY_LITERAL_E0)
			ch = 0xe0;
	}
	wrenSetSlotDouble(vm, 0, (double)ch);
}

static void
fn_Conio_kbhit(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, kbhit() != 0);
}

static void
fn_Conio_clrscr(WrenVM *vm)
{
	(void)vm;
	clrscr();
}

static void
fn_Conio_clreol(WrenVM *vm)
{
	(void)vm;
	clreol();
}

/* ciolib_savescreen returns a malloc'd struct ciolib_screen *.
 * Round-trip the pointer through a Wren Num — pointers are typically
 * aligned and fit in the 53-bit double mantissa on every supported
 * host.  restorescreen frees the screen on success. */
static void
fn_Conio_savescreen(WrenVM *vm)
{
	struct ciolib_screen *scrn = savescreen();
	wrenSetSlotDouble(vm, 0, (double)(uintptr_t)scrn);
}

static void
fn_Conio_restorescreen(WrenVM *vm)
{
	uintptr_t v = (uintptr_t)wrenGetSlotDouble(vm, 1);
	struct ciolib_screen *scrn = (struct ciolib_screen *)v;
	if (scrn == NULL)
		return;
	restorescreen(scrn);
	freescreen(scrn);
}

/* ----- Conn -------------------------------------------------------- */

static void
fn_Conn_send(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	/* Use raw variant so onOutput hooks don't recurse when a script
	 * sends from inside its own hook. */
	if (len > 0)
		conn_send_raw(s, (size_t)len, 0);
}

static void
fn_Conn_sendRaw(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	if (len > 0)
		conn_send_raw(s, (size_t)len, 0);
}

static void
fn_Conn_close(WrenVM *vm)
{
	(void)vm;
	conn_close();
}

static void
fn_Conn_connected(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, conn_connected());
}

static void
fn_Conn_type(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	int t = (st != NULL && st->bbs != NULL) ? st->bbs->conn_type : 0;
	wrenSetSlotDouble(vm, 0, (double)t);
}

static void
fn_Conn_inbufBytes(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_inbuf));
}

static void
fn_Conn_outbufBytes(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)conn_buf_bytes(&conn_outbuf));
}

static void
fn_Conn_inbufPeek(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if (n > 65536)
		n = 65536;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_peek(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

static void
fn_Conn_inbufGet(WrenVM *vm)
{
	int n = (int)wrenGetSlotDouble(vm, 1);
	if (n <= 0) {
		wrenSetSlotBytes(vm, 0, "", 0);
		return;
	}
	if (n > 65536)
		n = 65536;
	char *tmp = malloc((size_t)n);
	if (tmp == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = conn_buf_get(&conn_inbuf, tmp, (size_t)n);
	wrenSetSlotBytes(vm, 0, tmp, got);
	free(tmp);
}

static void
fn_Conn_outbufPut(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	size_t put = 0;
	if (len > 0)
		put = conn_buf_put(&conn_outbuf, s, (size_t)len);
	wrenSetSlotDouble(vm, 0, (double)put);
}

/* ----- Cterm ------------------------------------------------------- */

static void
fn_Cterm_emulation(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->emulation : 0.0);
}

static void
fn_Cterm_x(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->xpos : 0.0);
}

static void
fn_Cterm_y(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->ypos : 0.0);
}

static void
fn_Cterm_attr(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->attr : 0.0);
}

static void
fn_Cterm_doorwayMode(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->doorway_mode : 0.0);
}

static void
fn_Cterm_music(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0,
	    cterm != NULL ? (double)cterm->music_enable : 0.0);
}

static void
fn_Cterm_width(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->width : 0.0);
}

static void
fn_Cterm_height(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, cterm != NULL ? (double)cterm->height : 0.0);
}

static void
fn_Cterm_write(WrenVM *vm)
{
	int len = 0;
	const char *s = wrenGetSlotBytes(vm, 1, &len);
	int speed = 0;
	if (cterm != NULL && len > 0)
		cterm_write(cterm, s, len, &speed);
}

/* ----- BBS --------------------------------------------------------- */

#define BBS_FIELD_STR(setter)                                           \
	do {                                                            \
		struct wren_host_state *st = wren_host_state();         \
		if (st != NULL && st->bbs != NULL)                      \
			wrenSetSlotString(vm, 0, st->bbs->setter);      \
		else                                                    \
			wrenSetSlotString(vm, 0, "");                   \
	} while (0)

#define BBS_FIELD_NUM(field)                                            \
	do {                                                            \
		struct wren_host_state *st = wren_host_state();         \
		double v = (st != NULL && st->bbs != NULL)              \
		    ? (double)st->bbs->field : 0.0;                     \
		wrenSetSlotDouble(vm, 0, v);                            \
	} while (0)

static void fn_BBS_name(WrenVM *vm)     { BBS_FIELD_STR(name); }
static void fn_BBS_addr(WrenVM *vm)     { BBS_FIELD_STR(addr); }
static void fn_BBS_port(WrenVM *vm)     { BBS_FIELD_NUM(port); }
static void fn_BBS_connType(WrenVM *vm) { BBS_FIELD_NUM(conn_type); }
static void fn_BBS_user(WrenVM *vm)     { BBS_FIELD_STR(user); }
static void fn_BBS_password(WrenVM *vm) { BBS_FIELD_STR(password); }
static void fn_BBS_syspass(WrenVM *vm)  { BBS_FIELD_STR(syspass); }
static void fn_BBS_music(WrenVM *vm)    { BBS_FIELD_NUM(music); }
static void fn_BBS_rip(WrenVM *vm)      { BBS_FIELD_NUM(rip); }
static void fn_BBS_comment(WrenVM *vm)  { BBS_FIELD_STR(comment); }

/* ----- Cache ------------------------------------------------------- */

static void
fn_Cache_basePath(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	char fn[MAX_PATH + 1] = "";
	if (st != NULL && st->bbs != NULL)
		get_cache_fn_base(st->bbs, fn, sizeof(fn));
	wrenSetSlotString(vm, 0, fn);
}

static void
fn_Cache_subdirPath(WrenVM *vm)
{
	struct wren_host_state *st = wren_host_state();
	const char *sub = wrenGetSlotString(vm, 1);
	char fn[MAX_PATH + 1] = "";
	if (st != NULL && st->bbs != NULL && sub != NULL)
		get_cache_fn_subdir(st->bbs, fn, sizeof(fn), sub);
	wrenSetSlotString(vm, 0, fn);
}

static void
fn_Cache_readFile(WrenVM *vm)
{
	const char *name = wrenGetSlotString(vm, 1);
	struct wren_host_state *st = wren_host_state();
	if (name == NULL || st == NULL || st->bbs == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char base[MAX_PATH + 1];
	get_cache_fn_base(st->bbs, base, sizeof(base));
	char path[MAX_PATH + 1];
	snprintf(path, sizeof(path), "%s%s", base, name);
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 16 * 1024 * 1024) {
		fclose(f);
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *buf = malloc((size_t)sz);
	if (buf == NULL) {
		fclose(f);
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t got = fread(buf, 1, (size_t)sz, f);
	fclose(f);
	wrenSetSlotBytes(vm, 0, buf, got);
	free(buf);
}

static void
fn_Cache_writeFile(WrenVM *vm)
{
	const char *name = wrenGetSlotString(vm, 1);
	int len = 0;
	const char *contents = wrenGetSlotBytes(vm, 2, &len);
	struct wren_host_state *st = wren_host_state();
	if (name == NULL || contents == NULL || st == NULL || st->bbs == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char base[MAX_PATH + 1];
	get_cache_fn_base(st->bbs, base, sizeof(base));
	char path[MAX_PATH + 1];
	snprintf(path, sizeof(path), "%s%s", base, name);
	FILE *f = fopen(path, "wb");
	if (f == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	size_t put = fwrite(contents, 1, (size_t)len, f);
	fclose(f);
	wrenSetSlotBool(vm, 0, put == (size_t)len);
}

/* ----- Console (log buffer accessor) ------------------------------ */

static void
fn_Console_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_count());
}

static void
fn_Console_total(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, (double)wren_log_total());
}

/* `Console[seq]` — entry by sequence number.  Returns null when seq
 * is outside the currently-buffered range (already evicted, or not
 * yet written). */
static void
fn_Console_subscript(WrenVM *vm)
{
	double d = wrenGetSlotDouble(vm, 1);
	if (d < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wren_log_emit(vm, (uint64_t)d, 0);
}

static void
fn_Console_clear(WrenVM *vm)
{
	(void)vm;
	wren_log_clear();
}

/* Wren iteration protocol over seq numbers: start at the oldest seq
 * still in the buffer, end at total-1 (inclusive).  Skipped if the
 * buffer is empty.  iteratorValue returns the entry at that seq. */
static void
fn_Console_iterate(WrenVM *vm)
{
	int      count = wren_log_count();
	uint64_t total = wren_log_total();
	if (count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, (double)(total - (uint64_t)count));
		return;
	}
	uint64_t next = (uint64_t)wrenGetSlotDouble(vm, 1) + 1;
	if (next >= total)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)next);
}

static void
fn_Console_iteratorValue(WrenVM *vm)
{
	uint64_t seq = (uint64_t)wrenGetSlotDouble(vm, 1);
	wren_log_emit(vm, seq, 0);
}

/* ----- Hook -------------------------------------------------------- */

static void
fn_Hook_onKey(WrenVM *vm)    { wren_host_register_hook(vm, WREN_HOOK_KEY,    1); }
static void
fn_Hook_onInput(WrenVM *vm)  { wren_host_register_hook(vm, WREN_HOOK_INPUT,  1); }
static void
fn_Hook_onOutput(WrenVM *vm) { wren_host_register_hook(vm, WREN_HOOK_OUTPUT, 1); }
static void
fn_Hook_onMouse(WrenVM *vm)  { wren_host_register_hook(vm, WREN_HOOK_MOUSE,  1); }
static void
fn_Hook_onStatus(WrenVM *vm) { wren_host_register_hook(vm, WREN_HOOK_STATUS, 1); }

static void
fn_Hook_every(WrenVM *vm)
{
	int ms = (int)wrenGetSlotDouble(vm, 1);
	wren_host_register_timer(vm, ms, 2);
}

/* ----- Cell / Cells / vmem text bindings --------------------------
 *
 * `Cell` wraps a single struct vmem_cell.  Two flavours: standalone
 * (Cell.new()) owns its own bytes; view (created by Cells.[i]) reads
 * and writes through to its parent Cells's buffer, with a Wren-level
 * pin on the parent so the buffer outlives the view.
 *
 * `Cells` owns a malloc'd buffer of vmem_cells returned by getText.
 * It is iterable and indexable but has no add/insert/clear bindings,
 * so scripts can mutate individual cells but not change the count.
 * -------------------------------------------------------------------- */

struct wren_cells {
	int               count;
	struct vmem_cell *buf;          /* malloc'd; freed at finalize */
};

struct wren_cell {
	struct vmem_cell   c;            /* used when standalone */
	struct wren_cells *parent;       /* NULL = standalone */
	int                index;        /* index into parent->buf in view mode */
	WrenHandle        *parent_handle;/* pin on parent for view mode */
};

static struct vmem_cell *
cell_data(struct wren_cell *wc)
{
	return (wc->parent != NULL) ? &wc->parent->buf[wc->index] : &wc->c;
}

static struct vmem_cell *
slot_cell_data(WrenVM *vm, int slot)
{
	return cell_data(wrenGetSlotForeign(vm, slot));
}

/* Decode the first UTF-8 codepoint from `s`; return bytes consumed,
 * or 0 on truncated/invalid input. */
static int
decode_utf8_first(const char *s, int len, uint32_t *cp_out)
{
	if (s == NULL || len <= 0)
		return 0;
	unsigned char b0 = (unsigned char)s[0];
	if (b0 < 0x80) {
		*cp_out = b0;
		return 1;
	}
	int      n;
	uint32_t cp;
	if ((b0 & 0xE0) == 0xC0) {
		n  = 2;
		cp = b0 & 0x1Fu;
	} else if ((b0 & 0xF0) == 0xE0) {
		n  = 3;
		cp = b0 & 0x0Fu;
	} else if ((b0 & 0xF8) == 0xF0) {
		n  = 4;
		cp = b0 & 0x07u;
	} else {
		return 0;
	}
	if (len < n)
		return 0;
	for (int i = 1; i < n; i++) {
		unsigned char bi = (unsigned char)s[i];
		if ((bi & 0xC0) != 0x80)
			return 0;
		cp = (cp << 6) | (bi & 0x3Fu);
	}
	*cp_out = cp;
	return n;
}

/* Encode codepoint `cp` as UTF-8 into `out`; return bytes written. */
static int
encode_utf8(uint32_t cp, char out[4])
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0u | (cp >> 6));
		out[1] = (char)(0x80u | (cp & 0x3Fu));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xE0u | (cp >> 12));
		out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		out[2] = (char)(0x80u | (cp & 0x3Fu));
		return 3;
	}
	out[0] = (char)(0xF0u | (cp >> 18));
	out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
	out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
	out[3] = (char)(0x80u | (cp & 0x3Fu));
	return 4;
}

/* --- Cell allocate / finalize ---
 *
 * Cell.new() in Wren land — fill a standalone cell with the current
 * mode's default attribute, fg, bg, a space character, font 0, and
 * no hyperlink.  `parent` stays NULL so accessors operate on `c`. */
static void
wren_cell_allocate(WrenVM *vm)
{
	struct wren_cell *wc = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*wc));
	memset(wc, 0, sizeof(*wc));
	struct text_info ti;
	gettextinfo(&ti);
	wc->c.legacy_attr  = ti.normattr;
	ciolib_attr2palette(ti.normattr, &wc->c.fg, &wc->c.bg);
	wc->c.ch           = 0x20;
	wc->c.font         = 0;
	wc->c.hyperlink_id = 0;
}

static void
wren_cell_finalize(void *data)
{
	struct wren_cell *wc = data;
	if (wc->parent_handle != NULL) {
		struct wren_host_state *st = wren_host_state();
		if (st != NULL && st->vm != NULL)
			wrenReleaseHandle(st->vm, wc->parent_handle);
		wc->parent_handle = NULL;
	}
}

/* --- Cells allocate / finalize ---
 *
 * Allocator leaves the struct empty; Conio.getText fills count and
 * buf after construction.  Finalize releases the malloc'd buffer. */
static void
wren_cells_allocate(WrenVM *vm)
{
	struct wren_cells *cs = wrenSetSlotNewForeign(vm, 0, 0, sizeof(*cs));
	cs->count = 0;
	cs->buf   = NULL;
}

static void
wren_cells_finalize(void *data)
{
	struct wren_cells *cs = data;
	free(cs->buf);
	cs->buf   = NULL;
	cs->count = 0;
}

/* ----- Conio.getText / Conio.putText ----- */

static void
fn_Conio_getText(WrenVM *vm)
{
	int sx = (int)wrenGetSlotDouble(vm, 1);
	int sy = (int)wrenGetSlotDouble(vm, 2);
	int ex = (int)wrenGetSlotDouble(vm, 3);
	int ey = (int)wrenGetSlotDouble(vm, 4);

	if (sx < 1 || sy < 1 || ex < sx || ey < sy) {
		wrenSetSlotNull(vm, 0);
		return;
	}

	size_t            n   = (size_t)(ex - sx + 1) * (size_t)(ey - sy + 1);
	struct vmem_cell *buf = calloc(n, sizeof *buf);
	if (buf == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	if (!ciolib_vmem_gettext(sx, sy, ex, ey, buf)) {
		free(buf);
		wrenSetSlotNull(vm, 0);
		return;
	}

	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->cells_class == NULL) {
		free(buf);
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenSetSlotHandle(vm, 1, st->cells_class);
	struct wren_cells *cs =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*cs));
	cs->count = (int)n;
	cs->buf   = buf;
}

static void
fn_Conio_putText(WrenVM *vm)
{
	int sx = (int)wrenGetSlotDouble(vm, 1);
	int sy = (int)wrenGetSlotDouble(vm, 2);
	int ex = (int)wrenGetSlotDouble(vm, 3);
	int ey = (int)wrenGetSlotDouble(vm, 4);

	if (sx < 1 || sy < 1 || ex < sx || ey < sy) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 5) != WREN_TYPE_LIST) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0, "putText: 5th argument must be a List of Cell");
		wrenAbortFiber(vm, 0);
		return;
	}
	int n_expected = (ex - sx + 1) * (ey - sy + 1);
	int n_got      = wrenGetListCount(vm, 5);
	if (n_got != n_expected) {
		wrenEnsureSlots(vm, 1);
		wrenSetSlotString(vm, 0,
		    "putText: list length does not match region size");
		wrenAbortFiber(vm, 0);
		return;
	}

	struct vmem_cell *buf = calloc((size_t)n_expected, sizeof *buf);
	if (buf == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	wrenEnsureSlots(vm, 7);
	for (int i = 0; i < n_expected; i++) {
		wrenGetListElement(vm, 5, i, 6);
		if (wrenGetSlotType(vm, 6) != WREN_TYPE_FOREIGN) {
			free(buf);
			wrenSetSlotString(vm, 0,
			    "putText: list element is not a Cell");
			wrenAbortFiber(vm, 0);
			return;
		}
		struct wren_cell *wc = wrenGetSlotForeign(vm, 6);
		buf[i] = *cell_data(wc);
	}

	int rv = ciolib_vmem_puttext(sx, sy, ex, ey, buf);
	free(buf);
	wrenSetSlotBool(vm, 0, rv != 0);
}

/* ----- Cell field accessors ----- */

static void
fn_Cell_ch(WrenVM *vm)
{
	struct vmem_cell *d  = slot_cell_data(vm, 0);
	uint32_t          cp = cpoint_from_cpchar(CIOLIB_CP437, d->ch);
	char              buf[4];
	int               len = encode_utf8(cp, buf);
	wrenSetSlotBytes(vm, 0, buf, (size_t)len);
}

static void
fn_Cell_ch_set(WrenVM *vm)
{
	struct vmem_cell *d   = slot_cell_data(vm, 0);
	int               len = 0;
	const char       *s   = wrenGetSlotBytes(vm, 1, &len);
	uint32_t          cp  = 0;
	if (decode_utf8_first(s, len, &cp) == 0) {
		d->ch = 0;
		return;
	}
	d->ch = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, '?');
}

static void
fn_Cell_chByte(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->ch);
}

static void
fn_Cell_chByte_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->ch = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_font(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->font);
}

static void
fn_Cell_font_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->font = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_legacyAttr(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->legacy_attr);
}

static void
fn_Cell_legacyAttr_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->legacy_attr = (uint8_t)(int)wrenGetSlotDouble(vm, 1);
}

static void
fn_Cell_bright(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x08) != 0);
}

static void
fn_Cell_bright_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (wrenGetSlotBool(vm, 1))
		d->legacy_attr |= 0x08u;
	else
		d->legacy_attr &= (uint8_t)~0x08u;
}

static void
fn_Cell_blink(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotBool(vm, 0, (d->legacy_attr & 0x80) != 0);
}

static void
fn_Cell_blink_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (wrenGetSlotBool(vm, 1))
		d->legacy_attr |= 0x80u;
	else
		d->legacy_attr &= (uint8_t)~0x80u;
}

/* fg/bg follow the same pattern: bit 31 selects RGB vs palette; bits
 * 24..30 are out-of-scope flags preserved verbatim across writes; the
 * low 24 bits hold the value being read or written. */

static void
fn_Cell_fgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->fg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

static void
fn_Cell_fgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

static void
fn_Cell_fgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->fg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->fg & 0x00FFFFFFu));
}

static void
fn_Cell_fgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->fg = (d->fg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

static void
fn_Cell_bgPalette(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (d->bg & CIOLIB_COLOR_RGB)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

static void
fn_Cell_bgPalette_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | (n & 0x00FFFFFFu);
}

static void
fn_Cell_bgRgb(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	if (!(d->bg & CIOLIB_COLOR_RGB))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotDouble(vm, 0, (double)(d->bg & 0x00FFFFFFu));
}

static void
fn_Cell_bgRgb_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	uint32_t          n = (uint32_t)(int64_t)wrenGetSlotDouble(vm, 1);
	d->bg = (d->bg & 0x7F000000u) | 0x80000000u | (n & 0x00FFFFFFu);
}

static void
fn_Cell_hyperlinkId(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)d->hyperlink_id);
}

static void
fn_Cell_hyperlinkId_set(WrenVM *vm)
{
	struct vmem_cell *d = slot_cell_data(vm, 0);
	d->hyperlink_id = (uint16_t)(int)wrenGetSlotDouble(vm, 1);
}

/* ----- Cells accessors ----- */

static void
fn_Cells_count(WrenVM *vm)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	wrenSetSlotDouble(vm, 0, (double)cs->count);
}

/* Materialize a Cell view into slot 0.  Pins the parent Cells (slot 0
 * receiver) via a Wren handle so the buffer outlives the view. */
static void
cells_make_view(WrenVM *vm, int idx)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	if (idx < 0 || idx >= cs->count) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->cell_class == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	WrenHandle *parent_handle = wrenGetSlotHandle(vm, 0);

	wrenEnsureSlots(vm, 3);
	wrenSetSlotHandle(vm, 2, st->cell_class);
	struct wren_cell *wc = wrenSetSlotNewForeign(vm, 0, 2, sizeof(*wc));
	memset(wc, 0, sizeof(*wc));
	wc->parent        = cs;
	wc->index         = idx;
	wc->parent_handle = parent_handle;
}

static void
fn_Cells_subscript(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	cells_make_view(vm, i);
}

/* Wren iteration: null -> 0; n -> n+1; past end -> false. */
static void
fn_Cells_iterate(WrenVM *vm)
{
	struct wren_cells *cs = wrenGetSlotForeign(vm, 0);
	if (cs->count == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	int next = (int)wrenGetSlotDouble(vm, 1) + 1;
	if (next >= cs->count)
		wrenSetSlotBool(vm, 0, false);
	else
		wrenSetSlotDouble(vm, 0, (double)next);
}

static void
fn_Cells_iteratorValue(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	cells_make_view(vm, i);
}

/* ----- Font ----- */

static void
fn_Font_name(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	if (i < 0 || i > 256 || conio_fontdata[i].desc == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, conio_fontdata[i].desc);
}

static void
fn_Font_count(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, 257.0);
}

static void
fn_Font_available(WrenVM *vm)
{
	int i = (int)wrenGetSlotDouble(vm, 1);
	wrenSetSlotBool(vm, 0, ciolib_checkfont(i) != 0);
}

/* ----- Hyperlinks (Map-like read interface) ----- */

static void
fn_Hyperlinks_subscript(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *url = ciolib_get_hyperlink_url((uint16_t)id);
	if (url == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, url);
	free(url);
}

static void
fn_Hyperlinks_containsKey(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char *url = ciolib_get_hyperlink_url((uint16_t)id);
	wrenSetSlotBool(vm, 0, url != NULL);
	free(url);
}

static void
fn_Hyperlinks_add(WrenVM *vm)
{
	int         len   = 0;
	const char *uri_b = wrenGetSlotBytes(vm, 1, &len);
	if (uri_b == NULL || len <= 0) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	char *uri_z = malloc((size_t)len + 1);
	if (uri_z == NULL) {
		wrenSetSlotDouble(vm, 0, 0.0);
		return;
	}
	memcpy(uri_z, uri_b, (size_t)len);
	uri_z[len] = '\0';

	char *id_param_z = NULL;
	if (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING) {
		int         plen = 0;
		const char *pb   = wrenGetSlotBytes(vm, 2, &plen);
		id_param_z = malloc((size_t)plen + 1);
		if (id_param_z != NULL) {
			memcpy(id_param_z, pb, (size_t)plen);
			id_param_z[plen] = '\0';
		}
	}

	uint16_t id = ciolib_add_hyperlink(uri_z, id_param_z);
	free(uri_z);
	free(id_param_z);
	wrenSetSlotDouble(vm, 0, (double)id);
}

static void
fn_Hyperlinks_params(WrenVM *vm)
{
	int id = (int)wrenGetSlotDouble(vm, 1);
	if (id <= 0 || id > 65535) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *params = ciolib_get_hyperlink_params((uint16_t)id);
	if (params == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, params);
	free(params);
}

/* ----- Lookup table -----------------------------------------------
 *
 * Wren signatures: foreign static methods are reported with the
 * leading "static " prefix in `signature`.  Getters look like "name"
 * (no parens); methods look like "name(_)" / "name(_,_)" with one
 * underscore per argument.
 * -------------------------------------------------------------------- */

struct binding {
	const char        *className;
	bool               isStatic;
	const char        *signature;
	WrenForeignMethodFn fn;
};

static const struct binding BINDINGS[] = {
	/* Conio (all static) */
	{ "Conio", true,  "putch(_)",            fn_Conio_putch       },
	{ "Conio", true,  "cputs(_)",            fn_Conio_cputs       },
	{ "Conio", true,  "gotoxy(_,_)",         fn_Conio_gotoxy      },
	{ "Conio", true,  "wherex",              fn_Conio_wherex      },
	{ "Conio", true,  "wherey",              fn_Conio_wherey      },
	{ "Conio", true,  "textattr(_)",         fn_Conio_textattr    },
	{ "Conio", true,  "screenSize",          fn_Conio_screenSize  },
	{ "Conio", true,  "ungetch(_)",          fn_Conio_ungetch     },
	{ "Conio", true,  "getch",               fn_Conio_getch       },
	{ "Conio", true,  "kbhit",               fn_Conio_kbhit       },
	{ "Conio", true,  "clrscr()",            fn_Conio_clrscr      },
	{ "Conio", true,  "clreol()",            fn_Conio_clreol      },
	{ "Conio", true,  "savescreen",          fn_Conio_savescreen  },
	{ "Conio", true,  "restorescreen(_)",    fn_Conio_restorescreen },
	{ "Conio", true,  "getText(_,_,_,_)",    fn_Conio_getText     },
	{ "Conio", true,  "putText(_,_,_,_,_)",  fn_Conio_putText     },

	/* Cell (all instance) */
	{ "Cell",  false, "ch",              fn_Cell_ch              },
	{ "Cell",  false, "ch=(_)",          fn_Cell_ch_set          },
	{ "Cell",  false, "chByte",          fn_Cell_chByte          },
	{ "Cell",  false, "chByte=(_)",      fn_Cell_chByte_set      },
	{ "Cell",  false, "font",            fn_Cell_font            },
	{ "Cell",  false, "font=(_)",        fn_Cell_font_set        },
	{ "Cell",  false, "legacyAttr",      fn_Cell_legacyAttr      },
	{ "Cell",  false, "legacyAttr=(_)",  fn_Cell_legacyAttr_set  },
	{ "Cell",  false, "bright",          fn_Cell_bright          },
	{ "Cell",  false, "bright=(_)",      fn_Cell_bright_set      },
	{ "Cell",  false, "blink",           fn_Cell_blink           },
	{ "Cell",  false, "blink=(_)",       fn_Cell_blink_set       },
	{ "Cell",  false, "fgPalette",       fn_Cell_fgPalette       },
	{ "Cell",  false, "fgPalette=(_)",   fn_Cell_fgPalette_set   },
	{ "Cell",  false, "fgRgb",           fn_Cell_fgRgb           },
	{ "Cell",  false, "fgRgb=(_)",       fn_Cell_fgRgb_set       },
	{ "Cell",  false, "bgPalette",       fn_Cell_bgPalette       },
	{ "Cell",  false, "bgPalette=(_)",   fn_Cell_bgPalette_set   },
	{ "Cell",  false, "bgRgb",           fn_Cell_bgRgb           },
	{ "Cell",  false, "bgRgb=(_)",       fn_Cell_bgRgb_set       },
	{ "Cell",  false, "hyperlinkId",     fn_Cell_hyperlinkId     },
	{ "Cell",  false, "hyperlinkId=(_)", fn_Cell_hyperlinkId_set },

	/* Cells (all instance) */
	{ "Cells", false, "count",            fn_Cells_count         },
	{ "Cells", false, "[_]",              fn_Cells_subscript     },
	{ "Cells", false, "iterate(_)",       fn_Cells_iterate       },
	{ "Cells", false, "iteratorValue(_)", fn_Cells_iteratorValue },

	/* Font (all static) */
	{ "Font",  true,  "name(_)",      fn_Font_name      },
	{ "Font",  true,  "count",        fn_Font_count     },
	{ "Font",  true,  "available(_)", fn_Font_available },

	/* Hyperlinks (all static) */
	{ "Hyperlinks", true, "[_]",            fn_Hyperlinks_subscript   },
	{ "Hyperlinks", true, "containsKey(_)", fn_Hyperlinks_containsKey },
	{ "Hyperlinks", true, "add(_,_)",       fn_Hyperlinks_add         },
	{ "Hyperlinks", true, "params(_)",      fn_Hyperlinks_params      },

	/* Console */
	{ "Console", true, "count",            fn_Console_count         },
	{ "Console", true, "total",            fn_Console_total         },
	{ "Console", true, "[_]",              fn_Console_subscript     },
	{ "Console", true, "clear()",          fn_Console_clear         },
	{ "Console", true, "iterate(_)",       fn_Console_iterate       },
	{ "Console", true, "iteratorValue(_)", fn_Console_iteratorValue },

	/* Conn */
	{ "Conn",  true, "send(_)",        fn_Conn_send         },
	{ "Conn",  true, "sendRaw(_)",     fn_Conn_sendRaw      },
	{ "Conn",  true, "close()",        fn_Conn_close        },
	{ "Conn",  true, "connected",      fn_Conn_connected    },
	{ "Conn",  true, "type",           fn_Conn_type         },
	{ "Conn",  true, "inbufBytes",     fn_Conn_inbufBytes   },
	{ "Conn",  true, "outbufBytes",    fn_Conn_outbufBytes  },
	{ "Conn",  true, "inbufPeek(_)",   fn_Conn_inbufPeek    },
	{ "Conn",  true, "inbufGet(_)",    fn_Conn_inbufGet     },
	{ "Conn",  true, "outbufPut(_)",   fn_Conn_outbufPut    },

	/* Cterm */
	{ "Cterm", true, "emulation",      fn_Cterm_emulation   },
	{ "Cterm", true, "x",              fn_Cterm_x           },
	{ "Cterm", true, "y",              fn_Cterm_y           },
	{ "Cterm", true, "attr",           fn_Cterm_attr        },
	{ "Cterm", true, "doorwayMode",    fn_Cterm_doorwayMode },
	{ "Cterm", true, "music",          fn_Cterm_music       },
	{ "Cterm", true, "width",          fn_Cterm_width       },
	{ "Cterm", true, "height",         fn_Cterm_height      },
	{ "Cterm", true, "write(_)",       fn_Cterm_write       },

	/* BBS */
	{ "BBS",   true, "name",           fn_BBS_name          },
	{ "BBS",   true, "addr",           fn_BBS_addr          },
	{ "BBS",   true, "port",           fn_BBS_port          },
	{ "BBS",   true, "connType",       fn_BBS_connType      },
	{ "BBS",   true, "user",           fn_BBS_user          },
	{ "BBS",   true, "password",       fn_BBS_password      },
	{ "BBS",   true, "syspass",        fn_BBS_syspass       },
	{ "BBS",   true, "music",          fn_BBS_music         },
	{ "BBS",   true, "rip",            fn_BBS_rip           },
	{ "BBS",   true, "comment",        fn_BBS_comment       },

	/* Cache */
	{ "Cache", true, "basePath",       fn_Cache_basePath    },
	{ "Cache", true, "subdirPath(_)",  fn_Cache_subdirPath  },
	{ "Cache", true, "readFile(_)",    fn_Cache_readFile    },
	{ "Cache", true, "writeFile(_,_)", fn_Cache_writeFile   },

	/* Hook */
	{ "Hook",  true, "onKey(_)",       fn_Hook_onKey        },
	{ "Hook",  true, "onInput(_)",     fn_Hook_onInput      },
	{ "Hook",  true, "onOutput(_)",    fn_Hook_onOutput     },
	{ "Hook",  true, "onMouse(_)",     fn_Hook_onMouse      },
	{ "Hook",  true, "onStatus(_)",    fn_Hook_onStatus     },
	{ "Hook",  true, "every(_,_)",     fn_Hook_every        },
};

WrenForeignMethodFn
wren_bind_lookup(const char *module, const char *className, bool isStatic,
    const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm") != 0)
		return NULL;
	for (size_t i = 0; i < sizeof(BINDINGS) / sizeof(BINDINGS[0]); i++) {
		if (BINDINGS[i].isStatic == isStatic &&
		    strcmp(BINDINGS[i].className, className) == 0 &&
		    strcmp(BINDINGS[i].signature, signature) == 0)
			return BINDINGS[i].fn;
	}
	return NULL;
}

WrenForeignClassMethods
wren_bind_lookup_class(const char *module, const char *className)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module == NULL || strcmp(module, "syncterm") != 0)
		return none;
	if (strcmp(className, "Cell") == 0) {
		WrenForeignClassMethods m = {
			wren_cell_allocate, wren_cell_finalize
		};
		return m;
	}
	if (strcmp(className, "Cells") == 0) {
		WrenForeignClassMethods m = {
			wren_cells_allocate, wren_cells_finalize
		};
		return m;
	}
	return none;
}
