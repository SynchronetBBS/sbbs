/* Internal bounded formatting interface. */
#ifndef _INC_ODFORMAT
#define _INC_ODFORMAT

#include <stdarg.h>
#include <stddef.h>

int ODVsnprintf(char *buffer, size_t size, const char *format, va_list args);

#endif /* !_INC_ODFORMAT */
