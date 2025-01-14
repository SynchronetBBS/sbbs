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
#include "startup.h"

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
	bool connected;
	int connect_error;
	int disconnect_reason;
	ulong max_clients;
	ulong error_count;
	ulong served;
	link_list_t client_list;
	struct startup* startup;
	enum server_state server_state;
};

enum topic_depth {
	TOPIC_OTHER,
	TOPIC_ROOT,         // sbbs/*
	TOPIC_BBS_LEVEL,    // sbbs/BBSID
	TOPIC_BBS,          // sbbs/BBSID/*
	TOPIC_BBS_ACTION,   // sbbs/BBSID/action/*
	TOPIC_HOST_LEVEL,   // sbbs/BBSID/host/HOSTNAME
	TOPIC_HOST,         // sbbs/BBSID/host/HOSTNAME/*
	TOPIC_HOST_EVENT,   // sbbs/BBSID/host/HOSTNAME/event/*
	TOPIC_SERVER_LEVEL, // sbbs/BBSID/host/HOSTNAME/server/SERVER
	TOPIC_SERVER,       // sbbs/BBSID/host/HOSTNAME/server/SERVER/*
};

#define MQTT_SUCCESS 0 // Same as MOSQ_ERR_SUCCESS
#define MQTT_FAILURE 100 // See library-specific error values for other non-zero results

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int mqtt_init(struct mqtt*, scfg_t*, struct startup*);
DLLEXPORT int mqtt_startup(struct mqtt*, scfg_t*, struct startup*, const char* version
                           , int (*lputs)(int level, const char* str));
DLLEXPORT int mqtt_online(struct mqtt*);
DLLEXPORT int mqtt_server_state(struct mqtt*, enum server_state);
DLLEXPORT int mqtt_errormsg(struct mqtt*, int level, const char*);
DLLEXPORT int mqtt_terminating(struct mqtt*);
DLLEXPORT void mqtt_shutdown(struct mqtt*);
DLLEXPORT char* mqtt_libver(char* str, size_t size);
DLLEXPORT char* mqtt_topic(struct mqtt*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
DLLEXPORT int mqtt_subscribe(struct mqtt*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
DLLEXPORT int mqtt_lputs(struct mqtt*, enum topic_depth, int level, const char* str);
DLLEXPORT int mqtt_pub_noval(struct mqtt*, enum topic_depth, const char* key);
DLLEXPORT int mqtt_pub_strval(struct mqtt*, enum topic_depth, const char* key, const char* str);
DLLEXPORT int mqtt_pub_uintval(struct mqtt*, enum topic_depth, const char* key, ulong value);
DLLEXPORT int mqtt_pub_message(struct mqtt*, enum topic_depth, const char* key, const void* buf, size_t len, bool retain);
DLLEXPORT int mqtt_pub_timestamped_msg(struct mqtt*, enum topic_depth, const char* key, time_t, const char* msg);
DLLEXPORT int mqtt_open(struct mqtt*);
DLLEXPORT void mqtt_close(struct mqtt*);
DLLEXPORT int mqtt_connect(struct mqtt*, const char* bind_address);
DLLEXPORT int mqtt_disconnect(struct mqtt*);
DLLEXPORT int mqtt_thread_start(struct mqtt*);
DLLEXPORT int mqtt_thread_stop(struct mqtt*);
DLLEXPORT int mqtt_client_on(struct mqtt*, bool on, int sock, client_t* client, bool update);
DLLEXPORT int mqtt_client_max(struct mqtt*, ulong count);
DLLEXPORT int mqtt_client_count(struct mqtt*);
DLLEXPORT int mqtt_user_login_fail(struct mqtt*, client_t*, const char* username);
DLLEXPORT int mqtt_user_login(struct mqtt*, client_t*);
DLLEXPORT int mqtt_user_logout(struct mqtt*, client_t*, time_t);
DLLEXPORT int mqtt_file_upload(struct mqtt*, user_t*, int dirnum, const char* fname, off_t size, client_t*);
DLLEXPORT int mqtt_file_download(struct mqtt*, user_t*, int dirnum, const char* fname, off_t size, client_t*);
DLLEXPORT int mqtt_putnodedat(struct mqtt*, int number, node_t*);

#ifdef __cplusplus
}
#endif

#endif // MQTT_H_
