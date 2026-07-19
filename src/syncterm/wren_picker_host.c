#include "wren_picker_host.h"

#include "wren_bind_internal.h"
#include "wren_host.h"
#include "wren_host_internal.h"

#include "dirwrap.h"
#include "genwrap.h"
#include "syncterm.h"
#include "wren.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct wren_host_state picker_state;
static bool picker_active;

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

static const struct embedded_script *
embedded_module(const char *name, const char *event)
{
	for (const struct embedded_script *script = EMBEDDED_SCRIPTS;
	    script->name != NULL; script++) {
		if (strcmp(script->name, name) != 0)
			continue;
		if ((event == NULL && script->event == NULL) ||
		    (event != NULL && script->event != NULL &&
		    strcmp(script->event, event) == 0))
			return script;
	}
	return NULL;
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

	/* The recovery module is interpreted from the embedded copy before
	 * any user code, but keep this guard for imports during bootstrap. */
	if (strcmp(name, "picker_bootstrap") == 0) {
		const struct embedded_script *script = embedded_module(name, NULL);
		if (script != NULL)
			result.source = script->source;
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
		len = snprintf(path, sizeof(path), "%sauto/picker/%s.wren",
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
		    strcmp(script->event, "picker") == 0)) {
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
		"Cell", "Clipboard", "Codepage", "Color", "Console",
		"CustomCursor", "Host", "Input", "KeyEvent", "MouseEvent",
		"Palette", "REPL", "Screen", "ScreenFonts", "ScreenSupports",
		"ScreenWindow", "Surface", "VideoFlags",
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
	if (strcmp(class_name, "Input") == 0) {
		static const char *const denied[] = {
			"pushClaim_(_,_)" , "setupMouseEvents()",
			"synthesizePhysicalKey(_,_)" , "ungetKey_(_)",
			"ungetMouse_(_)", "ungetPhysicalKey_(_)",
		};
		for (size_t i = 0; i < sizeof(denied) / sizeof(denied[0]); i++) {
			if (strcmp(signature, denied[i]) == 0)
				return false;
		}
	}
	if (strcmp(class_name, "Host") != 0)
		return true;
	return strcmp(signature, "logUnread") == 0 ||
	    strcmp(signature, "logUnreadError") == 0 ||
	    strcmp(signature, "print(_)") == 0 ||
	    strcmp(signature, "textTerminal") == 0;
}

static void
picker_capability_denied(WrenVM *vm)
{
	wren_throw(vm, "this capability is not available in the picker VM");
}

static WrenForeignMethodFn
bind_foreign_method(WrenVM *vm, const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	(void)vm;
	if (strcmp(module, "syncterm_picker") == 0)
		return picker_capability_denied;
	if (strcmp(module, "syncterm") != 0)
		return NULL;
	WrenForeignMethodFn fn = wren_bind_lookup(module, class_name,
	    is_static, signature);
	if (fn == NULL)
		return NULL;
	if (!generic_method_allowed(class_name, signature))
		return picker_capability_denied;
	return fn;
}

static WrenForeignClassMethods
bind_foreign_class(WrenVM *vm, const char *module, const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	(void)vm;
	if (strcmp(module, "syncterm") != 0 ||
	    !generic_class_allowed(class_name))
		return none;
	return wren_bind_lookup_class(module, class_name);
}

static void
write_output(WrenVM *vm, const char *message)
{
	(void)vm;
	wren_log_write(message);
	fputs(message, stderr);
}

static void
report_error(WrenVM *vm, WrenErrorType type, const char *module, int line,
    const char *message)
{
	(void)vm;
	wren_log_error(type, module, line, message);
	const char *kind = type == WREN_ERROR_COMPILE ? "compile" :
	    type == WREN_ERROR_RUNTIME ? "runtime" : "stack";
	fprintf(stderr, "[wren-picker] %s %s:%d: %s\n", kind,
	    module != NULL ? module : "?", line,
	    message != NULL ? message : "");
}

static void
module_name(const char *path, char *name, size_t size)
{
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *back = strrchr(path, '\\');
	if (back != NULL && (base == NULL || back > base))
		base = back;
#endif
	base = base == NULL ? path : base + 1;
	strlcpy(name, base, size);
	char *dot = strrchr(name, '.');
	if (dot != NULL)
		*dot = '\0';
}

static void
load_event_scripts(void)
{
	glob_t files;
	memset(&files, 0, sizeof(files));
	bool have_files = false;
	char dir[MAX_PATH + 1];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS,
	    false) != NULL) {
		char pattern[MAX_PATH + 32];
		snprintf(pattern, sizeof(pattern), "%sauto/picker/*.wren", dir);
		have_files = glob(pattern, 0, NULL, &files) == 0;
	}

	for (const struct embedded_script *script = EMBEDDED_SCRIPTS;
	    script->name != NULL; script++) {
		if (script->event == NULL || strcmp(script->event, "picker") != 0)
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
		if (!overridden)
			(void)wrenInterpret(picker_state.vm, script->name,
			    script->source);
	}
	if (have_files) {
		for (size_t i = 0; i < files.gl_pathc; i++) {
			char name[256];
			module_name(files.gl_pathv[i], name, sizeof(name));
			if (wrenHasModule(picker_state.vm, name))
				continue;
			char *source = read_script_file(files.gl_pathv[i]);
			if (source != NULL)
				(void)wrenInterpret(picker_state.vm, name, source);
			else
				fprintf(stderr, "[wren-picker] cannot read %s\n",
				    files.gl_pathv[i]);
			free(source);
		}
		globfree(&files);
	}
}

bool
wren_picker_host_init(void)
{
	if (picker_active)
		return true;
	memset(&picker_state, 0, sizeof(picker_state));
	picker_state.input_epoch = 1;

	WrenConfiguration config;
	wrenInitConfiguration(&config);
	config.writeFn = write_output;
	config.errorFn = report_error;
	config.bindForeignMethodFn = bind_foreign_method;
	config.bindForeignClassFn = bind_foreign_class;
	config.loadModuleFn = load_module;

	struct wren_host_state *old = wren_host_select_state(&picker_state);
	picker_state.vm = wrenNewVM(&config);
	const struct embedded_script *bootstrap =
	    embedded_module("picker_bootstrap", NULL);
	if (picker_state.vm != NULL && bootstrap != NULL &&
	    wrenInterpret(picker_state.vm, bootstrap->name,
	    bootstrap->source) == WREN_RESULT_SUCCESS) {
		picker_active = true;
		load_event_scripts();
	}
	if (!picker_active && picker_state.vm != NULL) {
		wren_log_shutdown();
		wrenFreeVM(picker_state.vm);
		picker_state.vm = NULL;
	}
	wren_host_select_state(old);
	return picker_active;
}

void
wren_picker_host_shutdown(void)
{
	if (!picker_active)
		return;
	struct wren_host_state *old = wren_host_select_state(&picker_state);
	wren_log_shutdown();
	wrenFreeVM(picker_state.vm);
	wren_host_select_state(old == &picker_state ? NULL : old);
	memset(&picker_state, 0, sizeof(picker_state));
	picker_active = false;
}

bool
wren_picker_host_active(void)
{
	return picker_active;
}
