/*
 * CRC code. :)
 */

#include "defines.h"

  long CRCValue ( void *Data, _INT16 DataSize )
  {
    char *p;
    long CRC = 0;

    p = (char *)Data;

    while (p < ((char *)Data+DataSize))
    {
      CRC += (*p);
      p++;
    }

    return CRC;
  }

  _INT16 CheckCRC ( void *Data, _INT16 Size, long CRC )
  {
    return (CRCValue(Data, Size) == CRC);
  }
