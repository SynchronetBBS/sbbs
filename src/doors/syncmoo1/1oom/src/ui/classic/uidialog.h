#ifndef INC_1OOM_UIDIALOG_H
#define INC_1OOM_UIDIALOG_H

struct game_s;

extern int ui_dialog_yesno(struct game_s *g, int pi, const char *str, int x, int y, int we);
extern int ui_dialog_choose(struct game_s *g, int pi, const char *str, int x, int y, int we);

#endif
