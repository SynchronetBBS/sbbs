#ifndef JS_SOCKET_H
#define JS_SOCKET_H

#include <cryptlib.h>
#include "gen_defs.h"
#include "sbbs.h"
#include "sockwrap.h"
#include "multisock.h"

typedef struct
{
	SOCKET sock;
	BOOL external;      /* externally created, don't close */
	BOOL debug;
	BOOL nonblocking;
	BOOL is_connected;
	BOOL network_byte_order;
	int last_error;
	char last_error_str[SOCKET_STRERROR_BUFLEN];
	int type;
	union xp_sockaddr remote_addr;
	CRYPT_SESSION session;
	BOOL tls_server;
	char *hostname;
	struct xpms_set *set;
	size_t unflushed;
	char peeked_byte;
	BOOL peeked;
	uint16_t local_port;
	js_callback_t *js_cb;
	int tls_minver;
	JSObject *tls_psk;
	bool tls_nameverify;
	bool tls_disable_certverify;
	bool tls_clientauth;
	bool tls_enhanced_certcheck;
	struct js_socket_tls_pending *tls_pending;
} js_socket_private_t;

/* Written to js_event_connect.sv[1] by the asynchronous connect thread */
struct js_connect_result {
	int error;		/* 0 on success */
	BOOL gai;		/* error is a getaddrinfo() result, not a socket error */
};

bool js_socket_tls_readable(js_socket_private_t* p);
void js_socket_cancel_tls(JSContext *cx, JSObject *obj);
JSBool js_socket_finish_tls(JSContext *cx, JSObject *obj);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
