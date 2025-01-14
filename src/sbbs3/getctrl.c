/* Synchronet get "control" directory function */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>
#include "getctrl.h"
#include "sbbsdefs.h"

const char* get_ctrl_dir(bool warn)
{
	char* p = getenv("SBBSCTRL");
	if (p == NULL || *p == '\0') {
		if (warn)
			fprintf(stderr, "!SBBSCTRL environment variable not set, using default value: " SBBSCTRL_DEFAULT "\n\n");
		p = SBBSCTRL_DEFAULT;
	}
	return p;
}
