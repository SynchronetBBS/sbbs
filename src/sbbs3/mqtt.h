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

#ifndef MQTT_H
#define MQTT_H

#include "scfgdefs.h"

#include <stdarg.h>
#include <stdint.h>

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
	const char* host;
	const char* server;
};

enum topic_depth {
	TOPIC_ROOT,
	TOPIC_BBS,
	TOPIC_HOST,
	TOPIC_SERVER
};

#define MQTT_SUCCESS 0 // Same as MOSQ_ERR_SUCCESS
#define MQTT_FAILURE 100 // See library-specific error values for other non-zero results

#ifdef __cplusplus
extern "C" {
#endif

int mqtt_init(struct mqtt*, scfg_t*, const char* host, const char* server);
char* mqtt_libver(char* str, size_t size);
char* mqtt_topic(struct mqtt*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
int mqtt_subscribe(struct mqtt*, enum topic_depth, char* str, size_t size, const char* fmt, ...);
int mqtt_lputs(struct mqtt*, enum topic_depth, int level, const char* str);
int mqtt_pub_strval(struct mqtt*, enum topic_depth, const char* key, const char* str);
int mqtt_pub_uintval(struct mqtt*, enum topic_depth, const char* key, ulong value);
int mqtt_pub_message(struct mqtt*, enum topic_depth, const char* key, const void* buf, size_t len);
int mqtt_open(struct mqtt*);
void mqtt_close(struct mqtt*);
int mqtt_connect(struct mqtt*, const char* bind_address);
int mqtt_disconnect(struct mqtt*);
int mqtt_thread_start(struct mqtt*);
int mqtt_thread_stop(struct mqtt*);

#ifdef __cplusplus
}
#endif

#endif // MQTT_H
