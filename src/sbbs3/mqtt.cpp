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
#include "mqtt_client.h"
#include "mqtt_broker.h"
#include "startup.h"
#include "xpdatetime.h"
#include "date_str.h"
#include "userdat.h"
#include "scfglib.h"    /* is_valid_dirnum() */
#include "genwrap.h"    /* truncsp() */
#include "threadwrap.h"

#include <mutex>

static inline mqtt5::LocalClient *local_client(struct mqtt *mqtt)
{
	return static_cast<mqtt5::LocalClient *>(mqtt->local);
}

#ifndef USE_MOSQUITTO
static inline mqtt5::Client *ext_client(struct mqtt *mqtt)
{
	return static_cast<mqtt5::Client *>(mqtt->handle);
}
#endif

static int local_publish(struct mqtt *mqtt, const char *topic,
                          const void *payload, size_t len, int qos, bool retain,
                          const mqtt5::Properties *props = nullptr)
{
	auto *broker = mqtt5::Broker::instance();
	if (!broker)
		return MQTT_FAILURE;
	return broker->local_publish(local_client(mqtt), topic, payload, len, qos, retain, props);
}

static int local_subscribe(struct mqtt *mqtt, const char *topic, int qos)
{
	auto *broker = mqtt5::Broker::instance();
	if (!broker)
		return MQTT_FAILURE;
	return broker->local_subscribe(local_client(mqtt), topic, qos);
}

static struct startup *s_broker_startup = NULL;
static bool s_broker_logging = false;

static int mqtt_broker_lputs(void *cbdata, int level, const char *str)
{
	(void)cbdata;
	struct startup *startup = s_broker_startup;
	if (startup && startup->lputs)
		startup->lputs(startup->cbdata, level, str);

	if (s_broker_logging || !str)
		return 0;
	s_broker_logging = true;

	auto *broker = mqtt5::Broker::instance();
	if (broker) {
		char topic[64];
		char timestamp[32];
		time_to_isoDateTimeStr(time(NULL), xpTimeZone_local(), timestamp, sizeof(timestamp));

		mqtt5::Properties props;
		props.add(mqtt5::PROP_USER_PROPERTY,
		          mqtt5::PropertyValue::from_pair("time", timestamp));

		snprintf(topic, sizeof(topic), "$SYS/broker/log/%d", level);
		broker->publish_sys(topic, str, strlen(str), &props);
	}

	s_broker_logging = false;
	return 0;
}

static void mqtt_local_message_callback(void *cbdata, const char *topic,
                                        const void *payload, size_t len)
{
	auto *m = static_cast<struct mqtt *>(cbdata);
	if (m && m->cfg)
		mqtt_dispatch_message(m, topic, payload, len);
}

#ifndef USE_MOSQUITTO
static int (*s_mqtt_lputs)(int level, const char *str);
static pthread_mutex_t s_mqtt_lputs_mutex;
static pthread_once_t s_mqtt_lputs_once = PTHREAD_ONCE_INIT;

static void mqtt_lputs_mutex_init(void)
{
	pthread_mutex_init(&s_mqtt_lputs_mutex, NULL);
}

static int mqtt_lprintf_adapter(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	if (s_mqtt_lputs != NULL)
		return s_mqtt_lputs(level, sbuf);
	return 0;
}
#endif

const char* server_type_desc(enum server_type type)
{
	switch (type) {
		case SERVER_TERM:   return "term";
		case SERVER_FTP:    return "ftp";
		case SERVER_WEB:    return "web";
		case SERVER_MAIL:   return "mail";
		case SERVER_SERVICES: return "srvc";
		case SERVER_COUNT:
		default:
			return "???";
	}
}

const char* server_state_desc(enum server_state state)
{
	switch (state) {
		case SERVER_STOPPED: return "stopped";
		case SERVER_INIT: return "initializing";
		case SERVER_READY: return "ready";
		case SERVER_PAUSED: return "paused";
		case SERVER_RELOADING: return "reloading";
		case SERVER_STOPPING: return "stopping";
		default: return "???";
	}
}

const char* log_level_desc(int level)
{
	switch (level) {
		case LOG_DEBUG: return "debug";
		case LOG_INFO: return "info";
		case LOG_NOTICE: return "notice";
		case LOG_WARNING: return "warn";
		case LOG_ERR: return "error";
		default:
			if (level <= LOG_CRIT)
				return "critical";
			break;
	}
	return "????";
}

int mqtt_init(struct mqtt* mqtt, scfg_t* cfg, struct startup* startup)
{
	char hostname[256] = "undefined-hostname";

	if (mqtt == NULL || cfg == NULL || startup == NULL)
		return MQTT_FAILURE;
	mqtt->connected = false;
	if (!cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	mqtt->handle = NULL;
	mqtt->cfg = cfg;
	mqtt->startup = startup;
	listInit(&mqtt->client_list, LINK_LIST_MUTEX);
#ifdef _WIN32
	WSADATA WSAData;
	WSAStartup(MAKEWORD(1, 1), &WSAData);
#endif
	gethostname(hostname, sizeof(hostname));
	mqtt->host = strdup(hostname);
#ifdef USE_MOSQUITTO
	return mosquitto_lib_init();
#else
	return MQTT_SUCCESS;
#endif
}

static char* format_topic(struct mqtt* mqtt, enum server_type type, enum topic_depth depth, char* str, size_t size, const char* sbuf)
{
	switch (depth) {
		case TOPIC_ROOT:
			safe_snprintf(str, size, "sbbs/%s", sbuf);
			break;
		case TOPIC_BBS_LEVEL:
			safe_snprintf(str, size, "sbbs/%s", mqtt->cfg->sys_id);
			break;
		case TOPIC_BBS:
			safe_snprintf(str, size, "sbbs/%s/%s", mqtt->cfg->sys_id, sbuf);
			break;
		case TOPIC_BBS_ACTION:
			safe_snprintf(str, size, "sbbs/%s/action/%s", mqtt->cfg->sys_id, sbuf);
			break;
		case TOPIC_HOST_LEVEL:
			safe_snprintf(str, size, "sbbs/%s/host/%s", mqtt->cfg->sys_id, mqtt->host);
			break;
		case TOPIC_HOST:
			safe_snprintf(str, size, "sbbs/%s/host/%s/%s", mqtt->cfg->sys_id, mqtt->host, sbuf);
			break;
		case TOPIC_HOST_EVENT:
			safe_snprintf(str, size, "sbbs/%s/host/%s/event/%s", mqtt->cfg->sys_id, mqtt->host, sbuf);
			break;
		case TOPIC_SERVER:
			safe_snprintf(str, size, "sbbs/%s/host/%s/server/%s/%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc(type), sbuf);
			break;
		case TOPIC_SERVER_LEVEL:
			safe_snprintf(str, size, "sbbs/%s/host/%s/server/%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc(type));
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
	char*   p;
	va_list argptr;
	char    sbuf[1024] = "";

	if (fmt != NULL) {
		va_start(argptr, fmt);
		vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
		sbuf[sizeof(sbuf) - 1] = 0;
		va_end(argptr);
	}

	REPLACE_CHARS(sbuf, ' ', '_', p);
	format_topic(mqtt, (enum server_type)mqtt->startup->type, depth, str, size, sbuf);
	return str;
}

static int mqtt_sub(struct mqtt* mqtt, const char* topic)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	if (mqtt->local && topic != NULL)
		return local_subscribe(mqtt, topic, mqtt->cfg->mqtt.subscribe_qos);
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL && topic != NULL)
		return mosquitto_subscribe(mqtt->handle, /* msg-id: */ NULL, topic, mqtt->cfg->mqtt.subscribe_qos);
#else
	if (mqtt->handle != NULL && topic != NULL)
		return ext_client(mqtt)->subscribe(topic, mqtt->cfg->mqtt.subscribe_qos);
#endif
	return MQTT_FAILURE;
}

int mqtt_subscribe(struct mqtt* mqtt, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);

	format_topic(mqtt, (enum server_type)mqtt->startup->type, depth, str, size, sbuf);

	return mqtt_sub(mqtt, str);
}

int mqtt_lputs(struct mqtt* mqtt, enum topic_depth depth, int level, const char* str)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	if (level > mqtt->cfg->mqtt.log_level)
		return MQTT_SUCCESS;
	if (mqtt->local) {
		if (str != NULL) {
			int  result;
			char sub[128];
			char timestamp[32];
			time_to_isoDateTimeStr(time(NULL), xpTimeZone_local(), timestamp, sizeof(timestamp));
			mqtt5::Properties props;
			props.add(mqtt5::PROP_USER_PROPERTY, mqtt5::PropertyValue::from_pair("time", timestamp));
			mqtt_topic(mqtt, depth, sub, sizeof(sub), "log/%d", level);
			result = local_publish(mqtt, sub, str, strlen(str),
			                       mqtt->cfg->mqtt.publish_qos, true, &props);
			if (result == MQTT_SUCCESS) {
				char lvl[32];
				sprintf(lvl, "%d", level);
				props.add(mqtt5::PROP_USER_PROPERTY, mqtt5::PropertyValue::from_pair("level", lvl));
				mqtt_topic(mqtt, depth, sub, sizeof(sub), "log");
				result = local_publish(mqtt, sub, str, strlen(str),
				                       mqtt->cfg->mqtt.publish_qos, true, &props);
			}
			return result;
		}
		return MQTT_SUCCESS;
	}
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL && str != NULL) {
		int                 result;
		char                sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "log/%d", level);
		mosquitto_property* props = NULL;
		if (mqtt->cfg->mqtt.protocol_version >= 5) {
			char timestamp[32];
			time_to_isoDateTimeStr(time(NULL), xpTimeZone_local(), timestamp, sizeof(timestamp));
			mosquitto_property_add_string_pair(&props, MQTT_PROP_USER_PROPERTY, "time", timestamp);
		}
		result = mosquitto_publish_v5(mqtt->handle,
		                              /* mid: */ NULL,
		                              /* topic: */ sub,
		                              /* payloadlen */ strlen(str),
		                              /* payload */ str,
		                              /* qos */ mqtt->cfg->mqtt.publish_qos,
		                              /* retain */ true,
		                              /* properties */ props);
		if (result == MQTT_SUCCESS) {
			mqtt_topic(mqtt, depth, sub, sizeof(sub), "log");
			if (mqtt->cfg->mqtt.protocol_version >= 5) {
				char lvl[32];
				sprintf(lvl, "%d", level);
				mosquitto_property_add_string_pair(&props, MQTT_PROP_USER_PROPERTY, "level", lvl);
			}
			result = mosquitto_publish_v5(mqtt->handle,
			                              /* mid: */ NULL,
			                              /* topic: */ sub,
			                              /* payloadlen */ strlen(str),
			                              /* payload */ str,
			                              /* qos */ mqtt->cfg->mqtt.publish_qos,
			                              /* retain */ true,
			                              /* properties */ props);
		}
		mosquitto_property_free_all(&props);
		return result;
	}
#else
	if (mqtt->handle != NULL && str != NULL) {
		int  result;
		char sub[128];
		char timestamp[32];
		time_to_isoDateTimeStr(time(NULL), xpTimeZone_local(), timestamp, sizeof(timestamp));
		mqtt5::Properties props;
		props.add(mqtt5::PROP_USER_PROPERTY, mqtt5::PropertyValue::from_pair("time", timestamp));
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "log/%d", level);
		result = ext_client(mqtt)->publish(sub, str, strlen(str),
		                                   mqtt->cfg->mqtt.publish_qos, true, &props);
		if (result == MQTT_SUCCESS) {
			char lvl[32];
			sprintf(lvl, "%d", level);
			props.add(mqtt5::PROP_USER_PROPERTY, mqtt5::PropertyValue::from_pair("level", lvl));
			mqtt_topic(mqtt, depth, sub, sizeof(sub), "log");
			result = ext_client(mqtt)->publish(sub, str, strlen(str),
			                                   mqtt->cfg->mqtt.publish_qos, true, &props);
		}
		return result;
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_noval(struct mqtt* mqtt, enum topic_depth depth, const char* key)
{
	return mqtt_pub_strval(mqtt, depth, key, NULL);
}

int mqtt_pub_strval(struct mqtt* mqtt, enum topic_depth depth, const char* key, const char* str)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	char sub[128];
	mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
	if (mqtt->local)
		return local_publish(mqtt, sub, str, (str == NULL) ? 0 : strlen(str),
		                    mqtt->cfg->mqtt.publish_qos, true);
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL) {
		return mosquitto_publish_v5(mqtt->handle,
		                            /* mid: */ NULL,
		                            /* topic: */ sub,
		                            /* payloadlen */ (str == NULL) ? 0 : strlen(str),
		                            /* payload */ str,
		                            /* qos */ mqtt->cfg->mqtt.publish_qos,
		                            /* retain */ true,
		                            /* properties */ NULL);
	}
#else
	if (mqtt->handle != NULL)
		return ext_client(mqtt)->publish(sub, str, (str == NULL) ? 0 : strlen(str),
		                                mqtt->cfg->mqtt.publish_qos, true);
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_timestamped_msg(struct mqtt* mqtt, enum topic_depth depth, const char* key, time_t t, const char* msg)
{
	char timestamp[64];
	char str[1024];

	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	time_to_isoDateTimeStr(t, xpTimeZone_local(), timestamp, sizeof(timestamp));
	SAFEPRINTF2(str, "%s\t%s", timestamp, msg);
	return mqtt_pub_strval(mqtt, depth, key, str);
}

/* Publish to a host- or server-level topic with a key longer than 128 chars (room for IPv6). */
static int pub_long_key(struct mqtt* mqtt, enum topic_depth depth, const char* key, const char* str, bool retain)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	char sub[256];
	mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
	if (mqtt->local)
		return local_publish(mqtt, sub, str, (str == NULL) ? 0 : strlen(str),
		                    mqtt->cfg->mqtt.publish_qos, retain);
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL) {
		return mosquitto_publish_v5(mqtt->handle,
		                            /* mid: */ NULL,
		                            /* topic: */ sub,
		                            /* payloadlen */ (str == NULL) ? 0 : strlen(str),
		                            /* payload */ str,
		                            /* qos */ mqtt->cfg->mqtt.publish_qos,
		                            /* retain */ retain,
		                            /* properties */ NULL);
	}
#else
	if (mqtt->handle != NULL)
		return ext_client(mqtt)->publish(sub, str, (str == NULL) ? 0 : strlen(str),
		                                mqtt->cfg->mqtt.publish_qos, retain);
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_login_attempt(struct mqtt* mqtt, const login_attempt_t* attempt)
{
	char ip[INET6_ADDRSTRLEN];
	char iso_first[64];
	char iso_last[64];
	/* Worst-case escape expansion is 4 bytes per source byte (\xNN). */
	char prot[sizeof(attempt->prot) * 4 + 1];
	char user[sizeof(attempt->user) * 4 + 1];
	char msg[1024];
	char key[64 + INET6_ADDRSTRLEN];

	if (mqtt == NULL || mqtt->cfg == NULL || attempt == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	if (inet_addrtop((union xp_sockaddr*)&attempt->addr, ip, sizeof(ip)) == NULL)
		return MQTT_FAILURE;
	time_to_isoDateTimeStr(attempt->first, xpTimeZone_local(), iso_first, sizeof(iso_first));
	time_to_isoDateTimeStr(attempt->time,  xpTimeZone_local(), iso_last,  sizeof(iso_last));
	c_escape_str(attempt->prot, prot, sizeof(prot) - 1, /* ctrl_only: */ true);
	c_escape_str(attempt->user, user, sizeof(user) - 1, /* ctrl_only: */ true);
	snprintf(msg, sizeof(msg), "%s\t%s\t%lu\t%lu\t%s\t%s",
	         iso_first, iso_last,
	         (ulong)attempt->count, (ulong)attempt->dupes,
	         prot, user);
	snprintf(key, sizeof(key), "login_attempts/%s", ip);
	return pub_long_key(mqtt, TOPIC_HOST, key, msg, /* retain: */ true);
}

int mqtt_pub_login_attempt_clear(struct mqtt* mqtt, const char* ip)
{
	char key[64 + INET6_ADDRSTRLEN];

	if (mqtt == NULL || mqtt->cfg == NULL || ip == NULL || *ip == '\0')
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	snprintf(key, sizeof(key), "login_attempts/%s", ip);
	/* Empty payload + retain = delete the retained message at that topic. */
	return pub_long_key(mqtt, TOPIC_HOST, key, /* str: */ NULL, /* retain: */ true);
}

long mqtt_clear_login_attempt_list(struct mqtt* mqtt, link_list_t* list)
{
	list_node_t* node;
	long         count = 0;

	if (list == NULL)
		return -1;
	/* When MQTT isn't publishing, skip the per-IP walk and fall through to a plain clear. */
	if (mqtt == NULL || !mqtt->connected)
		return loginAttemptListClear(list);
	if (!listLock(list))
		return -1;
	for (node = list->first; node != NULL; node = node->next) {
		const login_attempt_t* a = (const login_attempt_t*)node->data;
		if (a != NULL) {
			char ipbuf[INET6_ADDRSTRLEN];
			if (inet_addrtop((union xp_sockaddr*)&a->addr, ipbuf, sizeof(ipbuf)) != NULL)
				mqtt_pub_login_attempt_clear(mqtt, ipbuf);
		}
		count++;
	}
	listFreeNodes(list);
	listUnlock(list);
	return count;
}

int mqtt_pub_max_concurrent(struct mqtt* mqtt, const char* ip, ulong strikes)
{
	char str[32];
	char key[64 + INET6_ADDRSTRLEN];

	if (mqtt == NULL || mqtt->cfg == NULL || ip == NULL || *ip == '\0')
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	snprintf(str, sizeof(str), "%lu", strikes);
	snprintf(key, sizeof(key), "max_concurrent/%s", ip);
	return pub_long_key(mqtt, TOPIC_SERVER, key, str, /* retain: */ true);
}

int mqtt_pub_max_concurrent_clear(struct mqtt* mqtt, const char* ip)
{
	char key[64 + INET6_ADDRSTRLEN];

	if (mqtt == NULL || mqtt->cfg == NULL || ip == NULL || *ip == '\0')
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	snprintf(key, sizeof(key), "max_concurrent/%s", ip);
	return pub_long_key(mqtt, TOPIC_SERVER, key, /* str: */ NULL, /* retain: */ true);
}

int mqtt_pub_uintval(struct mqtt* mqtt, enum topic_depth depth, const char* key, ulong value)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	char str[128];
	sprintf(str, "%lu", value);
	char sub[128];
	mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
	if (mqtt->local)
		return local_publish(mqtt, sub, str, strlen(str),
		                    mqtt->cfg->mqtt.publish_qos, true);
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL) {
		return mosquitto_publish_v5(mqtt->handle,
		                            /* mid: */ NULL,
		                            /* topic: */ sub,
		                            /* payloadlen */ strlen(str),
		                            /* payload */ str,
		                            /* qos */ mqtt->cfg->mqtt.publish_qos,
		                            /* retain */ true,
		                            /* properties */ NULL);
	}
#else
	if (mqtt->handle != NULL)
		return ext_client(mqtt)->publish(sub, str, strlen(str),
		                                mqtt->cfg->mqtt.publish_qos, true);
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_message(struct mqtt* mqtt, enum topic_depth depth, const char* key, const void* buf, size_t len, bool retain)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (!mqtt->connected)
		return MQTT_SUCCESS;
	char sub[128];
	mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
	if (mqtt->local)
		return local_publish(mqtt, sub, buf, len,
		                    mqtt->cfg->mqtt.publish_qos, retain);
#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL) {
		return mosquitto_publish_v5(mqtt->handle,
		                            /* mid: */ NULL,
		                            /* topic: */ sub,
		                            /* payloadlen */ len,
		                            /* payload */ buf,
		                            /* qos */ mqtt->cfg->mqtt.publish_qos,
		                            /* retain */ retain,
		                            /* properties */ NULL);
	}
#else
	if (mqtt->handle != NULL)
		return ext_client(mqtt)->publish(sub, buf, len,
		                                mqtt->cfg->mqtt.publish_qos, retain);
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
	safe_snprintf(str, size, "mqtt5-internal");
	return str;
#endif
}

int mqtt_open(struct mqtt* mqtt)
{
	char client_id[256];
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (mqtt->handle != NULL) // already open
		return MQTT_FAILURE;
	snprintf(client_id, sizeof(client_id), "sbbs-%s-%s-%s", mqtt->cfg->sys_id, mqtt->host, server_type_desc((enum server_type)mqtt->startup->type));
#ifdef USE_MOSQUITTO
	mqtt->handle = mosquitto_new(client_id, /* clean_session: */ true, /* userdata: */ mqtt);
	return mqtt->handle == NULL ? MQTT_FAILURE : MQTT_SUCCESS;
#else
	mqtt->handle = new mqtt5::Client();
	return MQTT_SUCCESS;
#endif
}

void mqtt_close(struct mqtt* mqtt)
{
	if (mqtt->handle != NULL) {
#ifdef USE_MOSQUITTO
		mosquitto_destroy(mqtt->handle);
#else
		delete ext_client(mqtt);
#endif
		mqtt->handle = NULL;
		listFree(&mqtt->client_list);
	}
	FREE_AND_NULL(mqtt->host);
}

#ifdef USE_MOSQUITTO
static int pw_callback(char* buf, int size, int rwflag, void* userdata)
{
	struct mqtt* mqtt = (struct mqtt*)userdata;

	strncpy(buf, mqtt->cfg->mqtt.tls.keypass, size);
	return strlen(mqtt->cfg->mqtt.tls.keypass);
}
#endif

int mqtt_connect(struct mqtt* mqtt, const char* bind_address)
{
	if (mqtt == NULL || mqtt->handle == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	char  topic[128];
	char* username = mqtt->cfg->mqtt.username;
	char* password = mqtt->cfg->mqtt.password;

	if (*username == '\0')
		username = NULL;
	if (*password == '\0')
		password = NULL;
	if (mqtt->cfg->mqtt.tls.mode == MQTT_TLS_SBBS) {
		username = NULL;
		password = mqtt->cfg->sys_pass;
		mosquitto_int_option(mqtt->handle, MOSQ_OPT_PROTOCOL_VERSION, 5);
	}
	else
		mosquitto_int_option(mqtt->handle, MOSQ_OPT_PROTOCOL_VERSION, mqtt->cfg->mqtt.protocol_version);
	mosquitto_username_pw_set(mqtt->handle, username, password);
	char value[128];
	SAFECOPY(value, "DISCONNECTED");
	mosquitto_will_set(mqtt->handle
	                   , mqtt_topic(mqtt, TOPIC_SERVER_LEVEL, topic, sizeof(topic), NULL)
	                   , strlen(value), value, /* QOS: */ 2, /* retain: */ true);
	if (mqtt->cfg->mqtt.tls.mode == MQTT_TLS_CERT) {
		char* certfile = NULL;
		char* keyfile = NULL;
		if (mqtt->cfg->mqtt.tls.certfile[0] && mqtt->cfg->mqtt.tls.keyfile[0]) {
			certfile = mqtt->cfg->mqtt.tls.certfile;
			keyfile = mqtt->cfg->mqtt.tls.keyfile;
		}
		int result = mosquitto_tls_set(mqtt->handle,
		                               mqtt->cfg->mqtt.tls.cafile,
		                               NULL, // capath
		                               certfile,
		                               keyfile,
		                               pw_callback);
		if (result != MOSQ_ERR_SUCCESS)
			return result;
	}
	else if (mqtt->cfg->mqtt.tls.mode == MQTT_TLS_PSK) {
		int result = mosquitto_tls_psk_set(mqtt->handle,
		                                   mqtt->cfg->mqtt.tls.psk,
		                                   mqtt->cfg->mqtt.tls.identity,
		                                   NULL // ciphers (default)
		                                   );
		if (result != MOSQ_ERR_SUCCESS)
			return result;
	}
	else if (mqtt->cfg->mqtt.tls.mode == MQTT_TLS_SBBS) {
		user_t user = {};
		user.number = 1;
		int result = MQTT_FAILURE;
		if ((getuserdat(mqtt->cfg, &user) == USER_SUCCESS)
		    && user.number == 1
		    && user_is_sysop(&user)) {
			strlwr(user.pass);
			char hexpass[LEN_PASS * 2 + 1];

			for (size_t i = 0; user.pass[i]; i++) {
				const char hd[] = "0123456789ABCDEF";
				hexpass[i*2] = hd[(((uint8_t *)user.pass)[i] & 0xf0) >> 4];
				hexpass[i*2+1] = hd[user.pass[i] & 0x0f];
			}
			strlwr(user.alias);
			result = mosquitto_tls_psk_set(mqtt->handle,
			                      hexpass,
			                      user.alias,
			                      NULL // ciphers (default)
			                      );
		}
		if (result != MOSQ_ERR_SUCCESS)
			return result;
	}
	return mosquitto_connect_bind(mqtt->handle,
	                              mqtt->cfg->mqtt.broker_addr,
	                              mqtt->cfg->mqtt.broker_port,
	                              mqtt->cfg->mqtt.keepalive,
	                              bind_address);
#else
	{
		char topic[128];
		char value[128];
		char* username = mqtt->cfg->mqtt.username;
		char* password = mqtt->cfg->mqtt.password;
		if (*username == '\0')
			username = NULL;
		if (*password == '\0')
			password = NULL;
		if (mqtt->cfg->mqtt.tls.mode == MQTT_TLS_SBBS) {
			username = NULL;
			password = mqtt->cfg->sys_pass;
		}
		SAFECOPY(value, "DISCONNECTED");
		auto *client = ext_client(mqtt);
		client->set_will(mqtt_topic(mqtt, TOPIC_SERVER_LEVEL, topic, sizeof(topic), NULL),
		                 value, strlen(value), /* qos: */ 2, /* retain: */ true);
		pthread_once(&s_mqtt_lputs_once, mqtt_lputs_mutex_init);
		pthread_mutex_lock(&s_mqtt_lputs_mutex);
		s_mqtt_lputs = mqtt->lputs;
		int ret = client->connect(mqtt->cfg->mqtt.broker_addr,
		                          mqtt->cfg->mqtt.broker_port,
		                          NULL, username, password,
		                          mqtt->cfg->mqtt.keepalive, 5,
		                          mqtt->cfg->mqtt.tls.mode,
		                          mqtt->cfg->mqtt.tls.psk,
		                          mqtt->cfg->mqtt.tls.identity,
		                          mqtt->cfg->mqtt.tls.cafile,
		                          mqtt->cfg->mqtt.tls.certfile,
		                          mqtt->cfg->mqtt.tls.keyfile,
		                          mqtt->cfg->mqtt.tls.keypass,
		                          mqtt->cfg, mqtt_lprintf_adapter);
		pthread_mutex_unlock(&s_mqtt_lputs_mutex);
		return ret;
	}
#endif
}

int mqtt_disconnect(struct mqtt* mqtt)
{
	if (mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_disconnect(mqtt->handle);
#else
	mqtt->connected = false;
	ext_client(mqtt)->disconnect();
	return MQTT_SUCCESS;
#endif
}

#ifndef USE_MOSQUITTO
static void mqtt5client_pump_thread(void *arg)
{
	struct mqtt* mqtt = (struct mqtt*)arg;
	SetThreadName("mqtt-pump");
	auto *client = ext_client(mqtt);
	mqtt->pump_running = true;
	while (mqtt->connected) {
		client->pump(100);
		mqtt5::ReceivedMessage *rm;
		while ((rm = client->read(0)) != NULL)
			mqtt_dispatch_message(mqtt, rm->topic.c_str(), rm->payload.data(), rm->payload.size());
	}
	mqtt->pump_running = false;
}
#endif

int mqtt_thread_start(struct mqtt* mqtt)
{
	if (mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_start(mqtt->handle);
#else
	if (_beginthread(mqtt5client_pump_thread, 0, (void *)mqtt) == (uintptr_t)-1)
		return MQTT_FAILURE;
	return MQTT_SUCCESS;
#endif
}

int mqtt_thread_stop(struct mqtt* mqtt)
{
	if (mqtt == NULL || mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_stop(mqtt->handle, /* force: */ false);
#else
	mqtt->connected = false;
	for (int i = 0; i < 50 && mqtt->pump_running; i++)
		SLEEP(100);
	return MQTT_SUCCESS;
#endif
}

static int lprintf(int (*lputs)(int level, const char* str), int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	if (lputs == NULL)
		return -1;
	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return lputs(level, sbuf);
}

static void mqtt_connect_callback_common(struct mqtt* mqtt)
{
	char str[128];

	if ((enum server_type)mqtt->startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)mqtt->startup;
		for (int i = bbs_startup->first_node; i <= bbs_startup->last_node; ++i) {
			mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "node/%d/input", i);
			mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "node/%d/set/#", i);
			mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "node/%d/msg", i);
		}
		mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "exec");
		mqtt_subscribe(mqtt, TOPIC_BBS, str, sizeof(str), "call");
	}
	mqtt_subscribe(mqtt, TOPIC_SERVER, str, sizeof(str), "recycle");
	mqtt_subscribe(mqtt, TOPIC_HOST, str, sizeof(str), "recycle");
	mqtt_subscribe(mqtt, TOPIC_SERVER, str, sizeof(str), "pause");
	mqtt_subscribe(mqtt, TOPIC_HOST, str, sizeof(str), "pause");
	mqtt_subscribe(mqtt, TOPIC_SERVER, str, sizeof(str), "resume");
	mqtt_subscribe(mqtt, TOPIC_HOST, str, sizeof(str), "resume");
	mqtt_subscribe(mqtt, TOPIC_SERVER, str, sizeof(str), "clear");
	mqtt_subscribe(mqtt, TOPIC_HOST, str, sizeof(str), "clear");
	if (mqtt->server_version != NULL) {
		mqtt_server_startup(mqtt);
		mqtt->server_version = NULL;
	} else {
		mqtt_server_state(mqtt, mqtt->server_state);
	}
}

#ifdef USE_MOSQUITTO

static void mqtt_connect_callback(struct mosquitto* mosq, void* cbdata, int rc)
{
	struct mqtt* mqtt = (struct mqtt*)cbdata;

	if (rc == MQTT_SUCCESS) {
		mqtt->connected = true;
		mqtt_connect_callback_common(mqtt);
	}
	else
		mqtt->connect_error = rc;
}

static void mqtt_disconnect_callback(struct mosquitto* mosq, void* cbdata, int rc)
{
	struct mqtt* mqtt = (struct mqtt*)cbdata;
	mqtt->disconnect_reason = rc;
	mqtt->connected = false;
}

#endif // USE_MOSQUITTO

static ulong payload_ulong(const void* payload, size_t payloadlen, ulong deflt)
{
	if (payloadlen < 1)
		return deflt;
	return strtoul((const char*)payload, NULL, 0);
}

void mqtt_dispatch_message(struct mqtt* mqtt, const char* msg_topic, const void* payload, size_t payloadlen)
{
	char topic[128];

	if ((enum server_type)mqtt->startup->type == SERVER_TERM) {
		bbs_startup_t* bbs_startup = (bbs_startup_t*)mqtt->startup;
		for (int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/input", i);
			if (strcmp(msg_topic, topic) != 0)
				continue;
			if (bbs_startup->node_inbuf != NULL && bbs_startup->node_inbuf[i - 1] != NULL)
				RingBufWrite(bbs_startup->node_inbuf[i - 1], (const uint8_t *)payload, payloadlen);
			return;
		}
		for (int i = bbs_startup->first_node; i <= bbs_startup->last_node; i++) {
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/msg", i)) == 0) {
				(void)putnmsg(mqtt->cfg, i, (char*)payload);
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/status", i)) == 0) {
				set_node_status(mqtt->cfg, i, (enum node_status)payload_ulong(payload, payloadlen, 0));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/errors", i)) == 0) {
				set_node_errors(mqtt->cfg, i, payload_ulong(payload, payloadlen, 0));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/misc", i)) == 0) {
				set_node_misc(mqtt->cfg, i, payload_ulong(payload, payloadlen, 0));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/lock", i)) == 0) {
				set_node_lock(mqtt->cfg, i, payload_ulong(payload, payloadlen, true));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/intr", i)) == 0) {
				set_node_interrupt(mqtt->cfg, i, payload_ulong(payload, payloadlen, true));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/down", i)) == 0) {
				set_node_down(mqtt->cfg, i, payload_ulong(payload, payloadlen, true));
				return;
			}
			if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "node/%d/set/rerun", i)) == 0) {
				set_node_rerun(mqtt->cfg, i, payload_ulong(payload, payloadlen, true));
				return;
			}
		}
		if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "exec")) == 0) {
			for (int i = 0; i < mqtt->cfg->total_events; i++) {
				if (stricmp(mqtt->cfg->event[i]->code, (const char *)payload) != 0)
					continue;
				if (mqtt->cfg->event[i]->node != NODE_ANY
				    && (mqtt->cfg->event[i]->node < bbs_startup->first_node || mqtt->cfg->event[i]->node > bbs_startup->last_node)
				    && !(mqtt->cfg->event[i]->misc & EVENT_EXCL))
					break;
				if (!(mqtt->cfg->event[i]->misc & EVENT_DISABLED))
					mqtt->cfg->event[i]->last = -1;
				break;
			}
			return;
		}
		if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_BBS, topic, sizeof(topic), "call")) == 0) {
			for (int i = 0; i < mqtt->cfg->total_qhubs; i++) {
				if (stricmp(mqtt->cfg->qhub[i]->id, (const char *)payload) != 0)
					continue;
				if (mqtt->cfg->qhub[i]->node != NODE_ANY
				    && (mqtt->cfg->qhub[i]->node < bbs_startup->first_node || mqtt->cfg->qhub[i]->node > bbs_startup->last_node))
					break;
				if (mqtt->cfg->qhub[i]->enabled)
					mqtt->cfg->qhub[i]->last = -1;
				break;
			}
			return;
		}
	}
	if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_HOST, topic, sizeof(topic), "recycle")) == 0
	    || strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_SERVER, topic, sizeof(topic), "recycle")) == 0) {
		mqtt->startup->recycle_now = true;
		return;
	}
	if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_HOST, topic, sizeof(topic), "pause")) == 0
	    || strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_SERVER, topic, sizeof(topic), "pause")) == 0) {
		mqtt->startup->paused = true;
		return;
	}
	if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_HOST, topic, sizeof(topic), "resume")) == 0
	    || strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_SERVER, topic, sizeof(topic), "resume")) == 0) {
		mqtt->startup->paused = false;
		return;
	}
	if (strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_HOST, topic, sizeof(topic), "clear")) == 0
	    || strcmp(msg_topic, mqtt_topic(mqtt, TOPIC_SERVER, topic, sizeof(topic), "clear")) == 0) {
		size_t len = payloadlen;
		if (len >= sizeof(mqtt->clear_attempts_ip))
			len = sizeof(mqtt->clear_attempts_ip) - 1;
		if (len > 0)
			memcpy(mqtt->clear_attempts_ip, payload, len);
		mqtt->clear_attempts_ip[len] = '\0';
		truncsp(mqtt->clear_attempts_ip);
		mqtt->startup->clear_attempts_now = true;
		return;
	}
}

#ifdef USE_MOSQUITTO
static void mqtt_message_received(struct mosquitto* mosq, void* cbdata, const struct mosquitto_message* msg)
{
	(void)mosq;
	struct mqtt* mqtt = (struct mqtt*)cbdata;
	mqtt_dispatch_message(mqtt, msg->topic, msg->payload, msg->payloadlen);
}
#endif // USE_MOSQUITTO

// 'version' argument should not point to stack memory as it'll be read later (i.e. in connect callback)
int mqtt_startup(struct mqtt* mqtt, scfg_t* cfg, struct startup* startup, const char* version
                 , int (*lputs)(int level, const char* str))
{
	int  result = MQTT_FAILURE;
	char str[128];

	if (mqtt == NULL || cfg == NULL || version == NULL)
		return MQTT_FAILURE;

	if (!cfg->mqtt.enabled)
		return MQTT_SUCCESS;

	if (cfg->mqtt.internal_broker) {
		char hostname[128];
		gethostname(hostname, sizeof(hostname));
		const char *addr = cfg->mqtt.broker_addr;
		const char *configured_host = (startup != NULL && startup->host_name[0])
		                              ? startup->host_name : hostname;
		bool local = (addr[0] == '\0'
		              || stricmp(addr, "localhost") == 0
		              || strcmp(addr, "127.0.0.1") == 0
		              || strcmp(addr, "::1") == 0
		              || stricmp(addr, configured_host) == 0);

		if (local) {
			mqtt->cfg = cfg;
			mqtt->startup = startup;
			mqtt->host = strdup(hostname);
			mqtt->local = nullptr;
			mqtt->server_version = version;
			listInit(&mqtt->client_list, LINK_LIST_MUTEX);

			auto *broker = mqtt5::Broker::instance();
			if (!broker) {
				static std::mutex start_mutex;
				std::lock_guard<std::mutex> lock(start_mutex);
				broker = mqtt5::Broker::instance();
				if (!broker) {
					s_broker_startup = startup;
					static mqtt5::Broker s_broker;
					if (!s_broker.start(cfg, cfg->mqtt.broker_port,
					                    mqtt_broker_lputs, nullptr)) {
						if (lputs)
							lputs(LOG_ERR, "MQTT internal broker: failed to start");
						return MQTT_FAILURE;
					}
					broker = mqtt5::Broker::instance();
				}
			}

			char client_id[256];
			snprintf(client_id, sizeof(client_id), "sbbs-%s-%s-%s",
			         cfg->sys_id, mqtt->host, server_type_desc((enum server_type)startup->type));

			auto *lc = broker->register_local(client_id, mqtt_local_message_callback, mqtt);
			mqtt->local = lc;
			mqtt->connected = true;
			mqtt_connect_callback_common(mqtt);
			if (version) {
				mqtt_server_startup(mqtt);
				mqtt->server_version = NULL;
			}

			if (lputs)
				lputs(LOG_INFO, "MQTT internal broker: connected (local)");
			return MQTT_SUCCESS;
		}
		lprintf(lputs, LOG_INFO, "MQTT internal broker: connecting to remote %s:%hu",
		        addr, cfg->mqtt.broker_port);
		/* fall through to external client path */
	}

	result = mqtt_init(mqtt, cfg, startup);
	if (result != MQTT_SUCCESS) {
		lprintf(lputs, LOG_ERR, "MQTT init failure: %d", result);
	} else {
		mqtt->lputs = lputs;
		lprintf(lputs, LOG_DEBUG, "MQTT lib: %s", mqtt_libver(str, sizeof(str)));
		result = mqtt_open(mqtt);
		if (result != MQTT_SUCCESS) {
			lprintf(lputs, LOG_ERR, "MQTT open failure: %d", result);
		} else {
			result = mqtt_thread_start(mqtt);
			if (result != MQTT_SUCCESS) {
				lprintf(lputs, LOG_ERR, "MQTT error %d starting pub/sub thread", result);
				mqtt_close(mqtt);
			} else {
				mqtt->server_version = version;
#ifdef USE_MOSQUITTO
				if (mqtt->handle != NULL) {
					mosquitto_connect_callback_set(mqtt->handle, mqtt_connect_callback);
					mosquitto_disconnect_callback_set(mqtt->handle, mqtt_disconnect_callback);
				}
#endif
				lprintf(lputs, LOG_INFO, "MQTT connecting to broker %s:%u", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
				result = mqtt_connect(mqtt, /* bind_address: */ NULL);
				if (result == MQTT_SUCCESS) {
					lprintf(lputs, LOG_DEBUG, "MQTT broker-connect (%s:%d) successful", cfg->mqtt.broker_addr, cfg->mqtt.broker_port);
#ifndef USE_MOSQUITTO
					mqtt->connected = true;
					mqtt_connect_callback_common(mqtt);
#endif
					mqtt_pub_noval(mqtt, TOPIC_SERVER, "client");
				} else {
					mqtt_shutdown(mqtt);
					lprintf(lputs, LOG_ERR, "MQTT broker-connect (%s:%d) failure: %d", cfg->mqtt.broker_addr, cfg->mqtt.broker_port, result);
				}
			}
		}
	}

#ifdef USE_MOSQUITTO
	if (mqtt->handle != NULL)
		mosquitto_message_callback_set(mqtt->handle, mqtt_message_received);
#endif

	return result;
}

int mqtt_server_startup(struct mqtt* mqtt)
{
	int result = mqtt_server_state(mqtt, mqtt->server_state);
	mqtt_pub_strval(mqtt, TOPIC_BBS_LEVEL, NULL, mqtt->cfg->sys_name);
	mqtt_pub_strval(mqtt, TOPIC_HOST_LEVEL, NULL, mqtt->startup->host_name);
	mqtt_pub_strval(mqtt, TOPIC_SERVER, "version", mqtt->server_version);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "served", mqtt->served);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "highwater", mqtt->highwater);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "error_count", mqtt->error_count);
	return result;
}

int mqtt_server_state(struct mqtt* mqtt, enum server_state state)
{
	char str[128];

	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

	if (mqtt->cfg->mqtt.verbose) {
		char errors[64] = "";
		if (mqtt->error_count)
			snprintf(errors, sizeof(errors), "%lu error%s", mqtt->error_count, mqtt->error_count > 1 ? "s" : "");
		char served[64] = "";
		if (mqtt->served)
			snprintf(served, sizeof(served), "%lu served", mqtt->served);
		char max_clients[64] = "";
		if (mqtt->max_clients)
			snprintf(max_clients, sizeof(max_clients), "/%lu", mqtt->max_clients);
		char clients[64] = "";
		if (mqtt->client_list.count)
			snprintf(clients, sizeof(clients), "%d%s clients", mqtt->client_list.count, max_clients);
		snprintf(str, sizeof(str), "%s\t%s\t%s\t%s"
		         , server_state_desc(state)
		         , clients
		         , served
		         , errors);
	} else
		SAFECOPY(str, server_state_desc(state));
	if (mqtt->server_state != state)
		mqtt->server_state = state;
	int result = mqtt_pub_strval(mqtt, TOPIC_SERVER_LEVEL, NULL, str);
	if (result == MQTT_SUCCESS && mqtt->connected) {
		char topic[128];
		snprintf(topic, sizeof(topic), "state/%s", server_state_desc(state));
		mqtt_pub_timestamped_msg(mqtt, TOPIC_SERVER, topic, time(NULL), server_state_desc(mqtt->server_state));
	}
	return result;
}

int mqtt_errormsg(struct mqtt* mqtt, int level, const char* msg)
{
	char   topic[128];
	time_t t = time(NULL);

	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	++mqtt->error_count;
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "error_count", mqtt->error_count);
	if (mqtt->cfg->mqtt.verbose)
		mqtt_server_state(mqtt, mqtt->server_state);
	snprintf(topic, sizeof(topic), "error/%s", log_level_desc(level));
	return mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, t, msg);
}

int mqtt_client_max(struct mqtt* mqtt, ulong count)
{
	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	mqtt->max_clients = count;
	return mqtt_client_count(mqtt);
}

static void format_client_info(char* str, size_t size, int sock, client_t* client, time_t t)
{
	char timestamp[32];

	snprintf(str, size, "%s\t%s\t%d\t%s\t%s\t%s\t%hu\t%d"
	         , time_to_isoDateTimeStr(t, xpTimeZone_local(), timestamp, sizeof(timestamp))
	         , client->protocol
	         , client->usernum
	         , client->user
	         , client->addr
	         , client->host
	         , client->port
	         , sock
	         );
}

int mqtt_client_on(struct mqtt* mqtt, bool on, int sock, client_t* client, bool update)
{
	#define MAX_CLIENT_STRLEN 512
	char str[MAX_CLIENT_STRLEN + 1];

	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;

	if (!mqtt->connected)
		return MQTT_SUCCESS;

	listLock(&mqtt->client_list);
	if (on) {
		if (update) {
			list_node_t* node;

			if ((node = listFindTaggedNode(&mqtt->client_list, sock)) != NULL) {
				memcpy(node->data, client, sizeof(client_t));
				format_client_info(str, sizeof(str), sock, client, time(NULL));
				mqtt_pub_strval(mqtt, TOPIC_SERVER, "client/action/update", str);
			}
		} else {
			listAddNodeData(&mqtt->client_list, client, sizeof(client_t), sock, LAST_NODE);
			format_client_info(str, sizeof(str), sock, client, client->time);
			mqtt_pub_strval(mqtt, TOPIC_SERVER, "client/action/connect", str);
		}
	} else {
		client = (client_t *)listRemoveTaggedNode(&mqtt->client_list, sock, /* free_data: */ false);
		if (client != NULL) {
			format_client_info(str, sizeof(str), sock, client, time(NULL));
			mqtt_pub_strval(mqtt, TOPIC_SERVER, "client/action/disconnect", str);
			FREE_AND_NULL(client);
		}
		mqtt->served++;
	}

	str_list_t list = strListInit();
	size_t     client_count = 0;
	for (list_node_t* node = mqtt->client_list.first; node != NULL; node = node->next) {
		client_t* client = (client_t *)node->data;
		format_client_info(str, sizeof(str), node->tag, client, client->time);
		strListPush(&list, str);
		client_count++;
	}
	listUnlock(&mqtt->client_list);
	char* buf = NULL;
	if (client_count > 0) {
		size_t buflen = client_count * MAX_CLIENT_STRLEN * 2;
		buf = (char *)malloc(buflen);
		strListJoin(list, buf, buflen, "\n");
	}
	strListFree(&list);

	mqtt_client_count(mqtt);
	mqtt_pub_uintval(mqtt, TOPIC_SERVER, "served", mqtt->served);
	int result = mqtt_pub_strval(mqtt, TOPIC_SERVER, "client/list", buf);
	free(buf);
	return result;
}

int mqtt_user_login_fail(struct mqtt* mqtt, client_t* client, const char* username)
{
	char str[128];
	char topic[128];

	if (mqtt == NULL || mqtt->cfg == NULL || client == NULL)
		return MQTT_FAILURE;

	if (!mqtt->connected)
		return MQTT_SUCCESS;

	if (username == NULL)
		return MQTT_FAILURE;
	snprintf(topic, sizeof(topic), "login_fail/%s", client->protocol);
	strlwr(topic);
	snprintf(str, sizeof(str), "%s\t%s\t%s"
	         , username
	         , client->addr
	         , client->host
	         );
	return mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, time(NULL), str);
}

int mqtt_user_login(struct mqtt* mqtt, client_t* client)
{
	char str[128];
	char topic[128];

	if (mqtt == NULL || mqtt->cfg == NULL || client == NULL)
		return MQTT_FAILURE;

	if (!mqtt->connected)
		return MQTT_SUCCESS;

	snprintf(topic, sizeof(topic), "login/%s", client->protocol);
	strlwr(topic);
	snprintf(str, sizeof(str), "%u\t%s\t%s\t%s"
	         , client->usernum
	         , client->user
	         , client->addr
	         , client->host
	         );
	return mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, time(NULL), str);
}

int mqtt_user_logout(struct mqtt* mqtt, client_t* client, time_t logintime)
{
	char str[128];
	char tmp[128];
	char topic[128];

	if (mqtt == NULL || mqtt->cfg == NULL || client == NULL)
		return MQTT_FAILURE;

	if (!mqtt->connected)
		return MQTT_SUCCESS;

	long tused = (long)(time(NULL) - logintime);
	if (tused < 0)
		tused = 0;
	snprintf(topic, sizeof(topic), "logout/%s", client->protocol);
	strlwr(topic);
	snprintf(str, sizeof(str), "%u\t%s\t%s\t%s\t%s"
	         , client->usernum
	         , client->user
	         , client->addr
	         , client->host
	         , sectostr(tused, tmp)
	         );
	return mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, time(NULL), str);
}


int mqtt_client_count(struct mqtt* mqtt)
{
	char str[128];

	if (mqtt == NULL || mqtt->cfg == NULL)
		return MQTT_FAILURE;
	if (mqtt->cfg->mqtt.verbose)
		mqtt_server_state(mqtt, mqtt->server_state);
	if (mqtt->max_clients)
		snprintf(str, sizeof(str), "%d total\t%ld max", mqtt->client_list.count, mqtt->max_clients);
	else
		snprintf(str, sizeof(str), "%d total", mqtt->client_list.count);
	return mqtt_pub_strval(mqtt, TOPIC_SERVER, "client", str);
}

void mqtt_shutdown(struct mqtt* mqtt)
{
	if (mqtt == NULL || mqtt->cfg == NULL || !mqtt->cfg->mqtt.enabled)
		return;
	if (mqtt->local) {
		auto *broker = mqtt5::Broker::instance();
		if (broker)
			broker->deregister_local(local_client(mqtt));
		mqtt->local = nullptr;
		mqtt->connected = false;
		return;
	}
	mqtt_disconnect(mqtt);
	mqtt_thread_stop(mqtt);
	mqtt_close(mqtt);
}

static int mqtt_file_xfer(struct mqtt* mqtt, user_t* user, int dirnum, const char* fname, off_t bytes, client_t* client, const char* xfer)
{
	if (mqtt == NULL || mqtt->cfg == NULL || user == NULL || fname == NULL || client == NULL)
		return MQTT_FAILURE;

	if (!dirnum_is_valid(mqtt->cfg, dirnum))
		return MQTT_FAILURE;

	if (!mqtt->connected)
		return MQTT_SUCCESS;

	char str[256];
	char topic[128];
	snprintf(topic, sizeof(topic), "%s/%s", xfer, mqtt->cfg->dir[dirnum]->code);
	snprintf(str, sizeof(str), "%u\t%s\t%s\t%" PRIdOFF "\t%s"
	         , user->number, user->alias, fname, bytes, client->protocol);
	return mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, time(NULL), str);
}

int mqtt_file_upload(struct mqtt* mqtt, user_t* user, int dirnum, const char* fname, off_t bytes, client_t* client)
{
	return mqtt_file_xfer(mqtt, user, dirnum, fname, bytes, client, "upload");
}

int mqtt_file_download(struct mqtt* mqtt, user_t* user, int dirnum, const char* fname, off_t bytes, client_t* client)
{
	return mqtt_file_xfer(mqtt, user, dirnum, fname, bytes, client, "download");
}

// number is one-based
int mqtt_putnodedat(struct mqtt* mqtt, int number, node_t* node)
{
	if (mqtt == NULL || node == NULL)
		return MQTT_FAILURE;

	char str[256];
	snprintf(str, sizeof str, "%u\t%u\t%u\t%u\t%x\t%u\t%u\t%u"
	         , node->status
	         , node->action
	         , node->useron
	         , node->connection
	         , node->misc
	         , node->aux
	         , node->extaux
	         , node->errors
	         );
	char topic[128];
	SAFEPRINTF(topic, "node/%u/status", number);
	int  result = mqtt_pub_strval(mqtt, TOPIC_BBS, topic, str);
	if (result == MQTT_SUCCESS && mqtt->cfg->mqtt.verbose) {
		SAFEPRINTF(topic, "node/%u", number);
		result = mqtt_pub_strval(mqtt, TOPIC_BBS, topic
		                         , nodestatus(mqtt->cfg, node, str, sizeof(str), number));
	}
	return result;
}
