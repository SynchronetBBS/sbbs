#ifndef OS_INFO_H
#define OS_INFO_H

#include "genwrap.h"

/* Command processor/shell environment variable name */
#ifdef __unix__
	#define OS_CMD_SHELL_ENV_VAR	"SHELL"
#else	/* DOS/Windows/OS2 */
	#define OS_CMD_SHELL_ENV_VAR	"COMSPEC"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

DLLEXPORT char*		os_version(char *str, size_t);
DLLEXPORT char*		os_cpuarch(char *str, size_t);
DLLEXPORT char*		os_cmdshell(void);

#if defined(__cplusplus)
}
#endif

#endif
