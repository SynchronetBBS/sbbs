#ifndef INC_1OOM_BOOLVEC_H
#define INC_1OOM_BOOLVEC_H

#include <string.h>

#include "types.h"

#define BOOLVEC_DECLARE(_name_, _fnum_) uint8_t _name_[((_fnum_) + 7) / 8]
#define BOOLVEC_CLEAR(_name_, _fnum_) do { if ((_fnum_) <= 8) { (_name_)[0] = 0; } else { memset(&(_name_), 0, sizeof(_name_)); } } while (0)
#define BOOLVEC_COPY(_named_, _names_, _fnum_) do { if ((_fnum_) <= 8) { (_named_)[0] = (_names_)[0]; } else { memcpy(&(_named_), &(_names_), sizeof(_named_)); } } while (0)
#define BOOLVEC_COMP(_name1_, _name2_, _fnum_) (((_fnum_) <= 8) ? ((_name1_)[0] == (_name2_)[0]) : (memcmp((_name1_), (_name2_), ((_fnum_) + 7) / 8) == 0))
#define BOOLVEC_PTRPARAMI(_name_) uint8_t *_name_
#define BOOLVEC_SET0(_name_, _i_) do { int ti = (_i_); _name_[ti / 8] &= ~(1 << (ti & 7)); } while (0)
#define BOOLVEC_SET1(_name_, _i_) do { int ti = (_i_); _name_[ti / 8] |=  (1 << (ti & 7)); } while (0)
#define BOOLVEC_SET(_name_, _i_, _v_) do { int ti = (_i_); uint8_t *tp = &(_name_[ti / 8]); if ((_v_)) { *tp |= (1 << (ti & 7)); } else { *tp &= ~(1 << (ti & 7)); } } while (0)
#define BOOLVEC_IS0(_name_, _i_) ((_name_[(_i_) / 8] & (1 << ((_i_) & 7))) == 0)
#define BOOLVEC_IS1(_name_, _i_) ((_name_[(_i_) / 8] & (1 << ((_i_) & 7))) != 0)
#define BOOLVEC_IS_CLEAR(_name_, _fnum_) (((_fnum_) <= 8) ? ((_name_)[0] == 0) : (/*FIXME*/0))
#define BOOLVEC_ONLY1(_name_, _fnum_) (((_fnum_) <= 8) ? (((_name_)[0] != 0) && (((_name_)[0] & ((_name_)[0] - 1)) == 0)) : (/*FIXME*/0))

#define BOOLVEC_TBL_DECLARE(_name_, _tnum_, _fnum_) uint8_t _name_[(_tnum_)][((_fnum_) + 7) / 8]
#define BOOLVEC_TBL_PTRPARAMM(_name_, _tnum_) &(_name_[(_tnum_)][0])
#define BOOLVEC_TBL_CLEAR(_name_, _tnum_, _fnum_) memset(&(_name_), 0, sizeof(_name_))
#define BOOLVEC_TBL_COPY1(_named_, _names_, _tnum_, _fnum_) BOOLVEC_COPY(_named_[_tnum_], _names_, _fnum_)
#define BOOLVEC_TBL_SET0(_name_, _tnum_, _i_) BOOLVEC_SET0(_name_[_tnum_], _i_)
#define BOOLVEC_TBL_SET1(_name_, _tnum_, _i_) BOOLVEC_SET1(_name_[_tnum_], _i_)
#define BOOLVEC_TBL_SET(_name_, _tnum_, _i_) BOOLVEC_SET(_name_[_tnum_], _i_)
#define BOOLVEC_TBL_IS0(_name_, _tnum_, _i_) BOOLVEC_IS0(_name_[_tnum_], _i_)
#define BOOLVEC_TBL_IS1(_name_, _tnum_, _i_) BOOLVEC_IS1(_name_[_tnum_], _i_)

#endif
