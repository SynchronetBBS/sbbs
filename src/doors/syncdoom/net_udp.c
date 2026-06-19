//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2026 Rob Swindell / syncdoom
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Networking module using plain UDP sockets (via xpdev sockwrap).
//     A drop-in replacement for Chocolate Doom's SDL_net transport
//     (net_sdl): same net_module_t interface, no SDL dependency.
//     IPv4 only, matching the protocol revision this net layer is from.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sockwrap.h"       // xpdev: SOCKET, closesocket, ioctlsocket, socket_readable

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_packet.h"
#include "net_udp.h"
#include "z_zone.h"

#define DEFAULT_PORT 2342

// The server binds the loopback interface by default, so a freshly-spawned
// dedicated server is reachable only from same-host clients.  A sysop opts into
// off-box (cross-host / federation) play by overriding this with -bindaddr.
#define DEFAULT_BIND_HOST "127.0.0.1"

static boolean     initted = false;
static int         port = DEFAULT_PORT;
static const char *bind_host = DEFAULT_BIND_HOST;
static SOCKET      udpsocket = INVALID_SOCKET;

typedef struct
{
	net_addr_t net_addr;
	struct sockaddr_in sock_addr;
} addrpair_t;

static addrpair_t **addr_table;
static int          addr_table_size = -1;

// Initializes the address table

static void NET_UDP_InitAddrTable(void)
{
	addr_table_size = 16;

	addr_table = Z_Malloc(sizeof(addrpair_t *) * addr_table_size,
	                      PU_STATIC, 0);
	memset(addr_table, 0, sizeof(addrpair_t *) * addr_table_size);
}

static boolean AddressesEqual(struct sockaddr_in *a, struct sockaddr_in *b)
{
	return a->sin_addr.s_addr == b->sin_addr.s_addr
	       && a->sin_port == b->sin_port;
}

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.

static net_addr_t *NET_UDP_FindAddress(struct sockaddr_in *addr)
{
	addrpair_t *new_entry;
	int         empty_entry = -1;
	int         i;

	if (addr_table_size < 0)
	{
		NET_UDP_InitAddrTable();
	}

	for (i = 0; i < addr_table_size; ++i)
	{
		if (addr_table[i] != NULL
		    && AddressesEqual(addr, &addr_table[i]->sock_addr))
		{
			return &addr_table[i]->net_addr;
		}

		if (empty_entry < 0 && addr_table[i] == NULL)
			empty_entry = i;
	}

	// Was not found in list.  We need to add it.
	// Is there any space in the table? If not, increase the table size.

	if (empty_entry < 0)
	{
		addrpair_t **new_addr_table;
		int          new_addr_table_size;

		// after reallocing, we will add this in as the first entry
		// in the new block of memory

		empty_entry = addr_table_size;

		new_addr_table_size = addr_table_size * 2;
		new_addr_table = Z_Malloc(sizeof(addrpair_t *) * new_addr_table_size,
		                          PU_STATIC, 0);
		memset(new_addr_table, 0, sizeof(addrpair_t *) * new_addr_table_size);
		memcpy(new_addr_table, addr_table,
		       sizeof(addrpair_t *) * addr_table_size);
		Z_Free(addr_table);
		addr_table = new_addr_table;
		addr_table_size = new_addr_table_size;
	}

	// Add a new entry

	new_entry = Z_Malloc(sizeof(addrpair_t), PU_STATIC, 0);

	new_entry->sock_addr = *addr;
	new_entry->net_addr.handle = &new_entry->sock_addr;
	new_entry->net_addr.module = &net_udp_module;

	addr_table[empty_entry] = new_entry;

	return &new_entry->net_addr;
}

static void NET_UDP_FreeAddress(net_addr_t *addr)
{
	int i;

	for (i = 0; i < addr_table_size; ++i)
	{
		if (addr == &addr_table[i]->net_addr)
		{
			Z_Free(addr_table[i]);
			addr_table[i] = NULL;
			return;
		}
	}

	I_Error("NET_UDP_FreeAddress: Attempted to remove an unused address!");
}

// Resolve a server bind-address string to an IPv4 address (network order).
// Empty, "0.0.0.0", "any" or "*" mean INADDR_ANY (all interfaces).  A dotted-
// quad or hostname is resolved.  On failure we fall back to loopback, not ANY,
// so a typo can't silently expose the socket to the whole network.

static unsigned long ResolveBindAddr(const char *host)
{
	struct addrinfo hints, *result;
	unsigned long   netaddr;

	if (host == NULL || host[0] == '\0'
	    || strcmp(host, "0.0.0.0") == 0
	    || strcmp(host, "any") == 0
	    || strcmp(host, "*") == 0)
	{
		return htonl(INADDR_ANY);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(host, NULL, &hints, &result) == 0 && result != NULL)
	{
		netaddr = ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(result);
		return netaddr;
	}

	fprintf(stderr, "syncdoom: bind address '%s' unresolved; using loopback\n",
	        host);
	return htonl(INADDR_LOOPBACK);
}

// Open the UDP socket, bound to the given port (0 = any/ephemeral, for a
// client) and local address (network order; INADDR_ANY for a client so the OS
// picks the source per destination).  Non-blocking and broadcast-capable.

static boolean OpenSocket(int bind_port, unsigned long bind_addr)
{
	struct sockaddr_in addr;
	unsigned long      nonblock = 1;
	int                broadcast = 1;

	udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (udpsocket == INVALID_SOCKET)
	{
		return false;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = bind_addr;
	addr.sin_port = htons((unsigned short) bind_port);

	if (bind(udpsocket, (struct sockaddr *) &addr, sizeof(addr)) != 0)
	{
		closesocket(udpsocket);
		udpsocket = INVALID_SOCKET;
		return false;
	}

	ioctlsocket(udpsocket, FIONBIO, &nonblock);
	setsockopt(udpsocket, SOL_SOCKET, SO_BROADCAST,
	           (void *) &broadcast, sizeof(broadcast));

	return true;
}

static void CheckPortArg(void)
{
	int p;

	//!
	// @category net
	// @arg <n>
	//
	// Use the specified UDP port for communications, instead of
	// the default (2342).
	//

	p = M_CheckParmWithArgs("-port", 1);
	if (p > 0)
		port = atoi(myargv[p + 1]);
}

static void CheckBindArg(void)
{
	int p;

	//!
	// @category net
	// @arg <address>
	//
	// Bind the (server) UDP socket to the specified local address.
	// The default is 127.0.0.1 (loopback only: same-host clients).
	// Use 0.0.0.0 to listen on all interfaces, or a specific local
	// IP/hostname to listen on one, for cross-host play.
	//

	p = M_CheckParmWithArgs("-bindaddr", 1);
	if (p > 0)
		bind_host = myargv[p + 1];
}

static boolean NET_UDP_InitClient(void)
{
	if (initted)
		return true;

	CheckPortArg();

	// A client binds an ephemeral port on INADDR_ANY (not the server's loopback
	// default) so the OS selects the right source address for each destination.
	if (!OpenSocket(0, htonl(INADDR_ANY)))
	{
		I_Error("NET_UDP_InitClient: Unable to open a socket!");
	}

	initted = true;

	return true;
}

static boolean NET_UDP_InitServer(void)
{
	if (initted)
		return true;

	CheckPortArg();
	CheckBindArg();

	if (!OpenSocket(port, ResolveBindAddr(bind_host)))
	{
		I_Error("NET_UDP_InitServer: Unable to bind to %s port %i",
		        bind_host, port);
	}

	initted = true;

	return true;
}

static void NET_UDP_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
	struct sockaddr_in ip;

	if (addr == &net_broadcast_addr)
	{
		memset(&ip, 0, sizeof(ip));
		ip.sin_family = AF_INET;
		ip.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		ip.sin_port = htons((unsigned short) port);
	}
	else
	{
		ip = *((struct sockaddr_in *) addr->handle);
	}

	if (sendto(udpsocket, (void *) packet->data, packet->len, 0,
	           (struct sockaddr *) &ip, sizeof(ip)) < 0)
	{
		I_Error("NET_UDP_SendPacket: Error transmitting packet: %s",
		        strerror(SOCKET_ERRNO));
	}
}

static boolean NET_UDP_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
	struct sockaddr_in remote;
	socklen_t          remote_len;
	byte               buf[1500];
	int                result;

	// Non-blocking: only attempt a recv when data is already waiting.

	if (!socket_readable(udpsocket, 0))
		return false;

	remote_len = sizeof(remote);
	result = recvfrom(udpsocket, (void *) buf, sizeof(buf), 0,
	                  (struct sockaddr *) &remote, &remote_len);

	if (result < 0)
	{
		// Nothing actually there (or a transient error): no packet.

		return false;
	}

	// Put the data into a new packet structure

	*packet = NET_NewPacket(result);
	memcpy((*packet)->data, buf, result);
	(*packet)->len = result;

	// Address

	*addr = NET_UDP_FindAddress(&remote);

	return true;
}

void NET_UDP_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
	struct sockaddr_in *ip;
	uint32_t            host;
	uint16_t            ipport;

	ip = (struct sockaddr_in *) addr->handle;
	host = ntohl(ip->sin_addr.s_addr);
	ipport = ntohs(ip->sin_port);

	M_snprintf(buffer, buffer_len, "%i.%i.%i.%i",
	           (host >> 24) & 0xff, (host >> 16) & 0xff,
	           (host >> 8) & 0xff, host & 0xff);

	// If we are using the default port we just need to show the IP address,
	// but otherwise we need to include the port.

	if (ipport != DEFAULT_PORT)
	{
		char portbuf[10];
		M_snprintf(portbuf, sizeof(portbuf), ":%i", ipport);
		M_StringConcat(buffer, portbuf, buffer_len);
	}
}

net_addr_t *NET_UDP_ResolveAddress(char *address)
{
	struct sockaddr_in ip;
	struct addrinfo    hints;
	struct addrinfo *  result;
	char *             addr_hostname;
	int                addr_port;
	char *             colon;
	char               portstr[16];

	colon = strchr(address, ':');

	if (colon != NULL)
	{
		addr_hostname = M_StringDuplicate(address);
		addr_hostname[colon - address] = '\0';
		addr_port = atoi(colon + 1);
	}
	else
	{
		addr_hostname = address;
		addr_port = port;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	M_snprintf(portstr, sizeof(portstr), "%i", addr_port);

	if (getaddrinfo(addr_hostname, portstr, &hints, &result) != 0
	    || result == NULL)
	{
		if (addr_hostname != address)
			free(addr_hostname);
		return NULL;        // unable to resolve
	}

	ip = *((struct sockaddr_in *) result->ai_addr);
	freeaddrinfo(result);

	if (addr_hostname != address)
		free(addr_hostname);

	return NET_UDP_FindAddress(&ip);
}

// Complete module

net_module_t net_udp_module =
{
	NET_UDP_InitClient,
	NET_UDP_InitServer,
	NET_UDP_SendPacket,
	NET_UDP_RecvPacket,
	NET_UDP_AddrToString,
	NET_UDP_FreeAddress,
	NET_UDP_ResolveAddress,
};
