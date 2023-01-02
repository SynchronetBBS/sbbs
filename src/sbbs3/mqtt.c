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

#include <string.h>

#include "mqtt.h"
#include "startup.h"

const char* server_type_desc(enum server_type type)
{
	switch(type) {
		case SERVER_TERM:	return "term";
		case SERVER_FTP:	return "ftp";
		case SERVER_WEB:	return "web";
		case SERVER_MAIL:	return "mail";
		case SERVER_SERVICES: return "srvc";
		case SERVER_COUNT:
		default:
			return "???";
	}
}

int mqtt_init(struct startup* startup, scfg_t* cfg)
{
	char hostname[256]="undefined-hostname";

	if(startup == NULL || cfg == NULL)
		return MQTT_FAILURE;
	if(!cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	memset(&startup->mqtt, 0, sizeof(startup->mqtt));
	startup->mqtt.cfg = cfg;
	listInit(&startup->mqtt.client_list, LINK_LIST_MUTEX);
#ifdef _WIN32
	WSADATA WSAData;	 
	WSAStartup(MAKEWORD(1,1), &WSAData);
#endif
	gethostname(hostname, sizeof(hostname));
	startup->mqtt.host = strdup(hostname);
#ifdef USE_MOSQUITTO
	return mosquitto_lib_init();
#endif
	return MQTT_FAILURE;
}

static char* format_topic(struct startup* startup, enum server_type type, enum topic_depth depth, char* str, size_t size, const char* sbuf)
{
	switch(depth) {
		case TOPIC_ROOT:
			safe_snprintf(str, size, "sbbs/%s", sbuf);
			break;
		case TOPIC_BBS:
			safe_snprintf(str, size, "sbbs/%s/%s", startup->mqtt.cfg->sys_id, sbuf);
			break;
		case TOPIC_HOST:
			safe_snprintf(str, size, "sbbs/%s/%s/%s", startup->mqtt.cfg->sys_id, startup->mqtt.host, sbuf);
			break;
		case TOPIC_SERVER:
			safe_snprintf(str, size, "sbbs/%s/%s/%s/%s", startup->mqtt.cfg->sys_id, startup->mqtt.host, server_type_desc(type), sbuf);
			break;
		case TOPIC_EVENT:
			safe_snprintf(str, size, "sbbs/%s/%s/event/%s", startup->mqtt.cfg->sys_id, startup->mqtt.host, sbuf);
			break;
		case TOPIC_OTHER:
		default:
			safe_snprintf(str, size, "%s", sbuf);
			break;
	}
	return str;
}

char* mqtt_topic(struct startup* startup, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr, fmt);
    vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1]=0;
    va_end(argptr);

	return format_topic(startup, startup->type, depth, str, size, sbuf);
}

static int mqtt_sub(struct startup* startup, const char* topic)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL && topic != NULL) {
		return mosquitto_subscribe(startup->mqtt.handle, /* msg-id: */NULL, topic, startup->mqtt.cfg->mqtt.subscribe_qos);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_subscribe(struct startup* startup, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr, fmt);
    vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1]=0;
    va_end(argptr);

	format_topic(startup, startup->type, depth, str, size, sbuf);

	return mqtt_sub(startup, str);
}

int mqtt_lputs(struct startup* startup, enum topic_depth depth, int level, const char* str)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	if(level > startup->mqtt.cfg->mqtt.log_level)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL && str != NULL) {
		char sub[128];
		mqtt_topic(startup, depth, sub, sizeof(sub), "log/%d", level);
		char lvl[32];
		sprintf(lvl, "%d", level);
		mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */false,
			/* properties */NULL);
		mqtt_topic(startup, depth, sub, sizeof(sub), "log");
		mosquitto_property* props = NULL;
		mosquitto_property_add_string_pair(&props, MQTT_PROP_USER_PROPERTY, "level", lvl);
		int result = mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */false,
			/* properties */props);
		mosquitto_property_free_all(&props);
		return result;
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_noval(struct startup* startup, enum topic_depth depth, const char* key)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL) {
		char sub[128];
		mqtt_topic(startup, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */0,
			/* payload */NULL,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}


int mqtt_pub_strval(struct startup* startup, enum topic_depth depth, const char* key, const char* str)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL) {
		char sub[128];
		mqtt_topic(startup, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_uintval(struct startup* startup, enum topic_depth depth, const char* key, ulong value)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL) {
		char str[128];
		sprintf(str, "%lu", value);
		char sub[128];
		mqtt_topic(startup, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_message(struct startup* startup, enum topic_depth depth, const char* key, const void* buf, size_t len)
{
	if(startup == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;
	if(!startup->mqtt.cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL) {
		char sub[128];
		mqtt_topic(startup, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(startup->mqtt.handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */len,
			/* payload */buf,
			/* qos */startup->mqtt.cfg->mqtt.publish_qos,
			/* retain */false,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

char* mqtt_libver(char* str, size_t size)
{
#ifdef USE_MOSQUITTO
		int major, minor, revision;
		mosquitto_lib_version(&major, &minor, &revision);
		safe_snprintf(str, size, "mosquitto %d.%d.%d", major, minor, revision);
		return str;
#else
		return NULL;
#endif
}

int mqtt_open(struct startup* startup)
{
	char client_id[256];
	if(startup == NULL)
		return MQTT_FAILURE;
	if(startup->mqtt.handle != NULL) // already open
		return MQTT_FAILURE;
	if(startup->mqtt.shared_instance)
		snprintf(client_id, sizeof(client_id), "sbbs-%s", startup->mqtt.host);
	else
		snprintf(client_id, sizeof(client_id), "sbbs-%s-%s", startup->mqtt.host, server_type_desc(startup->type));
#ifdef USE_MOSQUITTO
	startup->mqtt.handle = mosquitto_new(client_id, /* clean_session: */true, /* userdata: */startup);
	return startup->mqtt.handle == NULL ? MQTT_FAILURE : MQTT_SUCCESS;
#else
	return MQTT_FAILURE;
#endif
}

void mqtt_close(struct startup* startup)
{
#ifdef USE_MOSQUITTO
	if(startup->mqtt.handle != NULL) {
		mosquitto_destroy(startup->mqtt.handle);
		startup->mqtt.handle = NULL;
		listFree(&startup->mqtt.client_list);
	}
#endif
	FREE_AND_NULL(startup->mqtt.host);
}

static int pw_callback(char* buf, int size, int rwflag, void* userdata)
{
	struct startup* startup = (struct startup*)userdata;

	strncpy(buf, startup->mqtt.cfg->mqtt.tls.keypass, size);
	return strlen(startup->mqtt.cfg->mqtt.tls.keypass);
}

int mqtt_connect(struct startup* startup, const char* bind_address)
{
	if(startup == NULL || startup->mqtt.handle == NULL || startup->mqtt.cfg == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	char topic[128];
	char* username = startup->mqtt.cfg->mqtt.username;
	char* password = startup->mqtt.cfg->mqtt.password;
	if(*username == '\0')
		username = NULL;
	if(*password == '\0')
		password = NULL;
	mosquitto_int_option(startup->mqtt.handle, MOSQ_OPT_PROTOCOL_VERSION, startup->mqtt.cfg->mqtt.protocol_version);
	mosquitto_username_pw_set(startup->mqtt.handle, username, password);
	const char* value = "disconnected";
	mosquitto_will_set(startup->mqtt.handle
		,mqtt_topic(startup, TOPIC_HOST, topic, sizeof(topic), "status")
		,strlen(value), value, /* QOS: */2, /* retain: */true);
	if(startup->mqtt.cfg->mqtt.tls.mode == MQTT_TLS_CERT) {
		char* certfile = NULL;
		char* keyfile = NULL;
		if(startup->mqtt.cfg->mqtt.tls.certfile[0] && startup->mqtt.cfg->mqtt.tls.keyfile[0]) {
			certfile = startup->mqtt.cfg->mqtt.tls.certfile;
			keyfile = startup->mqtt.cfg->mqtt.tls.keyfile;
		}
		int result = mosquitto_tls_set(startup->mqtt.handle,
			startup->mqtt.cfg->mqtt.tls.cafile,
			NULL, // capath
			certfile,
			keyfile,
			pw_callback);
		if(result != MOSQ_ERR_SUCCESS)
			return result;
	}
	else if(startup->mqtt.cfg->mqtt.tls.mode == MQTT_TLS_PSK) {
		int result = mosquitto_tls_psk_set(startup->mqtt.handle,
			startup->mqtt.cfg->mqtt.tls.psk,
			startup->mqtt.cfg->mqtt.tls.identity,
			NULL // ciphers (default)
			);
		if(result != MOSQ_ERR_SUCCESS)
			return result;
	}
	return mosquitto_connect_bind(startup->mqtt.handle,
		startup->mqtt.cfg->mqtt.broker_addr,
		startup->mqtt.cfg->mqtt.broker_port,
		startup->mqtt.cfg->mqtt.keepalive,
		bind_address);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_disconnect(struct startup* startup)
{
	if(startup == NULL || startup->mqtt.handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_disconnect(startup->mqtt.handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_start(struct startup* startup)
{
	if(startup == NULL || startup->mqtt.handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_start(startup->mqtt.handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_stop(struct startup* startup)
{
	if(startup == NULL || startup->mqtt.handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_stop(startup->mqtt.handle, /* force: */false);
#else
	return MQTT_FAILURE;
#endif
}

static int lprintf(int (*lputs)(int level, const char* str), int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	if(lputs == NULL)
		return -1;
	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	return lputs(level,sbuf);
}

#ifdef USE_MOSQUITTO
static void mqtt_message_received(struct mosquitto* mosq, struct startup* startup, const struct mosquitto_message* msg)
{
	char topic[128];
//	lprintf(LOG_DEBUG, "MQTT message received (%d bytes) on %s", msg->payloadlen, msg->topic);
	if(startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)startup;
		for(int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			mqtt_topic(startup, TOPIC_BBS, topic, sizeof(topic), "node%d/input", i);
			if(strcmp(msg->topic, topic) != 0)
				continue;
			if(bbs_startup->node_inbuf != NULL && bbs_startup->node_inbuf[i - 1] != NULL)
				RingBufWrite(bbs_startup->node_inbuf[i - 1], msg->payload, msg->payloadlen);
			return;
		}
	}
	if(startup->mqtt.shared_instance) {
		if(strcmp(msg->topic, mqtt_topic(startup, TOPIC_HOST, topic, sizeof(topic), "recycle")) == 0) {
			// recycle all servers hosted by this instance
			for(int i = 0; i < SERVER_COUNT; i++) {
				if(startup->startup[i] == NULL)
					continue;
				startup->startup[i]->recycle_now = TRUE;
			}
			return;
		}
		for(int i = 0; i < SERVER_COUNT; i++) {
			if(startup->startup[i] == NULL)
				continue;
			char topic[128];
			if(strcmp(msg->topic, format_topic(startup, i, TOPIC_SERVER, topic, sizeof(topic), "recycle")) == 0) {
				// recycle just the specified server
				startup->startup[i]->recycle_now = TRUE;
				return;
			}
		}
	} else {
		if(strcmp(msg->topic, mqtt_topic(startup, TOPIC_SERVER, topic, sizeof(topic), "recycle")) == 0) {
			startup->recycle_now = true;
			return;
		}
	}
}
#endif // USE_MOSQUITTO

int mqtt_startup(struct startup* startup, scfg_t* cfg, const char* version
	,int (*lputs)(int level, const char* str)
	,BOOL shared_instance)
{
	int result = MQTT_FAILURE;
	char str[128];

	if(startup == NULL)
		return MQTT_FAILURE;

	if(cfg->mqtt.enabled) {
		result = mqtt_init(startup, cfg);
		if(result != MQTT_SUCCESS) {
			lprintf(lputs, LOG_INFO, "MQTT init failure: %d", result);
		} else {
			lprintf(lputs, LOG_INFO, "MQTT lib: %s", mqtt_libver(str, sizeof(str)));
			result = mqtt_open(startup);
			if(result != MQTT_SUCCESS) {
				lprintf(lputs, LOG_ERR, "MQTT open failure: %d", result);
			} else {
				result = mqtt_thread_start(startup);
				if(result != MQTT_SUCCESS) {
					lprintf(lputs, LOG_ERR, "Error %d starting pub/sub thread", result);
					mqtt_close(startup);
				} else {
					lprintf(lputs, LOG_INFO, "MQTT connecting to broker %s:%u", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
					result = mqtt_connect(startup, /* bind_address: */NULL);
					if(result == MQTT_SUCCESS) {
						lprintf(lputs, LOG_INFO, "MQTT broker-connect (%s:%d) successful", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
					} else {
						lprintf(lputs, LOG_ERR, "MQTT broker-connect (%s:%d) failure: %d", cfg->mqtt.broker_addr, cfg->mqtt.broker_port, result);
						mqtt_close(startup);
					}
				}
			}
		}
	}
	startup->mqtt.shared_instance = shared_instance;
	mqtt_pub_strval(startup, TOPIC_HOST, "version", version);
	mqtt_pub_strval(startup, TOPIC_HOST, "status", "initializing");
	for(enum topic_depth depth = TOPIC_HOST; depth <= TOPIC_SERVER; depth++) {
		mqtt_pub_noval(startup, depth, "error_count");
		mqtt_pub_noval(startup, depth, "thread_count");
		mqtt_pub_noval(startup, depth, "socket_count");
		mqtt_pub_noval(startup, depth, "client_count");
		mqtt_pub_noval(startup, depth, "client_list");
		mqtt_pub_noval(startup, depth, "served");
	}

#ifdef USE_MOSQUITTO
	mosquitto_message_callback_set(startup->mqtt.handle, mqtt_message_received);
#endif
	if(startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)startup;
		char str[128];
		for(int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			mqtt_subscribe(startup, TOPIC_BBS, str, sizeof(str), "node%d/input", i);
		}
	}
	if(startup->mqtt.shared_instance) {
		for(int i = 0; i <= SERVER_COUNT; i++) {
			char topic[128];
			mqtt_sub(startup, format_topic(startup, i, TOPIC_SERVER, topic, sizeof(topic), "recycle"));
		}
	} else
		mqtt_subscribe(startup, TOPIC_SERVER, str, sizeof(str), "recycle");

	// recycle-all topic
	mqtt_subscribe(startup, TOPIC_HOST, str, sizeof(str), "recycle");

	return result;
}

int mqtt_online(struct startup* startup)
{
	return mqtt_pub_strval(startup, TOPIC_HOST, "status", "online");
}

int mqtt_server_state(struct startup* startup, enum server_state state)
{
	return mqtt_pub_uintval(startup, TOPIC_SERVER, "state", state);
}

int mqtt_server_version(struct startup* startup, const char* str)
{
	return mqtt_pub_strval(startup, TOPIC_SERVER, "version", str);
}

int mqtt_errormsg(struct startup* startup, int level, const char* msg)
{
	if(startup == NULL)
		return MQTT_FAILURE;
	++startup->mqtt.error_count;
	mqtt_pub_uintval(startup, TOPIC_SERVER, "error_count", startup->mqtt.error_count);
	return mqtt_pub_strval(startup, TOPIC_HOST, "error", msg);
}

int mqtt_thread_count(struct startup* startup, enum topic_depth depth, ulong count)
{
	return mqtt_pub_uintval(startup, depth, "thread_count", count);
}

int mqtt_socket_count(struct startup* startup, enum topic_depth depth, ulong count)
{
	return mqtt_pub_uintval(startup, depth, "socket_count", count);
}

int mqtt_client_count(struct startup* startup, enum topic_depth depth, ulong count)
{
	return mqtt_pub_uintval(startup, depth, "client_count", count);
}

int mqtt_client_max(struct startup* startup, ulong count)
{
	return mqtt_pub_uintval(startup, TOPIC_SERVER, "max_clients", count);
}

int mqtt_client_on(struct startup* startup, BOOL on, int sock, client_t* client, BOOL update)
{
	if(startup == NULL)
		return MQTT_FAILURE;

	listLock(&startup->mqtt.client_list);
	if(on) {
		if(update) {
			list_node_t*	node;

			if((node=listFindTaggedNode(&startup->mqtt.client_list, sock)) != NULL)
				memcpy(node->data, client, sizeof(client_t));
		} else {
			listAddNodeData(&startup->mqtt.client_list, client, sizeof(client_t), sock, LAST_NODE);
			startup->mqtt.served++;
		}
	} else
		listRemoveTaggedNode(&startup->mqtt.client_list, sock, /* free_data: */TRUE);

	str_list_t list = strListInit();
	for(list_node_t* node = startup->mqtt.client_list.first; node != NULL; node = node->next) {
		client_t* client = node->data;
		strListAppendFormat(&list, "%ld\t%s\t%s\t%s\t%s\t%u\t%lu"
			,node->tag
			,client->protocol
			,client->user
			,client->addr
			,client->host
			,client->port
			,(ulong)client->time
			);
		}
	listUnlock(&startup->mqtt.client_list);
	char buf[1024]; // TODO
	strListJoin(list, buf, sizeof(buf), "\n");
	strListFree(&list);

	enum topic_depth depth = startup->mqtt.shared_instance ? TOPIC_HOST : TOPIC_SERVER;
	mqtt_client_count(startup, depth, startup->mqtt.client_list.count);
	mqtt_served_count(startup, depth, startup->mqtt.served);
	return mqtt_pub_strval(startup, depth, "client_list", buf);
}

int mqtt_served_count(struct startup* startup, enum topic_depth depth, ulong count)
{
	return mqtt_pub_uintval(startup, depth, "served", count);
}

int mqtt_terminating(struct startup* startup)
{
	return mqtt_pub_strval(startup, TOPIC_HOST, "status", "terminating");
}

void mqtt_shutdown(struct startup* startup)
{
	mqtt_pub_strval(startup, TOPIC_HOST, "status", "offline");
	mqtt_disconnect(startup);
	mqtt_thread_stop(startup);
	mqtt_close(startup);
}
