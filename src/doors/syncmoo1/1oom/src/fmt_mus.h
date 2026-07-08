#ifndef INC_1OOM_FMT_MUS_H
#define INC_1OOM_FMT_MUS_H

#include "types.h"

typedef enum {
    MUS_TYPE_UNKNOWN = 0,
    MUS_TYPE_LBXXMID,
    MUS_TYPE_MIDI,
    MUS_TYPE_WAV,
    MUS_TYPE_OGG,
    MUS_TYPE_FLAC
} mus_type_t;

extern mus_type_t fmt_mus_detect(const uint8_t *data, uint32_t len);
extern bool fmt_mus_convert_xmid(const uint8_t *data_in, uint32_t len_in, uint8_t **data_out_ptr, uint32_t *len_out_ptr, bool *tune_loops);

#endif
