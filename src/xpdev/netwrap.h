/* Network related wrapper functions */

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

#ifndef _NETWRAP_H
#define _NETWRAP_H

#include <stddef.h>		/* size_t */
#include "str_list.h"	/* string list functions and types */
#include "wrapdll.h"

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>	// struct in6_addr
#else
	#include <netinet/in.h>
#endif

#define IPv4_LOCALHOST	0x7f000001U	/* 127.0.0.1 */

#if defined(__cplusplus)
extern "C" {
#endif

DLLEXPORT const char* 	getHostNameByAddr(const char*);
DLLEXPORT str_list_t	getNameServerList(void);
DLLEXPORT void			freeNameServerList(str_list_t);
DLLEXPORT const char*	IPv4AddressToStr(uint32_t, char* dest, size_t size);
DLLEXPORT uint32_t		parseIPv4Address(const char*);
DLLEXPORT struct in6_addr parseIPv6Address(const char*);
DLLEXPORT bool isValidHostname(const char *str);
DLLEXPORT bool isValidAddressString(const char *str);
DLLEXPORT bool isResolvableHostname(const char *str);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
