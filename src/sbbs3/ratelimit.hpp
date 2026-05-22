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
#include <unordered_set>
#include <deque>
#include <atomic>
#include "threadwrap.h"     /* pthread_mutex_t (std::mutex crashes in older
                               MSVCP140.dll - see filterfile.hpp / issue #1089) */

class rateLimiter {

	public:
	rateLimiter(unsigned int maxRequests, unsigned int timeWindowSeconds)
		: maxRequests(maxRequests), timeWindowSeconds(timeWindowSeconds) {
		pthread_mutex_init(&mutex, nullptr);
	}
	rateLimiter(const rateLimiter&) = delete;
	rateLimiter& operator=(const rateLimiter&) = delete;
	~rateLimiter() {
		pthread_mutex_destroy(&mutex);
	}
	unsigned int maxRequests;
	unsigned int timeWindowSeconds;
	struct {
		std::string client{};
		unsigned int count{};
		time_t time{};
	} currHighwater, prevHighwater, lastLimited;
	std::atomic<uint> disallowed{};
	std::atomic<uint> repeat{};
	// If denials is non-NULL, it receives the running count of times this clientId
	// has been denied while continuously active (i.e. since its last idle cleanup).
	// Callers can use this as an escalation signal (e.g. to auto-filter the client).
	// If member is non-empty, it identifies the specific client (e.g. host IP)
	// within the clientId bucket (e.g. a subnet). The set of distinct members
	// that have been *denied* while the bucket is continuously active is tracked
	// and queryable via distinctMembers(), so callers can tell whether a bucket's
	// abuse is distributed across many members or attributable to just one.
	bool allowRequest(const std::string& clientId, unsigned* denials = nullptr, const std::string& member = std::string()) {
		if (denials != nullptr)
			*denials = 0;
		if (maxRequests == 0 || timeWindowSeconds == 0)
			return true;
		bool allowed;
		pthread_mutex_lock(&mutex);
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
			allowed = true; // Allow the request
		} else {
			if (lastLimited.client == clientId)
				++repeat;
			else {
				lastLimited.client = clientId;
				repeat = 0;
			}
			lastLimited.count = count;
			lastLimited.time = now;
			++disallowed;
			unsigned n = ++deniedCount[clientId];
			if (!member.empty())
				bucketMembers[clientId].insert(member);
			if (denials != nullptr)
				*denials = n;
			allowed = false; // Rate limit exceeded
		}
		pthread_mutex_unlock(&mutex);
		return allowed;
	}
	size_t cleanup() {
		size_t removed = 0;
		auto now = time(NULL);
		pthread_mutex_lock(&mutex);
		for (auto it = clientRequestTimes.begin(); it != clientRequestTimes.end();) {
			auto& requestTimes = it->second;
			while (!requestTimes.empty() && now - requestTimes.front() >= timeWindowSeconds) {
				requestTimes.pop_front();
				++removed;
			}
			if (requestTimes.empty()) {
				deniedCount.erase(it->first); // Reset escalation once the client goes idle
				bucketMembers.erase(it->first);
				it = clientRequestTimes.erase(it); // Remove client if no recent requests
			} else {
				++it;
			}
		}
		pthread_mutex_unlock(&mutex);
		return removed;
	}
	size_t client_count() {
		pthread_mutex_lock(&mutex);
		size_t count = clientRequestTimes.size();
		pthread_mutex_unlock(&mutex);
		return count;
	}
	// Number of distinct members (e.g. host IPs) that have been denied while a
	// bucket (e.g. a subnet) has been continuously active. Used to decide whether
	// to filter a whole subnet or just the single offending member.
	size_t distinctMembers(const std::string& clientId) {
		pthread_mutex_lock(&mutex);
		auto it = bucketMembers.find(clientId);
		size_t count = (it == bucketMembers.end()) ? 0 : it->second.size();
		pthread_mutex_unlock(&mutex);
		return count;
	}
	size_t total() {
		pthread_mutex_lock(&mutex);
		size_t total = 0;
		for (auto it = clientRequestTimes.begin(); it != clientRequestTimes.end(); ++it)
			total += it->second.size();
		pthread_mutex_unlock(&mutex);
		return total;
	}
	std::string most_active(size_t* count) {
		pthread_mutex_lock(&mutex);
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
		pthread_mutex_unlock(&mutex);
		return client;
	}
private:
	std::unordered_map<std::string, std::deque<time_t>> clientRequestTimes;
	std::unordered_map<std::string, unsigned> deniedCount;
	std::unordered_map<std::string, std::unordered_set<std::string>> bucketMembers;
	pthread_mutex_t mutex{};
};
