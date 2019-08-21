#ifndef JS_SOCKET_H
#define JS_SOCKET_H

#include <cryptlib.h>
#include "gen_defs.h"
#include "sockwrap.h"
#include "multisock.h"

typedef struct
{
	SOCKET	sock;
	BOOL	external;	/* externally created, don't close */
	BOOL	debug;
	BOOL	nonblocking;
	BOOL	is_connected;
	BOOL	network_byte_order;
	int		last_error;
	int		type;
	union xp_sockaddr	remote_addr;
	CRYPT_SESSION	session;
	BOOL	tls_server;
	char	*hostname;
	struct xpms_set	*set;
	size_t	unflushed;
	char	peeked_byte;
	BOOL	peeked;
	uint16_t local_port;
} js_socket_private_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
