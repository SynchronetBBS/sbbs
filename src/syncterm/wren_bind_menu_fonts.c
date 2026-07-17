#include "wren_bind_menu_fonts.h"

#include "fonts.h"
#include "gen_defs.h"
#include "syncterm.h"
#include "wren_bind_fs.h"
#include "wren_bind_internal.h"

#include <ciolib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct menu_font_model {
	struct font_files *fonts;
	size_t count;
	uint64_t generation;
	bool loaded;
	bool dirty;
};

struct wren_menu_font {
	enum syncterm_wren_foreign type;
	size_t index;
	uint64_t generation;
};

static struct menu_font_model model;

static bool
reload_model(void)
{
	int count;
	bool success;
	struct font_files *fonts = read_font_files(&count, &success);
	if (!success)
		return false;
	free_font_files(model.fonts);
	model.fonts = fonts;
	model.count = (size_t)count;
	model.generation++;
	model.loaded = true;
	model.dirty = false;
	return true;
}

void
wren_menu_fonts_bind_init(void)
{
	memset(&model, 0, sizeof(model));
	model.generation = 1;
	reload_model();
}

void
wren_menu_fonts_bind_shutdown(void)
{
	free_font_files(model.fonts);
	memset(&model, 0, sizeof(model));
}

static void
font_allocate(WrenVM *vm)
{
	struct wren_menu_font *font = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*font));
	memset(font, 0, sizeof(*font));
	font->type = SWF_MENU_FONT;
}

static void
font_finalize(void *data)
{
	(void)data;
}

static struct font_files *
font_check(WrenVM *vm)
{
	if (slot_foreign_type(vm, 0) != SWF_MENU_FONT) {
		wren_throw(vm, "MenuFont: invalid receiver");
		return NULL;
	}
	struct wren_menu_font *font = wrenGetSlotForeign(vm, 0);
	if (font->generation != model.generation || font->index >= model.count) {
		wren_throw(vm,
		    "MenuFont: stale handle; reacquire it from Menu.fonts");
		return NULL;
	}
	return &model.fonts[font->index];
}

static bool
slot_index(WrenVM *vm, int slot, size_t maximum, size_t *result)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_NUM) {
		wren_throw(vm, "expected an integer Num");
		return false;
	}
	double value = wrenGetSlotDouble(vm, slot);
	if (!isfinite(value) || trunc(value) != value || value < 0 ||
	    value > maximum) {
		wren_throw(vm, "integer is outside the allowed range");
		return false;
	}
	*result = (size_t)value;
	return true;
}

static bool
slot_font_name(WrenVM *vm, int slot, char name[51])
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "font name must be a String");
		return false;
	}
	int length;
	const char *value = wrenGetSlotBytes(vm, slot, &length);
	if (length < 1 || length > 50 ||
	    memchr(value, 0, (size_t)length) != NULL) {
		wren_throw(vm, "font name must contain 1 through 50 bytes");
		return false;
	}
	for (int i = 0; i < length; i++) {
		unsigned char ch = (unsigned char)value[i];
		if (ch < 0x20 || ch == '[' || ch == ']') {
			wren_throw(vm, "font name contains an invalid character");
			return false;
		}
	}
	memcpy(name, value, (size_t)length);
	name[length] = 0;
	return true;
}

static bool
name_available(const char *name, size_t except)
{
	for (size_t i = 0; i < model.count; i++) {
		if (i != except && stricmp(model.fonts[i].name, name) == 0)
			return false;
	}
	return true;
}

static void
push_font(WrenVM *vm, int slot, int class_slot, size_t index)
{
	struct wren_menu_font *font = wrenSetSlotNewForeign(vm, slot,
	    class_slot, sizeof(*font));
	font->type = SWF_MENU_FONT;
	font->index = index;
	font->generation = model.generation;
}

static void
fn_Menu_fonts(WrenVM *vm)
{
	if (!model.loaded && !reload_model()) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	wrenGetVariable(vm, "syncterm_menu", "MenuFont", 1);
	for (size_t i = 0; i < model.count; i++) {
		push_font(vm, 2, 1, i);
		wrenInsertInList(vm, 0, -1, 2);
	}
}

static void
fn_Menu_fontsDirty(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, model.dirty);
}

static void
fn_Menu_reloadFonts(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, reload_model());
}

static void
fn_Menu_createFont(WrenVM *vm)
{
	if (safe_mode || (!model.loaded && !reload_model()) ||
	    model.count >= 256 - CONIO_FIRST_FREE_FONT) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char name[51];
	size_t index;
	if (!slot_font_name(vm, 1, name) ||
	    !slot_index(vm, 2, model.count, &index))
		return;
	if (!name_available(name, SIZE_MAX)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char *stored_name = strdup(name);
	if (stored_name == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct font_files *fonts = realloc(model.fonts,
	    (model.count + 2) * sizeof(*fonts));
	if (fonts == NULL) {
		free(stored_name);
		wrenSetSlotNull(vm, 0);
		return;
	}
	model.fonts = fonts;
	memmove(&fonts[index + 1], &fonts[index],
	    (model.count - index + 1) * sizeof(*fonts));
	memset(&fonts[index], 0, sizeof(fonts[index]));
	fonts[index].name = stored_name;
	model.count++;
	model.generation++;
	model.dirty = true;
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm_menu", "MenuFont", 1);
	push_font(vm, 0, 1, index);
}

static void
fn_Menu_saveFonts(WrenVM *vm)
{
	if (!model.loaded || !save_font_files(model.fonts)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	load_font_files();
	model.dirty = false;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_MenuFont_name(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	if (font != NULL)
		wrenSetSlotString(vm, 0, font->name);
}

static void
fn_MenuFont_name_set(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	char name[51];
	if (font == NULL || !slot_font_name(vm, 1, name))
		return;
	struct wren_menu_font *handle = wrenGetSlotForeign(vm, 0);
	if (!name_available(name, handle->index)) {
		wren_throw(vm, "MenuFont.name: duplicate font name");
		return;
	}
	char *replacement = strdup(name);
	if (replacement == NULL) {
		wren_throw(vm, "MenuFont.name: out of memory");
		return;
	}
	free(font->name);
	font->name = replacement;
	model.dirty = true;
}

static char **
font_path(struct font_files *font, size_t slot)
{
	switch (slot) {
		case 0: return &font->path8x8;
		case 1: return &font->path8x14;
		case 2: return &font->path8x16;
		case 3: return &font->path12x20;
	}
	return NULL;
}

static off_t
font_size(size_t slot)
{
	static const off_t sizes[] = {2048, 3584, 4096, 10240};
	return sizes[slot];
}

static void
fn_MenuFont_path(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	size_t slot;
	if (font == NULL || !slot_index(vm, 1, 3, &slot))
		return;
	char *path = *font_path(font, slot);
	if (path == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, path);
}

static void
fn_MenuFont_setFile(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	size_t slot;
	if (font == NULL)
		return;
	if (safe_mode) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (!slot_index(vm, 1, 3, &slot))
		return;
	const char *path = wren_file_read_path(vm, 2);
	struct stat st;
	if (path == NULL || stat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
	    st.st_size != font_size(slot)) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char *replacement = strdup(path);
	if (replacement == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	char **stored = font_path(font, slot);
	free(*stored);
	*stored = replacement;
	model.dirty = true;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_MenuFont_clearFile(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	size_t slot;
	if (font == NULL)
		return;
	if (safe_mode) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	if (!slot_index(vm, 1, 3, &slot))
		return;
	char **stored = font_path(font, slot);
	free(*stored);
	*stored = NULL;
	model.dirty = true;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_MenuFont_delete(WrenVM *vm)
{
	struct font_files *font = font_check(vm);
	if (font == NULL)
		return;
	if (safe_mode) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	struct wren_menu_font *handle = wrenGetSlotForeign(vm, 0);
	free(font->name);
	free(font->path8x8);
	free(font->path8x14);
	free(font->path8x16);
	free(font->path12x20);
	memmove(&model.fonts[handle->index],
	    &model.fonts[handle->index + 1],
	    (model.count - handle->index) * sizeof(*model.fonts));
	model.count--;
	model.generation++;
	model.dirty = true;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_MenuFont_toString(WrenVM *vm)
{
	fn_MenuFont_name(vm);
}

struct binding {
	const char *class_name;
	bool is_static;
	const char *signature;
	WrenForeignMethodFn function;
};

static const struct binding bindings[] = {
	{ "Menu", true, "fonts", fn_Menu_fonts },
	{ "Menu", true, "fontsDirty", fn_Menu_fontsDirty },
	{ "Menu", true, "reloadFonts()", fn_Menu_reloadFonts },
	{ "Menu", true, "createFont(_,_)", fn_Menu_createFont },
	{ "Menu", true, "saveFonts()", fn_Menu_saveFonts },
	{ "MenuFont", false, "name", fn_MenuFont_name },
	{ "MenuFont", false, "name=(_)", fn_MenuFont_name_set },
	{ "MenuFont", false, "path(_)", fn_MenuFont_path },
	{ "MenuFont", false, "setFile(_,_)", fn_MenuFont_setFile },
	{ "MenuFont", false, "clearFile(_)", fn_MenuFont_clearFile },
	{ "MenuFont", false, "delete()", fn_MenuFont_delete },
	{ "MenuFont", false, "toString", fn_MenuFont_toString },
};

WrenForeignMethodFn
wren_menu_fonts_bind_lookup(const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm_menu") != 0)
		return NULL;
	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		if (bindings[i].is_static == is_static &&
		    strcmp(bindings[i].class_name, class_name) == 0 &&
		    strcmp(bindings[i].signature, signature) == 0)
			return bindings[i].function;
	}
	return NULL;
}

WrenForeignClassMethods
wren_menu_fonts_bind_lookup_class(const char *module,
    const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module != NULL && strcmp(module, "syncterm_menu") == 0 &&
	    strcmp(class_name, "MenuFont") == 0) {
		WrenForeignClassMethods methods = {
			font_allocate, font_finalize
		};
		return methods;
	}
	return none;
}
