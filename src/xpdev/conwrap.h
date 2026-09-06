/* Cross-platform local console I/O wrapppers */

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

#ifndef _CONWRAP_H
#define _CONWRAP_H

#include "wrapdll.h"

#if defined(__unix__)

    DLLEXPORT void _termios_setup(void);
    DLLEXPORT void _termios_reset(void);
    DLLEXPORT void _echo_on(void);
    DLLEXPORT void _echo_off(void);
    DLLEXPORT int kbhit(void);
    DLLEXPORT int getch(void);

#else	/* DOS-Based */

	#include <conio.h>

#endif

#endif	/* _CONWRAP_H */
