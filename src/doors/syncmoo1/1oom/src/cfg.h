#ifndef INC_1OOM_CFG_H
#define INC_1OOM_CFG_H

#include "types.h"

struct cfg_items_s {
    const char *name;
    void *var;
    bool (*check)(void *var);
    enum {
        CFG_TYPE_INT,
        CFG_TYPE_STR,
        CFG_TYPE_BOOL,
        CFG_TYPE_COMMENT
    } type;
};

#define CFG_ITEM_STR(_n_, _v_, _c_) { _n_, _v_, _c_, CFG_TYPE_STR }
#define CFG_ITEM_INT(_n_, _v_, _c_) { _n_, _v_, _c_, CFG_TYPE_INT }
#define CFG_ITEM_BOOL(_n_, _v_)     { _n_, _v_, 0, CFG_TYPE_BOOL }
#define CFG_ITEM_COMMENT(_s_)   { _s_, 0, 0, CFG_TYPE_COMMENT }
#define CFG_ITEM_END   { 0, 0, 0, CFG_TYPE_INT }

extern char *cfg_cfgname(void);

extern int cfg_load(const char *filename);
extern int cfg_save(const char *filename);

#endif
