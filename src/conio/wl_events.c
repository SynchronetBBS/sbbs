/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <threadwrap.h>
#include <genwrap.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#define BITMAP_CIOLIB_DRIVER
#include "ciolib.h"
#include "bitmap_con.h"
#include "wl_events.h"

/*
 * Dynamic loading: wl_dynload.h includes wayland-client-core.h for type
 * declarations, then defines macros to redirect wl_proxy and wl_display
 * calls through dlsym'd function pointers.  The generated protocol
 * headers below use those redirected names in their inline functions.
 */
#include "wl_dynload.h"
#include "wl_proto.h"

#include "scale.h"
#include "utf8_codepages.h"
#include "vidmodes.h"
#include "wl_cio.h"

/*
 * xkbcommon dynamic loading (no compile-time dependency)
 *
 * Opaque types and keysym values are stable ABI from X11/XKB.
 * If libxkbcommon.so is not available at runtime, we fall back
 * to the evdev scancode tables (US QWERTY layout).
 */
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

#define XKB_CONTEXT_NO_FLAGS		0
#define XKB_KEYMAP_FORMAT_TEXT_V1	1
#define XKB_KEYMAP_COMPILE_NO_FLAGS	0
#define XKB_STATE_MODS_EFFECTIVE	(1 << 3)
#define XKB_MOD_INVALID			(0xffffffffU)
#define XKB_KEYMAP_KEY_NO_KEYMAP	0

static struct {
	void *handle;
	struct xkb_context *(*context_new)(int flags);
	void (*context_unref)(struct xkb_context *);
	struct xkb_keymap *(*keymap_new_from_string)(struct xkb_context *,
	    const char *, int, int);
	void (*keymap_unref)(struct xkb_keymap *);
	struct xkb_state *(*state_new)(struct xkb_keymap *);
	void (*state_unref)(struct xkb_state *);
	int (*state_update_mask)(struct xkb_state *, uint32_t, uint32_t,
	    uint32_t, uint32_t, uint32_t, uint32_t);
	uint32_t (*state_key_get_one_sym)(struct xkb_state *, uint32_t);
	uint32_t (*state_key_get_utf32)(struct xkb_state *, uint32_t);
	int (*state_mod_name_is_active)(struct xkb_state *, const char *, int);
} xkb;

static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_kmap;
static struct xkb_state *xkb_st;

static int
xkb_dlopen(void)
{
	if (xkb.handle)
		return 0;
	xkb.handle = dlopen("libxkbcommon.so", RTLD_LAZY);
	if (!xkb.handle)
		xkb.handle = dlopen("libxkbcommon.so.0", RTLD_LAZY);
	if (!xkb.handle)
		return -1;

#define XLOAD(field, sym) do { \
	xkb.field = dlsym(xkb.handle, sym); \
	if (!xkb.field) { dlclose(xkb.handle); xkb.handle = NULL; return -1; } \
} while (0)

	XLOAD(context_new,            "xkb_context_new");
	XLOAD(context_unref,          "xkb_context_unref");
	XLOAD(keymap_new_from_string, "xkb_keymap_new_from_string");
	XLOAD(keymap_unref,           "xkb_keymap_unref");
	XLOAD(state_new,              "xkb_state_new");
	XLOAD(state_unref,            "xkb_state_unref");
	XLOAD(state_update_mask,      "xkb_state_update_mask");
	XLOAD(state_key_get_one_sym,  "xkb_state_key_get_one_sym");
	XLOAD(state_key_get_utf32,    "xkb_state_key_get_utf32");
	XLOAD(state_mod_name_is_active, "xkb_state_mod_name_is_active");

#undef XLOAD

	xkb_ctx = xkb.context_new(XKB_CONTEXT_NO_FLAGS);
	if (!xkb_ctx) {
		dlclose(xkb.handle);
		xkb.handle = NULL;
		return -1;
	}
	return 0;
}

static void
xkb_cleanup(void)
{
	if (xkb_st) {
		xkb.state_unref(xkb_st);
		xkb_st = NULL;
	}
	if (xkb_kmap) {
		xkb.keymap_unref(xkb_kmap);
		xkb_kmap = NULL;
	}
	if (xkb_ctx) {
		xkb.context_unref(xkb_ctx);
		xkb_ctx = NULL;
	}
	if (xkb.handle) {
		dlclose(xkb.handle);
		xkb.handle = NULL;
	}
}

/*
 * Wayland cursor theme (libwayland-cursor, dlopen'd at runtime)
 */
struct wl_cursor_theme;
struct wl_cursor {
	unsigned int image_count;
	struct wl_cursor_image **images;
	char *name;
};
struct wl_cursor_image {
	uint32_t width;
	uint32_t height;
	uint32_t hotspot_x;
	uint32_t hotspot_y;
	uint32_t delay;
};

static struct {
	void *handle;
	struct wl_cursor_theme *(*theme_load)(const char *, int,
	    struct wl_shm *);
	void (*theme_destroy)(struct wl_cursor_theme *);
	struct wl_cursor *(*theme_get_cursor)(struct wl_cursor_theme *,
	    const char *);
	struct wl_buffer *(*image_get_buffer)(struct wl_cursor_image *);
} wcur;

static struct wl_cursor_theme *cursor_theme;
static struct wl_surface *cursor_surf;
static enum ciolib_mouse_ptr current_mouse_ptr;

static int
wcur_dlopen(void)
{
	if (wcur.handle)
		return 0;
	wcur.handle = dlopen("libwayland-cursor.so", RTLD_LAZY);
	if (!wcur.handle)
		wcur.handle = dlopen("libwayland-cursor.so.0", RTLD_LAZY);
	if (!wcur.handle)
		return -1;

#define CLOAD(field, sym) do { \
	wcur.field = dlsym(wcur.handle, sym); \
	if (!wcur.field) { dlclose(wcur.handle); wcur.handle = NULL; return -1; } \
} while (0)

	CLOAD(theme_load, "wl_cursor_theme_load");
	CLOAD(theme_destroy, "wl_cursor_theme_destroy");
	CLOAD(theme_get_cursor, "wl_cursor_theme_get_cursor");
	CLOAD(image_get_buffer, "wl_cursor_image_get_buffer");

#undef CLOAD
	return 0;
}

static struct wl_pointer *wl_ptr;

static void
set_cursor_shape(uint32_t serial)
{
	const char *name;
	struct wl_cursor *cursor;
	struct wl_cursor_image *img;
	struct wl_buffer *buf;

	if (!cursor_theme || !cursor_surf)
		return;

	name = (current_mouse_ptr == CIOLIB_MOUSEPTR_BAR) ? "text" : "default";
	cursor = wcur.theme_get_cursor(cursor_theme, name);
	if (!cursor)
		return;
	img = cursor->images[0];
	buf = wcur.image_get_buffer(img);
	if (!buf)
		return;

	wl_surface_attach(cursor_surf, buf, 0, 0);
	wl_surface_damage_buffer(cursor_surf, 0, 0, img->width, img->height);
	wl_surface_commit(cursor_surf);
	wl_pointer_set_cursor(wl_ptr, serial, cursor_surf,
	    img->hotspot_x, img->hotspot_y);
}

/*
 * Wayland objects
 */
static struct wl_display *wl_dpy;
static struct wl_registry *wl_reg;
static struct wl_compositor *wl_comp;
static struct wl_shm *wl_shm_global;
static struct wl_seat *wl_seat_global;
static struct wl_keyboard *wl_kb;
static struct wl_surface *wl_surf;
static struct wl_region *wl_opaque_region;
static struct xdg_wm_base *xdg_base;
static struct xdg_surface *xdg_surf;
static struct xdg_toplevel *xdg_top;
static struct wp_viewporter *wl_viewporter;
static struct wp_viewport *wl_viewport;
static struct zxdg_decoration_manager_v1 *wl_deco_mgr;
static struct zxdg_toplevel_decoration_v1 *wl_deco;
static bool have_server_decorations;
static struct xdg_toplevel_icon_manager_v1 *wl_icon_mgr;
static struct wl_data_device_manager *wl_ddm;
static struct wl_data_device *wl_ddev;
static struct wl_data_source *wl_copy_source;
static struct wl_data_offer *wl_sel_offer;
static bool wl_sel_offer_has_text;
static bool wl_pending_offer_has_text;

/*
 * SHM double-buffer state
 */
struct wl_buffer_info {
	struct wl_buffer *buffer;
	void *data;
	int width;
	int height;
	bool busy;
};

static struct wl_buffer_info buffers[2];
static int current_buf;

/*
 * Display state
 */
static struct video_stats wl_cvstat;
static bool terminate;
static bool configured;
static bool fullscreen;
static bool maximized;
bool wl_internal_scaling = true;
sem_t wl_pastebuf_set;
sem_t wl_pastebuf_used;
char *wl_pastebuf;
pthread_mutex_t wl_copybuf_mutex;
char *wl_copybuf;
static int pending_width;
static int pending_height;
static int surface_width;
static int surface_height;

/*
 * Keyboard state
 */
static bool shift_held;
static bool ctrl_held;
static bool alt_held;
static int repeat_key;
static int repeat_rate_ms;
static int repeat_delay_ms;
static uint64_t repeat_since;
static bool repeat_in_delay;

/*
 * Mouse state
 */
static int mouse_surface_x;
static int mouse_surface_y;
static bool mouse_in_window;
static uint32_t last_input_serial;
static uint32_t last_pointer_serial;

/*
 * Keyboard scancode tables (same as X11 backend - evdev keycodes map directly)
 *
 * Evdev keycodes match AT Set 1 make codes, which are the indices used here.
 * Wayland wl_keyboard.key events provide evdev keycodes directly.
 */
static struct {
	WORD base;
	WORD shift;
	WORD ctrl;
	WORD alt;
} ScanCodes[] = {
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key  0 */
	{	0x011b, 0x011b, 0x011b, 0xffff }, /* key  1 - Escape key */
	{	0x0231, 0x0221, 0xffff, 0x7800 }, /* key  2 - '1' */
	{	0x0332, 0x0340, 0x0300, 0x7900 }, /* key  3 - '2' */
	{	0x0433, 0x0423, 0xffff, 0x7a00 }, /* key  4 - '3' */
	{	0x0534, 0x0524, 0xffff, 0x7b00 }, /* key  5 - '4' */
	{	0x0635, 0x0625, 0xffff, 0x7c00 }, /* key  6 - '5' */
	{	0x0736, 0x075e, 0x071e, 0x7d00 }, /* key  7 - '6' */
	{	0x0837, 0x0826, 0xffff, 0x7e00 }, /* key  8 - '7' */
	{	0x0938, 0x092a, 0xffff, 0x7f00 }, /* key  9 - '8' */
	{	0x0a39, 0x0a28, 0xffff, 0x8000 }, /* key 10 - '9' */
	{	0x0b30, 0x0b29, 0xffff, 0x8100 }, /* key 11 - '0' */
	{	0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* key 12 - '-' */
	{	0x0d3d, 0x0d2b, 0xffff, 0x8300 }, /* key 13 - '=' */
	{	0x0e08, 0x0e08, 0x0e7f, 0xffff }, /* key 14 - backspace */
	{	0x0f09, 0x0f00, 0xffff, 0xffff }, /* key 15 - tab */
	{	0x1071, 0x1051, 0x1011, 0x1000 }, /* key 16 - 'Q' */
	{	0x1177, 0x1157, 0x1117, 0x1100 }, /* key 17 - 'W' */
	{	0x1265, 0x1245, 0x1205, 0x1200 }, /* key 18 - 'E' */
	{	0x1372, 0x1352, 0x1312, 0x1300 }, /* key 19 - 'R' */
	{	0x1474, 0x1454, 0x1414, 0x1400 }, /* key 20 - 'T' */
	{	0x1579, 0x1559, 0x1519, 0x1500 }, /* key 21 - 'Y' */
	{	0x1675, 0x1655, 0x1615, 0x1600 }, /* key 22 - 'U' */
	{	0x1769, 0x1749, 0x1709, 0x1700 }, /* key 23 - 'I' */
	{	0x186f, 0x184f, 0x180f, 0x1800 }, /* key 24 - 'O' */
	{	0x1970, 0x1950, 0x1910, 0x1900 }, /* key 25 - 'P' */
	{	0x1a5b, 0x1a7b, 0x1a1b, 0xffff }, /* key 26 - '[' */
	{	0x1b5d, 0x1b7d, 0x1b1d, 0xffff }, /* key 27 - ']' */
	{	0x1c0d, 0x1c0d, 0x1c0a, 0xffff }, /* key 28 - CR */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 29 - control */
	{	0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* key 30 - 'A' */
	{	0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* key 31 - 'S' */
	{	0x2064, 0x2044, 0x2004, 0x2000 }, /* key 32 - 'D' */
	{	0x2166, 0x2146, 0x2106, 0x2100 }, /* key 33 - 'F' */
	{	0x2267, 0x2247, 0x2207, 0x2200 }, /* key 34 - 'G' */
	{	0x2368, 0x2348, 0x2308, 0x2300 }, /* key 35 - 'H' */
	{	0x246a, 0x244a, 0x240a, 0x2400 }, /* key 36 - 'J' */
	{	0x256b, 0x254b, 0x250b, 0x2500 }, /* key 37 - 'K' */
	{	0x266c, 0x264c, 0x260c, 0x2600 }, /* key 38 - 'L' */
	{	0x273b, 0x273a, 0xffff, 0xffff }, /* key 39 - ';' */
	{	0x2827, 0x2822, 0xffff, 0xffff }, /* key 40 - ''' */
	{	0x2960, 0x297e, 0xffff, 0xffff }, /* key 41 - '`' */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 42 - left shift */
	{	0x2b5c, 0x2b7c, 0x2b1c, 0xffff }, /* key 43 - '\' */
	{	0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* key 44 - 'Z' */
	{	0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* key 45 - 'X' */
	{	0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* key 46 - 'C' */
	{	0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* key 47 - 'V' */
	{	0x3062, 0x3042, 0x3002, 0x3000 }, /* key 48 - 'B' */
	{	0x316e, 0x314e, 0x310e, 0x3100 }, /* key 49 - 'N' */
	{	0x326d, 0x324d, 0x320d, 0x3200 }, /* key 50 - 'M' */
	{	0x332c, 0x333c, 0xffff, 0xffff }, /* key 51 - ',' */
	{	0x342e, 0x343e, 0xffff, 0xffff }, /* key 52 - '.' */
	{	0x352f, 0x353f, 0xffff, 0xffff }, /* key 53 - '/' */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 54 - right shift */
	{	0x372a, 0xffff, 0x3772, 0xffff }, /* key 55 - prt-scr */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 56 - Alt */
	{	0x3920, 0x3920, 0x3920, 0x3920 }, /* key 57 - space bar */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 58 - caps-lock */
	{	0x3b00, 0x5400, 0x5e00, 0x6800 }, /* key 59 - F1 */
	{	0x3c00, 0x5500, 0x5f00, 0x6900 }, /* key 60 - F2 */
	{	0x3d00, 0x5600, 0x6000, 0x6a00 }, /* key 61 - F3 */
	{	0x3e00, 0x5700, 0x6100, 0x6b00 }, /* key 62 - F4 */
	{	0x3f00, 0x5800, 0x6200, 0x6c00 }, /* key 63 - F5 */
	{	0x4000, 0x5900, 0x6300, 0x6d00 }, /* key 64 - F6 */
	{	0x4100, 0x5a00, 0x6400, 0x6e00 }, /* key 65 - F7 */
	{	0x4200, 0x5b00, 0x6500, 0x6f00 }, /* key 66 - F8 */
	{	0x4300, 0x5c00, 0x6600, 0x7000 }, /* key 67 - F9 */
	{	0x4400, 0x5d00, 0x6700, 0x7100 }, /* key 68 - F10 */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 69 - num-lock */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 70 - scroll-lock */
	{	0x4700, 0x3700, 0x7700, 0xffff }, /* key 71 - home */
	{	0x4800, 0x3800, 0x8d00, 0x9800 }, /* key 72 - cursor up */
	{	0x4900, 0x3900, 0x8400, 0xffff }, /* key 73 - page up */
	{	0x4a2d, 0x4a2d, 0xffff, 0xffff }, /* key 74 - minus sign */
	{	0x4b00, 0x3400, 0x7300, 0xffff }, /* key 75 - cursor left */
	{	0xffff, 0x4c35, 0xffff, 0xffff }, /* key 76 - center key */
	{	0x4d00, 0x3600, 0x7400, 0xffff }, /* key 77 - cursor right */
	{	0x4e2b, 0x4e2b, 0xffff, 0xffff }, /* key 78 - plus sign */
	{	0x4f00, 0x3100, 0x7500, 0xffff }, /* key 79 - end */
	{	0x5000, 0x3200, 0x9100, 0xa000 }, /* key 80 - cursor down */
	{	0x5100, 0x3300, 0x7600, 0xffff }, /* key 81 - page down */
	{	CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC }, /* key 82 - insert */
	{	CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_ALT_DC }, /* key 83 - delete */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
	{	0x8500, 0x8700, 0x8900, 0x8b00 }, /* key 87 - F11 */
	{	0x8600, 0x8800, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

/*
 * Extended evdev keycodes for keys outside the AT Set 1 range.
 * Evdev codes 96+ for non-numpad navigation keys.
 */
#define EVDEV_KEY_HOME		102
#define EVDEV_KEY_UP		103
#define EVDEV_KEY_PAGEUP	104
#define EVDEV_KEY_LEFT		105
#define EVDEV_KEY_RIGHT		106
#define EVDEV_KEY_END		107
#define EVDEV_KEY_DOWN		108
#define EVDEV_KEY_PAGEDOWN	109
#define EVDEV_KEY_INSERT	110
#define EVDEV_KEY_DELETE		111

static void update_surface_size(int w, int h);

/*
 * Alt+Arrow resize: snap scaling to next/previous integer multiplier.
 * Mirrors the SDL backend's bitmap_snap() approach.
 */
static void
snap_resize(bool grow)
{
	int w, h;

	assert_rwlock_wrlock(&vstatlock);
	bitmap_snap(grow, 0, 0);
	w = vstat.winwidth;
	h = vstat.winheight;
	wl_cvstat = vstat;
	assert_rwlock_unlock(&vstatlock);

	update_surface_size(w, h);
	bitmap_drv_request_pixels();
}

static void
send_key(WORD keyval)
{
	unsigned char buf[2];

	if (keyval == 0xffff)
		return;
	if (keyval & 0xff) {
		buf[0] = keyval & 0xff;
		write(wl_key_pipe[1], buf, 1);
	}
	else {
		buf[0] = 0;
		buf[1] = (keyval >> 8) & 0xff;
		write(wl_key_pipe[1], buf, 2);
	}
}

static void
send_scancode(uint32_t evdev_key, bool pressed)
{
	WORD keyval = 0xffff;

	/*
	 * Handle extended navigation keys that have evdev codes
	 * outside the ScanCodes[] table range.  Remap before the
	 * pressed check so release events match the stored repeat_key.
	 */
	switch (evdev_key) {
	case EVDEV_KEY_HOME:
		evdev_key = 71;
		break;
	case EVDEV_KEY_UP:
		evdev_key = 72;
		break;
	case EVDEV_KEY_PAGEUP:
		evdev_key = 73;
		break;
	case EVDEV_KEY_LEFT:
		evdev_key = 75;
		break;
	case EVDEV_KEY_RIGHT:
		evdev_key = 77;
		break;
	case EVDEV_KEY_END:
		evdev_key = 79;
		break;
	case EVDEV_KEY_DOWN:
		evdev_key = 80;
		break;
	case EVDEV_KEY_PAGEDOWN:
		evdev_key = 81;
		break;
	case EVDEV_KEY_INSERT:
		evdev_key = 82;
		break;
	case EVDEV_KEY_DELETE:
		evdev_key = 83;
		break;
	}

	if (!pressed) {
		if ((int)evdev_key == repeat_key)
			repeat_key = -1;
		return;
	}

	/* Alt+Left/Right: snap window scaling to prev/next integer */
	if (alt_held && !maximized && !fullscreen
	    && (evdev_key == 75 || evdev_key == 77)) {
		snap_resize(evdev_key == 77);
		return;
	}

	/*
	 * When xkbcommon is active and no Ctrl/Alt is held, try to
	 * get a character from xkb_state.  This gives correct results
	 * for non-US keyboard layouts.  Special keys (arrows, F-keys)
	 * return 0 from xkb_state_key_get_utf32 and fall through to
	 * the evdev scancode table below.
	 */
	if (xkb_st && !alt_held && !ctrl_held) {
		uint32_t utf32 = xkb.state_key_get_utf32(xkb_st,
		    evdev_key + 8);
		if (utf32 >= 0x20 && utf32 != 0x7f) {
			uint8_t ch = cpchar_from_unicode_cpoint(
			    getcodepage(), utf32,
			    utf32 < 128 ? (char)utf32 : 0);
			if (ch) {
				if (ch == 0xe0)
					write(wl_key_pipe[1], &ch, 1);
				write(wl_key_pipe[1], &ch, 1);
				repeat_key = evdev_key;
				repeat_since = xp_timer64();
				repeat_in_delay = true;
				return;
			}
		}
	}

	if (evdev_key < sizeof(ScanCodes) / sizeof(ScanCodes[0])) {
		if (alt_held)
			keyval = ScanCodes[evdev_key].alt;
		else if (ctrl_held)
			keyval = ScanCodes[evdev_key].ctrl;
		else if (shift_held)
			keyval = ScanCodes[evdev_key].shift;
		else
			keyval = ScanCodes[evdev_key].base;
	}

	if (keyval != 0xffff) {
		send_key(keyval);
		repeat_key = evdev_key;
		repeat_since = xp_timer64();
		repeat_in_delay = true;
	}
}

/*
 * SHM buffer management
 */

static int
create_shm_file(size_t size)
{
	int fd;

#ifdef SHM_ANON
	fd = shm_open(SHM_ANON, O_RDWR | O_CREAT | O_EXCL, 0600);
#else
	char name[] = "/syncterm-shm-XXXXXX";
	fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd >= 0)
		shm_unlink(name);
#endif
	if (fd < 0)
		return -1;
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct wl_buffer_info *bi = data;
	bi->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void
destroy_buffer(struct wl_buffer_info *bi)
{
	if (bi->buffer) {
		wl_buffer_destroy(bi->buffer);
		bi->buffer = NULL;
	}
	if (bi->data) {
		munmap(bi->data, bi->width * bi->height * 4);
		bi->data = NULL;
	}
	bi->busy = false;
}

static bool
create_buffer(struct wl_buffer_info *bi, int width, int height)
{
	int stride = width * 4;
	size_t size = stride * height;
	int fd;
	struct wl_shm_pool *pool;

	destroy_buffer(bi);

	fd = create_shm_file(size);
	if (fd < 0)
		return false;

	bi->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (bi->data == MAP_FAILED) {
		close(fd);
		bi->data = NULL;
		return false;
	}

	pool = wl_shm_create_pool(wl_shm_global, fd, size);
	close(fd);
	if (!pool) {
		munmap(bi->data, size);
		bi->data = NULL;
		return false;
	}

	bi->buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
	    stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	if (!bi->buffer) {
		munmap(bi->data, size);
		bi->data = NULL;
		return false;
	}

	wl_buffer_add_listener(bi->buffer, &buffer_listener, bi);
	bi->width = width;
	bi->height = height;
	bi->busy = false;
	return true;
}

static struct wl_buffer_info *
get_free_buffer(int width, int height)
{
	struct wl_buffer_info *bi;

	for (int i = 0; i < 2; i++) {
		int idx = (current_buf + i) % 2;
		bi = &buffers[idx];
		if (!bi->busy) {
			if (bi->width != width || bi->height != height) {
				if (!create_buffer(bi, width, height))
					return NULL;
			}
			current_buf = idx;
			return bi;
		}
	}
	return NULL;
}

/*
 * Mouse coordinate translation
 */
static bool
xlat_mouse_xy(int *x, int *y)
{
	int sw, sh;

	assert_rwlock_rdlock(&vstatlock);
	sw = vstat.scrnwidth;
	sh = vstat.scrnheight;
	assert_rwlock_unlock(&vstatlock);

	if (surface_width <= 0 || surface_height <= 0)
		return false;

	if (*x < 0)
		*x = 0;
	if (*y < 0)
		*y = 0;
	if (*x >= surface_width)
		*x = surface_width - 1;
	if (*y >= surface_height)
		*y = surface_height - 1;

	*x = (*x * sw) / surface_width;
	*y = (*y * sh) / surface_height;
	return true;
}

/*
 * Wayland protocol listeners
 */

/* xdg_toplevel_decoration (server-side decorations) */
static void
deco_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco,
    uint32_t mode)
{
	have_server_decorations =
	    (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static const struct zxdg_toplevel_decoration_v1_listener deco_listener = {
	.configure = deco_configure,
};

/* xdg_wm_base (ping/pong) */
static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial)
{
	xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener xdg_base_listener = {
	.ping = xdg_wm_base_ping,
};

/* xdg_surface (configure ack) */
static void
xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	xdg_surface_ack_configure(surface, serial);
	configured = true;
}

static const struct xdg_surface_listener xdg_surf_listener = {
	.configure = xdg_surface_configure,
};

/* xdg_toplevel (configure, close) */
static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
    int32_t width, int32_t height, struct wl_array *states)
{
	uint32_t *state;
	bool fs = false;
	bool max = false;

	wl_array_for_each(state, states) {
		if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN)
			fs = true;
		else if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED)
			max = true;
	}
	fullscreen = fs;
	maximized = max;

	if (width > 0 && height > 0) {
		pending_width = width;
		pending_height = height;
	}
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	uint16_t key = CIO_KEY_QUIT;
	write(wl_key_pipe[1], &key, 2);
}

static void
xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
    int32_t width, int32_t height)
{
}

static void
xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
    struct wl_array *capabilities)
{
}

static const struct xdg_toplevel_listener xdg_top_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
	.configure_bounds = xdg_toplevel_configure_bounds,
	.wm_capabilities = xdg_toplevel_wm_capabilities,
};

/* wl_keyboard */
static void
kb_keymap(void *data, struct wl_keyboard *keyboard,
    uint32_t format, int32_t fd, uint32_t size)
{
	char *map_str;

	if (!xkb_ctx || format != 1 /* WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 */) {
		close(fd);
		return;
	}

	map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map_str == MAP_FAILED)
		return;

	/* Tear down previous keymap/state if the compositor sends a new one */
	if (xkb_st) {
		xkb.state_unref(xkb_st);
		xkb_st = NULL;
	}
	if (xkb_kmap) {
		xkb.keymap_unref(xkb_kmap);
		xkb_kmap = NULL;
	}

	xkb_kmap = xkb.keymap_new_from_string(xkb_ctx, map_str,
	    XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_str, size);
	if (!xkb_kmap)
		return;

	xkb_st = xkb.state_new(xkb_kmap);
	if (!xkb_st) {
		xkb.keymap_unref(xkb_kmap);
		xkb_kmap = NULL;
	}
}

static void
kb_enter(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void
kb_leave(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, struct wl_surface *surface)
{
	shift_held = false;
	ctrl_held = false;
	alt_held = false;
	repeat_key = -1;
}

static void
kb_key(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

	/*
	 * Wayland sends evdev keycodes.  The evdev keycode is the
	 * Linux input event code, which is the AT Set 1 make code.
	 * The ScanCodes[] table uses AT Set 1 indices directly.
	 *
	 * Note: wl_keyboard sends key+8 for backward compat with X11,
	 * but the Wayland protocol specifies raw evdev codes without
	 * the +8 offset.  However, some compositors may or may not...
	 * The spec says: "key is a platform-specific key code... on
	 * Linux it is the evdev keycode" — no offset.
	 */
	send_scancode(key, pressed);
}

static void
kb_modifiers(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group)
{
	if (xkb_st) {
		xkb.state_update_mask(xkb_st,
		    mods_depressed, mods_latched, mods_locked,
		    0, 0, group);
		shift_held = xkb.state_mod_name_is_active(xkb_st,
		    "Shift", XKB_STATE_MODS_EFFECTIVE);
		ctrl_held = xkb.state_mod_name_is_active(xkb_st,
		    "Control", XKB_STATE_MODS_EFFECTIVE);
		alt_held = xkb.state_mod_name_is_active(xkb_st,
		    "Mod1", XKB_STATE_MODS_EFFECTIVE);
	}
	else {
		/*
		 * Without xkbcommon we can't properly interpret modifier
		 * indices.  Use bit positions from the evdev/XKB modifier
		 * map: Bit 0 = Shift, Bit 2 = Control, Bit 3 = Mod1 (Alt)
		 */
		shift_held = (mods_depressed & 1) != 0;
		ctrl_held = (mods_depressed & 4) != 0;
		alt_held = (mods_depressed & 8) != 0;
	}
}

static void
kb_repeat_info(void *data, struct wl_keyboard *keyboard,
    int32_t rate, int32_t delay)
{
	if (rate > 0)
		repeat_rate_ms = 1000 / rate;
	else
		repeat_rate_ms = 0;
	repeat_delay_ms = delay;
}

static const struct wl_keyboard_listener kb_listener = {
	.keymap = kb_keymap,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_modifiers,
	.repeat_info = kb_repeat_info,
};

/* wl_pointer */
static void
ptr_enter(void *data, struct wl_pointer *pointer,
    uint32_t serial, struct wl_surface *surface,
    wl_fixed_t sx, wl_fixed_t sy)
{
	mouse_in_window = true;
	mouse_surface_x = wl_fixed_to_int(sx);
	mouse_surface_y = wl_fixed_to_int(sy);
	last_pointer_serial = serial;
	set_cursor_shape(serial);
}

static void
ptr_leave(void *data, struct wl_pointer *pointer,
    uint32_t serial, struct wl_surface *surface)
{
	mouse_in_window = false;
}

static void
ptr_motion(void *data, struct wl_pointer *pointer,
    uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	int x, y, cx, cy;

	mouse_surface_x = wl_fixed_to_int(sx);
	mouse_surface_y = wl_fixed_to_int(sy);

	x = mouse_surface_x;
	y = mouse_surface_y;
	if (!xlat_mouse_xy(&x, &y))
		return;

	cx = x / wl_cvstat.charwidth + 1;
	cy = y / wl_cvstat.charheight + 1;
	if (cx < 1) cx = 1;
	if (cy < 1) cy = 1;
	if (cx > wl_cvstat.cols) cx = wl_cvstat.cols;
	if (cy > wl_cvstat.rows + 1) cy = wl_cvstat.rows + 1;
	ciomouse_gotevent(CIOLIB_MOUSE_MOVE, cx, cy, x, y);
}

static void
ptr_button(void *data, struct wl_pointer *pointer,
    uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	int x, y, cx, cy;
	int btn;

	last_input_serial = serial;

	/*
	 * Alt+left-click initiates interactive window move when the
	 * compositor hasn't provided server-side decorations.
	 * The ciolib mouse API doesn't expose keyboard modifiers,
	 * so applications can never see Alt+click — this is safe.
	 */
	if (!have_server_decorations
	    && alt_held
	    && button == 0x110
	    && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		xdg_toplevel_move(xdg_top, wl_seat_global, serial);
		return;
	}

	x = mouse_surface_x;
	y = mouse_surface_y;
	if (!xlat_mouse_xy(&x, &y))
		return;

	cx = x / wl_cvstat.charwidth + 1;
	cy = y / wl_cvstat.charheight + 1;
	if (cx < 1) cx = 1;
	if (cy < 1) cy = 1;
	if (cx > wl_cvstat.cols) cx = wl_cvstat.cols;
	if (cy > wl_cvstat.rows + 1) cy = wl_cvstat.rows + 1;

	/*
	 * Linux button codes: BTN_LEFT=0x110, BTN_RIGHT=0x111, BTN_MIDDLE=0x112
	 * CIOLIB buttons are 1-based: 1=left, 2=middle, 3=right
	 */
	switch (button) {
	case 0x110: btn = 1; break;
	case 0x111: btn = 3; break;
	case 0x112: btn = 2; break;
	default: return;
	}

	if (state == WL_POINTER_BUTTON_STATE_PRESSED)
		ciomouse_gotevent(CIOLIB_BUTTON_PRESS(btn), cx, cy, x, y);
	else
		ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(btn), cx, cy, x, y);
}

static void
ptr_axis(void *data, struct wl_pointer *pointer,
    uint32_t time, uint32_t axis, wl_fixed_t value)
{
	int x, y, cx, cy;

	if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
		return;

	x = mouse_surface_x;
	y = mouse_surface_y;
	if (!xlat_mouse_xy(&x, &y))
		return;

	cx = x / wl_cvstat.charwidth + 1;
	cy = y / wl_cvstat.charheight + 1;
	if (cx < 1) cx = 1;
	if (cy < 1) cy = 1;
	if (cx > wl_cvstat.cols) cx = wl_cvstat.cols;
	if (cy > wl_cvstat.rows + 1) cy = wl_cvstat.rows + 1;

	if (wl_fixed_to_double(value) < 0)
		ciomouse_gotevent(CIOLIB_BUTTON_PRESS(4), cx, cy, x, y);
	else
		ciomouse_gotevent(CIOLIB_BUTTON_PRESS(5), cx, cy, x, y);
}

static void ptr_frame(void *data, struct wl_pointer *pointer) {}
static void ptr_axis_source(void *data, struct wl_pointer *pointer, uint32_t source) {}
static void ptr_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {}
static void ptr_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {}
static void ptr_axis_value120(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t value120) {}
static void ptr_axis_relative_direction(void *data, struct wl_pointer *pointer, uint32_t axis, uint32_t direction) {}

static const struct wl_pointer_listener ptr_listener = {
	.enter = ptr_enter,
	.leave = ptr_leave,
	.motion = ptr_motion,
	.button = ptr_button,
	.axis = ptr_axis,
	.frame = ptr_frame,
	.axis_source = ptr_axis_source,
	.axis_stop = ptr_axis_stop,
	.axis_discrete = ptr_axis_discrete,
	.axis_value120 = ptr_axis_value120,
	.axis_relative_direction = ptr_axis_relative_direction,
};

/* wl_seat capabilities */
static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl_kb) {
		wl_kb = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl_kb, &kb_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl_ptr) {
		wl_ptr = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(wl_ptr, &ptr_listener, NULL);
	}
}

static void
seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

/* wl_data_source (copy side — compositor asks us for data) */
static void
ds_target(void *data, struct wl_data_source *source, const char *mime_type)
{
}

static void
ds_send(void *data, struct wl_data_source *source,
    const char *mime_type, int32_t fd)
{
	assert_pthread_mutex_lock(&wl_copybuf_mutex);
	if (wl_copybuf) {
		size_t len = strlen(wl_copybuf);
		size_t sent = 0;
		while (sent < len) {
			ssize_t rv = write(fd, wl_copybuf + sent, len - sent);
			if (rv <= 0)
				break;
			sent += rv;
		}
	}
	assert_pthread_mutex_unlock(&wl_copybuf_mutex);
	close(fd);
}

static void
ds_cancelled(void *data, struct wl_data_source *source)
{
	wl_data_source_destroy(source);
	if (wl_copy_source == source)
		wl_copy_source = NULL;
}

static void
ds_dnd_drop_performed(void *data, struct wl_data_source *source)
{
}

static void
ds_dnd_finished(void *data, struct wl_data_source *source)
{
}

static void
ds_action(void *data, struct wl_data_source *source, uint32_t action)
{
}

static const struct wl_data_source_listener ds_listener = {
	.target = ds_target,
	.send = ds_send,
	.cancelled = ds_cancelled,
	.dnd_drop_performed = ds_dnd_drop_performed,
	.dnd_finished = ds_dnd_finished,
	.action = ds_action,
};

/* wl_data_offer (paste side — track offered MIME types) */
static void
do_offer(void *data, struct wl_data_offer *offer, const char *mime_type)
{
	if (strcmp(mime_type, "text/plain;charset=utf-8") == 0
	    || strcmp(mime_type, "text/plain") == 0)
		wl_pending_offer_has_text = true;
}

static void
do_source_actions(void *data, struct wl_data_offer *offer,
    uint32_t source_actions)
{
}

static void
do_action(void *data, struct wl_data_offer *offer, uint32_t dnd_action)
{
}

static const struct wl_data_offer_listener offer_listener = {
	.offer = do_offer,
	.source_actions = do_source_actions,
	.action = do_action,
};

/* wl_data_device (selection tracking) */
static void
dd_data_offer(void *data, struct wl_data_device *dev,
    struct wl_data_offer *offer)
{
	wl_pending_offer_has_text = false;
	wl_data_offer_add_listener(offer, &offer_listener, NULL);
}

static void
dd_enter(void *data, struct wl_data_device *dev,
    uint32_t serial, struct wl_surface *surface,
    wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *offer)
{
}

static void
dd_leave(void *data, struct wl_data_device *dev)
{
}

static void
dd_motion(void *data, struct wl_data_device *dev,
    uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
}

static void
dd_drop(void *data, struct wl_data_device *dev)
{
}

static void
dd_selection(void *data, struct wl_data_device *dev,
    struct wl_data_offer *offer)
{
	if (wl_sel_offer && wl_sel_offer != offer)
		wl_data_offer_destroy(wl_sel_offer);
	wl_sel_offer = offer;
	wl_sel_offer_has_text = wl_pending_offer_has_text;
}

static const struct wl_data_device_listener dd_listener = {
	.data_offer = dd_data_offer,
	.enter = dd_enter,
	.leave = dd_leave,
	.motion = dd_motion,
	.drop = dd_drop,
	.selection = dd_selection,
};

/* wl_registry (global binding) */
static void
registry_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_compositor_interface.name) == 0)
		wl_comp = wl_registry_bind(registry, name,
		    &wl_compositor_interface, 4);
	else if (strcmp(interface, wl_shm_interface.name) == 0)
		wl_shm_global = wl_registry_bind(registry, name,
		    &wl_shm_interface, 1);
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		wl_seat_global = wl_registry_bind(registry, name,
		    &wl_seat_interface, 5);
		wl_seat_add_listener(wl_seat_global, &seat_listener, NULL);
	}
	else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		xdg_base = wl_registry_bind(registry, name,
		    &xdg_wm_base_interface, 2);
		xdg_wm_base_add_listener(xdg_base, &xdg_base_listener, NULL);
	}
	else if (strcmp(interface, wp_viewporter_interface.name) == 0)
		wl_viewporter = wl_registry_bind(registry, name,
		    &wp_viewporter_interface, 1);
	else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
		wl_deco_mgr = wl_registry_bind(registry, name,
		    &zxdg_decoration_manager_v1_interface, 1);
	else if (strcmp(interface, xdg_toplevel_icon_manager_v1_interface.name) == 0)
		wl_icon_mgr = wl_registry_bind(registry, name,
		    &xdg_toplevel_icon_manager_v1_interface, 1);
	else if (strcmp(interface, wl_data_device_manager_interface.name) == 0)
		wl_ddm = wl_registry_bind(registry, name,
		    &wl_data_device_manager_interface, 3);
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
    uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/*
 * Window setup and mode change
 */

static void
setup_surface(void)
{
	int w, h;

	assert_rwlock_rdlock(&vstatlock);
	bitmap_get_scaled_win_size(vstat.scaling, &w, &h, 0, 0);
	assert_rwlock_unlock(&vstatlock);

	wl_surf = wl_compositor_create_surface(wl_comp);
	xdg_surf = xdg_wm_base_get_xdg_surface(xdg_base, wl_surf);
	xdg_surface_add_listener(xdg_surf, &xdg_surf_listener, NULL);

	xdg_top = xdg_surface_get_toplevel(xdg_surf);
	xdg_toplevel_add_listener(xdg_top, &xdg_top_listener, NULL);
	xdg_toplevel_set_title(xdg_top, ciolib_initial_program_name);
	xdg_toplevel_set_app_id(xdg_top, ciolib_initial_program_class);

	if (fullscreen)
		xdg_toplevel_set_fullscreen(xdg_top, NULL);

	/* Request server-side decorations if available */
	if (wl_deco_mgr) {
		wl_deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
		    wl_deco_mgr, xdg_top);
		zxdg_toplevel_decoration_v1_add_listener(wl_deco,
		    &deco_listener, NULL);
		zxdg_toplevel_decoration_v1_set_mode(wl_deco,
		    ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	if (wl_viewporter) {
		wl_viewport = wp_viewporter_get_viewport(wl_viewporter, wl_surf);
		wp_viewport_set_destination(wl_viewport, w, h);
	}

	/*
	 * Declare our buffer is at scale 1 so the compositor does not
	 * apply HiDPI scaling on top of our own rendering.  We handle
	 * scaling ourselves (internal) or via viewporter (external).
	 */
	wl_surface_set_buffer_scale(wl_surf, 1);

	/* Make surface opaque */
	wl_opaque_region = wl_compositor_create_region(wl_comp);
	wl_region_add(wl_opaque_region, 0, 0, w, h);
	wl_surface_set_opaque_region(wl_surf, wl_opaque_region);

	surface_width = w;
	surface_height = h;

	wl_surface_commit(wl_surf);
	wl_display_roundtrip(wl_dpy);
}

static void
init_mode_internal(int mode)
{
	assert_rwlock_wrlock(&vstatlock);
	bitmap_drv_init_mode(mode, NULL, NULL, 0, 0);
	assert_rwlock_unlock(&vstatlock);
}

static int
init_mode(int mode)
{
	int w, h;

	init_mode_internal(mode);

	assert_rwlock_rdlock(&vstatlock);
	bitmap_get_scaled_win_size(vstat.scaling, &w, &h, 0, 0);
	wl_cvstat = vstat;
	assert_rwlock_unlock(&vstatlock);

	update_surface_size(w, h);
	bitmap_drv_request_pixels();
	sem_post(&wl_mode_set);
	return 0;
}

/*
 * Rendering
 */
static void
local_draw_rect(struct rectlist *rect)
{
	struct wl_buffer_info *bi;
	struct graphics_buffer *scaled = NULL;
	int bw, bh;
	uint32_t *src;

	if (!wl_surf || !configured)
		goto done;

	if (wl_internal_scaling) {
		int sw, sh;

		assert_rwlock_rdlock(&vstatlock);
		if (wl_cvstat.scrnwidth != rect->rect.width
		    || wl_cvstat.scrnheight != rect->rect.height) {
			assert_rwlock_unlock(&vstatlock);
			goto done;
		}
		bitmap_get_scaled_win_size(vstat.scaling, &sw, &sh,
		    vstat.winwidth, vstat.winheight);
		assert_rwlock_unlock(&vstatlock);

		if (sw < rect->rect.width || sh < rect->rect.height)
			goto done;

		scaled = do_scale(rect, sw, sh);
		if (!scaled)
			goto done;
		src = scaled->data;
		bw = scaled->w;
		bh = scaled->h;
	}
	else {
		src = rect->data;
		bw = rect->rect.width;
		bh = rect->rect.height;
	}

	bi = get_free_buffer(bw, bh);
	if (!bi)
		goto done;

	memcpy(bi->data, src, bw * bh * 4);
	bi->busy = true;

	wl_surface_attach(wl_surf, bi->buffer, 0, 0);
	wl_surface_damage_buffer(wl_surf, 0, 0, bw, bh);
	wl_surface_commit(wl_surf);

done:
	if (scaled)
		release_buffer(scaled);
	bitmap_drv_free_rect(rect);
}

/*
 * Read a local event from the pipe
 */
static void
readev(struct wl_local_event *lev)
{
	size_t got = 0;
	char *buf = (char *)lev;

	while (got < sizeof(*lev)) {
		int rv = read(wl_local_pipe[0], buf + got, sizeof(*lev) - got);
		if (rv > 0)
			got += rv;
	}
}

/*
 * Handle pending configure (resize)
 */
static void
update_surface_size(int w, int h)
{
	surface_width = w;
	surface_height = h;
	if (wl_viewport)
		wp_viewport_set_destination(wl_viewport, w, h);
	if (wl_opaque_region) {
		wl_region_destroy(wl_opaque_region);
		wl_opaque_region = wl_compositor_create_region(wl_comp);
		wl_region_add(wl_opaque_region, 0, 0, w, h);
		wl_surface_set_opaque_region(wl_surf, wl_opaque_region);
	}
}

static void
handle_configure(void)
{
	int w, h;

	if (pending_width <= 0 || pending_height <= 0)
		return;

	w = pending_width;
	h = pending_height;
	pending_width = 0;
	pending_height = 0;

	if (w == surface_width && h == surface_height)
		return;

	assert_rwlock_wrlock(&vstatlock);
	wl_cvstat.winwidth = vstat.winwidth = w;
	wl_cvstat.winheight = vstat.winheight = h;
	vstat.scaling = wl_cvstat.scaling =
	    bitmap_double_mult_inside(w, h);
	assert_rwlock_unlock(&vstatlock);

	update_surface_size(w, h);
	bitmap_drv_request_pixels();
}

/*
 * Toplevel icon via xdg-toplevel-icon-v1.
 * The protocol is optional — if the compositor doesn't advertise
 * xdg_toplevel_icon_manager_v1, this is a no-op.
 */
static void
set_toplevel_icon(uint32_t *pixels, unsigned long width)
{
	struct xdg_toplevel_icon_v1 *icon;
	int stride, fd;
	size_t size;
	struct wl_shm_pool *pool;
	struct wl_buffer *buf;
	void *data;

	if (!wl_icon_mgr || !xdg_top || !wl_shm_global)
		goto done;

	stride = width * 4;
	size = stride * width;

	fd = create_shm_file(size);
	if (fd < 0)
		goto done;

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		goto done;
	}

	memcpy(data, pixels, size);

	pool = wl_shm_create_pool(wl_shm_global, fd, size);
	close(fd);
	if (!pool) {
		munmap(data, size);
		goto done;
	}

	buf = wl_shm_pool_create_buffer(pool, 0, width, width,
	    stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	if (!buf) {
		munmap(data, size);
		goto done;
	}

	icon = xdg_toplevel_icon_manager_v1_create_icon(wl_icon_mgr);
	xdg_toplevel_icon_v1_add_buffer(icon, buf, 1);
	xdg_toplevel_icon_manager_v1_set_icon(wl_icon_mgr, xdg_top, icon);
	wl_surface_commit(wl_surf);

	xdg_toplevel_icon_v1_destroy(icon);
	wl_buffer_destroy(buf);
	munmap(data, size);

done:
	free(pixels);
}

/*
 * Key repeat handling (xp_timer64 uses CLOCK_MONOTONIC, returns ms)
 */
static void
handle_key_repeat(void)
{
	uint64_t now, elapsed;
	int threshold;

	if (repeat_key < 0 || repeat_rate_ms <= 0)
		return;

	now = xp_timer64();
	elapsed = now - repeat_since;
	threshold = repeat_in_delay ? repeat_delay_ms : repeat_rate_ms;

	if (elapsed >= (uint64_t)threshold) {
		send_scancode(repeat_key, true);
		repeat_since = now;
		repeat_in_delay = false;
	}
}

/*
 * Main event thread
 */
void
wl_event_thread(void *args)
{
	int mode = (int)(intptr_t)args;
	int wl_fd;
	int high_fd;
	fd_set fdset;
	struct timeval tv;

	SetThreadName("WL Events");

	if (mode == CIOLIB_MODE_WAYLAND_FULLSCREEN)
		fullscreen = true;

	repeat_key = -1;
	repeat_rate_ms = 33;
	repeat_delay_ms = 500;

	/* Try to load xkbcommon for keyboard layout support */
	xkb_dlopen();

	/* Connect to Wayland display */
	wl_dpy = wl_display_connect(NULL);
	if (!wl_dpy) {
		sem_post(&wl_init_complete);
		return;
	}

	/* Get registry and bind globals */
	wl_reg = wl_display_get_registry(wl_dpy);
	wl_registry_add_listener(wl_reg, &registry_listener, NULL);
	wl_display_roundtrip(wl_dpy);

	/* Verify required globals */
	if (!wl_comp || !wl_shm_global || !xdg_base || !wl_seat_global) {
		wl_display_disconnect(wl_dpy);
		wl_dpy = NULL;
		sem_post(&wl_init_complete);
		return;
	}

	/* Second roundtrip to get seat capabilities */
	wl_display_roundtrip(wl_dpy);

	if (!wl_kb) {
		wl_display_disconnect(wl_dpy);
		wl_dpy = NULL;
		sem_post(&wl_init_complete);
		return;
	}

	/* Set up cursor theme */
	if (wcur_dlopen() == 0) {
		cursor_theme = wcur.theme_load(NULL, 24, wl_shm_global);
		if (cursor_theme)
			cursor_surf = wl_compositor_create_surface(wl_comp);
	}

	/* Set up clipboard data device */
	if (wl_ddm && wl_seat_global) {
		wl_ddev = wl_data_device_manager_get_data_device(wl_ddm,
		    wl_seat_global);
		wl_data_device_add_listener(wl_ddev, &dd_listener, NULL);
	}

	/* Initialize bitmap driver */
	wl_internal_scaling =
	    (ciolib_initial_scaling_type == CIOLIB_SCALING_INTERNAL);
	assert_rwlock_wrlock(&vstatlock);
	if (ciolib_initial_scaling < 1.0)
		ciolib_initial_scaling = 1.0;
	wl_cvstat.scaling = vstat.scaling = ciolib_initial_scaling;
	if (load_vmode(&vstat, ciolib_initial_mode)) {
		assert_rwlock_unlock(&vstatlock);
		wl_display_disconnect(wl_dpy);
		wl_dpy = NULL;
		sem_post(&wl_init_complete);
		return;
	}
	wl_cvstat = vstat;
	assert_rwlock_unlock(&vstatlock);

	bitmap_drv_init(wl_drawrect, wl_flush);
	init_mode_internal(ciolib_initial_mode);

	/* Create window surface */
	setup_surface();

	wl_fd = wl_display_get_fd(wl_dpy);
	if (wl_local_pipe[0] > wl_fd)
		high_fd = wl_local_pipe[0];
	else
		high_fd = wl_fd;

	wl_initialized = 1;
	sem_post(&wl_init_complete);

	repeat_since = xp_timer64();

	for (; !terminate;) {
		tv.tv_sec = 0;
		tv.tv_usec = 20000;

		/* Flush outgoing requests */
		while (wl_display_prepare_read(wl_dpy) != 0)
			wl_display_dispatch_pending(wl_dpy);
		wl_display_flush(wl_dpy);

		FD_ZERO(&fdset);
		FD_SET(wl_fd, &fdset);
		FD_SET(wl_local_pipe[0], &fdset);

		int ret = select(high_fd + 1, &fdset, NULL, NULL, &tv);
		if (ret < 0) {
			wl_display_cancel_read(wl_dpy);
			if (errno == EINTR)
				continue;
			break;
		}

		if (FD_ISSET(wl_fd, &fdset))
			wl_display_read_events(wl_dpy);
		else
			wl_display_cancel_read(wl_dpy);

		wl_display_dispatch_pending(wl_dpy);
		wl_display_flush(wl_dpy);

		/* Handle configure events */
		if (configured)
			handle_configure();

		/* Handle local pipe commands */
		if (ret > 0 && FD_ISSET(wl_local_pipe[0], &fdset)) {
			struct wl_local_event lev;

			readev(&lev);
			switch (lev.type) {
			case WL_LOCAL_SETMODE:
				init_mode(lev.data.mode);
				break;
			case WL_LOCAL_SETTITLE:
				if (xdg_top)
					xdg_toplevel_set_title(xdg_top, lev.data.title);
				break;
			case WL_LOCAL_SETNAME:
				if (xdg_top)
					xdg_toplevel_set_app_id(xdg_top, lev.data.name);
				break;
			case WL_LOCAL_DRAWRECT:
				local_draw_rect(lev.data.rect);
				break;
			case WL_LOCAL_FLUSH:
				wl_display_flush(wl_dpy);
				break;
			case WL_LOCAL_BEEP:
				/* Wayland has no bell protocol; xdg-system-bell is staging */
				break;
			case WL_LOCAL_SETICON:
				set_toplevel_icon(lev.data.icon.pixels,
				    lev.data.icon.width);
				break;
			case WL_LOCAL_SETSCALING: {
				int w, h;

				assert_rwlock_wrlock(&vstatlock);
				bitmap_get_scaled_win_size(lev.data.scaling,
				    &w, &h, 0, 0);
				wl_cvstat.winwidth = vstat.winwidth = w;
				wl_cvstat.winheight = vstat.winheight = h;
				vstat.scaling = wl_cvstat.scaling =
				    bitmap_double_mult_inside(w, h);
				assert_rwlock_unlock(&vstatlock);
				update_surface_size(w, h);
				bitmap_drv_request_pixels();
				break;
			}
			case WL_LOCAL_SETSCALING_TYPE:
				wl_internal_scaling =
				    (lev.data.st == CIOLIB_SCALING_INTERNAL);
				bitmap_drv_request_pixels();
				break;
			case WL_LOCAL_COPY:
				if (wl_ddev && wl_ddm) {
					if (wl_copy_source)
						wl_data_source_destroy(
						    wl_copy_source);
					wl_copy_source =
					    wl_data_device_manager_create_data_source(
					    wl_ddm);
					wl_data_source_add_listener(
					    wl_copy_source,
					    &ds_listener, NULL);
					wl_data_source_offer(wl_copy_source,
					    "text/plain;charset=utf-8");
					wl_data_source_offer(wl_copy_source,
					    "text/plain");
					wl_data_device_set_selection(wl_ddev,
					    wl_copy_source,
					    last_input_serial);
				}
				break;
			case WL_LOCAL_PASTE:
				FREE_AND_NULL(wl_pastebuf);
				if (wl_copy_source) {
					/*
					 * We own the selection — copy from
					 * our own buffer to avoid deadlock
					 * (the compositor would send ds_send
					 * back to our blocked event thread).
					 */
					assert_pthread_mutex_lock(
					    &wl_copybuf_mutex);
					if (wl_copybuf)
						wl_pastebuf =
						    strdup(wl_copybuf);
					assert_pthread_mutex_unlock(
					    &wl_copybuf_mutex);
				}
				else if (wl_sel_offer
				    && wl_sel_offer_has_text) {
					int fds[2];
					if (pipe(fds) == 0) {
						char buf[4096];
						char *result = NULL;
						size_t total = 0;
						ssize_t n;

						wl_data_offer_receive(
						    wl_sel_offer,
						    "text/plain;charset=utf-8",
						    fds[1]);
						close(fds[1]);
						wl_display_flush(wl_dpy);
						while ((n = read(fds[0], buf,
						    sizeof(buf))) > 0) {
							char *tmp = realloc(
							    result,
							    total + n + 1);
							if (!tmp)
								break;
							result = tmp;
							memcpy(result + total,
							    buf, n);
							total += n;
						}
						close(fds[0]);
						if (result)
							result[total] = '\0';
						wl_pastebuf = result;
					}
				}
				sem_post(&wl_pastebuf_set);
				sem_wait(&wl_pastebuf_used);
				break;
			case WL_LOCAL_MOUSEPOINTER:
				current_mouse_ptr = lev.data.ptr;
				if (mouse_in_window)
					set_cursor_shape(last_pointer_serial);
				break;
			}
		}

		/* Key repeat */
		handle_key_repeat();
	}

	/* Cleanup */
	for (int i = 0; i < 2; i++)
		destroy_buffer(&buffers[i]);
	if (wl_deco)
		zxdg_toplevel_decoration_v1_destroy(wl_deco);
	if (wl_viewport)
		wp_viewport_destroy(wl_viewport);
	if (wl_opaque_region)
		wl_region_destroy(wl_opaque_region);
	if (xdg_top)
		xdg_toplevel_destroy(xdg_top);
	if (xdg_surf)
		xdg_surface_destroy(xdg_surf);
	if (wl_surf)
		wl_surface_destroy(wl_surf);
	if (wl_ptr)
		wl_pointer_destroy(wl_ptr);
	if (wl_kb)
		wl_keyboard_destroy(wl_kb);
	if (cursor_surf)
		wl_surface_destroy(cursor_surf);
	if (cursor_theme)
		wcur.theme_destroy(cursor_theme);
	if (wcur.handle)
		dlclose(wcur.handle);
	if (wl_copy_source)
		wl_data_source_destroy(wl_copy_source);
	if (wl_sel_offer)
		wl_data_offer_destroy(wl_sel_offer);
	if (wl_ddev)
		wl_data_device_destroy(wl_ddev);
	if (wl_icon_mgr)
		xdg_toplevel_icon_manager_v1_destroy(wl_icon_mgr);
	if (xdg_base)
		xdg_wm_base_destroy(xdg_base);
	xkb_cleanup();
	wl_display_disconnect(wl_dpy);
}

int
wl_available_opts(void)
{
	int opts = CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME;

	if (wl_icon_mgr)
		opts |= CONIO_OPT_SET_ICON;
	if (wl_viewporter)
		opts |= CONIO_OPT_EXTERNAL_SCALING;
	return opts;
}

bool
wl_has_clipboard(void)
{
	return wl_ddev != NULL;
}
