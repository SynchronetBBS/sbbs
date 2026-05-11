#ifndef MQTT_TOPIC_H
#define MQTT_TOPIC_H

#include "mqtt_protocol.h"
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mqtt5 {

struct Subscriber {
	std::string client_id;
	SubscriptionOptions opts;
};

class TopicTree {
public:
	void subscribe(const std::string &filter, const std::string &client_id,
	               const SubscriptionOptions &opts);
	void unsubscribe(const std::string &filter, const std::string &client_id);
	void unsubscribe_all(const std::string &client_id);

	using DeliverFn = std::function<void(const Subscriber &sub)>;
	void publish(const std::string &topic, const DeliverFn &deliver) const;

	void set_retained(const std::string &topic, std::shared_ptr<Message> msg);
	std::shared_ptr<Message> get_retained(const std::string &topic) const;

	using RetainedFn = std::function<void(const std::string &topic, std::shared_ptr<Message> msg)>;
	void match_retained(const std::string &filter, const RetainedFn &fn) const;

	void collect_retained(const std::string &filter, std::vector<std::shared_ptr<Message>> &out) const;

	size_t purge_expired_retained();

	bool validate_filter(const std::string &filter) const;
	static bool is_shared_filter(const std::string &filter);

private:
	struct Node {
		std::unordered_map<std::string, std::unique_ptr<Node>> children;
		std::unordered_map<std::string, Subscriber> subscribers;
		std::shared_ptr<Message> retained;
	};

	Node *ensure_node(const std::string &topic);
	const Node *find_node(const std::string &topic) const;

	void match_subscribers(const Node *node, const std::vector<std::string> &levels,
	                       size_t depth, const DeliverFn &deliver) const;
	void match_retained_node(const Node *node, const std::vector<std::string> &levels,
	                         size_t depth, const RetainedFn &fn) const;
	size_t purge_expired_node(Node *node, time_t now);

	static std::vector<std::string> split_topic(const std::string &topic);
	static std::string subscriber_key(const std::string &client_id, const std::string &filter);

	Node m_root;
};

} // namespace mqtt5

#endif
