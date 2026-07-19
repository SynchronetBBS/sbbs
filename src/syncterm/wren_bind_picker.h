#ifndef WREN_BIND_PICKER_H
#define WREN_BIND_PICKER_H

#include "wren.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PICKER_OPT_MASK_LOCKED        (1 << 0)
#define PICKER_OPT_MASK_CASE          (1 << 1)
#define PICKER_OPT_CASE_SORT          (1 << 2)
#define PICKER_OPT_SELECT_DIRECTORY   (1 << 3)
#define PICKER_OPT_PATH_MUST_EXIST    (1 << 4)
#define PICKER_OPT_FILE_MUST_EXIST    (1 << 5)
#define PICKER_OPT_SHOW_HIDDEN        (1 << 6)
#define PICKER_OPT_ALLOW_ENTRY        (1 << 8)
#define PICKER_OPT_CONFIRM_OVERWRITE  (1 << 9)
#define PICKER_OPT_CONFIRM_CREATE     (1 << 10)

enum wren_picker_mode {
	WREN_PICKER_FILE,
	WREN_PICKER_DIRECTORY,
	WREN_PICKER_MULTIPLE,
	WREN_PICKER_SAVE,
};

enum wren_picker_disposition {
	WREN_PICKER_DISPOSITION_NONE,
	WREN_PICKER_DISPOSITION_READ,
	WREN_PICKER_DISPOSITION_CREATE,
	WREN_PICKER_DISPOSITION_OVERWRITE,
};

struct wren_picker_call {
	uint64_t generation;
	bool active;
	bool completed;
	bool quit_application;
	enum wren_picker_mode mode;
	const char *title;
	const char *initial_path;
	const char *mask;
	int options;
	unsigned colors[6];
	char **paths;
	size_t path_count;
	enum wren_picker_disposition disposition;
};

WrenForeignMethodFn wren_picker_bind_lookup(const char *module,
    const char *class_name, bool is_static, const char *signature);
WrenForeignClassMethods wren_picker_bind_lookup_class(const char *module,
    const char *class_name);

bool wren_picker_bind_push_request(WrenVM *vm, int slot,
    struct wren_picker_call *call);
void wren_picker_call_dispose(struct wren_picker_call *call);

#endif
