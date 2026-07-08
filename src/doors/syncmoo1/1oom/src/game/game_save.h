#ifndef INC_1OOM_GAME_SAVE_H
#define INC_1OOM_GAME_SAVE_H

#include "types.h"

#define NUM_SAVES   6
#define NUM_ALL_SAVES   (NUM_SAVES + 1/*continue game*/ + 1/*undo*/ + 1/*init*/)
#define SAVE_NAME_LEN   20

#define GAME_SAVE_I_CONTINUE    (7 - 1)
#define GAME_SAVE_I_UNDO        (8 - 1)
#define GAME_SAVE_I_INIT        (9 - 1)

extern bool game_save_tbl_have_save[NUM_ALL_SAVES];
extern char game_save_tbl_name[NUM_ALL_SAVES][SAVE_NAME_LEN];

extern void *game_save_open_check_header(const char *filename, int i, bool update_table, char *savename, uint32_t *versionptr);
extern int game_save_get_slot_fname(char *fnamebuf, int buflen, int i);
extern int game_save_get_year_fname(char *fnamebuf, int buflen, int year);
extern int game_save_check_saves(char *fnamebuf, int buflen);

struct game_s;
extern int game_save_do_load_fname(const char *filename, char *savename, struct game_s *g);
extern int game_save_do_save_fname(const char *filename, const char *savename, const struct game_s *g, uint32_t version);
extern int game_save_do_load_i(int savei/*0..NUM_ALL_SAVES-1*/, struct game_s *g);
extern int game_save_do_save_i(int savei/*0..NUM_ALL_SAVES-1*/, const char *savename, const struct game_s *g);
extern int game_save_do_load_year(int year, char *savename, struct game_s *g);
extern int game_save_do_save_year(const char *savename, const struct game_s *g);

#endif
