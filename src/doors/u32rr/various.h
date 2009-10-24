#ifndef _VARIOUS_H_
#define _VARIOUS_H_

extern const char *color[11];

void Display_Menu(bool force, bool terse, bool *refresh, const char *name, const char *expert_prompt, void (*Meny)(void *), void *cbdata);
long level_raise(int level, long exp);
struct object *items(enum objtype type);
const char *immunity(int val);

#endif
