#ifndef _OOII_H_
#define _OOII_H_

#include <stdbool.h>

#define MAX_OOII_MODE 3

bool handle_ooii_code(unsigned char *codeStr, int *ooii_mode, unsigned char *retbuf, size_t retsize);

#endif
