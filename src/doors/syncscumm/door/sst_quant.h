/* sst_quant.h -- deterministic RGB888 -> 256-color median-cut quantizer
 * (pure C, no ScummVM). Used by the terminal graphics manager to present
 * the GUI overlay composite (Task 6), which -- unlike the game's native
 * CLUT8 path -- has no fixed palette of its own. */
#ifndef SYNCSCUMM_SST_QUANT_H_
#define SYNCSCUMM_SST_QUANT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Median-cut quantize RGB888 -> 256-color indexed. out_idx: w*h bytes,
 * out_pal768: 256 RGB triplets. Deterministic. Returns color count used. */
int sst_quant_rgb(const uint8_t *rgb, int w, int h,
                  uint8_t *out_idx, uint8_t *out_pal768);

#ifdef __cplusplus
}
#endif

#endif /* SYNCSCUMM_SST_QUANT_H_ */
