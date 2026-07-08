#ifndef INC_1OOM_FMT_SFX_H
#define INC_1OOM_FMT_SFX_H

#include "types.h"

typedef enum {
    SFX_TYPE_UNKNOWN = 0,
    SFX_TYPE_LBXVOC,
    SFX_TYPE_VOC,
    SFX_TYPE_WAV
} sfx_type_t;

extern sfx_type_t fmt_sfx_detect(const uint8_t *data, uint32_t len);
extern bool fmt_sfx_convert(const uint8_t *data_in, uint32_t len_in, uint8_t **data_out_ptr, uint32_t *len_out_ptr, sfx_type_t *type_out, int audiorate, bool add_wav_header);

#endif
