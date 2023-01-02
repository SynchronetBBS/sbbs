/* Synchronet MQTT (publish/subscribe messaging protocol) functions */

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

#ifndef MQTT_H_
#define MQTT_H_

#include "scfgdefs.h"
#include "client.h"
#include "server.h"
#include "link_list.h"
#include "dllexport.h"

struct startup;

#include <stdarg.h>

#ifdef USE_MOSQUITTO
	#include <mosquitto.h>
	#include <mqtt_protocol.h>
	typedef struct mosquitto* mqtt_handle_t;
#else
	typedef void* mqtt_handle_t;
#endif

struct mqtt {
	mqtt_handle_t handle;
	scfg_t* cfg;
	char* host;
	ulong error_count;
	ulong served;
	link_list_t client_list;
	BOOL shared_instance;
};

enum topic_depth {
	TOPIC_OTHER,
	TOPIC_ROOT,	// sbbs/*
	TOPIC_BBS,	// sbbs/BBS-ID/*
	TOPIC_HOST,	// sbbs/BBS-ID/hostname/*
	TOPIC_EVENT, // sbbs/BBS-ID/event/*
	TOPIC_SERVER // sbbs/BBS-ID/server/*
};

#define MQTT_SUCCESS 0 // Same as MOSQ_ERR_SUCCESS
#define MQTT_FAILURE 100 // See library-specific error values for other non-zero results

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int mqtt_init(struct startup*, scfg_t*);
DLLEXPORT int mqtt_startup(struct startup*, scfg_t*, const char* version
	,int (*lputs)(int level, const char* str)
	,BOOL shared_instance);
DLLEXPORT int mqtt_online(struct startup*);
DLLEXPORT int mqtt_server_state(struct startup*, enum server_state);
DLLEXPORT int mqtt_server_version(struct startup*, const char*);
DLLEXPORT int mqtt_errormsg(struct startup*, int level, const char*);
DLLEXPORT int mqtt_terminating(struct startup*);
DLLEXPORT void mqtt_shutdown(struct startup*);
DLLEXPORT char* mqtt_libver(char* str, size_t size);
DLLEXPORT char* mqtt_topic(struct startup*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
DLLEXPORT int mqtt_subscribe(struct startup*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
DLLEXPORT int mqtt_lputs(struct startup*, enum topic_depth, int level, const char* str);
DLLEXPORT int mqtt_pub_noval(struct startup*, enum topic_depth, const char* key);
DLLEXPORT int mqtt_pub_strval(struct startup*, enum topic_depth, const char* key, const char* str);
DLLEXPORT int mqtt_pub_uintval(struct startup*, enum topic_depth, const char* key, ulong value);
DLLEXPORT int mqtt_pub_message(struct startup*, enum topic_depth, const char* key, const void* buf, size_t len);
DLLEXPORT int mqtt_open(struct startup*);
DLLEXPORT void mqtt_close(struct startup*);
DLLEXPORT int mqtt_connect(struct startup*, const char* bind_address);
DLLEXPORT int mqtt_disconnect(struct startup*);
DLLEXPORT int mqtt_thread_start(struct startup*);
DLLEXPORT int mqtt_thread_stop(struct startup*);
DLLEXPORT int mqtt_thread_count(struct startup*, enum topic_depth, ulong count);
DLLEXPORT int mqtt_socket_count(struct startup*, enum topic_depth, ulong count);
DLLEXPORT int mqtt_served_count(struct startup*, enum topic_depth, ulong count);
DLLEXPORT int mqtt_client_count(struct startup*, enum topic_depth, ulong count);
DLLEXPORT int mqtt_client_on(struct startup*, BOOL on, int sock, client_t* client, BOOL update);
DLLEXPORT int mqtt_client_max(struct startup*, ulong count);

#ifdef __cplusplus
}
#endif

#endif // MQTT_H_
