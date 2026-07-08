#ifndef INC_1OOM_FMT_PIC_H
#define INC_1OOM_FMT_PIC_H

#include "types.h"

typedef enum {
    PIC_TYPE_UNKNOWN = 0,
    PIC_TYPE_EQUALDEF,
    PIC_TYPE_PCX
} pic_type_t;

struct pic_s {
    pic_type_t type;
    int w, h, pitch, len;
    uint8_t *pix;
    uint8_t *pal;
    uint8_t *coded;
};

extern bool fmt_pic_load(const char *filename, struct pic_s *pic);
extern bool fmt_pic_save(const char *filename, struct pic_s *pic);
extern void fmt_pic_free(struct pic_s *pic);

#endif
