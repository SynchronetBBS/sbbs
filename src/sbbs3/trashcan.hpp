/* Synchronet client/content-filtering (trashcan/twit) functions */

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

#ifndef TRASH_HPP_
#define TRASH_HPP_
#include "trash.h"
#include <mutex>
#include <time.h>

class trashCan {
	public:
		trashCan(scfg_t* cfg, const char* name) {
			trashcan_fname(cfg, name, fname, sizeof fname);
		}
		~trashCan() {
			strListFree(&list);
		}
		time_t timestamp{};
		unsigned int read_count{};
		unsigned int total_found{};
		time_t min_interval{ 10 }; // seconds
		bool listed(const char* str1, const char* str2 = nullptr, struct trash* details = nullptr) {
			const std::lock_guard<std::mutex> lock(mutex);
			time_t now = time(NULL);
			if ((now - lastftime_check) >= min_interval) {
				lastftime_check = now;
				time_t latest = fdate(fname);
				if (latest > timestamp) {
					strListFree(&list);
					list = findstr_list(fname);
					timestamp = latest;
					++read_count;
				}
			}
			bool result = trash_in_list(str1, str2, list, details);
			if (result)
				++total_found;
			return result;
		}
	private:
		char fname[MAX_PATH + 1];
		str_list_t list{};
		std::mutex mutex;
		time_t lastftime_check{};
};

#endif  /* Don't add anything after this line */
