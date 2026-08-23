/* Operating system information wrappers */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

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
