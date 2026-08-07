#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODFormat.h"
#include "ODInQue.h"
#include "ODSafe.h"
#include "ODSwap.h"
#include "ODVScreen.h"

static int Fail(int line)
{
   FILE *failure;

   failure = fopen("DOSFAIL.TXT", "w");
   if(failure != NULL)
   {
      fprintf(failure, "DOS runtime test failed at line %d\n", line);
      fclose(failure);
   }
   return(line);
}

#define CHECK(condition) do { if(!(condition)) return(Fail(__LINE__)); } while(0)

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
   char current_directory[68];
   char formatted[16];
   unsigned int bytes_per_cluster;
   unsigned int available_clusters;
   size_t result;
   size_t maximum = (size_t)-1;
   BYTE *large_input;
   BYTE *converted;
   BYTE *screen_snapshot;
   DWORD screen_snapshot_size;
   int input_size = 21846;
   tODInQueueHandle queue;
   FILE *sentinel;

   CHECK(_getdrv() >= 0 && _getdrv() < 26);
   memset(current_directory, 0, sizeof(current_directory));
   CHECK(_getcd(0, current_directory) == 0);
   CHECK(_dskspace(0, &bytes_per_cluster, &available_clusters) == 0);
   CHECK(bytes_per_cluster != 0 && available_clusters != 0);

   CHECK(ODSizeAdd(100, 23, &result) && result == 123);
   CHECK(!ODSizeAdd(maximum, 1, &result));
   CHECK(!ODSizeMultiply(maximum, 2, &result));
   CHECK(Format(formatted, sizeof(formatted), "%s-%u", "DOS", 16) == 6);
   CHECK(strcmp(formatted, "DOS-16") == 0);

   large_input = (BYTE *)malloc((size_t)input_size);
   CHECK(large_input != NULL);
   memset(large_input, 0x80, (size_t)input_size);
   converted = ODComCP437ToUnicode(large_input, &input_size);
   free(large_input);
   CHECK(converted == NULL);
   CHECK(od_control.od_error == ERR_LIMIT);

   CHECK(ODInQueueAlloc(&queue, INT_MAX) == kODRCNoMemory);
   CHECK(ODInQueueAlloc(&queue, 2) == kODRCSuccess);
   ODInQueueFree(queue);

   od_control.baud = 1;
   od_control.user_screenwidth = 132;
   od_control.user_screen_length = 50;
   ODSessionScreenInitialize(80, 25);
   CHECK(ODSessionScreenAvailable());
   CHECK(ODSessionScreenWidth() == 132);
   CHECK(ODSessionScreenHeight() == 50);
   screen_snapshot_size = ODSessionScreenSnapshotSize();
   CHECK(screen_snapshot_size == 48UL + 132UL * 50UL * 2UL);
   screen_snapshot = (BYTE *)malloc((size_t)screen_snapshot_size);
   CHECK(screen_snapshot != NULL);
   CHECK(ODSessionScreenSave(screen_snapshot, screen_snapshot_size));
   free(screen_snapshot);
   ODSessionScreenShutdown();

   od_control.user_screenwidth = 255;
   od_control.user_screen_length = 129;
   ODSessionScreenInitialize(80, 25);
   CHECK(!ODSessionScreenAvailable());
   CHECK(ODSessionScreenError() == ERR_LIMIT);
   ODSessionScreenShutdown();

   sentinel = fopen("DOSPASS.OK", "w");
   CHECK(sentinel != NULL);
   CHECK(fputs("OpenDoors DOS runtime tests passed\n", sentinel) >= 0);
   CHECK(fclose(sentinel) == 0);
   return(0);
}
