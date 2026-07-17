#include "bbslist_model.h"

#include "dirwrap.h"
#include "genwrap.h"
#include "syncterm.h"

#include <stdlib.h>
#include <string.h>

static void
free_contents(struct bbslist_model *model)
{
	if (model->entries != NULL)
		free_list(model->entries, (int)model->count);
	free(model->entries);
	free(model->records);
	model->entries = NULL;
	model->records = NULL;
	model->count = 0;
	model->loaded = false;
}

void
bbslist_model_init(struct bbslist_model *model)
{
	memset(model, 0, sizeof(*model));
	model->generation = 1;
}

void
bbslist_model_dispose(struct bbslist_model *model)
{
	free_contents(model);
	memset(&model->defaults, 0, sizeof(model->defaults));
	model->generation++;
}

static enum bbslist_read_status
load_path(struct bbslist_model *model, const char *path,
    struct bbslist *defaults, int type, const char *password)
{
	enum bbslist_read_status status = BBSLIST_READ_OK;
	int count = (int)model->count;
	if (!read_list_password(path, model->entries, defaults, &count, type,
	    password, &status))
		return status;
	model->count = (size_t)count;
	return BBSLIST_READ_OK;
}

static enum bbslist_read_status
load_contents(struct bbslist_model *model, const char *password)
{
	model->entries = calloc(BBSLIST_MAX_ENTRIES + 1,
	    sizeof(*model->entries));
	if (model->entries == NULL)
		return BBSLIST_READ_FAILED;

	enum bbslist_read_status status = load_path(model, settings.list_path,
	    &model->defaults, USER_BBSLIST, password);
	if (status != BBSLIST_READ_OK)
		return status;

	char shared[MAX_PATH + 1];
	if (get_syncterm_filename(shared, sizeof(shared), SYNCTERM_PATH_LIST,
	    true) != NULL && stricmp(shared, settings.list_path) != 0) {
		status = load_path(model, shared, NULL, SYSTEM_BBSLIST, NULL);
		if (status != BBSLIST_READ_OK)
			return status;
	}

	if (settings.webgets != NULL) {
		char cache[MAX_PATH + 1];
		if (get_syncterm_filename(cache, sizeof(cache),
		    SYNCTERM_PATH_SYSTEM_CACHE, false) != NULL) {
			backslash(cache);
			for (size_t i = 0; settings.webgets[i] != NULL; i++) {
				char path[MAX_PATH + 1];
				int len = snprintf(path, sizeof(path), "%s%s.lst",
				    cache, settings.webgets[i]->name);
				if (len < 0 || (size_t)len >= sizeof(path))
					continue;
				status = load_path(model, path, NULL,
				    SYSTEM_BBSLIST, NULL);
				if (status != BBSLIST_READ_OK)
					return status;
			}
		}
	}

	int count = (int)model->count;
	sort_bbs_list(model->entries, &count);
	model->records = calloc(model->count, sizeof(*model->records));
	if (model->count != 0 && model->records == NULL)
		return BBSLIST_READ_FAILED;
	for (size_t i = 0; i < model->count; i++) {
		model->records[i].bbs = model->entries[i];
		if (model->entries[i]->type == USER_BBSLIST) {
			strlcpy(model->records[i].persisted_name,
			    model->entries[i]->name,
			    sizeof(model->records[i].persisted_name));
		}
	}
	model->loaded = true;
	return BBSLIST_READ_OK;
}

enum bbslist_read_status
bbslist_model_load(struct bbslist_model *model, const char *password)
{
	struct bbslist_model replacement;
	bbslist_model_init(&replacement);
	enum bbslist_read_status status = load_contents(&replacement,
	    password);
	if (status != BBSLIST_READ_OK) {
		free_contents(&replacement);
		return status;
	}

	uint64_t next_generation = model->generation + 1;
	free_contents(model);
	*model = replacement;
	model->generation = next_generation;
	return BBSLIST_READ_OK;
}

struct bbslist_model_record *
bbslist_model_record(struct bbslist_model *model, struct bbslist *bbs)
{
	for (size_t i = 0; i < model->count; i++) {
		if (model->records[i].bbs == bbs)
			return &model->records[i];
	}
	return NULL;
}

bool
bbslist_model_name_available(const struct bbslist_model *model,
    const char *name, const struct bbslist *except)
{
	if (name == NULL || name[0] == 0 || strlen(name) > LIST_NAME_MAX ||
	    stricmp(name, "syncterm-system-cache") == 0)
		return false;
	for (size_t i = 0; i < model->count; i++) {
		if (model->entries[i] != except &&
		    stricmp(model->entries[i]->name, name) == 0)
			return false;
	}
	return true;
}

bool
bbslist_model_user_name_available(const struct bbslist_model *model,
    const char *name)
{
	if (name == NULL || name[0] == 0 || strlen(name) > LIST_NAME_MAX ||
	    stricmp(name, "syncterm-system-cache") == 0)
		return false;
	for (size_t i = 0; i < model->count; i++) {
		if (model->entries[i]->type == USER_BBSLIST &&
		    stricmp(model->entries[i]->name, name) == 0)
			return false;
	}
	return true;
}

static bool
append_record(struct bbslist_model *model, struct bbslist *bbs)
{
	struct bbslist_model_record *records = realloc(model->records,
	    (model->count + 1) * sizeof(*records));
	if (records == NULL)
		return false;
	model->records = records;
	model->records[model->count] = (struct bbslist_model_record) {
		.bbs = bbs,
		.dirty = true,
		.new_entry = true,
	};
	model->entries[model->count++] = bbs;
	model->entries[model->count] = NULL;
	return true;
}

struct bbslist *
bbslist_model_create(struct bbslist_model *model, const char *name,
    const struct bbslist *source)
{
	if (!model->loaded || model->count >= BBSLIST_MAX_ENTRIES ||
	    !bbslist_model_user_name_available(model, name))
		return NULL;
	struct bbslist *bbs = malloc(sizeof(*bbs));
	if (bbs == NULL)
		return NULL;
	memcpy(bbs, source != NULL ? source : &model->defaults, sizeof(*bbs));
	strlcpy(bbs->name, name, sizeof(bbs->name));
	bbs->type = USER_BBSLIST;
	bbs->id = (int)model->count;
	bbs->added = time(NULL);
	bbs->connected = 0;
	bbs->calls = 0;
	if (!append_record(model, bbs)) {
		free(bbs);
		return NULL;
	}
	model->generation++;
	bbslist_model_sort(model);
	return bbs;
}

void
bbslist_model_mark_dirty(struct bbslist_model *model, struct bbslist *bbs)
{
	if (bbs == &model->defaults) {
		model->defaults_dirty = true;
		return;
	}
	struct bbslist_model_record *record = bbslist_model_record(model, bbs);
	if (record != NULL)
		record->dirty = true;
}

bool
bbslist_model_rename(struct bbslist_model *model, struct bbslist *bbs,
    const char *name)
{
	struct bbslist_model_record *record = bbslist_model_record(model, bbs);
	if (record == NULL || bbs->type != USER_BBSLIST ||
	    !bbslist_model_name_available(model, name, bbs))
		return false;
	strlcpy(bbs->name, name, sizeof(bbs->name));
	record->dirty = true;
	bbslist_model_sort(model);
	return true;
}

bool
bbslist_model_save(struct bbslist_model *model, struct bbslist *bbs)
{
	if (bbs == &model->defaults) {
		if (!model->defaults_dirty)
			return true;
		if (!save_bbs_defaults(settings.list_path, &model->defaults))
			return false;
		model->defaults_dirty = false;
		return true;
	}
	struct bbslist_model_record *record = bbslist_model_record(model, bbs);
	if (record == NULL || bbs->type != USER_BBSLIST)
		return false;
	if (!record->dirty)
		return true;
	bool success;
	if (record->persisted_name[0] != 0 &&
	    strcmp(record->persisted_name, bbs->name) != 0) {
		success = rename_bbs(settings.list_path, record->persisted_name,
		    bbs);
	}
	else
		success = add_bbs(settings.list_path, bbs, record->new_entry);
	if (!success)
		return false;
	strlcpy(record->persisted_name, bbs->name,
	    sizeof(record->persisted_name));
	record->dirty = false;
	record->new_entry = false;
	return true;
}

bool
bbslist_model_delete(struct bbslist_model *model, struct bbslist *bbs)
{
	struct bbslist_model_record *record = bbslist_model_record(model, bbs);
	if (record == NULL || bbs->type != USER_BBSLIST)
		return false;
	if (record->persisted_name[0] != 0) {
		struct bbslist persisted = *bbs;
		strlcpy(persisted.name, record->persisted_name,
		    sizeof(persisted.name));
		if (!delete_bbs(settings.list_path, &persisted))
			return false;
	}
	size_t index = (size_t)(record - model->records);
	for (size_t i = 0; i < model->count; i++) {
		if (model->entries[i] == bbs) {
			memmove(&model->entries[i], &model->entries[i + 1],
			    (model->count - i) * sizeof(*model->entries));
			break;
		}
	}
	free(bbs);
	memmove(&model->records[index], &model->records[index + 1],
	    (model->count - index - 1) * sizeof(*model->records));
	model->count--;
	model->generation++;
	return true;
}

void
bbslist_model_sort(struct bbslist_model *model)
{
	int count = (int)model->count;
	sort_bbs_list(model->entries, &count);
}
