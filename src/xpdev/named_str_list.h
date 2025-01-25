#ifndef NAMED_STR_LIST_H
#define NAMED_STR_LIST_H

#define NAMED_STR_LIST_LAST_INDEX     (-((size_t)(1)))

#ifdef __cplusplus
extern "C" {
#endif

named_string_t *namedStrListInsert(named_string_t ***list, const char *name, const char *value, size_t index);
bool namedStrListDelete(named_string_t ***list, size_t index);
named_string_t *namedStrListFindName(named_string_t **list, const char *tmpn);

#ifdef __cplusplus
}
#endif

#endif
