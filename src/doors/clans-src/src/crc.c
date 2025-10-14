/*
 * CRC code. :)
 */

#include "defines.h"

int32_t CRCValue(void *Data, int DataSize)
{
	char *p;
	int32_t CRC = 0;

	p = (char *)Data;

	while (p < ((char *)Data+DataSize)) {
		CRC += (*p);
		p++;
	}

	return CRC;
}

int CheckCRC(void *Data, int Size, int32_t CRC)
{
	return (CRCValue(Data, Size) == CRC);
}
