/* Private C11 time compatibility interface; never installed. */
#ifndef DSSH_TIMESPEC_H
#define DSSH_TIMESPEC_H

#include <time.h>

#ifdef DSSH_TIMESPEC_GET_SHIM
#include "deucessh-portable.h"

#ifndef TIME_UTC
 #define TIME_UTC 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
DSSH_PRIVATE int dssh_timespec_get(struct timespec *ts, int base);
#ifdef __cplusplus
}
#endif

#define timespec_get dssh_timespec_get
#endif

#endif
