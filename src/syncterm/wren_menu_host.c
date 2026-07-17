#include "wren_menu_host.h"

#include "wren_bind_menu.h"
#include "wren_bind_internal.h"
#include "wren_host.h"
#include "wren_host_internal.h"

#include "bbslist.h"
#include "dirwrap.h"
#include "genwrap.h"
#include "syncterm.h"
#include "term.h"
#include "wren.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static struct wren_host_state menu_state;
static bool menu_active;
static WrenHandle *menu_run_handle;
static WrenHandle *menu_prepare_handle;
static WrenHandle *menu_offer_save_handle;
static WrenHandle *menu_ui_class;
static WrenHandle *menu_alert_handle;
static WrenHandle *menu_confirm_handle;
static WrenHandle *menu_prompt_handle;
static WrenHandle *menu_choice_handle;
static WrenHandle *menu_status_handle;
static WrenHandle *menu_status_clear_handle;
static struct ciolib_screen *menu_status_screen;
static struct bbslist selected_bbs;

static bool
valid_module_name(const char *name)
{
	if (name == NULL || *name == '\0')
		return false;
	for (const char *p = name; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!((c >= 'A' && c <= 'Z') ||
		      (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') || c == '_'))
			return false;
	}
	return true;
}

static char *
read_script_file(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	long len = ftell(fp);
	if (len < 0 || len > 4 * 1024 * 1024 ||
	    fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}
	char *source = malloc((size_t)len + 1);
	if (source == NULL) {
		fclose(fp);
		return NULL;
	}
	size_t got = fread(source, 1, (size_t)len, fp);
	fclose(fp);
	source[got] = '\0';
	return source;
}

static void
module_complete(WrenVM *vm, const char *name, WrenLoadModuleResult result)
{
	(void)vm;
	(void)name;
	free((void *)result.source);
}

static WrenLoadModuleResult
load_module(WrenVM *vm, const char *name)
{
	WrenLoadModuleResult result = { NULL, NULL, NULL };
	(void)vm;
	if (!valid_module_name(name)) {
		result.source =
		    "Fiber.abort(\"invalid Wren module name: only "
		    "[A-Za-z0-9_] allowed\")\n";
		return result;
	}

	char dir[MAX_PATH + 1];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS,
	    false) != NULL) {
		char path[MAX_PATH + 1];
		int len = snprintf(path, sizeof(path), "%s%s.wren", dir, name);
		if (len > 0 && (size_t)len < sizeof(path)) {
			result.source = read_script_file(path);
			if (result.source != NULL) {
				result.onComplete = module_complete;
				return result;
			}
		}
		len = snprintf(path, sizeof(path), "%sauto/menu/%s.wren",
		    dir, name);
		if (len > 0 && (size_t)len < sizeof(path)) {
			result.source = read_script_file(path);
			if (result.source != NULL) {
				result.onComplete = module_complete;
				return result;
			}
		}
	}

	for (const struct embedded_script *script = EMBEDDED_SCRIPTS;
	    script->name != NULL; script++) {
		if (strcmp(script->name, name) == 0 &&
		    (script->event == NULL ||
		    strcmp(script->event, "menu") == 0)) {
			result.source = script->source;
			return result;
		}
	}
	return result;
}

static bool
generic_class_allowed(const char *class_name)
{
	static const char *const allowed[] = {
		"Cell", "Clipboard", "Codepage", "Color", "CustomCursor",
		"Directory", "File", "FileError", "Font", "Format", "Host",
		"Hyperlinks", "Input", "KeyEvent", "MouseEvent", "Palette",
		"PhysicalKeyEvent", "Platform", "Screen", "ScreenFonts",
		"ScreenSupports", "ScreenWindow", "Surface", "VideoFlags",
	};
	for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
		if (strcmp(class_name, allowed[i]) == 0)
			return true;
	}
	return false;
}

static bool
generic_method_allowed(const char *class_name, const char *signature)
{
	if (strcmp(class_name, "CTerm") == 0)
		return strcmp(signature, "statusDisplay") == 0;
	if (!generic_class_allowed(class_name))
		return false;
	if (strcmp(class_name, "Input") == 0 &&
	    strcmp(signature, "setupMouseEvents()") == 0)
		return false;
	if (strcmp(class_name, "Host") != 0)
		return true;
	static const char *const host_allowed[] = {
		"altKeyName", "altKeyShort", "cacheDirectory", "downloadDir",
		"pickFile(_,_,_)",
		"pickFiles(_,_,_)",
		"pickSavePath(_,_)",
		"print(_)",
		"safeMode", "textTerminal",
	};
	for (size_t i = 0;
	    i < sizeof(host_allowed) / sizeof(host_allowed[0]); i++) {
		if (strcmp(signature, host_allowed[i]) == 0)
			return true;
	}
	return false;
}

static void
menu_capability_denied(WrenVM *vm)
{
	wren_throw(vm, "this capability is not available in the menu VM");
}

static WrenForeignMethodFn
bind_foreign_method(WrenVM *vm, const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	(void)vm;
	if (strcmp(module, "syncterm_menu") == 0)
		return wren_menu_bind_lookup(module, class_name, is_static,
		    signature);
	if (strcmp(module, "syncterm") != 0)
		return NULL;
	WrenForeignMethodFn fn = wren_bind_lookup(module, class_name,
	    is_static, signature);
	if (fn == NULL)
		return NULL;
	if (!generic_method_allowed(class_name, signature))
		return menu_capability_denied;
	return fn;
}

static WrenForeignClassMethods
bind_foreign_class(WrenVM *vm, const char *module, const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	(void)vm;
	if (strcmp(module, "syncterm_menu") == 0)
		return wren_menu_bind_lookup_class(module, class_name);
	if (strcmp(module, "syncterm") != 0 ||
	    !generic_class_allowed(class_name))
		return none;
	return wren_bind_lookup_class(module, class_name);
}

static void
write_output(WrenVM *vm, const char *text)
{
	(void)vm;
	fputs(text, stderr);
}

static void
report_error(WrenVM *vm, WrenErrorType type, const char *module, int line,
    const char *message)
{
	(void)vm;
	const char *kind = type == WREN_ERROR_COMPILE ? "compile" :
	    type == WREN_ERROR_RUNTIME ? "runtime" : "stack";
	fprintf(stderr, "[wren-menu] %s %s:%d: %s\n", kind,
	    module != NULL ? module : "?", line,
	    message != NULL ? message : "");
}

static void
module_name(const char *path, char *name, size_t size)
{
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *backslash = strrchr(path, '\\');
	if (backslash != NULL && (base == NULL || backslash > base))
		base = backslash;
#endif
	base = base == NULL ? path : base + 1;
	strlcpy(name, base, size);
	char *dot = strrchr(name, '.');
	if (dot != NULL)
		*dot = '\0';
}

static bool
load_event_scripts(void)
{
	glob_t files;
	memset(&files, 0, sizeof(files));
	bool have_files = false;
	char dir[MAX_PATH + 1];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS,
	    false) != NULL) {
		char pattern[MAX_PATH + 32];
		snprintf(pattern, sizeof(pattern), "%sauto/menu/*.wren", dir);
		have_files = glob(pattern, 0, NULL, &files) == 0;
	}

	bool success = true;
	for (const struct embedded_script *script = EMBEDDED_SCRIPTS;
	    script->name != NULL; script++) {
		if (script->event == NULL || strcmp(script->event, "menu") != 0)
			continue;
		bool overridden = false;
		if (have_files) {
			for (size_t i = 0; i < files.gl_pathc; i++) {
				char name[256];
				module_name(files.gl_pathv[i], name, sizeof(name));
				if (strcmp(name, script->name) == 0) {
					overridden = true;
					break;
				}
			}
		}
		if (!overridden &&
		    wrenInterpret(menu_state.vm, script->name, script->source) !=
		    WREN_RESULT_SUCCESS)
			success = false;
	}
	if (have_files) {
		for (size_t i = 0; i < files.gl_pathc; i++) {
			char name[256];
			module_name(files.gl_pathv[i], name, sizeof(name));
			if (wrenHasModule(menu_state.vm, name))
				continue;
			char *source = read_script_file(files.gl_pathv[i]);
			if (source == NULL ||
			    wrenInterpret(menu_state.vm, name, source) !=
			    WREN_RESULT_SUCCESS)
				success = false;
			free(source);
		}
		globfree(&files);
	}
	return success;
}

bool
wren_menu_host_init(void)
{
	if (menu_active)
		return true;
	memset(&menu_state, 0, sizeof(menu_state));
	menu_state.input_epoch = 1;
	wren_menu_bind_init();

	WrenConfiguration config;
	wrenInitConfiguration(&config);
	config.writeFn = write_output;
	config.errorFn = report_error;
	config.bindForeignMethodFn = bind_foreign_method;
	config.bindForeignClassFn = bind_foreign_class;
	config.loadModuleFn = load_module;

	struct wren_host_state *old = wren_host_select_state(&menu_state);
	menu_state.vm = wrenNewVM(&config);
	if (menu_state.vm != NULL)
		menu_active = load_event_scripts();
	if (!menu_active && menu_state.vm != NULL) {
		wrenFreeVM(menu_state.vm);
		menu_state.vm = NULL;
	}
	if (!menu_active)
		wren_menu_bind_shutdown();
	else {
		menu_run_handle = wrenMakeCallHandle(menu_state.vm, "run(_,_)");
		menu_prepare_handle = wrenMakeCallHandle(menu_state.vm, "prepare()");
		menu_offer_save_handle = wrenMakeCallHandle(menu_state.vm,
		    "offerSave(_)");
		if (wrenHasModule(menu_state.vm, "menu_host_ui") &&
		    wrenHasVariable(menu_state.vm, "menu_host_ui", "MenuHostUI")) {
			wrenEnsureSlots(menu_state.vm, 1);
			wrenGetVariable(menu_state.vm, "menu_host_ui", "MenuHostUI", 0);
			menu_ui_class = wrenGetSlotHandle(menu_state.vm, 0);
			menu_alert_handle = wrenMakeCallHandle(menu_state.vm,
			    "alert(_,_)");
			menu_confirm_handle = wrenMakeCallHandle(menu_state.vm,
			    "confirm(_,_)");
			menu_prompt_handle = wrenMakeCallHandle(menu_state.vm,
			    "prompt(_,_,_,_,_)");
			menu_choice_handle = wrenMakeCallHandle(menu_state.vm,
			    "choice(_,_,_,_)");
			menu_status_handle = wrenMakeCallHandle(menu_state.vm,
			    "status(_,_)");
			menu_status_clear_handle = wrenMakeCallHandle(menu_state.vm,
			    "statusClear()");
		}
	}
	wren_host_select_state(old);
	return menu_active;
}

void
wren_menu_host_shutdown(void)
{
	if (!menu_active)
		return;
	wren_menu_host_status_clear();
	struct wren_host_state *old = wren_host_select_state(&menu_state);
	if (menu_run_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_run_handle);
	menu_run_handle = NULL;
	if (menu_prepare_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_prepare_handle);
	if (menu_offer_save_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_offer_save_handle);
	menu_prepare_handle = NULL;
	menu_offer_save_handle = NULL;
	if (menu_ui_class != NULL)
		wrenReleaseHandle(menu_state.vm, menu_ui_class);
	if (menu_alert_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_alert_handle);
	if (menu_confirm_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_confirm_handle);
	if (menu_prompt_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_prompt_handle);
	if (menu_choice_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_choice_handle);
	if (menu_status_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_status_handle);
	if (menu_status_clear_handle != NULL)
		wrenReleaseHandle(menu_state.vm, menu_status_clear_handle);
	menu_ui_class = NULL;
	menu_alert_handle = NULL;
	menu_confirm_handle = NULL;
	menu_prompt_handle = NULL;
	menu_choice_handle = NULL;
	menu_status_handle = NULL;
	menu_status_clear_handle = NULL;
	wrenFreeVM(menu_state.vm);
	wren_menu_bind_shutdown();
	wren_host_select_state(old == &menu_state ? NULL : old);
	memset(&menu_state, 0, sizeof(menu_state));
	menu_active = false;
}

bool
wren_menu_host_active(void)
{
	return menu_active;
}

struct bbslist *
wren_menu_host_run(const char *current, bool connected)
{
	if (!menu_active || menu_state.vm == NULL || menu_run_handle == NULL ||
	    !wrenHasModule(menu_state.vm, "main_menu") ||
	    !wrenHasVariable(menu_state.vm, "main_menu", "MainMenu"))
		return NULL;

	wren_menu_host_status_clear();
	wren_host_input_barrier();
	struct wren_host_state *old = wren_host_select_state(&menu_state);
	wrenEnsureSlots(menu_state.vm, 3);
	wrenGetVariable(menu_state.vm, "main_menu", "MainMenu", 0);
	if (current == NULL)
		wrenSetSlotNull(menu_state.vm, 1);
	else
		wrenSetSlotString(menu_state.vm, 1, current);
	wrenSetSlotBool(menu_state.vm, 2, connected);

	WrenInterpretResult result = wrenCall(menu_state.vm, menu_run_handle);
	struct bbslist *selected = NULL;
	if (result == WREN_RESULT_SUCCESS && !connected &&
	    wrenGetSlotType(menu_state.vm, 0) != WREN_TYPE_NULL) {
		if (wren_menu_bind_copy_bbs(menu_state.vm, 0, &selected_bbs))
			selected = &selected_bbs;
		else
			fputs("[wren-menu] MainMenu.run returned an invalid BBS\n",
			    stderr);
	}
	wren_host_select_state(old);
	wren_host_input_barrier();
	return selected;
}

bool
wren_menu_host_offer_save_bbs(const struct bbslist *bbs)
{
	if (bbs == NULL || !menu_active || menu_state.vm == NULL ||
	    menu_prepare_handle == NULL || menu_offer_save_handle == NULL ||
	    !wrenHasModule(menu_state.vm, "main_menu") ||
	    !wrenHasVariable(menu_state.vm, "main_menu", "MainMenu"))
		return false;

	wren_menu_host_status_clear();
	wren_host_input_barrier();
	struct ciolib_screen *screen = cp437_savescrn();
	if (screen == NULL) {
		wren_host_input_barrier();
		return false;
	}

	struct wren_host_state *old = wren_host_select_state(&menu_state);
	wrenEnsureSlots(menu_state.vm, 1);
	wrenGetVariable(menu_state.vm, "main_menu", "MainMenu", 0);
	bool ready = wrenCall(menu_state.vm, menu_prepare_handle) ==
	    WREN_RESULT_SUCCESS &&
	    wrenGetSlotType(menu_state.vm, 0) == WREN_TYPE_BOOL &&
	    wrenGetSlotBool(menu_state.vm, 0);
	bool saved = false;
	if (ready) {
		wrenEnsureSlots(menu_state.vm, 3);
		wrenGetVariable(menu_state.vm, "main_menu", "MainMenu", 0);
		if (wren_menu_bind_push_transient_bbs(menu_state.vm, 1, bbs) &&
		    wrenCall(menu_state.vm, menu_offer_save_handle) ==
		    WREN_RESULT_SUCCESS &&
		    wrenGetSlotType(menu_state.vm, 0) == WREN_TYPE_BOOL)
			saved = wrenGetSlotBool(menu_state.vm, 0);
	}
	wren_host_select_state(old);
	restorescreen(screen);
	freescreen(screen);
	wren_host_input_barrier();
	return saved;
}

static bool
menu_ui_call(WrenHandle *handle, int slots, bool blocking)
{
	if (!menu_active || menu_state.vm == NULL || menu_ui_class == NULL ||
	    handle == NULL)
		return false;
	struct ciolib_screen *screen = NULL;
	if (blocking) {
		wren_host_input_barrier();
		screen = cp437_savescrn();
		if (screen == NULL) {
			wren_host_input_barrier();
			return false;
		}
	}
	struct wren_host_state *old = wren_host_select_state(&menu_state);
	wrenEnsureSlots(menu_state.vm, slots);
	wrenSetSlotHandle(menu_state.vm, 0, menu_ui_class);
	bool success = wrenCall(menu_state.vm, handle) == WREN_RESULT_SUCCESS;
	wren_host_select_state(old);
	if (blocking) {
		restorescreen(screen);
		freescreen(screen);
		wren_host_input_barrier();
	}
	return success;
}

bool
wren_menu_host_alert(const char *title, const char *message)
{
	if (!menu_active || menu_state.vm == NULL || menu_alert_handle == NULL)
		return false;
	wrenEnsureSlots(menu_state.vm, 3);
	wrenSetSlotString(menu_state.vm, 1, title == NULL ? "Alert" : title);
	wrenSetSlotString(menu_state.vm, 2, message == NULL ? "" : message);
	return menu_ui_call(menu_alert_handle, 3, true);
}

bool
wren_menu_host_confirm(const char *title, const char *message, bool *answer)
{
	if (answer == NULL || !menu_active || menu_state.vm == NULL ||
	    menu_confirm_handle == NULL)
		return false;
	wrenEnsureSlots(menu_state.vm, 3);
	wrenSetSlotString(menu_state.vm, 1, title == NULL ? "Confirm" : title);
	wrenSetSlotString(menu_state.vm, 2, message == NULL ? "" : message);
	if (!menu_ui_call(menu_confirm_handle, 3, true) ||
	    wrenGetSlotType(menu_state.vm, 0) != WREN_TYPE_BOOL)
		return false;
	*answer = wrenGetSlotBool(menu_state.vm, 0);
	return true;
}

int
wren_menu_host_prompt(const char *title, const char *message,
    const char *initial, size_t max_len, bool masked, char *result,
    size_t result_size)
{
	if (result == NULL || result_size == 0 || max_len > INT_MAX ||
	    !menu_active || menu_state.vm == NULL || menu_prompt_handle == NULL)
		return -2;
	wrenEnsureSlots(menu_state.vm, 6);
	wrenSetSlotString(menu_state.vm, 1, title == NULL ? "Prompt" : title);
	wrenSetSlotString(menu_state.vm, 2, message == NULL ? "" : message);
	wrenSetSlotString(menu_state.vm, 3, initial == NULL ? "" : initial);
	wrenSetSlotDouble(menu_state.vm, 4, (double)max_len);
	wrenSetSlotBool(menu_state.vm, 5, masked);
	if (!menu_ui_call(menu_prompt_handle, 6, true))
		return -2;
	if (wrenGetSlotType(menu_state.vm, 0) == WREN_TYPE_NULL)
		return -1;
	if (wrenGetSlotType(menu_state.vm, 0) != WREN_TYPE_STRING)
		return -2;
	int length;
	const char *value = wrenGetSlotBytes(menu_state.vm, 0, &length);
	if (length < 0 || (size_t)length >= result_size ||
	    (size_t)length > max_len)
		return -2;
	memcpy(result, value, (size_t)length);
	result[length] = 0;
	return length;
}

int
wren_menu_host_choice(const char *title, const char *message,
    const char *const *options, size_t count, int current)
{
	if (options == NULL || count == 0 || count > INT_MAX || current < 0 ||
	    (size_t)current >= count || !menu_active || menu_state.vm == NULL ||
	    menu_choice_handle == NULL)
		return -2;
	wrenEnsureSlots(menu_state.vm, 6);
	wrenSetSlotString(menu_state.vm, 1, title == NULL ? "Select" : title);
	wrenSetSlotString(menu_state.vm, 2, message == NULL ? "" : message);
	wrenSetSlotNewList(menu_state.vm, 3);
	for (size_t i = 0; i < count; i++) {
		wrenSetSlotString(menu_state.vm, 5,
		    options[i] == NULL ? "" : options[i]);
		wrenInsertInList(menu_state.vm, 3, -1, 5);
	}
	wrenSetSlotDouble(menu_state.vm, 4, current);
	if (!menu_ui_call(menu_choice_handle, 6, true))
		return -2;
	if (wrenGetSlotType(menu_state.vm, 0) == WREN_TYPE_NULL)
		return -1;
	if (wrenGetSlotType(menu_state.vm, 0) != WREN_TYPE_NUM)
		return -2;
	double value = wrenGetSlotDouble(menu_state.vm, 0);
	if (value < 0 || value >= (double)count || value != (int)value)
		return -2;
	return (int)value;
}

bool
wren_menu_host_status(const char *title, const char *const *lines,
    size_t count)
{
	if (lines == NULL || count == 0 || count > INT_MAX || !menu_active ||
	    menu_state.vm == NULL || menu_status_handle == NULL)
		return false;
	if (menu_status_screen == NULL) {
		menu_status_screen = cp437_savescrn();
		if (menu_status_screen == NULL)
			return false;
	}
	wrenEnsureSlots(menu_state.vm, 4);
	wrenSetSlotString(menu_state.vm, 1, title == NULL ? "Status" : title);
	wrenSetSlotNewList(menu_state.vm, 2);
	for (size_t i = 0; i < count; i++) {
		wrenSetSlotString(menu_state.vm, 3,
		    lines[i] == NULL ? "" : lines[i]);
		wrenInsertInList(menu_state.vm, 2, -1, 3);
	}
	if (menu_ui_call(menu_status_handle, 4, false))
		return true;
	wren_menu_host_status_clear();
	return false;
}

void
wren_menu_host_status_clear(void)
{
	if (menu_active && menu_state.vm != NULL && menu_ui_class != NULL &&
	    menu_status_clear_handle != NULL) {
		(void)menu_ui_call(menu_status_clear_handle, 1, false);
	}
	if (menu_status_screen != NULL) {
		restorescreen(menu_status_screen);
		freescreen(menu_status_screen);
		menu_status_screen = NULL;
	}
}
