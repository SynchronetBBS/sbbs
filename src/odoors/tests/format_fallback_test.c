#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define CHECK(condition) do { if(!(condition)) return(__LINE__); } while(0)

int ODFallbackVsnprintf(char *buffer, size_t size, const char *format,
   va_list args);

static int Format(char *buffer, size_t size, const char *format, ...)
{
   va_list args;
   int result;

   va_start(args, format);
   result = ODFallbackVsnprintf(buffer, size, format, args);
   va_end(args);
   return(result);
}

int main(void)
{
   char buffer[16];

   CHECK(Format(buffer, sizeof(buffer), "%s-%04d", "test", 7) == 9);
   CHECK(strcmp(buffer, "test-0007") == 0);
   CHECK(Format(buffer, 5, "%s", "truncate") == 8);
   CHECK(strcmp(buffer, "trun") == 0);
   CHECK(Format(buffer, 0, "%.2f", 1.25) == 4);
   return(0);
}
