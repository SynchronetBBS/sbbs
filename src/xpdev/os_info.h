#ifndef OS_INFO_H
#define OS_INFO_H

/* Command processor/shell environment variable name */
#ifdef __unix__
	#define OS_CMD_SHELL_ENV_VAR	"SHELL"
#else	/* DOS/Windows/OS2 */
	#define OS_CMD_SHELL_ENV_VAR	"COMSPEC"
#endif

DLLEXPORT char*		os_version(char *str, size_t);
DLLEXPORT char*		os_cpuarch(char *str, size_t);
DLLEXPORT char*		os_cmdshell(void);

#endif
