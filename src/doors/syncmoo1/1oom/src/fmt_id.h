#ifndef INC_1OOM_FMT_ID_H
#define INC_1OOM_FMT_ID_H

#define HDRID_LBXXMID   0xafde0100/*0xdeaf 1*/
#define HDRID_LBXVOC    0xafde0200/*0xdeaf 2*/
#define HDRID_MIDI      0x4d546864/*MThd*/
#define HDRID_VOC       0x43726561/*Crea*/
#define HDRID_WAV       0x52494646/*RIFF*/
#define HDRID_OGG       0x4f676753/*OggS*/
#define HDRID_FLAC      0x664c6143/*fLaC*/

#define HDR_LBXXMID_LEN 0x10
#define HDR_LBXVOC_LEN  0x10
#define HDR_VOC         ((const uint8_t *)"Creative Voice File\x1a\x1a\x00\x0a\x01\x29\x11")
#define HDR_VOC_LEN     0x1a
#define HDR_WAV_LEN     0x2c

#endif
