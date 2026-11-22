// Web Server request blocking by request-header pattern (ctrl/web_block.ini)

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

#ifndef WEB_BLOCK_HPP_
#define WEB_BLOCK_HPP_

#include <algorithm>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "ini_file.h"
#include "findstr.h"        // findstr_in_string()
#include "dirwrap.h"        // fdate()
#include "genwrap.h"
#include "sbbsdefs.h"       // LOG_*
#include "threadwrap.h"     // pthread_mutex_t (std::mutex crashes in older
                            // MSVCP140.dll - see filterfile.hpp / issue #1089)

#define WEB_BLOCK_SECTION_PREFIX "header:"

// A client's count of blocked requests is forgotten after this long with none
#define WEB_BLOCK_COUNT_IDLE    (60 * 60)   // seconds

// The file has one [header:<name>] section per request header to check.  In
// each section, the keys that are numbers hold the patterns (findstr syntax,
// tried in numeric key order) and the named keys hold the section's settings:
//
//   [header:user-agent]
//   0 = lightpanda~
//   1 = bytespider~
//   FilterThreshold = 3
//
// The file is re-read when its date changes (checked at most every
// cache_filter_files seconds), so edits take effect without a recycle.
class webBlock {
	public:
		struct rule {
			std::string header;                 // as named in the section
			std::vector<std::string> patterns;  // in numeric key order
			uint filter_threshold{};            // 0 = never filter
			bool has_filter_duration{};
			uint filter_duration{};             // seconds
			bool has_filter_silent{};
			bool filter_silent{};
		};

		// What blocked a request
		struct match {
			rule        section;
			std::string value;      // the header value checked ("" when absent)
			std::string pattern;    // the deciding pattern, or "" when an
			                        // allowlist (only ! patterns) matched none
		};

		typedef int (*log_func)(int level, const char* fmt, ...);

		webBlock() {
			pthread_mutex_init(&mutex, nullptr);
		}
		webBlock(const webBlock&) = delete;
		webBlock& operator=(const webBlock&) = delete;
		~webBlock() {
			pthread_mutex_destroy(&mutex);
		}

		void init(const char* path, time_t check_interval, log_func log) {
			pthread_mutex_lock(&mutex);
			strlcpy(fname, path, sizeof fname);
			fchk_interval = check_interval;
			this->log = log;
			clear();
			pthread_mutex_unlock(&mutex);
		}

		void reset() {
			pthread_mutex_lock(&mutex);
			clear();
			pthread_mutex_unlock(&mutex);
		}

		std::atomic<uint> fread_count{};
		std::atomic<uint> total_blocked{};
		char fname[MAX_PATH + 1]{};

		// Is the request blocked?  headers is the request's raw header lines
		// ("Name: value"); vhost is the host name the server uses for the
		// request (from the request line or the Host header, lower-case, with
		// no port), which [header:host] is matched against instead of the raw
		// Host header.
		bool blocked(str_list_t headers, const char* vhost, match* m) {
			bool result = false;
			pthread_mutex_lock(&mutex);
			refresh();
			for (const rule& r : rules) {
				if (r.patterns.empty())
					continue;
				if (stricmp(r.header.c_str(), "host") == 0) {
					result = check(r, vhost, m);
				} else {
					bool present = false;
					size_t len = r.header.size();
					for (size_t i = 0; headers != nullptr && headers[i] != nullptr && !result; i++) {
						const char* line = headers[i];
						if (strnicmp(line, r.header.c_str(), len) != 0 || line[len] != ':')
							continue;
						present = true;
						const char* value = line + len + 1;
						while (*value != '\0' && *value <= ' ')
							value++;
						result = check(r, value, m);
					}
					if (!present)
						result = check(r, nullptr, m);
				}
				if (result)
					break;
			}
			pthread_mutex_unlock(&mutex);
			if (result)
				++total_blocked;
			return result;
		}

		// Count a request blocked by header from client (an IP address or a
		// subnet, see rate_limit_key()), made by member (the IP address).
		// Returns the count since the client was last idle for
		// WEB_BLOCK_COUNT_IDLE; *members receives the number of distinct
		// member addresses counted in that time.
		unsigned count(const std::string& header, const std::string& client
		               , const std::string& member, size_t* members) {
			time_t now = time(nullptr);
			pthread_mutex_lock(&mutex);
			if (now - last_prune >= WEB_BLOCK_COUNT_IDLE) {
				for (auto it = counts.begin(); it != counts.end();) {
					if (now - it->second.last >= WEB_BLOCK_COUNT_IDLE)
						it = counts.erase(it);
					else
						++it;
				}
				last_prune = now;
			}
			client_count& c = counts[count_key(header, client)];
			if (now - c.last >= WEB_BLOCK_COUNT_IDLE) {
				c.n = 0;
				c.members.clear();
			}
			c.last = now;
			c.members.insert(member);
			unsigned n = ++c.n;
			if (members != nullptr)
				*members = c.members.size();
			pthread_mutex_unlock(&mutex);
			return n;
		}

		// Forget a client's count (e.g. once it has been filtered)
		void forget(const std::string& header, const std::string& client) {
			pthread_mutex_lock(&mutex);
			counts.erase(count_key(header, client));
			pthread_mutex_unlock(&mutex);
		}

		size_t rule_count() {
			pthread_mutex_lock(&mutex);
			refresh();
			size_t n = rules.size();
			pthread_mutex_unlock(&mutex);
			return n;
		}

	private:
		typedef std::pair<unsigned long, std::string> numbered_pattern;

		static bool numbered_before(const numbered_pattern& a, const numbered_pattern& b) {
			return a.first < b.first;
		}

		struct client_count {
			unsigned n{};
			time_t last{};
			std::unordered_set<std::string> members;
		};

		std::vector<rule> rules;
		std::unordered_map<std::string, client_count> counts;
		pthread_mutex_t mutex{};
		log_func log{};
		time_t fchk_interval{};
		time_t lastftime_check{};
		time_t timestamp{};
		time_t last_prune{};
		bool open_failed{};

		// Call with mutex locked
		void clear() {
			rules.clear();
			counts.clear();
			timestamp = 0;
			lastftime_check = 0;
			open_failed = false;
		}

		static std::string count_key(const std::string& header, const std::string& client) {
			std::string key;
			for (char ch : header)
				key += (char)tolower((unsigned char)ch);
			return key + '\n' + client;
		}

		// Same semantics as find2strs_in_list(): the first plain pattern that
		// matches blocks; the first ! pattern that matches ends the check
		// unblocked; a list that ends on a ! pattern blocks what matched none.
		static bool check(const rule& r, const char* value, match* m) {
			bool found = false;
			const std::string* decided = nullptr;
			for (const std::string& p : r.patterns) {
				found = findstr_in_string(value, p.c_str());
				decided = &p;
				if (found != (p[0] == '!'))
					break;
			}
			if (found && m != nullptr) {
				m->section = r;
				m->value = value == nullptr ? "" : value;
				m->pattern = (decided != nullptr && (*decided)[0] != '!') ? *decided : "";
			}
			return found;
		}

		static bool is_number(const char* s) {
			if (*s == '\0')
				return false;
			for (; *s != '\0'; s++) {
				if (*s < '0' || *s > '9')
					return false;
			}
			return true;
		}

		// Re-read the file if it has changed (call with mutex locked).  A file
		// that cannot be opened keeps the rules already loaded and is tried
		// again at the next check.
		void refresh() {
			time_t now = time(nullptr);
			if (fchk_interval && lastftime_check != 0 && (now - lastftime_check) < fchk_interval)
				return;
			lastftime_check = now;
			time_t latest = fdate(fname);
			if (latest == timestamp)
				return;
			if (latest == -1) {  // no file (or it was removed): no rules
				rules.clear();
				timestamp = latest;
				return;
			}
			FILE* fp = iniOpenFile(fname, /* for_modify: */ false);
			if (fp == nullptr) {
				if (!open_failed && log != nullptr)
					log(LOG_WARNING, "!ERROR %d (%s) opening %s", errno, strerror(errno), fname);
				open_failed = true;
				return;
			}
			open_failed = false;
			str_list_t ini = iniReadFile(fp);
			iniCloseFile(fp);
			++fread_count;
			std::vector<rule> loaded;
			parse(ini, loaded);
			iniFreeStringList(ini);
			rules.swap(loaded);
			// A file changed within the last second may have been read while
			// still being written: read it again at the next check
			timestamp = (latest >= now - 1) ? latest - 1 : latest;
		}

		void parse(str_list_t ini, std::vector<rule>& loaded) {
			str_list_t sections = iniGetSectionList(ini, WEB_BLOCK_SECTION_PREFIX);
			str_list_t all = iniGetSectionListWithDupes(ini, WEB_BLOCK_SECTION_PREFIX);
			for (size_t i = 0; sections != nullptr && sections[i] != nullptr; i++) {
				const char* section = sections[i];
				size_t copies = 0;
				for (size_t j = 0; all != nullptr && all[j] != nullptr; j++) {
					if (stricmp(all[j], section) == 0)
						copies++;
				}
				if (copies > 1 && log != nullptr)
					log(LOG_WARNING, "!%s: [%s] appears %u times; only the first is used"
					    , fname, section, (uint)copies);
				rule r;
				r.header = section + strlen(WEB_BLOCK_SECTION_PREFIX);
				if (r.header.empty())
					continue;
				std::vector<numbered_pattern> numbered;
				named_string_t** keys = iniGetNamedStringList(ini, section);
				for (size_t k = 0; keys != nullptr && keys[k] != nullptr; k++) {
					const char* name = keys[k]->name;
					const char* value = keys[k]->value;
					if (is_number(name) && *value != '\0')
						numbered.emplace_back(strtoul(name, nullptr, 10), value);
				}
				iniFreeNamedStringList(keys);
				std::stable_sort(numbered.begin(), numbered.end(), numbered_before);
				for (const auto& n : numbered)
					r.patterns.push_back(n.second);
				r.filter_threshold = iniGetUInteger(ini, section, "FilterThreshold", 0);
				if (iniGetExistingValue(ini, section, "FilterDuration", nullptr, nullptr) != nullptr) {
					r.has_filter_duration = true;
					r.filter_duration = (uint)iniGetDuration(ini, section, "FilterDuration", 0);
				}
				if (iniGetExistingValue(ini, section, "FilterSilent", nullptr, nullptr) != nullptr) {
					r.has_filter_silent = true;
					r.filter_silent = iniGetBool(ini, section, "FilterSilent", false);
				}
				loaded.push_back(std::move(r));
			}
			iniFreeStringList(all);
			iniFreeStringList(sections);
		}
};

#endif // WEB_BLOCK_HPP_
