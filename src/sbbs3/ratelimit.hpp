// Rate Limiter

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

#include <unordered_map>
#include <deque>
#include <atomic>

class rateLimiter {

	public:
	rateLimiter(unsigned int maxRequests, unsigned int timeWindowSeconds)
		: maxRequests(maxRequests), timeWindowSeconds(timeWindowSeconds) {}
	unsigned int maxRequests;
	unsigned int timeWindowSeconds;
	struct {
		std::string client{};
		unsigned int count{};
		time_t time{};
	} currHighwater, prevHighwater, lastLimited;
	std::atomic<uint> disallowed{};
	bool allowRequest(const std::string& clientId) {
		if (maxRequests == 0 || timeWindowSeconds == 0)
			return true;
		auto& requestTimes = clientRequestTimes[clientId];
		auto now = time(NULL);
		// Remove timestamps that are outside the time window
		while (!requestTimes.empty() && now - requestTimes.front() >= timeWindowSeconds) {
			requestTimes.pop_front();
		}
		size_t count = requestTimes.size();
		if (count < maxRequests) {
			requestTimes.push_back(now);
			if (++count >= currHighwater.count) {
				if (currHighwater.count > 0 && currHighwater.client != clientId)
					prevHighwater = currHighwater;
				currHighwater.count = count;
				currHighwater.client = clientId;
				currHighwater.time = now;
			}
			return true; // Allow the request
		} else {
			lastLimited.client = clientId;
			lastLimited.count = count;
			lastLimited.time = now;
			++disallowed;
			return false; // Rate limit exceeded
		}
	}
	size_t cleanup() {
		size_t removed = 0;
		auto now = time(NULL);
		for (auto it = clientRequestTimes.begin(); it != clientRequestTimes.end();) {
			auto& requestTimes = it->second;
			while (!requestTimes.empty() && now - requestTimes.front() >= timeWindowSeconds) {
				requestTimes.pop_front();
				++removed;
			}
			if (requestTimes.empty()) {
				it = clientRequestTimes.erase(it); // Remove client if no recent requests
			} else {
				++it;
			}
		}
		return removed;
	}
	size_t client_count() { return clientRequestTimes.size(); }
	size_t total() {
		size_t total = 0;
		for (auto it = clientRequestTimes.begin(); it != clientRequestTimes.end(); ++it)
			total += it->second.size();
		return total;
	}
	std::string most_active(size_t* count) {
		size_t max = 0;
		std::string client;
		for (auto it = clientRequestTimes.begin(); it != clientRequestTimes.end(); ++it) {
			size_t cc = it->second.size();
			if (cc > max) {
				max = cc;
				client = it->first;
			}
		}
		if (count != nullptr)
			*count = max;
		return client;
	}
private:
	std::unordered_map<std::string, std::deque<time_t>> clientRequestTimes;
};
