#include "wren_bind_menu_settings.h"

#include "bbslist.h"
#include "ini_file.h"
#include "ini_crypt.h"
#include "menu_settings.h"
#include "named_str_list.h"
#include "syncterm.h"
#include "theme.h"
#include "theme_cloud.h"
#include "webget.h"
#include "wren_bind_internal.h"

#include <ciolib.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__unix__) && defined(SOUNDCARD_H_IN) && \
    (SOUNDCARD_H_IN > 0) && !defined(_WIN32)
	#if SOUNDCARD_H_IN == 1
		#include <sys/soundcard.h>
	#elif SOUNDCARD_H_IN == 2
		#include <soundcard.h>
	#elif SOUNDCARD_H_IN == 3
		#include <linux/soundcard.h>
	#endif
#endif

struct wren_menu_settings {
	enum syncterm_wren_foreign type;
	struct syncterm_settings settings;
	uint64_t generation;
	bool dirty;
};

struct wren_menu_theme_document {
	enum syncterm_wren_foreign      type;
	struct syncterm_theme_document *document;
};

static uint64_t settings_generation;

void
wren_menu_settings_bind_init(void)
{
	settings_generation = 1;
}

static void
settings_allocate(WrenVM *vm)
{
	struct wren_menu_settings *ws = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*ws));
	memset(ws, 0, sizeof(*ws));
	ws->type = SWF_MENU_SETTINGS;
}

static void
settings_finalize(void *data)
{
	(void)data;
}

static void
theme_document_allocate(WrenVM *vm)
{
	struct wren_menu_theme_document *document =
	    wrenSetSlotNewForeign(vm, 0, 0, sizeof(*document));
	memset(document, 0, sizeof(*document));
	document->type = SWF_MENU_THEME_DOCUMENT;
}

static void
theme_document_finalize(void *data)
{
	struct wren_menu_theme_document *document = data;
	syncterm_theme_document_free(document->document);
}

static struct wren_menu_theme_document *
theme_document_check(WrenVM *vm)
{
	if (slot_foreign_type(vm, 0) != SWF_MENU_THEME_DOCUMENT) {
		wren_throw(vm, "ThemeDocument: invalid receiver");
		return NULL;
	}
	struct wren_menu_theme_document *document = wrenGetSlotForeign(vm, 0);
	if (document->document == NULL) {
		wren_throw(vm, "ThemeDocument: uninitialized document");
		return NULL;
	}
	return document;
}

static struct wren_menu_settings *
settings_check(WrenVM *vm)
{
	if (slot_foreign_type(vm, 0) != SWF_MENU_SETTINGS) {
		wren_throw(vm, "Settings: invalid receiver");
		return NULL;
	}
	struct wren_menu_settings *ws = wrenGetSlotForeign(vm, 0);
	if (ws->generation != settings_generation) {
		wren_throw(vm,
		    "Settings: stale snapshot; reacquire it from Menu.settings");
		return NULL;
	}
	return ws;
}

static bool
slot_string(WrenVM *vm, int slot, char *dest, size_t size)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "Settings: expected a String");
		return false;
	}
	int length;
	const char *value = wrenGetSlotBytes(vm, slot, &length);
	if (length < 0 || (size_t)length >= size ||
	    memchr(value, 0, (size_t)length) != NULL) {
		wren_throw(vm, "Settings: String is too long or contains NUL");
		return false;
	}
	memcpy(dest, value, (size_t)length);
	dest[length] = 0;
	return true;
}

static bool
slot_integer(WrenVM *vm, int slot, uint64_t minimum, uint64_t maximum,
    uint64_t *result)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_NUM) {
		wren_throw(vm, "Settings: expected an integer Num");
		return false;
	}
	double value = wrenGetSlotDouble(vm, slot);
	if (!isfinite(value) || trunc(value) != value || value < minimum ||
	    value > maximum) {
		wren_throw(vm, "Settings: integer is outside the allowed range");
		return false;
	}
	*result = (uint64_t)value;
	return true;
}

static bool
slot_bool(WrenVM *vm, int slot, bool *result)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_BOOL) {
		wren_throw(vm, "Settings: expected a Bool");
		return false;
	}
	*result = wrenGetSlotBool(vm, slot);
	return true;
}

#define SETTINGS_STRING_PROPERTY(name, field)                          \
	static void fn_Settings_##name(WrenVM *vm)                        \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		if (ws != NULL)                                               \
			wrenSetSlotString(vm, 0, ws->settings.field);             \
	}                                                                 \
	static void fn_Settings_##name##_set(WrenVM *vm)                  \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		if (ws != NULL && slot_string(vm, 1, ws->settings.field,      \
		    sizeof(ws->settings.field)))                              \
			ws->dirty = true;                                        \
	}

#define SETTINGS_BOOL_PROPERTY(name, field)                            \
	static void fn_Settings_##name(WrenVM *vm)                        \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		if (ws != NULL)                                               \
			wrenSetSlotBool(vm, 0, ws->settings.field);               \
	}                                                                 \
	static void fn_Settings_##name##_set(WrenVM *vm)                  \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		bool value;                                                    \
		if (ws != NULL && slot_bool(vm, 1, &value)) {                 \
			ws->settings.field = value;                              \
			ws->dirty = true;                                        \
		}                                                             \
	}

#define SETTINGS_NUM_PROPERTY(name, field, minimum, maximum)           \
	static void fn_Settings_##name(WrenVM *vm)                        \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		if (ws != NULL)                                               \
			wrenSetSlotDouble(vm, 0, (double)ws->settings.field);     \
	}                                                                 \
	static void fn_Settings_##name##_set(WrenVM *vm)                  \
	{                                                                 \
		struct wren_menu_settings *ws = settings_check(vm);          \
		uint64_t value;                                                \
		if (ws != NULL && slot_integer(vm, 1, minimum, maximum,       \
		    &value)) {                                                  \
			ws->settings.field = value;                              \
			ws->dirty = true;                                        \
		}                                                             \
	}

SETTINGS_BOOL_PROPERTY(confirmClose, confirm_close)
SETTINGS_BOOL_PROPERTY(promptSave, prompt_save)
SETTINGS_BOOL_PROPERTY(invertWheel, invert_wheel)
SETTINGS_STRING_PROPERTY(modemDevice, mdm.device_name)
SETTINGS_STRING_PROPERTY(modemInit, mdm.init_string)
SETTINGS_STRING_PROPERTY(modemDial, mdm.dial_string)
SETTINGS_STRING_PROPERTY(listPath, stored_list_path)
SETTINGS_STRING_PROPERTY(shellTerm, TERM)
SETTINGS_NUM_PROPERTY(startupMode, startup_mode, SCREEN_MODE_CURRENT,
    SCREEN_MODE_TERMINATOR - 1)
SETTINGS_NUM_PROPERTY(cursorStyle, defaultCursor, ST_CT_DEFAULT,
    ST_CT_SOLID_BLK)
SETTINGS_NUM_PROPERTY(scrollbackLines, backlines, 1, INT_MAX)
SETTINGS_NUM_PROPERTY(modemRate, mdm.com_rate, 0, UINT32_MAX)
SETTINGS_NUM_PROPERTY(customRows, custom_rows, 14, 255)
SETTINGS_NUM_PROPERTY(customColumns, custom_cols, 40, 255)
SETTINGS_NUM_PROPERTY(customAspectWidth, custom_aw, 1, INT_MAX)
SETTINGS_NUM_PROPERTY(customAspectHeight, custom_ah, 1, INT_MAX)
SETTINGS_NUM_PROPERTY(frameColor, theme_frame_color, 0, 16)
SETTINGS_NUM_PROPERTY(textColor, theme_text_color, 0, 16)
SETTINGS_NUM_PROPERTY(backgroundColor, theme_background_color, 0, 8)
SETTINGS_NUM_PROPERTY(inverseColor, theme_inverse_color, 0, 8)
SETTINGS_NUM_PROPERTY(lightbarColor, theme_lightbar_color, 0, 16)
SETTINGS_NUM_PROPERTY(lightbarBackgroundColor,
    theme_lightbar_background_color, 0, 8)

static bool
output_mode_available(uint64_t value)
{
	for (size_t i = 0; output_types[i] != NULL; i++) {
		if ((uint64_t)output_map[i] == value)
			return true;
	}
	return false;
}

static void
fn_Settings_outputMode(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws != NULL)
		wrenSetSlotDouble(vm, 0, ws->settings.output_mode);
}

static void
fn_Settings_outputMode_set(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	uint64_t value;
	if (ws == NULL || !slot_integer(vm, 1, 0, INT_MAX, &value))
		return;
	if (!output_mode_available(value)) {
		wren_throw(vm, "Settings.outputMode: unavailable in this build");
		return;
	}
	ws->settings.output_mode = (int)value;
	ws->dirty = true;
}

static void
fn_Settings_audioModes(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws != NULL)
		wrenSetSlotDouble(vm, 0, ws->settings.audio_output_modes);
}

static void
fn_Settings_audioModes_set(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	uint64_t value;
	if (ws == NULL || !slot_integer(vm, 1, 0, UINT32_MAX, &value))
		return;
	unsigned known = 0;
	for (size_t i = 0; audio_output_bits[i].name != NULL; i++)
		known |= audio_output_bits[i].bit;
	if (((unsigned)value & ~known) != 0) {
		wren_throw(vm, "Settings.audioModes: unknown backend bit");
		return;
	}
	ws->settings.audio_output_modes = (unsigned)value;
	ws->dirty = true;
}

static void
fn_Settings_customFontHeight(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws != NULL)
		wrenSetSlotDouble(vm, 0, ws->settings.custom_fontheight);
}

static void
fn_Settings_customFontHeight_set(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	uint64_t value;
	if (ws == NULL || !slot_integer(vm, 1, 8, 16, &value))
		return;
	if (value != 8 && value != 14 && value != 16) {
		wren_throw(vm,
		    "Settings.customFontHeight: expected 8, 14, or 16");
		return;
	}
	ws->settings.custom_fontheight = (int)value;
	ws->dirty = true;
}

static void
fn_Settings_scalingMode(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws != NULL)
		wrenSetSlotDouble(vm, 0,
		    menu_settings_scaling_mode(&ws->settings));
}

static void
fn_Settings_scalingMode_set(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	uint64_t value;
	if (ws != NULL && slot_integer(vm, 1, 0, 2, &value)) {
		menu_settings_set_scaling_mode(&ws->settings, (int)value);
		ws->dirty = true;
	}
}

static void
fn_Settings_kdfShift(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws == NULL)
		return;
	int shift = 15;
	if (strncmp(ws->settings.keyDerivationIterations, "scrypt-N", 8) == 0)
		shift = atoi(ws->settings.keyDerivationIterations + 8);
	wrenSetSlotDouble(vm, 0, shift);
}

static void
fn_Settings_kdfShift_set(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	uint64_t value;
	if (ws != NULL && slot_integer(vm, 1, 8, 24, &value)) {
		snprintf(ws->settings.keyDerivationIterations,
		    sizeof(ws->settings.keyDerivationIterations), "scrypt-N%u",
		    (unsigned)value);
		ws->dirty = true;
	}
}

static void
fn_Settings_dirty(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws != NULL)
		wrenSetSlotBool(vm, 0, ws->dirty);
}

static void
fn_Settings_apply(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws == NULL)
		return;
	bool success = menu_settings_apply(&ws->settings);
	if (success) {
		settings_generation++;
		ws->generation = settings_generation;
		ws->dirty = false;
	}
	wrenSetSlotBool(vm, 0, success);
}

static void
fn_Settings_save(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws == NULL)
		return;
	bool success = menu_settings_save(&ws->settings);
	if (success) {
		settings_generation++;
		ws->generation = settings_generation;
		ws->dirty = false;
	}
	wrenSetSlotBool(vm, 0, success);
}

static void
fn_Settings_reload(WrenVM *vm)
{
	struct wren_menu_settings *ws = settings_check(vm);
	if (ws == NULL)
		return;
	menu_settings_snapshot(&ws->settings);
	ws->dirty = false;
	wrenSetSlotNull(vm, 0);
}

static void
push_settings(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm_menu", "Settings", 1);
	struct wren_menu_settings *ws = wrenSetSlotNewForeign(vm, 0, 1,
	    sizeof(*ws));
	memset(ws, 0, sizeof(*ws));
	ws->type = SWF_MENU_SETTINGS;
	ws->generation = settings_generation;
	menu_settings_snapshot(&ws->settings);
}

static void
fn_Menu_settings(WrenVM *vm)
{
	push_settings(vm);
}

static void
push_string_list(WrenVM *vm, char *const *values)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; values[i] != NULL; i++) {
		wrenSetSlotString(vm, 1, values[i]);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
push_pairs(WrenVM *vm, const int *values, char *const *names)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; names[i] != NULL; i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, values[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, names[i]);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void fn_Menu_screenModes(WrenVM *vm) { push_string_list(vm, screen_modes); }
static void fn_Menu_cursorStyles(WrenVM *vm) { push_string_list(vm, cursor_descrs); }
static void fn_Menu_scalingModes(WrenVM *vm) { push_string_list(vm, scaling_names); }
static void fn_Menu_colors(WrenVM *vm) { push_string_list(vm, (char *const *)colour_names); }
static void fn_Menu_backgroundColors(WrenVM *vm) { push_string_list(vm, (char *const *)bg_colour_names); }

static void
fn_Menu_outputModes(WrenVM *vm)
{
	push_pairs(vm, output_map, output_types);
}

static void
fn_Menu_audioModes(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; audio_output_types[i].name != NULL; i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, audio_output_types[i].bit);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, audio_output_types[i].name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

struct build_option {
	const char *category;
	const char *name;
	bool enabled;
};

static const struct build_option build_options[] = {
#ifdef WITHOUT_DEUCESSH
	{ "Crypto", "DeuceSSH (SSH)", false },
#else
	{ "Crypto", "DeuceSSH (SSH)", true },
#endif
#if defined(XP_CRYPTO_BACKEND_OPENSSL)
	{ "Crypto", "OpenSSL (TLS, symmetric ciphers)", true },
#else
	{ "Crypto", "OpenSSL (TLS, symmetric ciphers)", false },
#endif
#if defined(XP_CRYPTO_BACKEND_BOTAN)
	{ "Crypto", "Botan 3 (TLS, symmetric ciphers)", true },
#else
	{ "Crypto", "Botan 3 (TLS, symmetric ciphers)", false },
#endif
#ifdef WITHOUT_OOII
	{ "Terminal", "Operation Overkill ][ Terminal", false },
#else
	{ "Terminal", "Operation Overkill ][ Terminal", true },
#endif
#ifdef WITH_JPEG_XL
	{ "Terminal", "JPEG XL support", true },
#else
	{ "Terminal", "JPEG XL support", false },
#endif
#ifdef WITH_SDL
	{ "Video", "SDL", true },
#else
	{ "Video", "SDL", false },
#endif
#ifdef DISABLE_X11
	{ "Video", "X11 Support", false },
#else
	{ "Video", "X11 Support", true },
#endif
#ifdef WITH_XINERAMA
	{ "Video", "Xinerama", true },
#else
	{ "Video", "Xinerama", false },
#endif
#ifdef WITH_XRANDR
	{ "Video", "XRandR", true },
#else
	{ "Video", "XRandR", false },
#endif
#ifdef WITH_XRENDER
	{ "Video", "XRender", true },
#else
	{ "Video", "XRender", false },
#endif
#ifdef WITH_GDI
	{ "Video", "GDI", true },
#else
	{ "Video", "GDI", false },
#endif
#ifdef WITH_WAYLAND
	{ "Video", "Wayland", true },
#else
	{ "Video", "Wayland", false },
#endif
#ifdef WITH_QUARTZ
	{ "Video", "Quartz", true },
#else
	{ "Video", "Quartz", false },
#endif
#if (defined SOUNDCARD_H_IN) && (SOUNDCARD_H_IN > 0) && defined(AFMT_U8)
	{ "Audio", "OSS", true },
#else
	{ "Audio", "OSS", false },
#endif
#ifdef WITH_SDL_AUDIO
	{ "Audio", "SDL", true },
#else
	{ "Audio", "SDL", false },
#endif
#ifdef USE_ALSA_SOUND
	{ "Audio", "ALSA", true },
#else
	{ "Audio", "ALSA", false },
#endif
#ifdef _WIN32
	{ "Audio", "WASAPI", true },
#else
	{ "Audio", "WASAPI", false },
#endif
#ifdef WITH_PORTAUDIO
	{ "Audio", "PortAudio", true },
#else
	{ "Audio", "PortAudio", false },
#endif
#ifdef WITH_PIPEWIRE
	{ "Audio", "PipeWire", true },
#else
	{ "Audio", "PipeWire", false },
#endif
#ifdef WITH_PULSEAUDIO
	{ "Audio", "PulseAudio", true },
#else
	{ "Audio", "PulseAudio", false },
#endif
#ifdef WITH_COREAUDIO
	{ "Audio", "Core Audio", true },
#else
	{ "Audio", "Core Audio", false },
#endif
#if defined(WITH_SNDFILE) || defined(STATIC_SNDFILE)
	{ "Audio", "libsndfile", true },
#else
	{ "Audio", "libsndfile", false },
#endif
};

static void
fn_Menu_buildOptions(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0;
	    i < sizeof(build_options) / sizeof(build_options[0]); i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotString(vm, 2, build_options[i].category);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, build_options[i].name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotBool(vm, 2, build_options[i].enabled);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_maxPathLength(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, MAX_PATH);
}

static void
fn_Menu_encryptionAvailable(WrenVM *vm)
{
#ifdef WITHOUT_CRYPTO
	wrenSetSlotBool(vm, 0, false);
#else
	wrenSetSlotBool(vm, 0, true);
#endif
}

static void
fn_Menu_currentScreenMode(WrenVM *vm)
{
	struct text_info ti;
	gettextinfo(&ti);
	wrenSetSlotDouble(vm, 0, ciolib_to_screen(ti.currmode));
}

static void
fn_Menu_setScreenMode(WrenVM *vm)
{
	uint64_t mode;
	if (!slot_integer(vm, 1, SCREEN_MODE_80X25,
	    SCREEN_MODE_TERMINATOR - 1, &mode))
		return;
	textmode(screen_to_ciolib((int)mode));
	set_default_cursor();
	wrenSetSlotBool(vm, 0, true);
}

static void
map_string(WrenVM *vm, int map_slot, int key_slot, int value_slot,
    const char *key, const char *value)
{
	wrenSetSlotString(vm, key_slot, key);
	wrenSetSlotString(vm, value_slot, value != NULL ? value : "");
	wrenSetMapValue(vm, map_slot, key_slot, value_slot);
}

static void
fn_Menu_fileLocations(WrenVM *vm)
{
	char global_list[MAX_PATH + 1] = "";
	char ini[MAX_PATH + 1] = "";
	char download[MAX_PATH + 1] = "";
	char cache[MAX_PATH + 1] = "";
	char keys[MAX_PATH + 1] = "";
	char scripts[MAX_PATH + 1] = "";
	char themes[MAX_PATH + 1] = "";
	get_syncterm_filename(global_list, sizeof(global_list),
	    SYNCTERM_PATH_LIST, true);
	get_syncterm_filename(ini, sizeof(ini), SYNCTERM_PATH_INI, false);
	get_syncterm_filename(download, sizeof(download),
	    SYNCTERM_DEFAULT_TRANSFER_PATH, false);
	get_syncterm_filename(cache, sizeof(cache), SYNCTERM_PATH_CACHE, false);
	get_syncterm_filename(keys, sizeof(keys), SYNCTERM_PATH_KEYS, false);
	get_syncterm_filename(scripts, sizeof(scripts), SYNCTERM_PATH_SCRIPTS,
	    false);
	get_syncterm_filename(themes, sizeof(themes), SYNCTERM_PATH_THEMES,
	    false);
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewMap(vm, 0);
	map_string(vm, 0, 1, 2, "globalList", global_list);
	map_string(vm, 0, 1, 2, "personalList", settings.list_path);
	map_string(vm, 0, 1, 2, "configuration", ini);
	map_string(vm, 0, 1, 2, "download", download);
	map_string(vm, 0, 1, 2, "cache", cache);
	map_string(vm, 0, 1, 2, "keys", keys);
	map_string(vm, 0, 1, 2, "scripts", scripts);
	map_string(vm, 0, 1, 2, "themes", themes);
}

static void
insert_nullable_string(WrenVM *vm, int list_slot, int value_slot,
    const char *value)
{
	if (value == NULL)
		wrenSetSlotNull(vm, value_slot);
	else
		wrenSetSlotString(vm, value_slot, value);
	wrenInsertInList(vm, list_slot, -1, value_slot);
}

static void
fn_Menu_themes(WrenVM *vm)
{
	if (!syncterm_theme_catalog_refresh(settings.theme_file)) {
		wren_throw(vm, "Menu.themes: unable to enumerate themes");
		return;
	}
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; i < syncterm_theme_catalog_count(); i++) {
		const struct syncterm_theme_catalog_entry *entry =
		    syncterm_theme_catalog_entry(i);
		wrenSetSlotNewList(vm, 1);
		insert_nullable_string(vm, 1, 2, entry->filename);
		insert_nullable_string(vm, 1, 2, entry->name);
		insert_nullable_string(vm, 1, 2, entry->author);
		insert_nullable_string(vm, 1, 2, entry->description);
		insert_nullable_string(vm, 1, 2, entry->version);
		insert_nullable_string(vm, 1, 2, entry->error);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_selectedThemeFile(WrenVM *vm)
{
	wrenSetSlotString(vm, 0, settings.theme_file);
}

static void
fn_Menu_selectedThemePackage(WrenVM *vm)
{
	wrenSetSlotString(vm, 0, settings.theme_package);
}

static bool
theme_filename_slot(WrenVM *vm, int slot, char *filename, size_t size)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "theme filename must be a String");
		return false;
	}
	int length;
	const char *value = wrenGetSlotBytes(vm, slot, &length);
	if (length < 0 || (size_t)length >= size ||
	    memchr(value, 0, (size_t)length) != NULL) {
		wren_throw(vm, "theme filename is too long or contains NUL");
		return false;
	}
	memcpy(filename, value, (size_t)length);
	filename[length] = '\0';
	return true;
}

static void
fn_Menu_previewTheme(WrenVM *vm)
{
	char filename[MAX_PATH + 1];
	if (!theme_filename_slot(vm, 1, filename, sizeof(filename)))
		return;
	const char *error = syncterm_theme_preview(filename);
	if (error == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, error);
}

static void
fn_Menu_cancelThemePreview(WrenVM *vm)
{
	syncterm_theme_cancel_preview();
	wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_selectTheme(WrenVM *vm)
{
	char filename[MAX_PATH + 1];
	if (!theme_filename_slot(vm, 1, filename, sizeof(filename)))
		return;
	char error[256];
	if (menu_settings_select_theme(filename, error, sizeof(error))) {
		settings_generation++;
		wrenSetSlotNull(vm, 0);
	}
	else
		wrenSetSlotString(vm, 0, error);
}

static void
fn_Menu_deleteTheme(WrenVM *vm)
{
	char filename[MAX_PATH + 1];
	if (!theme_filename_slot(vm, 1, filename, sizeof(filename)))
		return;
	bool selected = stricmp(settings.theme_file, filename) == 0;
	char error[256];
	if (menu_settings_delete_theme(filename, error, sizeof(error))) {
		if (selected)
			settings_generation++;
		wrenSetSlotNull(vm, 0);
	}
	else
		wrenSetSlotString(vm, 0, error);
}

static void
fn_Menu_refreshCloudThemes(WrenVM *vm)
{
	char error[256] = "";
	bool loaded = syncterm_cloud_themes_refresh(true,
	    settings.theme_package, error, sizeof(error));
	if (loaded && settings.theme_package[0] != '\0') {
		struct syncterm_theme *theme = NULL;
		char reload_error[256];
		if (syncterm_theme_prepare(&settings, true, &theme, reload_error,
		    sizeof(reload_error)) && theme != NULL) {
			syncterm_theme_install(theme);
			settings_generation++;
		}
	}
	if (!loaded || error[0] != '\0')
		wrenSetSlotString(vm, 0, error[0] != '\0' ? error :
		    "unable to load the cloud theme catalog");
	else
		wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_cloudThemes(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; i < syncterm_cloud_theme_count(); i++) {
		const struct syncterm_cloud_theme_entry *entry =
		    syncterm_cloud_theme_entry(i);
		wrenSetSlotNewList(vm, 1);
		insert_nullable_string(vm, 1, 2, entry->package);
		insert_nullable_string(vm, 1, 2, entry->name);
		insert_nullable_string(vm, 1, 2, entry->author);
		insert_nullable_string(vm, 1, 2, entry->description);
		insert_nullable_string(vm, 1, 2, entry->version);
		wrenSetSlotBool(vm, 2, entry->cached);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotBool(vm, 2, entry->update_available);
		wrenInsertInList(vm, 1, -1, 2);
		insert_nullable_string(vm, 1, 2, entry->error);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static bool
theme_package_slot(WrenVM *vm, int slot, char *package, size_t size)
{
	if (!theme_filename_slot(vm, slot, package, size))
		return false;
	if (!syncterm_theme_valid_package(package)) {
		wren_throw(vm, "invalid theme package");
		return false;
	}
	return true;
}

static void
fn_Menu_previewCloudTheme(WrenVM *vm)
{
	char package[MAX_PATH + 1];
	if (!theme_package_slot(vm, 1, package, sizeof(package)))
		return;
	const char *error = syncterm_cloud_theme_preview(package);
	if (error == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, error);
}

static void
fn_Menu_selectCloudTheme(WrenVM *vm)
{
	char package[MAX_PATH + 1];
	if (!theme_package_slot(vm, 1, package, sizeof(package)))
		return;
	char error[256];
	if (!syncterm_cloud_theme_cache(package, error, sizeof(error)) ||
	    !menu_settings_select_theme_package(package, error, sizeof(error))) {
		wrenSetSlotString(vm, 0, error);
		return;
	}
	settings_generation++;
	wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_deleteCloudTheme(WrenVM *vm)
{
	char package[MAX_PATH + 1];
	if (!theme_package_slot(vm, 1, package, sizeof(package)))
		return;
	bool selected = strcmp(settings.theme_package, package) == 0;
	char error[256];
	if (menu_settings_delete_theme_package(package, error, sizeof(error))) {
		if (selected)
			settings_generation++;
		wrenSetSlotNull(vm, 0);
	}
	else
		wrenSetSlotString(vm, 0, error);
}

static void
fn_Menu_cacheCloudTheme(WrenVM *vm)
{
	char package[MAX_PATH + 1];
	if (!theme_package_slot(vm, 1, package, sizeof(package)))
		return;
	char error[256];
	if (syncterm_cloud_theme_cache(package, error, sizeof(error)))
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, error);
}

static void
push_theme_document(WrenVM *vm, struct syncterm_theme_document *source)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "syncterm_menu", "ThemeDocument", 1);
	struct wren_menu_theme_document *document =
	    wrenSetSlotNewForeign(vm, 0, 1, sizeof(*document));
	memset(document, 0, sizeof(*document));
	document->type = SWF_MENU_THEME_DOCUMENT;
	document->document = source;
}

static void
fn_Menu_newThemeDocument(WrenVM *vm)
{
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_theme_document_new(error, sizeof(error));
	if (document == NULL) {
		wren_throw(vm, error[0] != '\0' ? error :
		    "unable to create theme document");
		return;
	}
	push_theme_document(vm, document);
}

static void
fn_Menu_openThemeDocument(WrenVM *vm)
{
	char filename[MAX_PATH + 1];
	if (!theme_filename_slot(vm, 1, filename, sizeof(filename)))
		return;
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_theme_document_open(filename, error, sizeof(error));
	if (document == NULL) {
		wren_throw(vm, error[0] != '\0' ? error :
		    "unable to open theme document");
		return;
	}
	push_theme_document(vm, document);
}

static void
fn_Menu_copyCloudTheme(WrenVM *vm)
{
	char package[MAX_PATH + 1];
	if (!theme_package_slot(vm, 1, package, sizeof(package)))
		return;
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_cloud_theme_copy(package, error, sizeof(error));
	if (document == NULL) {
		wren_throw(vm, error[0] != '\0' ? error :
		    "unable to copy cloud theme");
		return;
	}
	push_theme_document(vm, document);
}

static bool
theme_document_string_slot(WrenVM *vm, int slot, char *value, size_t size)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "ThemeDocument: expected a String");
		return false;
	}
	int length;
	const char *source = wrenGetSlotBytes(vm, slot, &length);
	if (length < 0 || (size_t)length >= size ||
	    memchr(source, 0, (size_t)length) != NULL) {
		wren_throw(vm, "ThemeDocument: String is too long or contains NUL");
		return false;
	}
	memcpy(value, source, (size_t)length);
	value[length] = '\0';
	return true;
}

static void
theme_document_result(WrenVM *vm, bool success, const char *error)
{
	if (success)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0,
		    error != NULL && error[0] != '\0' ? error : "operation failed");
}

static void
fn_ThemeDocument_filename(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotString(vm, 0,
		    syncterm_theme_document_filename(document->document));
}

static void
fn_ThemeDocument_metadata(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	uint64_t field;
	if (document == NULL || !slot_integer(vm, 1, 0,
	    SYNCTERM_THEME_METADATA_FIELD_COUNT - 1, &field))
		return;
	const char *value = syncterm_theme_document_metadata(document->document,
	    (unsigned)field);
	if (value == NULL)
		wrenSetSlotNull(vm, 0);
	else
		wrenSetSlotString(vm, 0, value);
}

static void
fn_ThemeDocument_setMetadata(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	uint64_t field;
	char value[INI_MAX_VALUE_LEN];
	if (document == NULL || !slot_integer(vm, 1, 0,
	    SYNCTERM_THEME_METADATA_FIELD_COUNT - 1, &field) ||
	    !theme_document_string_slot(vm, 2, value, sizeof(value)))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_set_metadata(document->document,
	    (unsigned)field, value, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_styles(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document == NULL)
		return;
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	size_t count = syncterm_theme_document_style_count(document->document);
	for (size_t i = 0; i < count; i++) {
		struct syncterm_theme_document_style style;
		if (!syncterm_theme_document_style(document->document, i, &style))
			continue;
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotString(vm, 2, style.role);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotBool(vm, 2, style.builtin);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotDouble(vm, 2, style.present);
		wrenInsertInList(vm, 1, -1, 2);
		for (unsigned field = 0; field < SYNCTERM_THEME_STYLE_FIELD_COUNT;
		    field++) {
			wrenSetSlotDouble(vm, 2, style.values[field]);
			wrenInsertInList(vm, 1, -1, 2);
		}
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_ThemeDocument_setStyle(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char role[INI_MAX_VALUE_LEN];
	uint64_t field;
	uint64_t mode;
	uint64_t value = 0;
	if (document == NULL || !theme_document_string_slot(vm, 1, role,
	    sizeof(role)) || !slot_integer(vm, 2, 0,
	    SYNCTERM_THEME_STYLE_FIELD_COUNT - 1, &field) ||
	    !slot_integer(vm, 3, 0, SYNCTERM_THEME_VALUE_EXPLICIT, &mode))
		return;
	if (mode == SYNCTERM_THEME_VALUE_EXPLICIT &&
	    !slot_integer(vm, 4, 0, 0xffffff, &value))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_set_style(document->document,
	    role, (unsigned)field, (enum syncterm_theme_value_mode)mode,
	    (int)value, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_addStyle(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char role[INI_MAX_VALUE_LEN];
	if (document == NULL || !theme_document_string_slot(vm, 1, role,
	    sizeof(role)))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_add_style(document->document,
	    role, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_removeStyle(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char role[INI_MAX_VALUE_LEN];
	if (document == NULL || !theme_document_string_slot(vm, 1, role,
	    sizeof(role)))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_remove_style(document->document,
	    role, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_glyphs(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document == NULL)
		return;
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	size_t count = syncterm_theme_document_glyph_count(document->document);
	for (size_t i = 0; i < count; i++) {
		struct syncterm_theme_document_glyph glyph;
		if (!syncterm_theme_document_glyph(document->document, i, &glyph))
			continue;
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotString(vm, 2, glyph.name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotBool(vm, 2, glyph.builtin);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotDouble(vm, 2, glyph.present);
		wrenInsertInList(vm, 1, -1, 2);
		for (unsigned field = 0; field < SYNCTERM_THEME_GLYPH_FIELD_COUNT;
		    field++) {
			wrenSetSlotDouble(vm, 2, glyph.values[field]);
			wrenInsertInList(vm, 1, -1, 2);
		}
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_ThemeDocument_setGlyph(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char name[INI_MAX_VALUE_LEN];
	uint64_t field;
	uint64_t mode;
	uint64_t value = 0;
	if (document == NULL || !theme_document_string_slot(vm, 1, name,
	    sizeof(name)) || !slot_integer(vm, 2, 0,
	    SYNCTERM_THEME_GLYPH_FIELD_COUNT - 1, &field) ||
	    !slot_integer(vm, 3, 0, SYNCTERM_THEME_VALUE_EXPLICIT, &mode))
		return;
	if (mode == SYNCTERM_THEME_VALUE_EXPLICIT &&
	    !slot_integer(vm, 4, 0, 0xff, &value))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_set_glyph(document->document,
	    name, (unsigned)field, (enum syncterm_theme_value_mode)mode,
	    (int)value, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_addGlyph(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char name[INI_MAX_VALUE_LEN];
	uint64_t cp437;
	uint64_t ascii;
	if (document == NULL || !theme_document_string_slot(vm, 1, name,
	    sizeof(name)) || !slot_integer(vm, 2, 0, 0xff, &cp437) ||
	    !slot_integer(vm, 3, 0x20, 0x7e, &ascii))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_add_glyph(document->document,
	    name, (int)cp437, (int)ascii, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_removeGlyph(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char name[INI_MAX_VALUE_LEN];
	if (document == NULL || !theme_document_string_slot(vm, 1, name,
	    sizeof(name)))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_remove_glyph(document->document,
	    name, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_themeData(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wren_push_theme_data(vm,
		    syncterm_theme_document_preview(document->document));
}

static void
fn_ThemeDocument_dirty(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotBool(vm, 0,
		    syncterm_theme_document_dirty(document->document));
}

static void
fn_ThemeDocument_canUndo(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotBool(vm, 0,
		    syncterm_theme_document_can_undo(document->document));
}

static void
fn_ThemeDocument_canRedo(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotBool(vm, 0,
		    syncterm_theme_document_can_redo(document->document));
}

static void
fn_ThemeDocument_undo(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotBool(vm, 0,
		    syncterm_theme_document_undo(document->document));
}

static void
fn_ThemeDocument_redo(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document != NULL)
		wrenSetSlotBool(vm, 0,
		    syncterm_theme_document_redo(document->document));
}

static void
fn_ThemeDocument_import(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	char filename[MAX_PATH + 1];
	if (document == NULL || !theme_filename_slot(vm, 1, filename,
	    sizeof(filename)))
		return;
	char error[256] = "";
	bool success = syncterm_theme_document_import(document->document,
	    filename, error, sizeof(error));
	theme_document_result(vm, success, error);
}

static void
fn_ThemeDocument_save(WrenVM *vm)
{
	struct wren_menu_theme_document *document = theme_document_check(vm);
	if (document == NULL)
		return;
	char filename[MAX_PATH + 1];
	const char *save_as = NULL;
	if (wrenGetSlotType(vm, 1) == WREN_TYPE_STRING) {
		if (!theme_filename_slot(vm, 1, filename, sizeof(filename)))
			return;
		save_as = filename;
	}
	else if (wrenGetSlotType(vm, 1) != WREN_TYPE_NULL) {
		wren_throw(vm, "ThemeDocument.save: filename must be String or null");
		return;
	}
	bool force;
	if (!slot_bool(vm, 2, &force))
		return;
	char error[256] = "";
	enum syncterm_theme_document_error result =
	    syncterm_theme_document_save(document->document, save_as, force,
	    error, sizeof(error));
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotDouble(vm, 1, result);
	wrenInsertInList(vm, 0, -1, 1);
	if (result == SYNCTERM_THEME_DOCUMENT_OK)
		wrenSetSlotNull(vm, 1);
	else
		wrenSetSlotString(vm, 1,
		    error[0] != '\0' ? error : "unable to save theme");
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fn_Menu_encryptionAlgorithm(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, list_algo);
}

static void
fn_Menu_encryptionKeySize(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, list_keysize);
}

static void
fn_Menu_encryptionName(WrenVM *vm)
{
	if (list_algo == INI_CRYPT_ALGO_NONE) {
		wrenSetSlotString(vm, 0, "Not Encrypted");
		return;
	}
	char name[80];
	if (list_keysize != 0)
		snprintf(name, sizeof(name), "%s (%d)",
		    iniCryptGetAlgoName(list_algo), list_keysize);
	else
		strlcpy(name, iniCryptGetAlgoName(list_algo), sizeof(name));
	wrenSetSlotString(vm, 0, name);
}

static void
fn_Menu_setEncryption(WrenVM *vm)
{
	uint64_t algo;
	uint64_t keysize;
	if (!slot_integer(vm, 1, INI_CRYPT_ALGO_NONE,
	    INI_CRYPT_ALGO_CHACHA20, &algo) ||
	    !slot_integer(vm, 2, 0, 256, &keysize))
		return;
	const char *password = NULL;
	char supplied[sizeof(list_password)];
	if (wrenGetSlotType(vm, 3) == WREN_TYPE_STRING) {
		if (!slot_string(vm, 3, supplied, sizeof(supplied)))
			return;
		password = supplied;
	}
	else if (wrenGetSlotType(vm, 3) != WREN_TYPE_NULL) {
		wren_throw(vm,
		    "Menu.setEncryption: password must be a String or null");
		return;
	}
	wrenSetSlotBool(vm, 0, rewrite_bbslist_encryption(settings.list_path,
	    (enum iniCryptAlgo)algo, (int)keysize, password));
}

static size_t
web_list_count(void)
{
	size_t count = 0;
	if (settings.webgets != NULL) {
		while (settings.webgets[count] != NULL)
			count++;
	}
	return count;
}

static bool web_lists_dirty;

static char *
fetch_web_list_at(const char *cache, const char *name, const char *uri)
{
	struct webget_request request;
	memset(&request, 0, sizeof(request));
	if (!init_webget_req(&request, cache, name, uri))
		return strdup("Unable to initialize the web-list request");
	bool success = iniReadHttp(&request);
	char *error = success ? NULL : strdup(request.msg != NULL ?
	    request.msg : "Unable to fetch the web list");
	destroy_webget_req(&request);
	return error;
}

static char *
fetch_web_list(const char *name, const char *uri)
{
	char cache[MAX_PATH + 1];
	if (get_syncterm_filename(cache, sizeof(cache),
	    SYNCTERM_PATH_SYSTEM_CACHE, false) == NULL)
		return strdup("Unable to locate the web-list cache");
	return fetch_web_list_at(cache, name, uri);
}

static bool
insert_web_list(size_t index, const char *name, const char *uri)
{
	size_t count = web_list_count();
	if (index > count)
		index = count;
	named_string_t *entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return false;
	entry->name = strdup(name);
	entry->value = strdup(uri);
	if (entry->name == NULL || entry->value == NULL) {
		free(entry->name);
		free(entry->value);
		free(entry);
		return false;
	}
	named_string_t **items = realloc(settings.webgets,
	    (count + 2) * sizeof(*items));
	if (items == NULL) {
		free(entry->name);
		free(entry->value);
		free(entry);
		return false;
	}
	settings.webgets = items;
	memmove(&items[index + 1], &items[index],
	    (count - index + 1) * sizeof(*items));
	items[index] = entry;
	return true;
}

static void
remove_web_list(size_t index, bool free_entry)
{
	size_t count = web_list_count();
	named_string_t *entry = settings.webgets[index];
	memmove(&settings.webgets[index], &settings.webgets[index + 1],
	    (count - index) * sizeof(*settings.webgets));
	if (free_entry) {
		free(entry->name);
		free(entry->value);
		free(entry);
	}
}

static void
fn_Menu_webLists(WrenVM *vm)
{
	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);
	for (size_t i = 0; i < web_list_count(); i++) {
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotString(vm, 2, settings.webgets[i]->name);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, settings.webgets[i]->value);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

static void
fn_Menu_webListsDirty(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, web_lists_dirty);
}

static void
fn_Menu_addWebList(WrenVM *vm)
{
	/* Safe mode blocks save_webgets(), not working-list edits. */
	char name[INI_MAX_VALUE_LEN + 1];
	char uri[INI_MAX_VALUE_LEN + 1];
	uint64_t index;
	if (!slot_string(vm, 1, name, sizeof(name)) ||
	    !slot_string(vm, 2, uri, sizeof(uri)) ||
	    !slot_integer(vm, 3, 0, web_list_count(), &index))
		return;
	if (name[0] == 0 || uri[0] == 0 ||
	    stricmp(name, "System List") == 0 ||
	    (settings.webgets != NULL &&
	    namedStrListFindName(settings.webgets, name) != NULL)) {
		wrenSetSlotString(vm, 0, "Invalid or duplicate web-list name");
		return;
	}
	char cache[MAX_PATH + 1];
	if (get_syncterm_filename(cache, sizeof(cache),
	    SYNCTERM_PATH_SYSTEM_CACHE, false) != NULL) {
		char *error = fetch_web_list_at(cache, name, uri);
		if (error != NULL) {
			wrenSetSlotString(vm, 0, error);
			free(error);
			return;
		}
	}
	if (!insert_web_list((size_t)index, name, uri)) {
		wrenSetSlotString(vm, 0, "Unable to allocate the web-list entry");
		return;
	}
	web_lists_dirty = true;
	wrenSetSlotNull(vm, 0);
}

static void
fn_Menu_updateWebList(WrenVM *vm)
{
	uint64_t index;
	char uri[INI_MAX_VALUE_LEN + 1];
	size_t count = web_list_count();
	if (count == 0 || !slot_integer(vm, 1, 0, count - 1, &index) ||
	    !slot_string(vm, 2, uri, sizeof(uri)))
		return;
	if (strcmp(settings.webgets[index]->value, uri) == 0) {
		wrenSetSlotBool(vm, 0, true);
		return;
	}
	char *replacement = strdup(uri);
	if (replacement == NULL) {
		wrenSetSlotBool(vm, 0, false);
		return;
	}
	free(settings.webgets[index]->value);
	settings.webgets[index]->value = replacement;
	web_lists_dirty = true;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_Menu_deleteWebList(WrenVM *vm)
{
	uint64_t index;
	size_t count = web_list_count();
	if (count == 0 || !slot_integer(vm, 1, 0, count - 1, &index))
		return;
	remove_web_list((size_t)index, true);
	web_lists_dirty = true;
	wrenSetSlotBool(vm, 0, true);
}

static void
fn_Menu_saveWebLists(WrenVM *vm)
{
	if (!web_lists_dirty) {
		wrenSetSlotBool(vm, 0, true);
		return;
	}
	bool success = save_webgets();
	if (success)
		web_lists_dirty = false;
	wrenSetSlotBool(vm, 0, success);
}

static void
fn_Menu_refreshWebList(WrenVM *vm)
{
	uint64_t index;
	size_t count = web_list_count();
	if (count == 0 || !slot_integer(vm, 1, 0, count - 1, &index))
		return;
	char *error = fetch_web_list(settings.webgets[index]->name,
	    settings.webgets[index]->value);
	if (error == NULL)
		wrenSetSlotNull(vm, 0);
	else {
		wrenSetSlotString(vm, 0, error);
		free(error);
	}
}

struct binding {
	const char *class_name;
	bool is_static;
	const char *signature;
	WrenForeignMethodFn function;
};

#define SETTINGS_GETSET(name)                                          \
	{ "Settings", false, #name, fn_Settings_##name },                \
	{ "Settings", false, #name "=(_)", fn_Settings_##name##_set }

static const struct binding bindings[] = {
	{ "Menu", true, "settings", fn_Menu_settings },
	{ "Menu", true, "screenModes", fn_Menu_screenModes },
	{ "Menu", true, "outputModes", fn_Menu_outputModes },
	{ "Menu", true, "cursorStyles", fn_Menu_cursorStyles },
	{ "Menu", true, "audioModes", fn_Menu_audioModes },
	{ "Menu", true, "buildOptions", fn_Menu_buildOptions },
	{ "Menu", true, "maxPathLength", fn_Menu_maxPathLength },
	{ "Menu", true, "encryptionAvailable", fn_Menu_encryptionAvailable },
	{ "Menu", true, "scalingModes", fn_Menu_scalingModes },
	{ "Menu", true, "colors", fn_Menu_colors },
	{ "Menu", true, "backgroundColors", fn_Menu_backgroundColors },
	{ "Menu", true, "currentScreenMode", fn_Menu_currentScreenMode },
	{ "Menu", true, "setScreenMode(_)", fn_Menu_setScreenMode },
	{ "Menu", true, "fileLocations", fn_Menu_fileLocations },
	{ "Menu", true, "themes", fn_Menu_themes },
	{ "Menu", true, "selectedThemeFile", fn_Menu_selectedThemeFile },
	{ "Menu", true, "selectedThemePackage", fn_Menu_selectedThemePackage },
	{ "Menu", true, "previewTheme(_)", fn_Menu_previewTheme },
	{ "Menu", true, "cancelThemePreview()", fn_Menu_cancelThemePreview },
	{ "Menu", true, "selectTheme(_)", fn_Menu_selectTheme },
	{ "Menu", true, "deleteTheme(_)", fn_Menu_deleteTheme },
	{ "Menu", true, "newThemeDocument()", fn_Menu_newThemeDocument },
	{ "Menu", true, "openThemeDocument(_)", fn_Menu_openThemeDocument },
	{ "Menu", true, "refreshCloudThemes()", fn_Menu_refreshCloudThemes },
	{ "Menu", true, "cloudThemes", fn_Menu_cloudThemes },
	{ "Menu", true, "previewCloudTheme(_)", fn_Menu_previewCloudTheme },
	{ "Menu", true, "selectCloudTheme(_)", fn_Menu_selectCloudTheme },
	{ "Menu", true, "deleteCloudTheme(_)", fn_Menu_deleteCloudTheme },
	{ "Menu", true, "cacheCloudTheme(_)", fn_Menu_cacheCloudTheme },
	{ "Menu", true, "copyCloudTheme(_)", fn_Menu_copyCloudTheme },
	{ "Menu", true, "encryptionAlgorithm", fn_Menu_encryptionAlgorithm },
	{ "Menu", true, "encryptionKeySize", fn_Menu_encryptionKeySize },
	{ "Menu", true, "encryptionName", fn_Menu_encryptionName },
	{ "Menu", true, "setEncryption(_,_,_)", fn_Menu_setEncryption },
	{ "Menu", true, "webLists", fn_Menu_webLists },
	{ "Menu", true, "webListsDirty", fn_Menu_webListsDirty },
	{ "Menu", true, "addWebList(_,_,_)", fn_Menu_addWebList },
	{ "Menu", true, "updateWebList(_,_)", fn_Menu_updateWebList },
	{ "Menu", true, "deleteWebList(_)", fn_Menu_deleteWebList },
	{ "Menu", true, "saveWebLists()", fn_Menu_saveWebLists },
	{ "Menu", true, "refreshWebList(_)", fn_Menu_refreshWebList },
	{ "ThemeDocument", false, "filename", fn_ThemeDocument_filename },
	{ "ThemeDocument", false, "metadata(_)", fn_ThemeDocument_metadata },
	{ "ThemeDocument", false, "setMetadata(_,_)",
	    fn_ThemeDocument_setMetadata },
	{ "ThemeDocument", false, "styles", fn_ThemeDocument_styles },
	{ "ThemeDocument", false, "setStyle(_,_,_,_)",
	    fn_ThemeDocument_setStyle },
	{ "ThemeDocument", false, "addStyle(_)", fn_ThemeDocument_addStyle },
	{ "ThemeDocument", false, "removeStyle(_)",
	    fn_ThemeDocument_removeStyle },
	{ "ThemeDocument", false, "glyphs", fn_ThemeDocument_glyphs },
	{ "ThemeDocument", false, "setGlyph(_,_,_,_)",
	    fn_ThemeDocument_setGlyph },
	{ "ThemeDocument", false, "addGlyph(_,_,_)",
	    fn_ThemeDocument_addGlyph },
	{ "ThemeDocument", false, "removeGlyph(_)",
	    fn_ThemeDocument_removeGlyph },
	{ "ThemeDocument", false, "themeData", fn_ThemeDocument_themeData },
	{ "ThemeDocument", false, "dirty", fn_ThemeDocument_dirty },
	{ "ThemeDocument", false, "canUndo", fn_ThemeDocument_canUndo },
	{ "ThemeDocument", false, "canRedo", fn_ThemeDocument_canRedo },
	{ "ThemeDocument", false, "undo()", fn_ThemeDocument_undo },
	{ "ThemeDocument", false, "redo()", fn_ThemeDocument_redo },
	{ "ThemeDocument", false, "importTheme(_)", fn_ThemeDocument_import },
	{ "ThemeDocument", false, "save(_,_)", fn_ThemeDocument_save },
	{ "Settings", false, "dirty", fn_Settings_dirty },
	{ "Settings", false, "apply()", fn_Settings_apply },
	{ "Settings", false, "save()", fn_Settings_save },
	{ "Settings", false, "reload()", fn_Settings_reload },
	SETTINGS_GETSET(confirmClose),
	SETTINGS_GETSET(promptSave),
	SETTINGS_GETSET(invertWheel),
	SETTINGS_GETSET(modemDevice),
	SETTINGS_GETSET(modemInit),
	SETTINGS_GETSET(modemDial),
	SETTINGS_GETSET(listPath),
	SETTINGS_GETSET(shellTerm),
	SETTINGS_GETSET(startupMode),
	SETTINGS_GETSET(outputMode),
	SETTINGS_GETSET(cursorStyle),
	SETTINGS_GETSET(audioModes),
	SETTINGS_GETSET(scrollbackLines),
	SETTINGS_GETSET(modemRate),
	SETTINGS_GETSET(scalingMode),
	SETTINGS_GETSET(kdfShift),
	SETTINGS_GETSET(customRows),
	SETTINGS_GETSET(customColumns),
	SETTINGS_GETSET(customFontHeight),
	SETTINGS_GETSET(customAspectWidth),
	SETTINGS_GETSET(customAspectHeight),
	SETTINGS_GETSET(frameColor),
	SETTINGS_GETSET(textColor),
	SETTINGS_GETSET(backgroundColor),
	SETTINGS_GETSET(inverseColor),
	SETTINGS_GETSET(lightbarColor),
	SETTINGS_GETSET(lightbarBackgroundColor),
};

WrenForeignMethodFn
wren_menu_settings_bind_lookup(const char *module, const char *class_name,
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
wren_menu_settings_bind_lookup_class(const char *module,
    const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module != NULL && strcmp(module, "syncterm_menu") == 0 &&
	    strcmp(class_name, "Settings") == 0) {
		WrenForeignClassMethods methods = {
			settings_allocate, settings_finalize
		};
		return methods;
	}
	if (module != NULL && strcmp(module, "syncterm_menu") == 0 &&
	    strcmp(class_name, "ThemeDocument") == 0) {
		WrenForeignClassMethods methods = {
			theme_document_allocate, theme_document_finalize
		};
		return methods;
	}
	return none;
}
