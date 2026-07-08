#ifndef INC_1OOM_PBX_H
#define INC_1OOM_PBX_H

#include "types.h"

#define PBX_HEADER_LEN 16
#define PBX_MAGIC  "1oomPBX "
#define PBX_OFFS_VERSION   8
#define PBX_OFFS_ITEMS     12
#define PBX_VERSION   1

#define PBX_ITEM_HEADER_LEN 32
#define PBX_OFFS_ITEM_LEN   0
#define PBX_OFFS_ITEM_TYPE  4
#define PBX_OFFS_ITEM_INDEX 6
#define PBX_OFFS_ITEM_ID    8
#define PBX_OFFS_ITEM_OFFS  PBX_ITEM_HEADER_LEN
#define PBX_ITEM_ID_LEN (PBX_ITEM_HEADER_LEN - PBX_OFFS_ITEM_ID)

typedef enum {
    PBX_ITEM_TYPE_NAME = 0,
    PBX_ITEM_TYPE_DESC,
    PBX_ITEM_TYPE_LBXP,
    PBX_ITEM_TYPE_STRP,
    PBX_ITEM_TYPE_NUMP,
    PBX_ITEM_TYPE_LBXO
} pbx_item_type_t;

struct pbx_add_cbs {  /* int callbacks return 1 if data is not to be freed */
    int (*name)(void *ctx, const char *filename, int pbxi, uint8_t *str, uint32_t len);
    int (*desc)(void *ctx, const char *filename, int pbxi, uint8_t *str, uint32_t len);
    int (*lbxp)(void *ctx, const char *filename, int pbxi, const char *id, uint16_t itemi, uint8_t *data, uint32_t len);
    bool (*strp)(void *ctx, const char *filename, int pbxi, const char *id, const uint8_t *patchstr, int itemi, uint32_t len);
    bool (*nump)(void *ctx, const char *filename, int pbxi, const char *id, const int32_t *patchnums, int first, int num);
    int (*lbxo)(void *ctx, const char *filename, int pbxi, const char *id, uint16_t itemi, uint8_t *data, uint32_t len, uint32_t itemoffs);
};

extern int pbx_add_file(const char *filename, struct pbx_add_cbs *cbs, void *ctx);
extern int pbx_queue_file(const char *filename);
extern int pbx_apply_queued_files(void);

#endif
