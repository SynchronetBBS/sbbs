#ifndef _VARIOUS_H_
#define _VARIOUS_H_

#include "files.h"

extern const char *color[11];

struct player;

void Display_Menu(bool force, bool terse, bool *refresh, const char *name, const char *expert_prompt, void (*Meny)(void *), void *cbdata);
long level_raise(int level, long exp);
struct object *items(enum objtype type);
const char *immunity(int val);
void Reduce_Player_Resurrections(struct player *pl, bool typeinfo);
void decplayermoney(struct player *pl, long coins);
void incplayermoney(struct player *pl, long coins);
void Quick_Healing(struct player *pl);
void Healing(struct player *pl);
int HitChance(int skill);

#endif
