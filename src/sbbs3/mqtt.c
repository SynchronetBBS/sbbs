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

int mqtt_init(struct mqtt* mqtt, scfg_t* cfg, const char* host, const char* server)
{
	if(cfg != NULL && !cfg->mqtt.enabled)
		return MQTT_SUCCESS;
	if(mqtt != NULL) {
		memset(mqtt, 0, sizeof(*mqtt));
		mqtt->cfg = cfg;
		mqtt->host = host;
		mqtt->server = server;
#ifdef USE_MOSQUITTO
		return mosquitto_lib_init();
#endif
	}
	return MQTT_FAILURE;
}

static char* format_topic(struct mqtt* mqtt, enum topic_depth depth, char* str, size_t size, const char* sbuf)
{
	switch(depth) {
		case TOPIC_ROOT:
			safe_snprintf(str, size, "sbbs/%s", sbuf);
			break;
		case TOPIC_BBS:
			safe_snprintf(str, size, "sbbs/%s/%s", mqtt->cfg->sys_id, sbuf);
			break;
		case TOPIC_HOST:
			safe_snprintf(str, size, "sbbs/%s/%s/%s", mqtt->cfg->sys_id, mqtt->host, sbuf);
			break;
		case TOPIC_SERVER:
			safe_snprintf(str, size, "sbbs/%s/%s/%s/%s", mqtt->cfg->sys_id, mqtt->host, mqtt->server, sbuf);
			break;
		default:
			safe_snprintf(str, size, "%s", sbuf);
			break;
	}
	return str;
}

char* mqtt_topic(struct mqtt* mqtt, enum topic_depth depth, char* str, size_t size, const char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr, fmt);
    vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1]=0;
    va_end(argptr);

	return format_topic(mqtt, depth, str, size, sbuf);
}

static int mqtt_sub(struct mqtt* mqtt, const char* topic)
{
	if(mqtt != NULL && mqtt->cfg != NULL && !mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt != NULL && mqtt->handle != NULL && topic != NULL) {
		return mosquitto_subscribe(mqtt->handle, /* mid: */NULL, topic, mqtt->cfg->mqtt.qos);
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

	format_topic(mqtt, depth, str, size, sbuf);

	return mqtt_sub(mqtt, str);
}

int mqtt_lputs(struct mqtt* mqtt, enum topic_depth depth, int level, const char* str)
{
	if(mqtt != NULL && mqtt->cfg != NULL && !mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt != NULL && mqtt->handle != NULL && str != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "log/%d", level);
		char lvl[32];
		sprintf(lvl, "%d", level);
		mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.qos,
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
			/* qos */mqtt->cfg->mqtt.qos,
			/* retain */false,
			/* properties */props);
		mosquitto_property_free_all(&props);
		return result;
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_strval(struct mqtt* mqtt, enum topic_depth depth, const char* key, const char* str)
{
	if(mqtt != NULL && mqtt->cfg != NULL && !mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt != NULL && mqtt->handle != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_uintval(struct mqtt* mqtt, enum topic_depth depth, const char* key, ulong value)
{
	if(mqtt != NULL && mqtt->cfg != NULL && !mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt != NULL && mqtt->handle != NULL) {
		char str[128];
		sprintf(str, "%lu", value);
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */strlen(str),
			/* payload */str,
			/* qos */mqtt->cfg->mqtt.qos,
			/* retain */true,
			/* properties */NULL);
	}
#endif
	return MQTT_FAILURE;
}

int mqtt_pub_message(struct mqtt* mqtt, enum topic_depth depth, const char* key, const void* buf, size_t len)
{
	if(mqtt != NULL && mqtt->cfg != NULL && !mqtt->cfg->mqtt.enabled)
		return MQTT_SUCCESS;
#ifdef USE_MOSQUITTO
	if(mqtt != NULL && mqtt->handle != NULL) {
		char sub[128];
		mqtt_topic(mqtt, depth, sub, sizeof(sub), "%s", key);
		return mosquitto_publish_v5(mqtt->handle,
			/* mid: */NULL,
			/* topic: */sub,
			/* payloadlen */len,
			/* payload */buf,
			/* qos */mqtt->cfg->mqtt.qos,
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
	if(mqtt->handle != NULL) // already open
		return MQTT_FAILURE;
#ifdef USE_MOSQUITTO
	mqtt->handle = mosquitto_new(/* id: */NULL, /* clean_session: */true, /* obj: */NULL);
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
	}
#endif
}

int mqtt_connect(struct mqtt* mqtt, const char* bind_address)
{
	if(mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	mosquitto_int_option(mqtt->handle, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
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
	if(mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_disconnect(mqtt->handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_start(struct mqtt* mqtt)
{
	if(mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_start(mqtt->handle);
#else
	return MQTT_FAILURE;
#endif
}

int mqtt_thread_stop(struct mqtt* mqtt)
{
	if(mqtt->handle == NULL)
		return MQTT_FAILURE;

#ifdef USE_MOSQUITTO
	return mosquitto_loop_stop(mqtt->handle, /* force: */true);
#else
	return MQTT_FAILURE;
#endif
}

