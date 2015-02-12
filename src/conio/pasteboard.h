#ifndef _PASTEBOARD_H_
#define _PASTEBOARD_H_

#include <stddef.h>

void OSX_copytext(const char *text, size_t len);
char *OSX_getcliptext(void);

#endif
