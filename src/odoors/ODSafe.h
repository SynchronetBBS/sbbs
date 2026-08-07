/* Internal checked-size helpers.  Keep this header usable by ANSI C89 DOS
 * compilers as well as the hosted builds. */
#ifndef _INC_ODSAFE
#define _INC_ODSAFE

#include <stddef.h>

int ODSizeAdd(size_t left, size_t right, size_t *result);
int ODSizeMultiply(size_t left, size_t right, size_t *result);

#endif /* !_INC_ODSAFE */
