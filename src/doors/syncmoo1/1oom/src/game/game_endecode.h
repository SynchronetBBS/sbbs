#ifndef INC_1OOM_GAME_ENDECODE_H
#define INC_1OOM_GAME_ENDECODE_H

#define SG_1OOM_EN_DUMMY(_n_)  memset(&buf[pos], 0, (_n_)), pos += (_n_)
#define SG_1OOM_DE_DUMMY(_n_)  pos += (_n_)
#define SG_1OOM_EN_U8(_v_)  buf[pos++] = (_v_)
#define SG_1OOM_DE_U8(_v_)  (_v_) = buf[pos++]
#define SG_1OOM_EN_U16(_v_)  SET_LE_16(&buf[pos], (_v_)), pos += 2
#define SG_1OOM_DE_U16(_v_)  (_v_) = GET_LE_16(&buf[pos]), pos += 2
#define SG_1OOM_EN_U32(_v_)  SET_LE_32(&buf[pos], (_v_)), pos += 4
#define SG_1OOM_DE_U32(_v_)  (_v_) = GET_LE_32(&buf[pos]), pos += 4
#define SG_1OOM_EN_BV(_v_, _n_)  do { int l = ((_n_) + 7) / 8; memcpy(&buf[pos], (_v_), l); pos += l; } while (0)
#define SG_1OOM_DE_BV(_v_, _n_)  do { int l = ((_n_) + 7) / 8; memcpy((_v_), &buf[pos], l); pos += l; } while (0)
#define SG_1OOM_EN_TBL_U8(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { buf[pos] = (_v_)[i_]; ++pos; } } while (0)
#define SG_1OOM_DE_TBL_U8(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { (_v_)[i_] = buf[pos]; ++pos; } } while (0)
#define SG_1OOM_EN_TBL_U16(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { SET_LE_16(&buf[pos], (_v_)[i_]); pos += 2; } } while (0)
#define SG_1OOM_DE_TBL_U16(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { (_v_)[i_] = GET_LE_16(&buf[pos]); pos += 2; } } while (0)
#define SG_1OOM_EN_TBL_U32(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { SET_LE_32(&buf[pos], (_v_)[i_]); pos += 4; } } while (0)
#define SG_1OOM_DE_TBL_U32(_v_, _n_)  do { for (int i_ = 0; i_ < (_n_); ++i_) { (_v_)[i_] = GET_LE_32(&buf[pos]); pos += 4; } } while (0)
#define SG_1OOM_EN_TBL_TBL_U8(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { buf[pos++] = (_v_)[o_][i_]; } } } while (0)
#define SG_1OOM_DE_TBL_TBL_U8(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { (_v_)[o_][i_] = buf[pos++]; } } } while (0)
#define SG_1OOM_EN_TBL_TBL_U16(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { SET_LE_16(&buf[pos], (_v_)[o_][i_]); pos += 2; } } } while (0)
#define SG_1OOM_DE_TBL_TBL_U16(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { (_v_)[o_][i_] = GET_LE_16(&buf[pos]); pos += 2; } } } while (0)
#define SG_1OOM_EN_TBL_TBL_U32(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { SET_LE_32(&buf[pos], (_v_)[o_][i_]); pos += 4; } } } while (0)
#define SG_1OOM_DE_TBL_TBL_U32(_v_, _no_, _ni_)  do { for (int o_ = 0; o_ < (_no_); ++o_) { for (int i_ = 0; i_ < (_ni_); ++i_) { (_v_)[o_][i_] = GET_LE_32(&buf[pos]); pos += 4; } } } while (0)

#endif
