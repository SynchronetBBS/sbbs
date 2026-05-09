#include "mqtt_topic.h"

namespace mqtt5 {

std::vector<std::string> TopicTree::split_topic(const std::string &topic)
{
	std::vector<std::string> levels;
	size_t start = 0;
	for (;;) {
		size_t pos = topic.find('/', start);
		if (pos == std::string::npos) {
			levels.push_back(topic.substr(start));
			break;
		}
		levels.push_back(topic.substr(start, pos - start));
		start = pos + 1;
	}
	return levels;
}

std::string TopicTree::subscriber_key(const std::string &client_id, const std::string &filter)
{
	return client_id + '\0' + filter;
}

bool TopicTree::is_shared_filter(const std::string &filter)
{
	return filter.size() > 6 && filter.compare(0, 6, "$share") == 0 && filter[6] == '/';
}

bool TopicTree::validate_filter(const std::string &filter) const
{
	if (filter.empty())
		return false;

	auto levels = split_topic(filter);
	for (size_t i = 0; i < levels.size(); ++i) {
		const auto &lev = levels[i];
		if (lev == "+")
			continue;
		if (lev == "#") {
			if (i != levels.size() - 1)
				return false;
			continue;
		}
		if (lev.find('+') != std::string::npos || lev.find('#') != std::string::npos)
			return false;
	}
	return true;
}

// ── Node navigation ─────────────────────────────────────────────

TopicTree::Node *TopicTree::ensure_node(const std::string &topic)
{
	auto levels = split_topic(topic);
	Node *node = &m_root;
	for (const auto &lev : levels) {
		auto &child = node->children[lev];
		if (!child)
			child = std::unique_ptr<Node>(new Node());
		node = child.get();
	}
	return node;
}

const TopicTree::Node *TopicTree::find_node(const std::string &topic) const
{
	auto levels = split_topic(topic);
	const Node *node = &m_root;
	for (const auto &lev : levels) {
		auto it = node->children.find(lev);
		if (it == node->children.end())
			return nullptr;
		node = it->second.get();
	}
	return node;
}

// ── Subscribe / Unsubscribe ─────────────────────────────────────

void TopicTree::subscribe(const std::string &filter, const std::string &client_id,
                          const SubscriptionOptions &opts)
{
	auto levels = split_topic(filter);
	Node *node = &m_root;
	for (const auto &lev : levels) {
		auto &child = node->children[lev];
		if (!child)
			child = std::unique_ptr<Node>(new Node());
		node = child.get();
	}
	std::string key = subscriber_key(client_id, filter);
	node->subscribers[key] = {client_id, opts};
}

void TopicTree::unsubscribe(const std::string &filter, const std::string &client_id)
{
	auto levels = split_topic(filter);
	std::vector<std::pair<Node *, std::string>> path;
	Node *cur = &m_root;
	for (const auto &lev : levels) {
		auto it = cur->children.find(lev);
		if (it == cur->children.end())
			return;
		path.push_back({cur, lev});
		cur = it->second.get();
	}
	std::string key = subscriber_key(client_id, filter);
	cur->subscribers.erase(key);

	for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
		Node *parent = path[i].first;
		const std::string &lev = path[i].second;
		Node *child = parent->children[lev].get();
		if (child->children.empty() && child->subscribers.empty() && !child->retained)
			parent->children.erase(lev);
		else
			break;
	}
}

void TopicTree::unsubscribe_all(const std::string &client_id)
{
	std::string prefix = client_id + '\0';
	std::function<void(Node *)> walk = [&](Node *node) {
		auto it = node->subscribers.begin();
		while (it != node->subscribers.end()) {
			if (it->first.compare(0, prefix.size(), prefix) == 0)
				it = node->subscribers.erase(it);
			else
				++it;
		}
		for (auto it = node->children.begin(); it != node->children.end(); ++it)
			walk(it->second.get());
	};
	walk(&m_root);
}

// ── Publish — find matching subscribers ─────────────────────────

void TopicTree::publish(const std::string &topic, const DeliverFn &deliver) const
{
	auto levels = split_topic(topic);
	match_subscribers(&m_root, levels, 0, deliver);
}

void TopicTree::match_subscribers(const Node *node, const std::vector<std::string> &levels,
                                  size_t depth, const DeliverFn &deliver) const
{
	if (depth == levels.size()) {
		for (auto it = node->subscribers.begin(); it != node->subscribers.end(); ++it)
			deliver(it->second);
		auto hash_it = node->children.find("#");
		if (hash_it != node->children.end()) {
			for (auto it2 = hash_it->second->subscribers.begin(); it2 != hash_it->second->subscribers.end(); ++it2)
				deliver(it2->second);
		}
		return;
	}

	auto exact = node->children.find(levels[depth]);
	if (exact != node->children.end())
		match_subscribers(exact->second.get(), levels, depth + 1, deliver);

	auto plus = node->children.find("+");
	if (plus != node->children.end())
		match_subscribers(plus->second.get(), levels, depth + 1, deliver);

	auto hash = node->children.find("#");
	if (hash != node->children.end()) {
		for (auto it2 = hash->second->subscribers.begin(); it2 != hash->second->subscribers.end(); ++it2)
			deliver(it2->second);
	}
}

// ── Retained messages ───────────────────────────────────────────

void TopicTree::set_retained(const std::string &topic, std::shared_ptr<Message> msg)
{
	Node *node = ensure_node(topic);
	if (msg && msg->payload.empty())
		node->retained.reset();
	else
		node->retained = std::move(msg);
}

std::shared_ptr<Message> TopicTree::get_retained(const std::string &topic) const
{
	const Node *node = find_node(topic);
	return node ? node->retained : nullptr;
}

void TopicTree::match_retained(const std::string &filter, const RetainedFn &fn) const
{
	auto levels = split_topic(filter);
	match_retained_node(&m_root, levels, 0, fn);
}

void TopicTree::match_retained_node(const Node *node, const std::vector<std::string> &levels,
                                     size_t depth, const RetainedFn &fn) const
{
	if (depth == levels.size()) {
		if (node->retained)
			fn(node->retained->topic, node->retained);
		return;
	}

	const auto &lev = levels[depth];

	if (lev == "#") {
		std::function<void(const Node *, const std::string &)> walk_all =
			[&](const Node *n, const std::string &prefix) {
			if (n->retained)
				fn(n->retained->topic, n->retained);
			for (auto it = n->children.begin(); it != n->children.end(); ++it) {
				std::string path = prefix.empty() ? it->first : prefix + "/" + it->first;
				walk_all(it->second.get(), path);
			}
		};
		walk_all(node, "");
		return;
	}

	if (lev == "+") {
		for (auto it = node->children.begin(); it != node->children.end(); ++it)
			match_retained_node(it->second.get(), levels, depth + 1, fn);
		return;
	}

	auto it = node->children.find(lev);
	if (it != node->children.end())
		match_retained_node(it->second.get(), levels, depth + 1, fn);
}

void TopicTree::collect_retained(const std::string &filter, std::vector<std::shared_ptr<Message>> &out) const
{
	match_retained(filter, [&out](const std::string &, std::shared_ptr<Message> msg) {
		out.push_back(std::move(msg));
	});
}

} // namespace mqtt5
