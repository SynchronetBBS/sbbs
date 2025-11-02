/*
 * CRC code. :)
 */

#include "crc.h"
#include "defines.h"

int32_t CRCValue(const void *Data, ptrdiff_t DataSize)
{
	const char *p;
	int32_t CRC = 0;

	p = (char *)Data;

	while (p < ((char *)Data + DataSize)) {
		CRC += (*p);
		p++;
	}

	return CRC;
}
