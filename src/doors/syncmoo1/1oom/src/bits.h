#ifndef INC_1OOM_BITS_H
#define INC_1OOM_BITS_H

#include "config.h"
#include "types.h"

/* Allow unaligned access for various platforms */
#if defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
#define ALLOW_UNALIGNED_ACCESS
#endif
#if defined(__m68020__) || defined(__m68030__) || defined(__m68040__) || defined(__m68060__)
#define ALLOW_UNALIGNED_ACCESS
#endif
#if defined(__powerpc__) || defined(__ppc__)
#define ALLOW_UNALIGNED_ACCESS
#endif

#define BSWAP_16(_v_)   ((uint16_t)((((_v_) >> 8) & 0xff) | ((_v_) << 8)))

#if !defined WORDS_BIGENDIAN && defined ALLOW_UNALIGNED_ACCESS
#define SET_LE_16(_p_,_v_) ((*((uint16_t *)(_p_))) = (_v_))
#define GET_LE_16(_p_) (*((uint16_t const *)(_p_)))
#define SET_LE_32(_p_,_v_) ((*((uint32_t *)(_p_))) = (_v_))
#define GET_LE_32(_p_) (*((uint32_t const *)(_p_)))
#else
#define SET_LE_16(_p_,_v_) (((uint8_t *)(_p_))[0] = ((_v_) & 0xffu), ((uint8_t *)(_p_))[1] = (((_v_) >> 8) & 0xffu))
#define GET_LE_16(_p_) (((uint16_t)(((uint8_t const *)(_p_))[0])) | (((uint16_t)(((uint8_t const *)(_p_))[1])) << 8))
#define SET_LE_32(_p_,_v_) (((uint8_t *)(_p_))[0] = ((_v_) & 0xffu), ((uint8_t *)(_p_))[1] = (((_v_) >> 8) & 0xffu), ((uint8_t *)(_p_))[2] = (((_v_) >> 16) & 0xffu), ((uint8_t *)(_p_))[3] = (((_v_) >> 24) & 0xffu))
#define GET_LE_32(_p_) (((uint32_t)GET_LE_16((_p_))) | (((uint32_t)GET_LE_16((_p_) + 2)) << 16))
#endif
#define GET_LE_24(_p_) (((uint32_t)GET_LE_16((_p_))) | (((uint32_t)*(((uint8_t const *)(_p_)) + 2)) << 16))

#if defined WORDS_BIGENDIAN && defined ALLOW_UNALIGNED_ACCESS
#define SET_BE_16(_p_,_v_) ((*((uint16_t *)(_p_))) = (_v_))
#define GET_BE_16(_p_) (*((uint16_t const *)(_p_)))
#define SET_BE_32(_p_,_v_) ((*((uint32_t *)(_p_))) = (_v_))
#define GET_BE_32(_p_) (*((uint16_t const *)(_p_)))
#else
#define SET_BE_16(_p_,_v_) (((uint8_t *)(_p_))[1] = ((_v_) & 0xffu), ((uint8_t *)(_p_))[0] = (((_v_) >> 8) & 0xffu))
#define GET_BE_16(_p_) (((uint16_t)(((uint8_t const *)(_p_))[1])) | (((uint16_t)(((uint8_t const *)(_p_))[0])) << 8))
#define SET_BE_32(_p_,_v_) (((uint8_t *)(_p_))[3] = ((_v_) & 0xffu), ((uint8_t *)(_p_))[2] = (((_v_) >> 8) & 0xffu), ((uint8_t *)(_p_))[1] = (((_v_) >> 16) & 0xffu), ((uint8_t *)(_p_))[0] = (((_v_) >> 24) & 0xffu))
#define GET_BE_32(_p_) (((uint32_t)GET_BE_16((_p_) + 2)) | (((uint32_t)GET_BE_16((_p_))) << 16))
#endif
#define SET_BE_24(_p_, _v_) (((uint8_t *)(_p_))[2] = ((_v_) & 0xffu), ((uint8_t *)(_p_))[1] = (((_v_) >> 8) & 0xffu), ((uint8_t *)(_p_))[0] = (((_v_) >> 16) & 0xffu))
#define GET_BE_24(_p_) ((((uint32_t)GET_BE_16((_p_))) << 8) | ((uint32_t)*(((uint8_t const *)(_p_)) + 2)))

#endif
