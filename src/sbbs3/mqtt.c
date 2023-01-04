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

const char* server_state_desc(enum server_state state)
{
	switch(state) {
		case SERVER_STOPPED: return "stopped";
		case SERVER_INIT: return "initializing";
		case SERVER_READY: return "ready";
		case SERVER_RELOADING: return "reloading";
		case SERVER_STOPPING: return "stopping";
		case SERVER_DISCONNECTED: return "disconnected";
		default: return "???";
	}
}

int mqtt_init(struct mqtt* mqtt, scfg_t* cfg, struct startup* startup)
{
	char hostname[256]="undefined-hostname";

	if(mqtt == NULL || cfg == NULL || startup == NULL)
		return MQTT_FAILURE;
	if(!cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	mqtt->handle = NULL;
	mqtt->cfg = cfg;
	mqtt->startup = startup;
	listInit(&mqtt->client_list, LINK_LIST_MUTEX);
#ifdef _WIN32
	WSADATA WSAData;	 
	WSAStartup(MAKEWORD(1,1), &WSAData);
#endif
	gethostname(hostname, sizeof(hostname));
	mqtt->host = strdup(hostname);
#ifdef USE_MOSQUITTO
	return mosquitto_lib_init();
#endif
	return MQTT_FAILURE;
}

static char* format_topic(struct mqtt* mqtt, enum server_type type, enum topic_depth depth, char* str, size_t size, const char* sbuf)
{
	switch(depth) {
		case TOPIC_ROOT:
			safe_snprintf(str, size, "sbbs/%s", sbuf);
			break;
		case TOPIC_BBS:
			safe_snprintf(str, size, "sbbs/%s/%s", mqtt->cfg->sys_id, sbuf);
			break;
		case TOPIC_BBS_LEVEL:
			safe_snprintf(str, size, "sbbs/%s", mqtt->cfg->sys_id);
			break;
		case TOPIC_HOST:
			safe_snprintf(str, size, "sbbs/%s/%s/%s", mqtt->cfg->sys_id, mqtt->host, sbuf);
			break;
		case TOPIC_HOST_LEVEL:
			safe_snprintf(str, size, "sbbs/%s/%s", mqtt->cfg->sys_id, mqtt->host);
			break;
		case TOPIC_SERVER:
			safe_snprintf(str, size, "sbbs/%s/%s/%s/%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc(type), sbuf);
			break;
		case TOPIC_SERVER_LEVEL:
			safe_snprintf(str, size, "sbbs/%s/%s/%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc(type));
			break;
		case TOPIC_EVENT:
			safe_snprintf(str, size, "sbbs/%s/%s/event/%s", mqtt->cfg->sys_id, mqtt->host, sbuf);
			break;
		case TOPIC_OTHER:
		default:
			safe_snprintf(str, size, "%s", sbuf);
			break;
	}
	return str;
}

char* mqtt_topic(struct mqtt* mqtt, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024]="";

	if(fmt != NULL) {
		va_start(argptr, fmt);
		vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
		sbuf[sizeof(sbuf) - 1]=0;
		va_end(argptr);
	}

	return format_topic(mqtt, mqtt->startup->type, depth, str, size, sbuf);
}

static int mqtt_sub(struct mqtt* mqtt, const char* topic)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL && topic != NULL) {
		return mosquitto_subscribe(mqtt->handle, /* msg-id: */NULL, topic, mqtt->cfg->mqtt.subscribe_qos);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_subscribe(struct mqtt* mqtt, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr, fmt);
    vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1]=0;
    va_end(argptr);

	format_topic(mqtt, mqtt->startup->type, depth, str, size, sbuf);

	return mqtt_sub(mqtt, str);
}

int mqtt_lputs(struct mqtt* mqtt, enum topic_depth depth, int level, const char* str)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	if(level > mqtt->cfg->mqtt.log_level)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL && str != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "log/%d", level);
		char lvl[32];
		sprintf(lvl, "%d", level);
		mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.publish_qos,
			/* retain */false,
			/* properties */NULL);
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "log");
		mosquitto_property* props = NULL;
		mosquitto_property_add_string_pair(&props, MQTT_PROP_USER_PROPERTY, "level", lvl);
		int result = mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.publish_qos,
			/* retain */false,
			/* properties */props);
		mosquitto_property_free_all(&props);
		return result;
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_noval(struct mqtt* mqtt, enum topic_depth depth, const char* key)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */0,
			/* payload */NULL,
			/* qos */mqtt->cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}


int mqtt_pub_strval(struct mqtt* mqtt, enum topic_depth depth, const char* key, const char* str)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_uintval(struct mqtt* mqtt, enum topic_depth depth, const char* key, ulong value)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL) {
		char str[128];
		sprintf(str, "%lu", value);
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.publish_qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_message(struct mqtt* mqtt, enum topic_depth depth, const char* key, const void* buf, size_t len)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */len,
			/* payload */buf,
			/* qos */mqtt->cfg->mqtt.publish_qos,
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

int mqtt_open(struct mqtt* mqtt)
{
	char client_id[256];
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(mqtt->handle != NULL) // already open
		return MQTT_FAILURE;
	snprintf(client_id, sizeof(client_id), "sbbs-%s-%s-%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc(mqtt->startup->type));
#ifdef USE_MOSQUITTO
	mqtt->handle = mosquitto_new(client_id, /* clean_session: */true, /* userdata: */mqtt);
	return mqtt->handle == NULL ? MQTT_FAILURE : MQTT_SUCCESS;
#else
	return MQTT_FAILURE;
#endif
}

void mqtt_close(struct mqtt* mqtt)
{
#ifdef USE_MOSQUITTO
	if(mqtt->handle != NULL) {
		mosquitto_destroy(mqtt->handle);
		mqtt->handle = NULL;
		listFree(&mqtt->client_list);
	}
#endif
	FREE_AND_NULL(mqtt->host);
}

static int pw_callback(char* buf, int size, int rwflag, void* userdata)
{
	struct mqtt* mqtt = (struct mqtt*)userdata;

	strncpy(buf, mqtt->cfg->mqtt.tls.keypass, size);
	return strlen(mqtt->cfg->mqtt.tls.keypass);
}

static char* server_state_str(char* str, size_t size, enum server_state state)
{
	snprintf(str, size, "%u\t%s", state, server_state_desc(state));
	return str;
}

int mqtt_connect(struct mqtt* mqtt, const char* bind_address)
{
	if(mqtt == NULL || mqtt->handle == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	char topic[128];
	char* username = mqtt->cfg->mqtt.username;
	char* password = mqtt->cfg->mqtt.password;
	if(*username == '\0')
		username = NULL;
	if(*password == '\0')
		password = NULL;
	mosquitto_int_option(mqtt->handle, MOSQ_OPT_PROTOCOL_VERSION, mqtt->cfg->mqtt.protocol_version);
	mosquitto_username_pw_set(mqtt->handle, username, password);
	char value[128];
	server_state_str(value, sizeof(value), SERVER_DISCONNECTED);
	mosquitto_will_set(mqtt->handle
		,mqtt_topic(mqtt, TOPIC_SERVER_LEVEL, topic, sizeof(topic), NULL)
		,strlen(value), value, /* QOS: */2, /* retain: */true);
	if(mqtt->cfg->mqtt.tls.mode == MQTT_TLS_CERT) {
		char* certfile = NULL;
		char* keyfile = NULL;
		if(mqtt->cfg->mqtt.tls.certfile[0] && mqtt->cfg->mqtt.tls.keyfile[0]) {
			certfile = mqtt->cfg->mqtt.tls.certfile;
			keyfile = mqtt->cfg->mqtt.tls.keyfile;
		}
		int result = mosquitto_tls_set(mqtt->handle,
			mqtt->cfg->mqtt.tls.cafile,
			NULL, // capath
			certfile,
			keyfile,
			pw_callback);
		if(result != MOSQ_ERR_SUCCESS)
			return result;
	}
	else if(mqtt->cfg->mqtt.tls.mode == MQTT_TLS_PSK) {
		int result = mosquitto_tls_psk_set(mqtt->handle,
			mqtt->cfg->mqtt.tls.psk,
			mqtt->cfg->mqtt.tls.identity,
			NULL // ciphers (default)
			);
		if(result != MOSQ_ERR_SUCCESS)
			return result;
	}
	return mosquitto_connect_bind(mqtt->handle,
		mqtt->cfg->mqtt.broker_addr,
		mqtt->cfg->mqtt.broker_port,
		mqtt->cfg->mqtt.keepalive,
		bind_address);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_disconnect(struct mqtt* mqtt)
{
	if(mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_disconnect(mqtt->handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_start(struct mqtt* mqtt)
{
	if(mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_start(mqtt->handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_stop(struct mqtt* mqtt)
{
	if(mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_stop(mqtt->handle, /* force: */false);
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
	return lputs(level, sbuf);
}

#ifdef USE_MOSQUITTO
static void mqtt_message_received(struct mosquitto* mosq, void* cbdata, const struct mosquitto_message* msg)
{
	char topic[128];
	struct mqtt* mqtt = (struct mqtt*)cbdata;

	if(mqtt->startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)mqtt->startup;
		for(int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "nodes/%d/input", i);
			if(strcmp(msg->topic, topic) != 0)
				continue;
			if(bbs_startup->node_inbuf != NULL && bbs_startup->node_inbuf[i - 1] != NULL)
				RingBufWrite(bbs_startup->node_inbuf[i - 1], msg->payload, msg->payloadlen);
			return;
		}
	}
	if(strcmp(msg->topic, mqtt_topic(mqtt, TOPIC_HOST, topic, sizeof(topic), "recycle")) == 0
		|| strcmp(msg->topic, mqtt_topic(mqtt, TOPIC_SERVER, topic, sizeof(topic), "recycle")) == 0) {
		mqtt->startup->recycle_now = true;
		return;
	}
}
#endif // USE_MOSQUITTO

int mqtt_startup(struct mqtt* mqtt, scfg_t* cfg, struct startup* startup, const char* version
	,int (*lputs)(int level, const char* str))
{
	int result = MQTT_FAILURE;
	char str[128];

	if(mqtt == NULL || cfg == NULL || version == NULL)
		return MQTT_FAILURE;

	if(!cfg->mqtt.enabled)
		return MQTT_SUCCESS;

	result = mqtt_init(mqtt, cfg, startup);
	if(result != MQTT_SUCCESS) {
		lprintf(lputs, LOG_ERR, "MQTT init failure: %d", result);
	} else {
		lprintf(lputs, LOG_DEBUG, "MQTT lib: %s", mqtt_libver(str, sizeof(str)));
		result = mqtt_open(mqtt);
		if(result != MQTT_SUCCESS) {
			lprintf(lputs, LOG_ERR, "MQTT open failure: %d", result);
		} else {
			result = mqtt_thread_start(mqtt);
			if(result != MQTT_SUCCESS) {
				lprintf(lputs, LOG_ERR, "Error %d starting pub/sub thread", result);
				mqtt_close(mqtt);
			} else {
				lprintf(lputs, LOG_DEBUG, "MQTT connecting to broker %s:%u", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
				result = mqtt_connect(mqtt, /* bind_address: */NULL);
				if(result == MQTT_SUCCESS) {
					lprintf(lputs, LOG_INFO, "MQTT broker-connect (%s:%d) successful", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
				} else {
					lprintf(lputs, LOG_ERR, "MQTT broker-connect (%s:%d) failure: %d", cfg->mqtt.broker_addr, cfg->mqtt.broker_port, result);
					mqtt_close(mqtt);
				}
			}
		}
	}
	mqtt_server_state(mqtt, SERVER_INIT);
	mqtt_pub_strval(mqtt, TOPIC_BBS_LEVEL, NULL, mqtt->cfg->sys_name);
	mqtt_pub_strval(mqtt, TOPIC_HOST_LEVEL, NULL, startup->host_name);
	mqtt_pub_strval(mqtt, TOPIC_SERVER, "version", version);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "served", mqtt->served);

#ifdef USE_MOSQUITTO
	mosquitto_message_callback_set(mqtt->handle, mqtt_message_received);
#endif
	if(mqtt->startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)startup;
		char str[128];
		for(int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "nodes/%d/input", i);
		}
	}
	mqtt_pub_noval(mqtt, TOPIC_SERVER, "recycle");
	mqtt_subscribe(mqtt, TOPIC_SERVER, str, sizeof(str), "recycle");
	mqtt_subscribe(mqtt, TOPIC_HOST, str, sizeof(str), "recycle");

	return result;
}

int mqtt_server_state(struct mqtt* mqtt, enum server_state state)
{
	char str[128];

	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

	if(mqtt->cfg->mqtt.verbose) {
		char tmp[256];
		char errors[64] = "";
		if(mqtt->error_count)
			snprintf(errors, sizeof(errors), "%lu errors", mqtt->error_count);
		char served[64] = "";
		if(mqtt->served)
			snprintf(served, sizeof(served), "%lu served", mqtt->served);
		char max_clients[64] = "";
		if(mqtt->max_clients)
			snprintf(max_clients, sizeof(max_clients), "/%u", mqtt->max_clients);
		char clients[64] = "";
		if(mqtt->client_list.count)
			snprintf(clients, sizeof(clients), "%u%s clients", mqtt->client_list.count, max_clients);
		snprintf(str, sizeof(str), "%s\t%s\t%s\t%s"
			,server_state_str(tmp, sizeof(tmp), state)
			,clients
			,served
			,errors);
	} else
		server_state_str(str, sizeof(str), state);
	mqtt->server_state = state;
	return mqtt_pub_strval(mqtt, TOPIC_SERVER_LEVEL, NULL, str);
}

int mqtt_errormsg(struct mqtt* mqtt, int level, const char* msg)
{
	if(mqtt == NULL)
		return MQTT_FAILURE;
	++mqtt->error_count;
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "error_count", mqtt->error_count);
	if(mqtt->cfg->mqtt.verbose)
		mqtt_server_state(mqtt, mqtt->server_state);
	return mqtt_pub_strval(mqtt, TOPIC_BBS, "error", msg);
}

int mqtt_client_max(struct mqtt* mqtt, ulong count)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	mqtt->max_clients = count;
	if(mqtt->cfg->mqtt.verbose)
		mqtt_server_state(mqtt, mqtt->server_state);
	return mqtt_pub_uintval(mqtt, TOPIC_SERVER, "max_clients", count);
}

int mqtt_client_on(struct mqtt* mqtt, BOOL on, int sock, client_t* client, BOOL update)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

	if(!mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;

	listLock(&mqtt->client_list);
	if(on) {
		if(update) {
			list_node_t*	node;

			if((node=listFindTaggedNode(&mqtt->client_list, sock)) != NULL)
				memcpy(node->data, client, sizeof(client_t));
		} else {
			listAddNodeData(&mqtt->client_list, client, sizeof(client_t), sock, LAST_NODE);
			mqtt->served++;
		}
	} else
		listRemoveTaggedNode(&mqtt->client_list, sock, /* free_data: */TRUE);

	str_list_t list = strListInit();
	for(list_node_t* node = mqtt->client_list.first; node != NULL; node = node->next) {
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
	listUnlock(&mqtt->client_list);
	char buf[1024]; // TODO
	strListJoin(list, buf, sizeof(buf), "\n");
	strListFree(&list);

	mqtt_client_count(mqtt);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "served", mqtt->served);
	return mqtt_pub_strval(mqtt, TOPIC_SERVER, "client_list", buf);
}

int mqtt_client_count(struct mqtt* mqtt)
{
	if(mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if(mqtt->cfg->mqtt.verbose)
		mqtt_server_state(mqtt, mqtt->server_state);
	return mqtt_pub_uintval(mqtt, TOPIC_SERVER, "client_count", mqtt->client_list.count);
}

void mqtt_shutdown(struct mqtt* mqtt)
{
	if(mqtt != NULL && mqtt->cfg != NULL && mqtt->cfg->mqtt.enabled) {
		mqtt_disconnect(mqtt);
		mqtt_thread_stop(mqtt);
		mqtt_close(mqtt);
	}
}
