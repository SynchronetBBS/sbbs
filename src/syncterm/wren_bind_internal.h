#ifndef WREN_BIND_INTERNAL_H
#define WREN_BIND_INTERNAL_H

/* Shared across the wren_bind_*.c modules: the foreign-class type
 * tag enum, the universal struct header that carries it, and three
 * small inline helpers every module ends up using.  Each split
 * binding file (wren_bind_screen.c, wren_bind_sftp.c, …) includes
 * this; wren_bind.c itself does too. */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wren.h"

/* Discriminator for foreign objects in this binding.  Every wren_*
 * struct stamped at the front with one of these tags so foreign-method
 * bodies can identify *which* foreign they have — wrenGetSlotType
 * only tells us "WREN_TYPE_FOREIGN", not the class.  SWF_NONE = 0 so
 * an allocator that forgets to set the tag fails any specific check
 * loudly instead of pretending to be a different type.
 *
 * Adding a tag: also add the foreign struct in the matching module,
 * the allocate/finalize pair, the wren_bind_lookup_class entry in
 * wren_bind.c, and (if needed) a cached class handle in
 * struct wren_host_state. */
enum syncterm_wren_foreign {
	SWF_NONE = 0,
	SWF_KEY_EVENT,
	SWF_PHYSICAL_KEY_EVENT,
	SWF_MOUSE_EVENT,
	SWF_EXTATTR,
	SWF_LAST_COLUMN_FLAG,
	SWF_FLOWCONTROL,
	SWF_DIRECTORY,
	SWF_FILE,
	SWF_FILE_ERROR,
	SWF_WON_ERROR,
	SWF_CONN_ERROR,
	SWF_CELL,
	SWF_SURFACE,
	SWF_PIXEL_BUFFER,
	SWF_PIXEL_MASK,
	SWF_PIXEL_BLIT,
	SWF_CUSTOM_CURSOR,
	SWF_VIDEO_FLAGS,
	SWF_HOOK_HANDLE,
	SWF_CLAIM_HANDLE,
	SWF_SFTP_ENTRY,
	SWF_SFTP_STAT,
	SWF_SFTP_HANDLE,
	SWF_SFTP_ERROR,
	SWF_TIMER_ELAPSED,
	SWF_MENU_BBS,
	SWF_MENU_SETTINGS,
	SWF_MENU_FONT,
	SWF_MENU_THEME_DOCUMENT,
	SWF_PICKER_REQUEST,
};

/* Every foreign struct's first field has this exact name and type, so
 * a (struct wren_foreign_header *) cast yields the tag regardless of
 * which struct we hold. */
struct wren_foreign_header {
	enum syncterm_wren_foreign type;
};

/* Returns the tag for the foreign object in `slot`, or SWF_NONE if
 * the slot doesn't hold a foreign value. */
static inline enum syncterm_wren_foreign
slot_foreign_type(WrenVM *vm, int slot)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_FOREIGN)
		return SWF_NONE;
	struct wren_foreign_header *h = wrenGetSlotForeign(vm, slot);
	return h->type;
}

/* Drop a foreign class from the syncterm module into `slot`, suitable
 * to hand to wrenSetSlotNewForeign as the class of a new instance.
 * The class handle is captured into `*cache` on first use and reused
 * thereafter — saves the symbol-table lookup on every allocation
 * without forcing the caller to populate the cache up front.  Caller
 * must wrenEnsureSlots beforehand. */
static inline void
load_class_into_slot(WrenVM *vm, WrenHandle **cache, const char *name,
    int slot)
{
	if (*cache != NULL) {
		wrenSetSlotHandle(vm, slot, *cache);
		return;
	}
	wrenGetVariable(vm, "syncterm", name, slot);
	*cache = wrenGetSlotHandle(vm, slot);
}

/* Abort the calling fiber with `msg` as the error.  Convenience
 * wrapper around wrenSetSlotString + wrenAbortFiber. */
static inline void
wren_throw(WrenVM *vm, const char *msg)
{
	wrenEnsureSlots(vm, 1);
	wrenSetSlotString(vm, 0, msg);
	wrenAbortFiber(vm, 0);
}

/* Decode the first UTF-8 codepoint from `s` into `*cp_out`; returns
 * the byte length consumed, or 0 on truncated/invalid input.  Lives
 * in wren_bind_screen.c (the file that owns Cell.ch=) but is shared
 * by every binding that handles Wren-side UTF-8 strings. */
int decode_utf8_first(const char *s, int len, uint32_t *cp_out);

struct syncterm_theme;
void wren_push_theme_data(WrenVM *vm, const struct syncterm_theme *theme);

#endif /* WREN_BIND_INTERNAL_H */
