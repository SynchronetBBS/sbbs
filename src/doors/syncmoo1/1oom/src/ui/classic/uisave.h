#ifndef INC_1OOM_UISAVE_H
#define INC_1OOM_UISAVE_H

struct game_s;
/* returns -1 on cancel or 0..5 on save game */
extern int ui_save_game(struct game_s *g);

#endif
