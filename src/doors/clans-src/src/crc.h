#include "defines.h"

#ifndef CRC_H
#define CRC_H

int32_t CRCValue(const void *Data, int DataSize);

int CheckCRC(const void *Data, int Size, int32_t CRC);

#endif
