#ifndef DSSH_PRIVATE_KEY_FILE_H
#define DSSH_PRIVATE_KEY_FILE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *dssh_private_key_open(const char *path);

#ifdef __cplusplus
}
#endif

#endif
