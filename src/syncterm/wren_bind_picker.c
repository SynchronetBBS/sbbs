#include "wren_bind_picker.h"

#include "wren_bind_internal.h"

#include "dirwrap.h"
#include "genwrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

struct wren_picker_request {
	enum syncterm_wren_foreign type;
	struct wren_picker_call *call;
	uint64_t generation;
};

struct picker_entry {
	char *name;
	bool directory;
	off_t size;
	time_t modified;
	char timestamp[24];
};

struct picker_entries {
	struct picker_entry *items;
	size_t count;
	size_t capacity;
};

static uint64_t request_generation;

static struct wren_picker_call *
request_call(WrenVM *vm)
{
	if (slot_foreign_type(vm, 0) != SWF_PICKER_REQUEST) {
		wren_throw(vm, "PickerRequest: invalid receiver");
		return NULL;
	}
	struct wren_picker_request *request = wrenGetSlotForeign(vm, 0);
	if (request == NULL || request->call == NULL ||
	    !request->call->active ||
	    request->generation != request->call->generation) {
		wren_throw(vm, "PickerRequest: picker call has ended");
		return NULL;
	}
	return request->call;
}

static bool
slot_path(WrenVM *vm, int slot, char *path, size_t size,
    const char *operation)
{
	if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		char error[128];
		snprintf(error, sizeof(error), "%s: expected a path String",
		    operation);
		wren_throw(vm, error);
		return false;
	}
	int length;
	const char *value = wrenGetSlotBytes(vm, slot, &length);
	if (length < 0 || (size_t)length >= size ||
	    memchr(value, 0, (size_t)length) != NULL) {
		char error[128];
		snprintf(error, sizeof(error),
		    "%s: path is too long or contains NUL", operation);
		wren_throw(vm, error);
		return false;
	}
	memcpy(path, value, (size_t)length);
	path[length] = '\0';
	return true;
}

static bool
normalize_directory(const char *source, char *dest, size_t size)
{
#ifdef _WIN32
	if (source == NULL || *source == '\0' ||
	    strcmp(source, "\\\\?\\") == 0) {
		return strlcpy(dest, "\\\\?\\", size) < size;
	}
	if (strncmp(source, "\\\\?\\", 4) == 0)
		source += 4;
#endif
	if (source == NULL || *source == '\0')
		source = ".";
	if (FULLPATH(dest, source, size) == NULL)
		return false;
#ifdef __unix__
	if (dest[0] == '\0') {
		if (size < 2)
			return false;
		strcpy(dest, "/");
	}
#endif
	if (strlen(dest) + 2 > size)
		return false;
	backslash(dest);
	return true;
}

static bool
directory_is_root(const char *path)
{
#ifdef __unix__
	return strcmp(path, "/") == 0;
#elif defined(_WIN32)
	return strcmp(path, "\\\\?\\") == 0;
#else
	(void)path;
	return false;
#endif
}

static void
entries_dispose(struct picker_entries *entries)
{
	for (size_t i = 0; i < entries->count; i++)
		free(entries->items[i].name);
	free(entries->items);
	memset(entries, 0, sizeof(*entries));
}

static bool
entries_contains(const struct picker_entries *entries, const char *name)
{
	for (size_t i = 0; i < entries->count; i++) {
		if (strcmp(entries->items[i].name, name) == 0)
			return true;
	}
	return false;
}

static bool
entries_add(struct picker_entries *entries, const char *name,
    bool directory, const char *full_path)
{
	if (name == NULL || *name == '\0' || entries_contains(entries, name))
		return true;
	if (entries->count == entries->capacity) {
		size_t capacity = entries->capacity == 0 ? 16 :
		    entries->capacity * 2;
		struct picker_entry *items = realloc(entries->items,
		    capacity * sizeof(*items));
		if (items == NULL)
			return false;
		entries->items = items;
		entries->capacity = capacity;
	}
	struct picker_entry *entry = &entries->items[entries->count];
	memset(entry, 0, sizeof(*entry));
	entry->name = strdup(name);
	if (entry->name == NULL)
		return false;
	entry->directory = directory;
	struct stat st;
	if (full_path != NULL && stat(full_path, &st) == 0) {
		entry->size = st.st_size;
		entry->modified = st.st_mtime;
		struct tm *tm = localtime(&st.st_mtime);
		if (tm != NULL)
			strftime(entry->timestamp, sizeof(entry->timestamp),
			    "%Y-%m-%d %H:%M", tm);
	}
	entries->count++;
	return true;
}

static int
compare_entries_ci(const void *left, const void *right)
{
	const struct picker_entry *a = left;
	const struct picker_entry *b = right;
	int result = stricmp(a->name, b->name);
	if (result != 0)
		return result;
	return strcmp(a->name, b->name);
}

static int
compare_entries_cs(const void *left, const void *right)
{
	const struct picker_entry *a = left;
	const struct picker_entry *b = right;
	return strcmp(a->name, b->name);
}

static const char *
bare_glob_name(const char *path, char *scratch, size_t size)
{
	strlcpy(scratch, path, size);
	size_t length = strlen(scratch);
	while (length > 0 && IS_PATH_DELIM(scratch[length - 1]))
		scratch[--length] = '\0';
	const char *name = getfname(scratch);
	return name == NULL ? scratch : name;
}

static int
picker_glob(bool case_sensitive, const char *pattern, int flags, glob_t *out)
{
	memset(out, 0, sizeof(*out));
	return case_sensitive ? glob(pattern, flags, NULL, out) :
	    globi(pattern, flags, NULL, out);
}

static bool
collect_pattern(struct picker_entries *entries, const char *directory,
    const char *mask, bool want_directories, bool case_sensitive)
{
	char pattern[MAX_PATH * 4 + 2];
	int length = snprintf(pattern, sizeof(pattern), "%s%s", directory,
	    mask);
	if (length < 0 || (size_t)length >= sizeof(pattern))
		return false;
	glob_t paths;
	int result = picker_glob(case_sensitive, pattern,
	    want_directories ? GLOB_MARK : 0, &paths);
	if (result != 0 && result != GLOB_NOMATCH)
		globfree(&paths);
	if (result != 0 && result != GLOB_NOMATCH)
		return false;
	bool success = true;
	for (size_t i = 0; i < paths.gl_pathc; i++) {
		bool directory_entry = isdir(paths.gl_pathv[i]);
		if (directory_entry != want_directories)
			continue;
		char scratch[MAX_PATH * 4 + 1];
		const char *name = bare_glob_name(paths.gl_pathv[i], scratch,
		    sizeof(scratch));
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
			continue;
		if (!entries_add(entries, name, directory_entry,
		    paths.gl_pathv[i])) {
			success = false;
			break;
		}
	}
	globfree(&paths);
	return success;
}

static bool
collect_entries(struct picker_entries *entries, const char *directory,
    const char *mask, bool want_directories, int options)
{
	bool case_sensitive = (options & PICKER_OPT_MASK_CASE) != 0;
	const char *pattern = want_directories ? "*" : mask;
	if (!collect_pattern(entries, directory, pattern, want_directories,
	    case_sensitive))
		return false;
	if (options & PICKER_OPT_SHOW_HIDDEN) {
		char hidden[MAX_PATH * 4 + 2];
		int length = snprintf(hidden, sizeof(hidden), ".%s", pattern);
		if (length < 0 || (size_t)length >= sizeof(hidden) ||
		    !collect_pattern(entries, directory, hidden, want_directories,
		    case_sensitive))
			return false;
	}
	if (entries->count > 1) {
		size_t first = strcmp(entries->items[0].name, "..") == 0 ? 1 : 0;
		qsort(entries->items + first, entries->count - first,
		    sizeof(*entries->items),
		    (options & PICKER_OPT_CASE_SORT) ? compare_entries_cs :
		    compare_entries_ci);
	}
	return true;
}

#ifdef _WIN32
static bool
collect_drives(struct picker_entries *entries)
{
	unsigned long drives = _getdrives();
	char path[] = "A:\\";
	for (int i = 0; i < 26; i++) {
		if ((drives & (1ul << i)) == 0)
			continue;
		path[0] = (char)('A' + i);
		if (!entries_add(entries, path, true, path))
			return false;
	}
	return true;
}
#endif

static void
push_entry_list(WrenVM *vm, int slot, const struct picker_entries *entries)
{
	wrenEnsureSlots(vm, slot + 3);
	wrenSetSlotNewList(vm, slot);
	for (size_t i = 0; i < entries->count; i++) {
		const struct picker_entry *entry = &entries->items[i];
		wrenSetSlotNewList(vm, slot + 1);
		wrenSetSlotString(vm, slot + 2, entry->name);
		wrenInsertInList(vm, slot + 1, -1, slot + 2);
		wrenSetSlotBool(vm, slot + 2, entry->directory);
		wrenInsertInList(vm, slot + 1, -1, slot + 2);
		wrenSetSlotDouble(vm, slot + 2, (double)entry->size);
		wrenInsertInList(vm, slot + 1, -1, slot + 2);
		wrenSetSlotDouble(vm, slot + 2, (double)entry->modified);
		wrenInsertInList(vm, slot + 1, -1, slot + 2);
		wrenSetSlotString(vm, slot + 2, entry->timestamp);
		wrenInsertInList(vm, slot + 1, -1, slot + 2);
		wrenInsertInList(vm, slot, -1, slot + 1);
	}
}

static void
request_allocate(WrenVM *vm)
{
	struct wren_picker_request *request = wrenSetSlotNewForeign(vm, 0, 0,
	    sizeof(*request));
	request->type = SWF_PICKER_REQUEST;
	request->call = NULL;
	request->generation = 0;
}

static void
request_finalize(void *data)
{
	struct wren_picker_request *request = data;
	request->call = NULL;
}

bool
wren_picker_bind_push_request(WrenVM *vm, int slot,
    struct wren_picker_call *call)
{
	if (vm == NULL || call == NULL || slot < 0)
		return false;
	call->generation = ++request_generation;
	call->active = true;
	wrenEnsureSlots(vm, slot + 2);
	wrenGetVariable(vm, "syncterm_picker", "PickerRequest", slot + 1);
	struct wren_picker_request *request = wrenSetSlotNewForeign(vm, slot,
	    slot + 1, sizeof(*request));
	request->type = SWF_PICKER_REQUEST;
	request->call = call;
	request->generation = call->generation;
	return true;
}

void
wren_picker_call_dispose(struct wren_picker_call *call)
{
	if (call == NULL)
		return;
	call->active = false;
	for (size_t i = 0; i < call->path_count; i++)
		free(call->paths[i]);
	free(call->paths);
	call->paths = NULL;
	call->path_count = 0;
}

static void fn_request_mode(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	static const char *const names[] = { "file", "directory", "multiple", "save" };
	wrenSetSlotString(vm, 0, names[call->mode]);
}

#define REQUEST_STRING_GETTER(name, field) \
	static void fn_request_##name(WrenVM *vm) \
	{ \
		struct wren_picker_call *call = request_call(vm); \
		if (call != NULL) \
			wrenSetSlotString(vm, 0, call->field != NULL ? call->field : ""); \
	}

REQUEST_STRING_GETTER(title, title)
REQUEST_STRING_GETTER(initial_path, initial_path)
REQUEST_STRING_GETTER(mask, mask)

static void fn_request_options(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call != NULL)
		wrenSetSlotDouble(vm, 0, (double)call->options);
}

static void
fn_request_initial(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	char path[MAX_PATH * 4 + 1];
	char selected[MAX_PATH + 1] = "";
	const char *initial = call->initial_path;
	if (initial == NULL || *initial == '\0')
		initial = ".";
	if (!isdir(initial) && fexist(initial)) {
		char full[MAX_PATH * 4 + 1];
		char drive[MAX_PATH + 1], dir[MAX_PATH * 4 + 1];
		char name[MAX_PATH + 1], ext[MAX_PATH + 1];
		if (FULLPATH(full, initial, sizeof(full)) == NULL) {
			wrenSetSlotNull(vm, 0);
			return;
		}
		_splitpath(full, drive, dir, name, ext);
		snprintf(path, sizeof(path), "%s%s", drive, dir);
		snprintf(selected, sizeof(selected), "%s%s", name, ext);
	} else {
		strlcpy(path, initial, sizeof(path));
	}
	char normalized[MAX_PATH * 4 + 1];
	if (!normalize_directory(path, normalized, sizeof(normalized))) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotString(vm, 1, normalized);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotString(vm, 1, selected);
	wrenInsertInList(vm, 0, -1, 1);
}

static void
fn_request_list(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	char input[MAX_PATH * 4 + 1], mask[MAX_PATH * 4 + 1];
	if (!slot_path(vm, 1, input, sizeof(input), "PickerRequest.list") ||
	    !slot_path(vm, 2, mask, sizeof(mask), "PickerRequest.list"))
		return;
	if (mask[0] == '\0')
		strcpy(mask, "*");
	char directory[MAX_PATH * 4 + 1];
	if (!normalize_directory(input, directory, sizeof(directory))) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	struct picker_entries dirs = { 0 }, files = { 0 };
	bool root = directory_is_root(directory);
	bool success = true;
#ifdef _WIN32
	if (root)
		success = collect_drives(&dirs);
	else
#endif
	{
		DIR *probe = opendir(directory);
		if (probe == NULL)
			success = false;
		else
			closedir(probe);
		if (success && !root)
			success = entries_add(&dirs, "..", true, NULL);
		if (success)
			success = collect_entries(&dirs, directory, "*", true,
			    call->options);
		if (success)
			success = collect_entries(&files, directory, mask, false,
			    call->options);
	}
	if (!success) {
		entries_dispose(&dirs);
		entries_dispose(&files);
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewList(vm, 0);
	wrenSetSlotString(vm, 1, directory);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotBool(vm, 1, root);
	wrenInsertInList(vm, 0, -1, 1);
	push_entry_list(vm, 1, &dirs);
	wrenInsertInList(vm, 0, -1, 1);
	push_entry_list(vm, 1, &files);
	wrenInsertInList(vm, 0, -1, 1);
	entries_dispose(&dirs);
	entries_dispose(&files);
}

static void
fn_request_join(WrenVM *vm)
{
	if (request_call(vm) == NULL)
		return;
	char base[MAX_PATH * 4 + 1], name[MAX_PATH * 4 + 1];
	if (!slot_path(vm, 1, base, sizeof(base), "PickerRequest.join") ||
	    !slot_path(vm, 2, name, sizeof(name), "PickerRequest.join"))
		return;
#ifdef _WIN32
	if (strcmp(base, "\\\\?\\") == 0) {
		wrenSetSlotString(vm, 0, name);
		return;
	}
#endif
	char combined[MAX_PATH * 8 + 1];
	int length = snprintf(combined, sizeof(combined), "%s%s", base, name);
	if (length < 0 || (size_t)length >= sizeof(combined)) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	char full[MAX_PATH * 4 + 1];
	if (FULLPATH(full, combined, sizeof(full)) == NULL) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	wrenSetSlotString(vm, 0, full);
}

static void
fn_request_resolve(WrenVM *vm)
{
	if (request_call(vm) == NULL)
		return;
	char base[MAX_PATH * 4 + 1], text[MAX_PATH * 4 + 1];
	if (!slot_path(vm, 1, base, sizeof(base), "PickerRequest.resolve") ||
	    !slot_path(vm, 2, text, sizeof(text), "PickerRequest.resolve"))
		return;
	char combined[MAX_PATH * 8 + 1];
	if (isfullpath(text))
		strlcpy(combined, text, sizeof(combined));
	else {
		int length = snprintf(combined, sizeof(combined), "%s%s", base,
		    text);
		if (length < 0 || (size_t)length >= sizeof(combined)) {
			wrenSetSlotNull(vm, 0);
			return;
		}
	}
	char full[MAX_PATH * 4 + 1];
	if (FULLPATH(full, combined, sizeof(full)) == NULL)
		strlcpy(full, combined, sizeof(full));
	char drive[MAX_PATH + 1], dir[MAX_PATH * 4 + 1];
	char name[MAX_PATH + 1], ext[MAX_PATH + 1];
	_splitpath(full, drive, dir, name, ext);
	char directory[MAX_PATH * 4 + 1];
	snprintf(directory, sizeof(directory), "%s%s", drive, dir);
	char normalized[MAX_PATH * 4 + 1];
	if (!normalize_directory(directory, normalized, sizeof(normalized))) {
		wrenSetSlotNull(vm, 0);
		return;
	}
	strlcpy(directory, normalized, sizeof(directory));
	char basename[MAX_PATH * 2 + 1];
	snprintf(basename, sizeof(basename), "%s%s", name, ext);
	bool wild = strchr(basename, '*') != NULL ||
	    strchr(basename, '?') != NULL;
	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);
#define INSERT_VALUE(statement) do { statement; wrenInsertInList(vm, 0, -1, 1); } while (0)
	INSERT_VALUE(wrenSetSlotString(vm, 1, full));
	INSERT_VALUE(wrenSetSlotString(vm, 1, directory));
	INSERT_VALUE(wrenSetSlotString(vm, 1, basename));
	INSERT_VALUE(wrenSetSlotBool(vm, 1, fexist(full)));
	INSERT_VALUE(wrenSetSlotBool(vm, 1, isdir(full)));
	INSERT_VALUE(wrenSetSlotBool(vm, 1, wild));
#undef INSERT_VALUE
}

static bool
store_paths(WrenVM *vm, struct wren_picker_call *call, int slot,
    bool multiple)
{
	if (call->completed) {
		wren_throw(vm, "PickerRequest: result is already complete");
		return false;
	}
	size_t count = 1;
	if (multiple) {
		if (wrenGetSlotType(vm, slot) != WREN_TYPE_LIST) {
			wren_throw(vm, "PickerRequest.acceptAll: expected a List");
			return false;
		}
		count = (size_t)wrenGetListCount(vm, slot);
		if (count == 0) {
			wren_throw(vm, "PickerRequest.acceptAll: list is empty");
			return false;
		}
	} else if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) {
		wren_throw(vm, "PickerRequest: expected a path String");
		return false;
	}
	char **paths = calloc(count, sizeof(*paths));
	if (paths == NULL) {
		wren_throw(vm, "PickerRequest: out of memory");
		return false;
	}
	wrenEnsureSlots(vm, slot + 2);
	for (size_t i = 0; i < count; i++) {
		int source = slot;
		if (multiple) {
			source = slot + 1;
			wrenGetListElement(vm, slot, (int)i, source);
		}
		char input[MAX_PATH + 1], full[MAX_PATH + 1];
		if (!slot_path(vm, source, input, sizeof(input),
		    "PickerRequest.accept")) {
			for (size_t j = 0; j < i; j++)
				free(paths[j]);
			free(paths);
			return false;
		}
		if (FULLPATH(full, input, sizeof(full)) == NULL ||
		    !isfullpath(full)) {
			for (size_t j = 0; j < i; j++)
				free(paths[j]);
			free(paths);
			wren_throw(vm, "PickerRequest: invalid absolute path");
			return false;
		}
		paths[i] = strdup(full);
		if (paths[i] == NULL) {
			for (size_t j = 0; j < i; j++)
				free(paths[j]);
			free(paths);
			wren_throw(vm, "PickerRequest: out of memory");
			return false;
		}
	}
	call->paths = paths;
	call->path_count = count;
	call->completed = true;
	return true;
}

static void
accept_one(WrenVM *vm, enum wren_picker_disposition disposition,
    enum wren_picker_mode required)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	if (required != call->mode) {
		wren_throw(vm, "PickerRequest: completion does not match picker mode");
		return;
	}
	if (store_paths(vm, call, 1, false))
		call->disposition = disposition;
	wrenSetSlotNull(vm, 0);
}

static void fn_request_accept(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	if (call->mode != WREN_PICKER_FILE &&
	    call->mode != WREN_PICKER_DIRECTORY) {
		wren_throw(vm, "PickerRequest.accept: picker is not single-select");
		return;
	}
	if (store_paths(vm, call, 1, false))
		call->disposition = WREN_PICKER_DISPOSITION_READ;
	wrenSetSlotNull(vm, 0);
}

static void fn_request_accept_all(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	if (call->mode != WREN_PICKER_MULTIPLE) {
		wren_throw(vm, "PickerRequest.acceptAll: picker is not multi-select");
		return;
	}
	if (store_paths(vm, call, 1, true))
		call->disposition = WREN_PICKER_DISPOSITION_READ;
	wrenSetSlotNull(vm, 0);
}

static void fn_request_accept_create(WrenVM *vm)
{
	accept_one(vm, WREN_PICKER_DISPOSITION_CREATE, WREN_PICKER_SAVE);
}

static void fn_request_accept_overwrite(WrenVM *vm)
{
	accept_one(vm, WREN_PICKER_DISPOSITION_OVERWRITE, WREN_PICKER_SAVE);
}

static void fn_request_cancel(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	if (!call->completed)
		call->completed = true;
	wrenSetSlotNull(vm, 0);
}

static void fn_request_quit_application(WrenVM *vm)
{
	struct wren_picker_call *call = request_call(vm);
	if (call == NULL)
		return;
	call->quit_application = true;
	if (!call->completed)
		call->completed = true;
	wrenSetSlotNull(vm, 0);
}

struct binding {
	const char *class_name;
	bool is_static;
	const char *signature;
	WrenForeignMethodFn method;
};

static const struct binding bindings[] = {
	{ "PickerRequest", false, "mode", fn_request_mode },
	{ "PickerRequest", false, "title", fn_request_title },
	{ "PickerRequest", false, "initialPath", fn_request_initial_path },
	{ "PickerRequest", false, "mask", fn_request_mask },
	{ "PickerRequest", false, "options", fn_request_options },
	{ "PickerRequest", false, "initial_()", fn_request_initial },
	{ "PickerRequest", false, "list_(_,_)", fn_request_list },
	{ "PickerRequest", false, "join_(_,_)", fn_request_join },
	{ "PickerRequest", false, "resolve_(_,_)", fn_request_resolve },
	{ "PickerRequest", false, "accept(_)", fn_request_accept },
	{ "PickerRequest", false, "acceptAll(_)", fn_request_accept_all },
	{ "PickerRequest", false, "acceptCreate(_)", fn_request_accept_create },
	{ "PickerRequest", false, "acceptOverwrite(_)", fn_request_accept_overwrite },
	{ "PickerRequest", false, "cancel()", fn_request_cancel },
	{ "PickerRequest", false, "quitApplication()", fn_request_quit_application },
};

WrenForeignMethodFn
wren_picker_bind_lookup(const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	if (module == NULL || strcmp(module, "syncterm_picker") != 0)
		return NULL;
	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		if (bindings[i].is_static == is_static &&
		    strcmp(bindings[i].class_name, class_name) == 0 &&
		    strcmp(bindings[i].signature, signature) == 0)
			return bindings[i].method;
	}
	return NULL;
}

WrenForeignClassMethods
wren_picker_bind_lookup_class(const char *module, const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	if (module == NULL || strcmp(module, "syncterm_picker") != 0 ||
	    strcmp(class_name, "PickerRequest") != 0)
		return none;
	WrenForeignClassMethods methods = { request_allocate, request_finalize };
	return methods;
}
