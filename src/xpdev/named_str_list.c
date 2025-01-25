#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "named_str_list.h"

named_string_t *
namedStrListInsert(named_string_t ***list, const char *name, const char *value, size_t index)
{
	size_t count;
	named_string_t **newlist;

	COUNT_LIST_ITEMS((*list), count);
	if (count == NAMED_STR_LIST_LAST_INDEX)
		return NULL;
	if (index == NAMED_STR_LIST_LAST_INDEX)
		index = count;
	if (index > count)
		index = count;
	newlist = (named_string_t **)realloc(*list, (count + 2) * sizeof(named_string_t*));
	if (newlist == NULL)
		return NULL;
	*list = newlist;
	memmove(&(*list)[index + 1], &(*list)[index], (count - index + 1) * sizeof(named_string_t*));
	(*list)[index] = malloc(sizeof(named_string_t));
	// TODO: If malloc() failed we truncated the list...
	if ((*list)[index]) {
		(*list)[index]->name = strdup(name);
		(*list)[index]->value = strdup(value);
	}
	return (*list)[count];
}

bool
namedStrListDelete(named_string_t ***list, size_t index)
{
	size_t count;
	named_string_t *old;
	named_string_t **newlist;

	COUNT_LIST_ITEMS(*list, count);
	if (index == NAMED_STR_LIST_LAST_INDEX)
		index = count - 1;
	if (index >= count)
		return false;
	newlist = (named_string_t **)realloc(*list, (count + 1) * sizeof(named_string_t*));
	if (newlist != NULL)
		*list = newlist;
	old = (*list)[index];
	memmove(&(*list)[index], &(*list)[index + 1], (count - index) * sizeof(named_string_t*));
	free(old->name);
	free(old->value);
	free(old);

	return true;
}

named_string_t *
namedStrListFindName(named_string_t **list, const char *tmpn)
{
	size_t i;
	for (i = 0; list[i]; i++) {
		if (stricmp(tmpn, list[i]->name) == 0)
			return list[i];
	}
	return NULL;
}
