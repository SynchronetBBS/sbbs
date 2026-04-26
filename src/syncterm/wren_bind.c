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

/* ----- Lookup table -----------------------------------------------
 *
 * Wren signatures: foreign static methods are reported with the
 * leading "static " prefix in `signature`.  Getters look like "name"
 * (no parens); methods look like "name(_)" / "name(_,_)" with one
 * underscore per argument.
 * -------------------------------------------------------------------- */

struct binding {
	const char        *className;
	const char        *signature;
	WrenForeignMethodFn fn;
};

static const struct binding BINDINGS[] = {
	/* Conio */
	{ "Conio", "putch(_)",       fn_Conio_putch       },
	{ "Conio", "cputs(_)",       fn_Conio_cputs       },
	{ "Conio", "gotoxy(_,_)",    fn_Conio_gotoxy      },
	{ "Conio", "wherex",         fn_Conio_wherex      },
	{ "Conio", "wherey",         fn_Conio_wherey      },
	{ "Conio", "textattr(_)",    fn_Conio_textattr    },
	{ "Conio", "screenSize",     fn_Conio_screenSize  },
	{ "Conio", "ungetch(_)",     fn_Conio_ungetch     },

	/* Conn */
	{ "Conn",  "send(_)",        fn_Conn_send         },
	{ "Conn",  "sendRaw(_)",     fn_Conn_sendRaw      },
	{ "Conn",  "close()",        fn_Conn_close        },
	{ "Conn",  "connected",      fn_Conn_connected    },
	{ "Conn",  "type",           fn_Conn_type         },
	{ "Conn",  "inbufBytes",     fn_Conn_inbufBytes   },
	{ "Conn",  "outbufBytes",    fn_Conn_outbufBytes  },
	{ "Conn",  "inbufPeek(_)",   fn_Conn_inbufPeek    },
	{ "Conn",  "inbufGet(_)",    fn_Conn_inbufGet     },
	{ "Conn",  "outbufPut(_)",   fn_Conn_outbufPut    },

	/* Cterm */
	{ "Cterm", "emulation",      fn_Cterm_emulation   },
	{ "Cterm", "x",              fn_Cterm_x           },
	{ "Cterm", "y",              fn_Cterm_y           },
	{ "Cterm", "attr",           fn_Cterm_attr        },
	{ "Cterm", "doorwayMode",    fn_Cterm_doorwayMode },
	{ "Cterm", "music",          fn_Cterm_music       },
	{ "Cterm", "width",          fn_Cterm_width       },
	{ "Cterm", "height",         fn_Cterm_height      },
	{ "Cterm", "write(_)",       fn_Cterm_write       },

	/* BBS */
	{ "BBS",   "name",           fn_BBS_name          },
	{ "BBS",   "addr",           fn_BBS_addr          },
	{ "BBS",   "port",           fn_BBS_port          },
	{ "BBS",   "connType",       fn_BBS_connType      },
	{ "BBS",   "user",           fn_BBS_user          },
	{ "BBS",   "music",          fn_BBS_music         },
	{ "BBS",   "rip",            fn_BBS_rip           },
	{ "BBS",   "comment",        fn_BBS_comment       },

	/* Cache */
	{ "Cache", "basePath",       fn_Cache_basePath    },
	{ "Cache", "subdirPath(_)",  fn_Cache_subdirPath  },
	{ "Cache", "readFile(_)",    fn_Cache_readFile    },
	{ "Cache", "writeFile(_,_)", fn_Cache_writeFile   },

	/* Hook */
	{ "Hook",  "onKey(_)",       fn_Hook_onKey        },
	{ "Hook",  "onInput(_)",     fn_Hook_onInput      },
	{ "Hook",  "onOutput(_)",    fn_Hook_onOutput     },
	{ "Hook",  "onMouse(_)",     fn_Hook_onMouse      },
	{ "Hook",  "onStatus(_)",    fn_Hook_onStatus     },
	{ "Hook",  "every(_,_)",     fn_Hook_every        },
};

WrenForeignMethodFn
wren_bind_lookup(const char *module, const char *className, bool isStatic,
    const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm") != 0)
		return NULL;
	if (!isStatic)
		return NULL;
	for (size_t i = 0; i < sizeof(BINDINGS) / sizeof(BINDINGS[0]); i++) {
		if (strcmp(BINDINGS[i].className, className) == 0 &&
		    strcmp(BINDINGS[i].signature, signature) == 0)
			return BINDINGS[i].fn;
	}
	return NULL;
}
