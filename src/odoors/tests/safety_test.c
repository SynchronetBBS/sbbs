#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODFormat.h"
#include "ODInQue.h"
#include "ODSafe.h"

#define CHECK(condition) do { if(!(condition)) return(__LINE__); } while(0)

BOOL ODComCP437ToUnicodeLen(const BYTE *buf, int size, size_t *length);
BYTE *ODComCP437ToUnicode(BYTE *buf, int *size);

static int Format(char *buffer, size_t size, const char *format, ...)
{
   va_list args;
   int result;

   va_start(args, format);
   result = ODVsnprintf(buffer, size, format, args);
   va_end(args);
   return(result);
}

int main(void)
{
   size_t result;
   size_t maximum = (size_t)-1;
   BYTE input[3];
   BYTE *converted;
   int converted_size;
   char buffer[8];
   tODInQueueHandle queue;

   CHECK(ODSizeAdd(10, 20, &result) && result == 30);
   CHECK(!ODSizeAdd(maximum, 1, &result));
   CHECK(ODSizeMultiply(7, 9, &result) && result == 63);
   CHECK(!ODSizeMultiply(maximum, 2, &result));

   input[0] = 'A';
   input[1] = 0x80;
   input[2] = 0xdb;
   CHECK(ODComCP437ToUnicodeLen(input, 3, &result));
   CHECK(result == 6);
   converted_size = 3;
   converted = ODComCP437ToUnicode(input, &converted_size);
   CHECK(converted != NULL);
   CHECK(converted_size == 6);
   CHECK(converted[0] == 'A');
   CHECK(converted[1] == 0xc3 && converted[2] == 0x87);
   CHECK(converted[3] == 0xe2 && converted[4] == 0x96
      && converted[5] == 0x88);
   free(converted);

   CHECK(Format(buffer, sizeof(buffer), "%s-%u", "abc", 42) == 6);
   CHECK(strcmp(buffer, "abc-42") == 0);
   CHECK(Format(buffer, 4, "%s", "abcdef") == 6);
   CHECK(strcmp(buffer, "abc") == 0);
   CHECK(Format(buffer, 0, "%08lu", 1234UL) == 8);

   CHECK(ODInQueueAlloc(&queue, 0) == kODRCInvalidCall);
   CHECK(ODInQueueAlloc(&queue, 1) == kODRCInvalidCall);
   CHECK(ODInQueueAlloc(&queue, 2) == kODRCSuccess);
   ODInQueueFree(queue);

   return(0);
}
