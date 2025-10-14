/*
 * CRC code. :)
 */

#include "defines.h"

long CRCValue(void *Data, int DataSize)
{
	char *p;
	long CRC = 0;

	p = (char *)Data;

	while (p < ((char *)Data+DataSize)) {
		CRC += (*p);
		p++;
	}

	return CRC;
}

int CheckCRC(void *Data, int Size, long CRC)
{
	return (CRCValue(Data, Size) == CRC);
}
