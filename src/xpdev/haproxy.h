/* HAPROXY PROTOCOL constant definitions */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
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

/* HAPROXY PROTO BINARY DEFINITIONS */
#define HAPROXY_AFINET          0x1     /* IPv4 Connection */
#define HAPROXY_AFINET6         0x2     /* IPv6 Connection */

/* HAPROXY COMMANDS */
#define HAPROXY_LOCAL           0x0     /* Connections instigated by the proxy, eg: health-check */
#define HAPROXY_PROXY           0x1     /* Relay connections */
