#include "wren_bind_menu.h"

#include "bbslist_model.h"
#include "comio.h"
#include "conn.h"
#include "genwrap.h"
#include "syncterm.h"
#include "term.h"
#include "window.h"
#include "wren_bind_internal.h"
#include "wren_bind_menu_fonts.h"
#include "wren_bind_menu_settings.h"
#include "wren_bind_screen.h"

#include <vidmodes.h>

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct wren_menu_bbs {
	enum syncterm_wren_foreign type;
	struct bbslist *bbs;
	uint64_t generation;
	bool is_defaults;
	bool is_transient;
};

static struct bbslist_model menu_model;
static struct bbslist menu_transient;
static uint64_t transient_generation;
static bool transient_valid;

static bool
menu_bbs_valid(const struct wren_menu_bbs *wb)
{
	if (!menu_model.loaded || wb == NULL || wb->bbs == NULL)
		return false;
	if (wb->is_defaults)
		return wb->bbs == &menu_model.defaults &&
		    wb->generation == menu_model.generation;
	if (wb->is_transient)
		return transient_valid && wb->bbs == &menu_transient &&
		    wb->generation == transient_generation;
	return wb->generation == menu_model.generation &&
	    bbslist_model_record(&menu_model, wb->bbs) != NULL;
}

bool
wren_menu_bind_copy_bbs(WrenVM *vm, int slot, struct bbslist *dest)
{
	if (vm == NULL || dest == NULL ||
	    slot_foreign_type(vm, slot) != SWF_MENU_BBS)
		return false;
	struct wren_menu_bbs *wb = wrenGetSlotForeign(vm, slot);
	if (!menu_bbs_valid(wb) || wb->is_defaults)
		return false;
	memcpy(dest, wb->bbs, sizeof(*dest));
	return true;
}

void
wren_menu_bind_init(void)
{
	bbslist_model_init(&menu_model);
	wren_menu_fonts_bind_init();
	wren_menu_settings_bind_init();
}

void
wren_menu_bind_shutdown(void)
{
	bbslist_model_dispose(&menu_model);
	wren_menu_fonts_bind_shutdown();
}

static void
menu_bbs_allocate(WrenVM *vm)
{
	struct wren_menu_bbs *wb = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*wb));
	wb->type = SWF_MENU_BBS;
	wb->bbs = NULL;
	wb->generation = 0;
	wb->is_defaults = false;
	wb->is_transient = false;
}

static void
menu_bbs_finalize(void *data)
{
	(void)data;
}

static struct bbslist *
bbs_check(WrenVM *vm)
{
	if (slot_foreign_type(vm, 0) != SWF_MENU_BBS) {
		wren_throw(vm, "BBS: invalid receiver");
		return NULL;
	}
	struct wren_menu_bbs *wb = wrenGetSlotForeign(vm, 0);
	if (!menu_bbs_valid(wb)) {
		wren_throw(vm, "BBS: stale handle; reacquire it from Menu.entries");
		return NULL;
	}
	return wb->bbs;
}

static struct bbslist *
bbs_check_mutable(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL && safe_mode && bbs != &menu_model.defaults) {
		wren_throw(vm, "BBS: directory entries are read-only in safe mode");
		return NULL;
	}
	if (bbs != NULL && bbs != &menu_model.defaults &&
	    bbs->type != USER_BBSLIST) {
		wren_throw(vm, "BBS: system entries are read-only");
		return NULL;
	}
	return bbs;
}

static void
push_bbs_kind(WrenVM *vm, int slot, struct bbslist *bbs, bool transient)
{
	wrenEnsureSlots(vm, slot + 2);
	wrenGetVariable(vm, "syncterm_menu", "BBS", slot + 1);
	struct wren_menu_bbs *wb = wrenSetSlotNewForeign(vm, slot, slot + 1,
	    sizeof(*wb));
	wb->type = SWF_MENU_BBS;
	wb->bbs = bbs;
	wb->generation = transient ? transient_generation :
	    menu_model.generation;
	wb->is_defaults = bbs == &menu_model.defaults;
	wb->is_transient = transient;
}

static void
push_bbs(WrenVM *vm, int slot, struct bbslist *bbs)
{
	push_bbs_kind(vm, slot, bbs, false);
}

bool
wren_menu_bind_push_transient_bbs(WrenVM *vm, int slot,
    const struct bbslist *source)
{
	if (vm == NULL || source == NULL || slot < 0 || !menu_model.loaded)
		return false;
	memcpy(&menu_transient, source, sizeof(menu_transient));
	transient_generation++;
	transient_valid = true;
	push_bbs_kind(vm, slot, &menu_transient, true);
	return true;
}

static bool
slot_string(WrenVM *vm, int slot, char *dest, size_t size)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "BBS: expected a String");
		return false;
	}
	int length = 0;
	const char *value = wrenGetSlotBytes(vm, slot, &length);
	if (length < 0 || (size_t)length >= size ||
	    memchr(value, 0, (size_t)length) != NULL) {
		wren_throw(vm, "BBS: String is too long or contains NUL");
		return false;
	}
	memcpy(dest, value, (size_t)length);
	dest[length] = 0;
	return true;
}

static bool
slot_integer(WrenVM *vm, int slot, int64_t minimum, int64_t maximum,
    int64_t *result)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_NUM) {
		wren_throw(vm, "BBS: expected an integer Num");
		return false;
	}
	double value = wrenGetSlotDouble(vm, slot);
	if (!isfinite(value) || trunc(value) != value ||
	    value < (double)minimum || value > (double)maximum) {
		wren_throw(vm, "BBS: integer is outside the allowed range");
		return false;
	}
	*result = (int64_t)value;
	return true;
}

static bool
slot_bool(WrenVM *vm, int slot, bool *result)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_BOOL) {
		wren_throw(vm, "BBS: expected a Bool");
		return false;
	}
	*result = wrenGetSlotBool(vm, slot);
	return true;
}

#define BBS_STRING_PROPERTY(name, field)                                \
	static void fn_BBS_##name(WrenVM *vm)                            \
	{                                                                 \
		struct bbslist *bbs = bbs_check(vm);                         \
		if (bbs != NULL)                                             \
			wrenSetSlotString(vm, 0, bbs->field);                  \
	}                                                                 \
	static void fn_BBS_##name##_set(WrenVM *vm)                       \
	{                                                                 \
		struct bbslist *bbs = bbs_check_mutable(vm);                 \
		if (bbs != NULL && slot_string(vm, 1, bbs->field,            \
		    sizeof(bbs->field)))                                      \
			bbslist_model_mark_dirty(&menu_model, bbs);             \
	}

#define BBS_NUM_PROPERTY(name, field, minimum, maximum)                 \
	static void fn_BBS_##name(WrenVM *vm)                            \
	{                                                                 \
		struct bbslist *bbs = bbs_check(vm);                         \
		if (bbs != NULL)                                             \
			wrenSetSlotDouble(vm, 0, (double)bbs->field);          \
	}                                                                 \
	static void fn_BBS_##name##_set(WrenVM *vm)                       \
	{                                                                 \
		struct bbslist *bbs = bbs_check_mutable(vm);                 \
		int64_t value;                                               \
		if (bbs != NULL && slot_integer(vm, 1, minimum, maximum,     \
		    &value)) {                                                \
			bbs->field = value;                                     \
			bbslist_model_mark_dirty(&menu_model, bbs);             \
		}                                                             \
	}

#define BBS_BOOL_PROPERTY(name, field)                                 \
	static void fn_BBS_##name(WrenVM *vm)                            \
	{                                                                 \
		struct bbslist *bbs = bbs_check(vm);                         \
		if (bbs != NULL)                                             \
			wrenSetSlotBool(vm, 0, bbs->field);                    \
	}                                                                 \
	static void fn_BBS_##name##_set(WrenVM *vm)                       \
	{                                                                 \
		struct bbslist *bbs = bbs_check_mutable(vm);                 \
		bool value;                                                  \
		if (bbs != NULL && slot_bool(vm, 1, &value)) {               \
			bbs->field = value;                                     \
			bbslist_model_mark_dirty(&menu_model, bbs);             \
		}                                                             \
	}

BBS_STRING_PROPERTY(addr, addr)
BBS_STRING_PROPERTY(user, user)
BBS_STRING_PROPERTY(password, password)
BBS_STRING_PROPERTY(syspass, syspass)
BBS_STRING_PROPERTY(comment, comment)
BBS_STRING_PROPERTY(termName, term_name)
BBS_STRING_PROPERTY(font, font)
BBS_STRING_PROPERTY(dlDir, dldir)
BBS_STRING_PROPERTY(ulDir, uldir)
BBS_STRING_PROPERTY(logFile, logfile)
BBS_STRING_PROPERTY(ghostProgram, ghost_program)

BBS_NUM_PROPERTY(port, port, 0, 65535)
BBS_NUM_PROPERTY(connType, conn_type, CONN_TYPE_UNKNOWN,
    CONN_TYPE_TERMINATOR - 1)
BBS_NUM_PROPERTY(music, music, CTERM_MUSIC_SYNCTERM, CTERM_MUSIC_ENABLED)
BBS_NUM_PROPERTY(rip, rip, RIP_VERSION_NONE, RIP_VERSION_3)
BBS_NUM_PROPERTY(addressFamily, address_family, ADDRESS_FAMILY_UNSPEC,
    ADDRESS_FAMILY_INET6)
BBS_NUM_PROPERTY(screenMode, screen_mode, SCREEN_MODE_CURRENT,
    SCREEN_MODE_TERMINATOR - 1)
BBS_NUM_PROPERTY(bpsRate, bpsrate, 0, INT_MAX)
BBS_NUM_PROPERTY(xferLogLevel, xfer_loglevel, 0, 7)
BBS_NUM_PROPERTY(telnetLogLevel, telnet_loglevel, 0, 7)
BBS_NUM_PROPERTY(stopBits, stop_bits, 1, 2)
BBS_NUM_PROPERTY(dataBits, data_bits, 7, 8)
BBS_NUM_PROPERTY(parity, parity, SYNCTERM_PARITY_NONE,
    SYNCTERM_PARITY_ODD)
BBS_NUM_PROPERTY(flowControl, flow_control, 0, 7)
BBS_NUM_PROPERTY(sortOrder, sort_order, INT32_MIN, INT32_MAX)

BBS_BOOL_PROPERTY(noStatus, nostatus)
BBS_BOOL_PROPERTY(hidePopups, hidepopups)
BBS_BOOL_PROPERTY(yellowIsYellow, yellow_is_yellow)
BBS_BOOL_PROPERTY(forceLcf, force_lcf)
BBS_BOOL_PROPERTY(appendLogFile, append_logfile)
BBS_BOOL_PROPERTY(telnetNoBinary, telnet_no_binary)
BBS_BOOL_PROPERTY(deferTelnetNegotiation, defer_telnet_negotiation)
BBS_BOOL_PROPERTY(sftpPublicKey, sftp_public_key)
BBS_BOOL_PROPERTY(sshAllowAes128Cbc, ssh_allow_aes128_cbc)
BBS_BOOL_PROPERTY(sshAcceptEarlyData, ssh_accept_early_data)
BBS_BOOL_PROPERTY(lfExpand, lf_expand)

static void
fn_BBS_name(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotString(vm, 0, bbs->name);
}

static void
fn_BBS_rename(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check_mutable(vm);
	char name[LIST_NAME_MAX + 1];
	if (bbs == NULL || !slot_string(vm, 1, name, sizeof(name)))
		return;
	wrenSetSlotBool(vm, 0,
	    bbslist_model_rename(&menu_model, bbs, name));
}

static void
fn_BBS_type(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotDouble(vm, 0, bbs->type);
}

static void
fn_BBS_id(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotDouble(vm, 0, bbs->id);
}

static void
fn_BBS_added(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotDouble(vm, 0, (double)bbs->added);
}

static void
fn_BBS_connected(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotDouble(vm, 0, (double)bbs->connected);
}

static void
fn_BBS_calls(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotDouble(vm, 0, bbs->calls);
}

static void
fn_BBS_connTypeName(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs == NULL)
		return;
	const char *name = conn_types[bbs->conn_type];
	wrenSetSlotString(vm, 0, name != NULL ? name : "Unknown");
}

static void
fn_BBS_dirty(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs == NULL)
		return;
	struct wren_menu_bbs *wb = wrenGetSlotForeign(vm, 0);
	if (wb->is_transient) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	struct bbslist_model_record *record = bbslist_model_record(
	    &menu_model, bbs);
	wrenSetSlotBool(vm, 0, bbs == &menu_model.defaults
	    ? menu_model.defaults_dirty : record != NULL && record->dirty);
}

static void
fn_BBS_save(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotBool(vm, 0, (!safe_mode ||
		    bbs == &menu_model.defaults) &&
		    bbslist_model_save(&menu_model, bbs));
}

static void
fn_BBS_delete(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotBool(vm, 0, !safe_mode &&
		    bbslist_model_delete(&menu_model, bbs));
}

static void
fn_BBS_palette(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs == NULL)
		return;
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	unsigned count = bbs->palette_size;
	if (count > 16)
		count = 16;
	for (unsigned i = 0; i < count; i++) {
		wrenSetSlotDouble(vm, 1, bbs->palette[i] & 0xffffffu);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_BBS_palette_set(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check_mutable(vm);
	if (bbs == NULL)
		return;
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_LIST) {
		wren_throw(vm, "BBS.palette: expected a List");
		return;
	}
	int count = wrenGetListCount(vm, 1);
	if (count < 0 || count > 16) {
		wren_throw(vm, "BBS.palette: expected at most 16 colors");
		return;
	}
	wrenEnsureSlots(vm, 3);
	for (int i = 0; i < count; i++) {
		wrenGetListElement(vm, 1, i, 2);
		int64_t value;
		if (!slot_integer(vm, 2, 0, 0xffffff, &value))
			return;
		bbs->palette[i] = (uint32_t)value;
	}
	bbs->palette_size = (unsigned)count;
	bbslist_model_mark_dirty(&menu_model, bbs);
}

static void
fn_BBS_toString(WrenVM *vm)
{
	struct bbslist *bbs = bbs_check(vm);
	if (bbs != NULL)
		wrenSetSlotString(vm, 0, bbs->name);
}

static void
fn_Menu_load(WrenVM *vm)
{
	const char *password = NULL;
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_STRING)
		password = wrenGetSlotString(vm, 1);
	else if (wrenGetSlotType(vm, 1) != WREN_TYPE_NULL) {
		wren_throw(vm, "Menu.load: password must be a String or null");
		return;
	}
	enum bbslist_read_status status = bbslist_model_load(&menu_model,
	    password);
	if (status == BBSLIST_READ_OK) {
		transient_valid = false;
		transient_generation++;
	}
	wrenSetSlotDouble(vm, 0, status);
}

static void
fn_Menu_quickConnect(WrenVM *vm)
{
	char url[MAX_PATH + 1];
	if (!menu_model.loaded || !slot_string(vm, 1, url, sizeof(url)) ||
	    url[0] == 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	memcpy(&menu_transient, &menu_model.defaults,
	    sizeof(menu_transient));
	parse_url(url, &menu_transient, menu_model.defaults.conn_type, false);
	transient_generation++;
	transient_valid = menu_transient.addr[0] != 0 ||
	    menu_transient.conn_type == CONN_TYPE_SHELL;
	if (!transient_valid)
		wrenSetSlotNull(vm, 0);
	else
		push_bbs_kind(vm, 0, &menu_transient, true);
}

static void
fn_Menu_offlineScrollback(WrenVM *vm)
{
	if (scrollback_buf == NULL || scrollback_cols == 0 ||
	    scrollback_lines == 0 || scrollback_cols > INT_MAX ||
	    scrollback_lines > INT_MAX ||
	    scrollback_lines > SIZE_MAX / scrollback_cols ||
	    (size_t)scrollback_lines * scrollback_cols > INT_MAX ||
	    (size_t)scrollback_lines * scrollback_cols >
	    SIZE_MAX / sizeof(struct vmem_cell)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	size_t count = (size_t)scrollback_cols * scrollback_lines;
	struct vmem_cell *copy = malloc(count * sizeof(*copy));
	if (copy == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	memcpy(copy, scrollback_buf, count * sizeof(*copy));
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm", "Surface", 1);
	struct wren_surface *surface = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*surface));
	surface->type = SWF_SURFACE;
	surface->count = (int)count;
	surface->buf = copy;
	surface->width = (int)scrollback_cols;
	surface->height = (int)scrollback_lines;
	surface->borrowed = false;
}

static void
fn_Menu_prepareOfflineScrollback(WrenVM *vm)
{
	if (scrollback_buf == NULL || scrollback_cols == 0 ||
	    scrollback_lines == 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	textmode((int)scrollback_mode);
	set_default_cursor();
	switch (ciolib_to_screen((int)scrollback_mode)) {
		case SCREEN_MODE_C64:
			setfont(33, true, 1);
			break;
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			setfont(35, true, 1);
			break;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			setfont(36, true, 1);
			break;
	}
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE) {
		for (size_t i = 0;
		    i < sizeof(dac_default) / sizeof(dac_default[0]); i++) {
			setpalette((uint32_t)i + 16,
			    dac_default[i].red << 8 | dac_default[i].red,
			    dac_default[i].green << 8 | dac_default[i].green,
			    dac_default[i].blue << 8 | dac_default[i].blue);
		}
	}
	for (int i = 1; i <= 4; i++)
		setfont(0, false, i);
	if (drawwin() != 0) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	set_modepalette(palettes[COLOUR_PALETTE]);
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_Menu_openUrl(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm, "Menu.openUrl: expected a String");
		return;
	}
	const char *url = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, url[0] != 0 && cio_api.openurl != NULL &&
	    cio_api.openurl(url));
}

static void
fn_Menu_offlineScrollbackHasStatus(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, !term.nostatus);
}

static void
fn_Menu_openHyperlink(WrenVM *vm)
{
	int64_t id;
	if (!slot_integer(vm, 1, 1, UINT16_MAX, &id))
		return;
	wrenSetSlotBool(vm, 0, ciolib_open_hyperlink((uint16_t)id));
}

static void
fn_Menu_setHyperlinkHover(WrenVM *vm)
{
	int64_t id;
	if (!slot_integer(vm, 1, 0, UINT16_MAX, &id))
		return;
	if (id != 0) {
		char *url = ciolib_get_hyperlink_url((uint16_t)id);
		if (url != NULL) {
			show_status_url(url);
			free(url);
		}
		mousepointer(CIOLIB_MOUSEPTR_ARROW);
	}
	else {
		show_status_url("");
		mousepointer(CIOLIB_MOUSEPTR_BAR);
	}
	wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_applicationTitle(WrenVM *vm)
{
	char title[80];

	snprintf(title, sizeof(title), "%.40s - %.30s", syncterm_version,
	    output_descrs[cio_api.mode]);
	wrenSetSlotString(vm, 0, title);
}

static void
fn_Menu_timeText(WrenVM *vm)
{
	static const char *const weekdays[] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char *const months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	char stamp[25] = "";
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	if (local != NULL) {
		int hour = local->tm_hour % 12;
		if (hour == 0)
			hour = 12;
		snprintf(stamp, sizeof(stamp), "%s %s %02d %4d %02d:%02d %s",
		    weekdays[local->tm_wday], months[local->tm_mon],
		    local->tm_mday, local->tm_year + 1900, hour, local->tm_min,
		    local->tm_hour >= 12 ? "pm" : "am");
	}
	wrenSetSlotString(vm, 0, stamp);
}

static void
fn_Menu_showEntry(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_NULL) {
		settitle(syncterm_version);
		wrenSetSlotBool(vm, 0, true);
		return;
	}
	if (slot_foreign_type(vm, 1) != SWF_MENU_BBS) {
		wren_throw(vm, "Menu.showEntry: entry must be a BBS or null");
		return;
	}
	struct wren_menu_bbs *wb = wrenGetSlotForeign(vm, 1);
	if (!menu_bbs_valid(wb) || wb->is_defaults) {
		wren_throw(vm, "Menu.showEntry: invalid BBS handle");
		return;
	}

	char last[32] = "Never";
	if (wb->bbs->connected != 0) {
		const char *stamp = ctime(&wb->bbs->connected);
		if (stamp != NULL)
			snprintf(last, sizeof(last), "%.24s", stamp);
	}
	char title[1024];
	snprintf(title, sizeof(title), "%s - %s (%u calls / Last: %s)",
	    syncterm_version, wb->bbs->name, wb->bbs->calls, last);
	settitle(title);
	wrenSetSlotBool(vm, 0, true);
}

static void
push_indexed_names(WrenVM *vm, char *const *names, int first)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (int i = first; names[i] != NULL && names[i][0] != 0; i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, i);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, names[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_connectionTypes(WrenVM *vm)
{
	push_indexed_names(vm, conn_types, 1);
}

static void
fn_Menu_defaultPort(WrenVM *vm)
{
	int64_t type;
	if (slot_integer(vm, 1, CONN_TYPE_UNKNOWN,
	    CONN_TYPE_TERMINATOR - 1, &type))
		wrenSetSlotDouble(vm, 0, conn_ports[type]);
}

static void
fn_Menu_rates(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; rate_names[i] != NULL; i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, rates[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, rate_names[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_serialRates(WrenVM *vm)
{
	char device[LIST_ADDR_MAX + 1];
	if (!slot_string(vm, 1, device, sizeof(device)))
		return;
	COM_HANDLE handle = comOpen(device);
	if (handle == COM_HANDLE_INVALID) {
		fn_Menu_rates(vm);
		return;
	}
	ulong values[63];
	size_t count = comGetBaudRates(handle, values,
	    sizeof(values) / sizeof(values[0]));
	comClose(handle);
	if (count == 0 || count > sizeof(values) / sizeof(values[0])) {
		fn_Menu_rates(vm);
		return;
	}

	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; i <= count; i++) {
		char name[32];
		ulong value = i < count ? values[i] : 0;
		if (i < count)
			snprintf(name, sizeof(name), "%lu", value);
		else
			strcpy(name, "Current");
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, (double)value);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void fn_Menu_musicModes(WrenVM *vm) { push_indexed_names(vm, music_names, 0); }
static char *const log_level_names[] = {
	"None", "Alerts", "Critical Errors", "Errors", "Warnings",
	"Notices", "Normal", "All (Debug)", NULL
};
static void fn_Menu_logLevels(WrenVM *vm) { push_indexed_names(vm, log_level_names, 0); }
static void fn_Menu_fontsCatalog(WrenVM *vm) { push_indexed_names(vm, font_names, 0); }

static const int address_family_values[] = {
	ADDRESS_FAMILY_UNSPEC, ADDRESS_FAMILY_INET, ADDRESS_FAMILY_INET6
};
static char *const address_family_names[] = {
	"As per DNS", "IPv4 only", "IPv6 only", NULL
};
static const int rip_values[] = {
	RIP_VERSION_NONE, RIP_VERSION_1, RIP_VERSION_3
};
static char *const rip_names[] = { "Off", "RIPv1", "RIPv3", NULL };
static const int flow_values[] = {
	COM_FLOW_CONTROL_RTS_CTS,
	COM_FLOW_CONTROL_XON_OFF,
	COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF,
	COM_FLOW_CONTROL_NONE
};
static char *const flow_names[] = {
	"RTS/CTS", "XON/XOFF", "RTS/CTS and XON/XOFF", "None", NULL
};
static const int parity_values[] = {
	SYNCTERM_PARITY_NONE, SYNCTERM_PARITY_EVEN, SYNCTERM_PARITY_ODD
};
static char *const parity_names[] = { "None", "Even", "Odd", NULL };

static void
push_value_names(WrenVM *vm, const int *values, char *const *names)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (int i = 0; names[i] != NULL; i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, values[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, names[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void fn_Menu_addressFamilies(WrenVM *vm) { push_value_names(vm, address_family_values, address_family_names); }
static void fn_Menu_ripModes(WrenVM *vm) { push_value_names(vm, rip_values, rip_names); }
static void fn_Menu_flowControls(WrenVM *vm) { push_value_names(vm, flow_values, flow_names); }
static void fn_Menu_parities(WrenVM *vm) { push_value_names(vm, parity_values, parity_names); }

static void
fn_Menu_paletteDefaults(WrenVM *vm)
{
	int64_t mode;
	if (!slot_integer(vm, 1, SCREEN_MODE_CURRENT,
	    SCREEN_MODE_TERMINATOR - 1, &mode))
		return;
	int vmode = find_vmode(screen_to_ciolib((int)mode));
	if (vmode < 0) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	int palette = vparams[vmode].palette;
	size_t count;
	switch (palette) {
		case PRESTEL_PALETTE:
			count = 8;
			break;
		case ATARI_PALETTE_4:
			count = 4;
			break;
		case ATARI_PALETTE_2:
			count = 2;
			break;
		default:
			count = 16;
			break;
	}
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, (double)count);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotNewList(vm, 1);
	for (size_t i = 0; i < 16; i++) {
		uint32_t entry = palettes[palette][i];
		uint32_t value = (uint32_t)dac_default[entry].red << 16 |
		    (uint32_t)dac_default[entry].green << 8 |
		    (uint32_t)dac_default[entry].blue;
		wrenSetSlotDouble(vm, 2, (double)value);
		wrenInsertInList(vm, 1, -1, 2);
	}
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fn_Menu_statusMessage(WrenVM *vm)
{
	int64_t status;
	if (!slot_integer(vm, 1, BBSLIST_READ_OK,
	    BBSLIST_READ_MIGRATION_INI_WRITE_FAILED, &status))
		return;
	wrenSetSlotString(vm, 0,
	    bbslist_read_status_string((enum bbslist_read_status)status));
}

static void
fn_Menu_entries(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	if (!menu_model.loaded)
		return;
	for (size_t i = 0; i < menu_model.count; i++) {
		push_bbs(vm, 1, menu_model.entries[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_canAppendEntry(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, menu_model.loaded &&
	    menu_model.count < BBSLIST_MAX_ENTRIES);
}

static void
fn_Menu_defaults(WrenVM *vm)
{
	if (!menu_model.loaded)
		wrenSetSlotNull(vm, 0);
	else
		push_bbs(vm, 0, &menu_model.defaults);
}

static void
fn_Menu_nameAvailable(WrenVM *vm)
{
	char name[LIST_NAME_MAX + 1];
	if (!slot_string(vm, 1, name, sizeof(name)))
		return;
	wrenSetSlotBool(vm, 0,
	    bbslist_model_user_name_available(&menu_model, name));
}

static void
fn_Menu_create(WrenVM *vm)
{
	char name[LIST_NAME_MAX + 1];
	if (!slot_string(vm, 1, name, sizeof(name)))
		return;
	struct bbslist *bbs = safe_mode ? NULL :
	    bbslist_model_create(&menu_model, name, NULL);
	if (bbs == NULL)
		wrenSetSlotNull(vm, 0);
	else
		push_bbs(vm, 0, bbs);
}

static void
fn_Menu_copy(WrenVM *vm)
{
	if (slot_foreign_type(vm, 1) != SWF_MENU_BBS) {
		wren_throw(vm, "Menu.copy: expected a BBS");
		return;
	}
	struct wren_menu_bbs *source = wrenGetSlotForeign(vm, 1);
	if (!menu_bbs_valid(source) || source->is_defaults) {
		wren_throw(vm, "Menu.copy: stale BBS handle");
		return;
	}
	char name[LIST_NAME_MAX + 1];
	if (!slot_string(vm, 2, name, sizeof(name)))
		return;
	struct bbslist *bbs = safe_mode ? NULL :
	    bbslist_model_create(&menu_model, name, source->bbs);
	if (bbs == NULL)
		wrenSetSlotNull(vm, 0);
	else
		push_bbs(vm, 0, bbs);
}

static void
fn_Menu_sort(WrenVM *vm)
{
	bbslist_model_sort(&menu_model);
	wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_sortFields(WrenVM *vm)
{
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 1; i <= bbslist_sort_field_count(); i++) {
		const char *name = bbslist_sort_field_name((int)i);
		if (name == NULL)
			continue;
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, (double)i);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotBool(vm, 2, bbslist_sort_field_reversed((int)i));
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_sortProfiles(WrenVM *vm)
{
	int order[64];
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; i < bbslist_sort_profile_count(); i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotString(vm, 2, bbslist_sort_profile_name(i));
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotNewList(vm, 2);
		size_t count = bbslist_sort_profile_order(i, order,
		    sizeof(order) / sizeof(order[0]));
		for (size_t j = 0; j < count; j++) {
			wrenSetSlotDouble(vm, 3, order[j]);
			wrenInsertInList(vm, 2, -1, 3);
		}
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_activeSortProfile(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, bbslist_active_sort_profile());
}

static bool
slot_sort_order(WrenVM *vm, int slot, int *order, size_t capacity,
    size_t *count)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_LIST) {
		wren_throw(vm, "sort profile order must be a List");
		return false;
	}
	int length = wrenGetListCount(vm, slot);
	if (length < 0 || (size_t)length > capacity ||
	    (size_t)length > bbslist_sort_field_count()) {
		wren_throw(vm, "sort profile order has an invalid length");
		return false;
	}
	wrenEnsureSlots(vm, slot + 2);
	for (int i = 0; i < length; i++) {
		wrenGetListElement(vm, slot, i, slot + 1);
		int64_t field;
		int64_t limit = (int64_t)bbslist_sort_field_count();
		if (!slot_integer(vm, slot + 1, -limit, limit, &field))
			return false;
		order[i] = (int)field;
	}
	*count = (size_t)length;
	return true;
}

static void
fn_Menu_saveSortProfiles(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, bbslist_save_sort_profiles());
}

static void
fn_Menu_setActiveSortProfile(WrenVM *vm)
{
	int64_t index;
	size_t count = bbslist_sort_profile_count();
	if (count == 0 || !slot_integer(vm, 1, 0, (int64_t)count - 1,
	    &index))
		return;
	bool success = bbslist_set_active_sort_profile((size_t)index);
	if (success)
		bbslist_model_sort(&menu_model);
	wrenSetSlotBool(vm, 0, success);
}

static void
fn_Menu_addSortProfile(WrenVM *vm)
{
	int64_t index;
	size_t profiles = bbslist_sort_profile_count();
	if (!slot_integer(vm, 1, 0, (int64_t)profiles, &index) ||
	    wrenGetSlotType(vm, 2) != WREN_TYPE_STRING) {
		if (wrenGetSlotType(vm, 2) != WREN_TYPE_STRING)
			wren_throw(vm, "sort profile name must be a String");
		return;
	}
	int order[64];
	size_t count;
	if (!slot_sort_order(vm, 3, order,
	    sizeof(order) / sizeof(order[0]), &count))
		return;
	wrenSetSlotBool(vm, 0, bbslist_add_sort_profile((size_t)index,
	    wrenGetSlotString(vm, 2), order, count));
}

static void
fn_Menu_updateSortProfile(WrenVM *vm)
{
	int64_t index;
	size_t profiles = bbslist_sort_profile_count();
	if (profiles == 0 || !slot_integer(vm, 1, 0,
	    (int64_t)profiles - 1, &index) ||
	    wrenGetSlotType(vm, 2) != WREN_TYPE_STRING) {
		if (wrenGetSlotType(vm, 2) != WREN_TYPE_STRING)
			wren_throw(vm, "sort profile name must be a String");
		return;
	}
	int order[64];
	size_t count;
	if (!slot_sort_order(vm, 3, order,
	    sizeof(order) / sizeof(order[0]), &count))
		return;
	bool success = bbslist_update_sort_profile((size_t)index,
	    wrenGetSlotString(vm, 2), order, count);
	if (success && index == bbslist_active_sort_profile())
		bbslist_model_sort(&menu_model);
	wrenSetSlotBool(vm, 0, success);
}

static void
fn_Menu_deleteSortProfile(WrenVM *vm)
{
	int64_t index;
	size_t profiles = bbslist_sort_profile_count();
	if (profiles == 0 || !slot_integer(vm, 1, 0,
	    (int64_t)profiles - 1, &index))
		return;
	bool success = bbslist_delete_sort_profile((size_t)index);
	if (success)
		bbslist_model_sort(&menu_model);
	wrenSetSlotBool(vm, 0, success);
}

struct binding {
	const char *class_name;
	bool is_static;
	const char *signature;
	WrenForeignMethodFn function;
};

#define BBS_GETSET(name)                                                \
	{ "BBS", false, #name, fn_BBS_##name },                         \
	{ "BBS", false, #name "=(_)", fn_BBS_##name##_set }

static const struct binding bindings[] = {
	{ "Menu", true, "load(_)", fn_Menu_load },
	{ "Menu", true, "quickConnect(_)", fn_Menu_quickConnect },
	{ "Menu", true, "offlineScrollback", fn_Menu_offlineScrollback },
	{ "Menu", true, "prepareOfflineScrollback()", fn_Menu_prepareOfflineScrollback },
	{ "Menu", true, "offlineScrollbackHasStatus", fn_Menu_offlineScrollbackHasStatus },
	{ "Menu", true, "openUrl(_)", fn_Menu_openUrl },
	{ "Menu", true, "openHyperlink(_)", fn_Menu_openHyperlink },
	{ "Menu", true, "setHyperlinkHover(_)", fn_Menu_setHyperlinkHover },
	{ "Menu", true, "applicationTitle", fn_Menu_applicationTitle },
	{ "Menu", true, "timeText", fn_Menu_timeText },
	{ "Menu", true, "showEntry(_)", fn_Menu_showEntry },
	{ "Menu", true, "statusMessage(_)", fn_Menu_statusMessage },
	{ "Menu", true, "entries", fn_Menu_entries },
	{ "Menu", true, "canAppendEntry", fn_Menu_canAppendEntry },
	{ "Menu", true, "defaults", fn_Menu_defaults },
	{ "Menu", true, "nameAvailable(_)", fn_Menu_nameAvailable },
	{ "Menu", true, "create(_)", fn_Menu_create },
	{ "Menu", true, "copy(_,_)", fn_Menu_copy },
	{ "Menu", true, "sort()", fn_Menu_sort },
	{ "Menu", true, "sortFields", fn_Menu_sortFields },
	{ "Menu", true, "sortProfiles", fn_Menu_sortProfiles },
	{ "Menu", true, "activeSortProfile", fn_Menu_activeSortProfile },
	{ "Menu", true, "saveSortProfiles()", fn_Menu_saveSortProfiles },
	{ "Menu", true, "setActiveSortProfile(_)", fn_Menu_setActiveSortProfile },
	{ "Menu", true, "addSortProfile(_,_,_)", fn_Menu_addSortProfile },
	{ "Menu", true, "updateSortProfile(_,_,_)", fn_Menu_updateSortProfile },
	{ "Menu", true, "deleteSortProfile(_)", fn_Menu_deleteSortProfile },
	{ "Menu", true, "connectionTypes", fn_Menu_connectionTypes },
	{ "Menu", true, "defaultPort(_)", fn_Menu_defaultPort },
	{ "Menu", true, "addressFamilies", fn_Menu_addressFamilies },
	{ "Menu", true, "rates", fn_Menu_rates },
	{ "Menu", true, "serialRates(_)", fn_Menu_serialRates },
	{ "Menu", true, "musicModes", fn_Menu_musicModes },
	{ "Menu", true, "ripModes", fn_Menu_ripModes },
	{ "Menu", true, "flowControls", fn_Menu_flowControls },
	{ "Menu", true, "parities", fn_Menu_parities },
	{ "Menu", true, "paletteDefaults(_)", fn_Menu_paletteDefaults },
	{ "Menu", true, "fontsCatalog", fn_Menu_fontsCatalog },
	{ "Menu", true, "logLevels", fn_Menu_logLevels },
	{ "BBS", false, "name", fn_BBS_name },
	{ "BBS", false, "rename(_)", fn_BBS_rename },
	{ "BBS", false, "type", fn_BBS_type },
	{ "BBS", false, "id", fn_BBS_id },
	{ "BBS", false, "added", fn_BBS_added },
	{ "BBS", false, "connected", fn_BBS_connected },
	{ "BBS", false, "calls", fn_BBS_calls },
	{ "BBS", false, "connTypeName", fn_BBS_connTypeName },
	{ "BBS", false, "dirty", fn_BBS_dirty },
	{ "BBS", false, "save()", fn_BBS_save },
	{ "BBS", false, "delete()", fn_BBS_delete },
	{ "BBS", false, "palette", fn_BBS_palette },
	{ "BBS", false, "palette=(_)", fn_BBS_palette_set },
	{ "BBS", false, "toString", fn_BBS_toString },
	BBS_GETSET(addr),
	BBS_GETSET(user),
	BBS_GETSET(password),
	BBS_GETSET(syspass),
	BBS_GETSET(comment),
	BBS_GETSET(termName),
	BBS_GETSET(font),
	BBS_GETSET(dlDir),
	BBS_GETSET(ulDir),
	BBS_GETSET(logFile),
	BBS_GETSET(ghostProgram),
	BBS_GETSET(port),
	BBS_GETSET(connType),
	BBS_GETSET(music),
	BBS_GETSET(rip),
	BBS_GETSET(addressFamily),
	BBS_GETSET(screenMode),
	BBS_GETSET(bpsRate),
	BBS_GETSET(xferLogLevel),
	BBS_GETSET(telnetLogLevel),
	BBS_GETSET(stopBits),
	BBS_GETSET(dataBits),
	BBS_GETSET(parity),
	BBS_GETSET(flowControl),
	BBS_GETSET(sortOrder),
	BBS_GETSET(noStatus),
	BBS_GETSET(hidePopups),
	BBS_GETSET(yellowIsYellow),
	BBS_GETSET(forceLcf),
	BBS_GETSET(appendLogFile),
	BBS_GETSET(telnetNoBinary),
	BBS_GETSET(deferTelnetNegotiation),
	BBS_GETSET(sftpPublicKey),
	BBS_GETSET(sshAllowAes128Cbc),
	BBS_GETSET(sshAcceptEarlyData),
	BBS_GETSET(lfExpand),
};

WrenForeignMethodFn
wren_menu_bind_lookup(const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm_menu") != 0)
		return NULL;
	WrenForeignMethodFn fn = wren_menu_fonts_bind_lookup(module,
	    class_name, is_static, signature);
	if (fn != NULL)
		return fn;
	fn = wren_menu_settings_bind_lookup(module,
	    class_name, is_static, signature);
	if (fn != NULL)
		return fn;
	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		if (bindings[i].is_static == is_static &&
		    strcmp(bindings[i].class_name, class_name) == 0 &&
		    strcmp(bindings[i].signature, signature) == 0)
			return bindings[i].function;
	}
	return NULL;
}

WrenForeignClassMethods
wren_menu_bind_lookup_class(const char *module, const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module == NULL || strcmp(module, "syncterm_menu") != 0)
		return none;
	WrenForeignClassMethods font_methods =
	    wren_menu_fonts_bind_lookup_class(module, class_name);
	if (font_methods.allocate != NULL || font_methods.finalize != NULL)
		return font_methods;
	WrenForeignClassMethods settings_methods =
	    wren_menu_settings_bind_lookup_class(module, class_name);
	if (settings_methods.allocate != NULL || settings_methods.finalize != NULL)
		return settings_methods;
	if (strcmp(class_name, "BBS") == 0) {
		WrenForeignClassMethods methods = {
			menu_bbs_allocate, menu_bbs_finalize
		};
		return methods;
	}
	return none;
}
