/* Internal bounded formatting interface. */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "ODFormat.h"

#ifndef OPENDOORS_HAVE_VSNPRINTF
int ODFallbackVsnprintf(char *buffer, size_t size, const char *format,
   va_list args);
#endif

int ODVsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
#ifdef OPENDOORS_HAVE_VSNPRINTF
   return(vsnprintf(buffer, size, format, args));
#else
   return(ODFallbackVsnprintf(buffer, size, format, args));
#endif
}
