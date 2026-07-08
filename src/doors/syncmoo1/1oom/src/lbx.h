#ifndef INC_1OOM_LBX_H
#define INC_1OOM_LBX_H

#include "types.h"

#define LBX_TYPE_GFX    0
#define LBX_TYPE_SOUND  1
#define LBX_TYPE_FONTS  2
#define LBX_TYPE_HELP   3
#define LBX_TYPE_DATA   5

struct lbx_s;

typedef enum {
    LBXFILE_BACKGRND,
    LBXFILE_COLONIES,
    LBXFILE_COUNCIL,
    LBXFILE_DESIGN,
    LBXFILE_DIPLOMAT,
    LBXFILE_EMBASSY,
    LBXFILE_EVENTMSG,
    LBXFILE_FIRING,
    LBXFILE_FONTS,
    LBXFILE_HELP,
    LBXFILE_INTRO,
    LBXFILE_INTRO2,
    LBXFILE_INTROSND,
    LBXFILE_LANDING,
    LBXFILE_MISSILE,
    LBXFILE_MUSIC,
    LBXFILE_NAMES,
    LBXFILE_NEBULA,
    LBXFILE_NEWSCAST,
    LBXFILE_PLANETS,
    LBXFILE_RESEARCH,
    LBXFILE_SCREENS,
    LBXFILE_SHIPS,
    LBXFILE_SHIPS2,
    LBXFILE_SOUNDFX,
    LBXFILE_SPACE,
    LBXFILE_SPIES,
    LBXFILE_STARMAP,
    LBXFILE_STARVIEW,
    LBXFILE_TECHNO,
    LBXFILE_V11,
    LBXFILE_VORTEX,
    LBXFILE_WINLOSE,
    LBXFILE_NUM
} lbxfile_e;

extern int lbxfile_init(void);
extern void lbxfile_shutdown(void);
extern int lbxfile_find_dir(void);

extern uint8_t *lbxfile_item_get(lbxfile_e file_id, uint16_t entry_id);
extern uint8_t *lbxfile_item_get_with_len(lbxfile_e file_id, uint16_t entry_id, uint32_t *len_ptr);
extern void lbxfile_item_release(lbxfile_e file_id, uint8_t *ptr);
extern void lbxfile_item_release_file(lbxfile_e file_id);

extern bool lbxfile_exists(lbxfile_e file_id);
extern const char *lbxfile_name(lbxfile_e file_id);
extern lbxfile_e lbxfile_id(const char *filename);
extern int lbxfile_type(lbxfile_e file_id);
extern int lbxfile_num_items(lbxfile_e file_id);
extern const char *lbxfile_item_name(lbxfile_e file_id, uint16_t entry_id);
extern int lbxfile_item_len(lbxfile_e file_id, uint16_t entry_id);
extern int lbxfile_item_offs(lbxfile_e file_id, uint16_t entry_id);

extern void lbxfile_add_patch(lbxfile_e file_id, uint16_t i, uint8_t *data, uint32_t len, const char *patchfilename);
extern void lbxfile_add_overwrite(lbxfile_e file_id, uint16_t i, uint32_t itemoffs, uint8_t *data, uint32_t len, const char *patchfilename);

#endif
