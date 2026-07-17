#ifndef BBSLIST_MODEL_H
#define BBSLIST_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bbslist.h"

struct bbslist_model_record {
	struct bbslist *bbs;
	char persisted_name[LIST_NAME_MAX + 1];
	bool dirty;
	bool new_entry;
};

struct bbslist_model {
	struct bbslist **entries;
	struct bbslist_model_record *records;
	size_t count;
	struct bbslist defaults;
	uint64_t generation;
	bool loaded;
	bool defaults_dirty;
};

void bbslist_model_init(struct bbslist_model *model);
void bbslist_model_dispose(struct bbslist_model *model);
enum bbslist_read_status bbslist_model_load(struct bbslist_model *model,
    const char *password);

struct bbslist_model_record *bbslist_model_record(
    struct bbslist_model *model, struct bbslist *bbs);
bool bbslist_model_name_available(const struct bbslist_model *model,
    const char *name, const struct bbslist *except);
bool bbslist_model_user_name_available(const struct bbslist_model *model,
    const char *name);
struct bbslist *bbslist_model_create(struct bbslist_model *model,
    const char *name, const struct bbslist *source);
bool bbslist_model_rename(struct bbslist_model *model,
    struct bbslist *bbs, const char *name);
bool bbslist_model_save(struct bbslist_model *model, struct bbslist *bbs);
bool bbslist_model_delete(struct bbslist_model *model, struct bbslist *bbs);
void bbslist_model_mark_dirty(struct bbslist_model *model,
    struct bbslist *bbs);
void bbslist_model_sort(struct bbslist_model *model);

#endif
