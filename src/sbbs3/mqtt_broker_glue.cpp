#include "mqtt_broker.h"
#include <mutex>

extern "C" {
#include "mqtt.h"
#include "xpdatetime.h"
const char* server_type_desc(enum server_type type);
}

static struct startup *s_broker_startup = NULL;
static bool s_broker_logging = false;

static int broker_lputs_adapter(void *cbdata, int level, const char *str)
{
	(void)cbdata;
	if (s_broker_startup && s_broker_startup->lputs)
		s_broker_startup->lputs(s_broker_startup->cbdata, level, str);

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

static void local_message_callback(void *cbdata, const char *topic,
                                   const void *payload, size_t len)
{
	auto *m = static_cast<struct mqtt *>(cbdata);
	if (!m || !m->cfg)
		return;
	mqtt_dispatch_message(m, topic, payload, len);
}

extern "C" int mqtt_internal_startup(struct mqtt *mqtt, scfg_t *cfg, struct startup *startup,
                                      const char *version,
                                      int (*lputs)(int level, const char *str))
{
	if (!mqtt || !cfg || !startup)
		return MQTT_FAILURE;

	mqtt->cfg = cfg;
	mqtt->startup = startup;
	char hostname[128];
	gethostname(hostname, sizeof(hostname));
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
			                    broker_lputs_adapter, nullptr)) {
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

	auto *lc = broker->register_local(client_id, local_message_callback, mqtt);
	mqtt->local = lc;
	mqtt->connected = true;

	/* Subscribe to the same topics as the mosquitto path */
	char sub[128];
	mqtt_topic(mqtt, TOPIC_SERVER, sub, sizeof(sub), "recycle");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_HOST, sub, sizeof(sub), "recycle");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_SERVER, sub, sizeof(sub), "pause");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_HOST, sub, sizeof(sub), "pause");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_SERVER, sub, sizeof(sub), "resume");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_HOST, sub, sizeof(sub), "resume");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_SERVER, sub, sizeof(sub), "clear");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	mqtt_topic(mqtt, TOPIC_HOST, sub, sizeof(sub), "clear");
	broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);

	if (startup->type == SERVER_TERM) {
		bbs_startup_t *bbs = (bbs_startup_t *)startup;
		for (int i = bbs->first_node; i <= bbs->last_node; ++i) {
			snprintf(sub, sizeof(sub), "sbbs/%s/node/%d/set/#", cfg->sys_id, i);
			broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
			snprintf(sub, sizeof(sub), "sbbs/%s/node/%d/msg", cfg->sys_id, i);
			broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
			snprintf(sub, sizeof(sub), "sbbs/%s/node/%d/input", cfg->sys_id, i);
			broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
		}
		snprintf(sub, sizeof(sub), "sbbs/%s/exec", cfg->sys_id);
		broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
		snprintf(sub, sizeof(sub), "sbbs/%s/call", cfg->sys_id);
		broker->local_subscribe(lc, sub, cfg->mqtt.subscribe_qos);
	}

	mqtt_server_state(mqtt, mqtt->server_state);
	if (version) {
		mqtt_server_startup(mqtt);
		mqtt->server_version = NULL;
	}

	if (lputs)
		lputs(LOG_INFO, "MQTT internal broker: connected");

	return MQTT_SUCCESS;
}

extern "C" int mqtt_internal_publish(struct mqtt *mqtt, const char *topic,
                                      const void *payload, size_t len,
                                      int qos, bool retain,
                                      const void *props)
{
	if (!mqtt || !mqtt->local)
		return MQTT_FAILURE;

	auto *broker = mqtt5::Broker::instance();
	if (!broker)
		return MQTT_FAILURE;

	return broker->local_publish(static_cast<mqtt5::LocalClient *>(mqtt->local),
	                              topic, payload, len, qos, retain,
	                              static_cast<const mqtt5::Properties *>(props));
}

extern "C" int mqtt_internal_subscribe(struct mqtt *mqtt, const char *topic, int qos)
{
	if (!mqtt || !mqtt->local)
		return MQTT_FAILURE;

	auto *broker = mqtt5::Broker::instance();
	if (!broker)
		return MQTT_FAILURE;

	return broker->local_subscribe(static_cast<mqtt5::LocalClient *>(mqtt->local),
	                                topic, qos);
}

extern "C" void mqtt_internal_shutdown(struct mqtt *mqtt)
{
	if (!mqtt || !mqtt->local)
		return;

	auto *broker = mqtt5::Broker::instance();
	if (broker)
		broker->deregister_local(static_cast<mqtt5::LocalClient *>(mqtt->local));
	mqtt->local = nullptr;
	mqtt->connected = false;
}

extern "C" void *mqtt_props_new(void)
{
	return new mqtt5::Properties();
}

extern "C" void mqtt_props_add_string_pair(void *props, const char *key, const char *val)
{
	if (props && key && val)
		static_cast<mqtt5::Properties *>(props)->add(
			mqtt5::PROP_USER_PROPERTY,
			mqtt5::PropertyValue::from_pair(key, val));
}

extern "C" void mqtt_props_free(void *props)
{
	delete static_cast<mqtt5::Properties *>(props);
}
