/* mmap() style cross-platform development wrappers */

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

#ifndef _XPMAP_H
#define _XPMAP_H

#include "gen_defs.h"
#include "wrapdll.h"

enum xpmap_type {
	XPMAP_READ,
	XPMAP_WRITE,
	XPMAP_COPY
};

#if defined(__unix__)

#include <sys/mman.h>
struct xpmapping {
	void	*addr;
	int		fd;
	size_t	size;
};

#elif defined(_WIN32)

struct xpmapping {
	void		*addr;
	HANDLE		fd;
	HANDLE		md;
	uint64_t	size;
};

#else

	#error "Need mmap wrappers."

#endif

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT struct xpmapping* xpmap(const char *filename, enum xpmap_type type);
DLLEXPORT void xpunmap(struct xpmapping *map);
#ifdef __cplusplus
}
#endif

#endif
