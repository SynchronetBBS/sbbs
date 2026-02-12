/* Synchronet *cached* client/content-filtering (trashcan/twit) file classes */

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

#ifndef FILTERFILE_HPP_
#define FILTERFILE_HPP_
#include "trash.h"
#include <atomic>
#include <mutex>
#include <time.h>

class filterFile {
	public:
		filterFile(scfg_t* cfg, const char* fname)
			: fchk_interval(cfg->cache_filter_files) {
			if (getfname(fname) == fname)
				snprintf(this->fname, sizeof this->fname, "%s%s", cfg->ctrl_dir, fname);
			else
				strlcpy(this->fname, fname, sizeof this->fname);
		}
		filterFile() = default;
		~filterFile() {
			strListFree(&list);
		}
		std::atomic<uint> fread_count{};
		std::atomic<uint> total_found{};
		time_t fchk_interval; // seconds
		char fname[MAX_PATH + 1];
		bool listed(const char* str1, const char* str2 = nullptr, struct trash* details = nullptr) {
			bool result;
			time_t now = time(nullptr);
			if (fchk_interval) {
				const std::lock_guard<std::mutex> lock(mutex);
				if ((now - lastftime_check) >= fchk_interval) {
					lastftime_check = now;
					time_t latest = fdate(fname);
					if (latest > timestamp) {
						strListFree(&list);
						list = findstr_list(fname);
						timestamp = latest;
						++fread_count;
					}
				}
				result = trash_in_list(str1, str2, list, details);
			} else {
				char str[FINDSTR_MAX_LINE_LEN + 1];
				result = find2strs(str1, str2, fname, str);
				++fread_count;
				if (details != nullptr) {
					if (trash_parse_details(str, details, nullptr, 0)
						&& details->expires && details->expires <= now)
						result = false;
				}
			}
			if (result)
				++total_found;
			return result;
		}
	private:
		str_list_t list{};
		std::mutex mutex;
		time_t lastftime_check{};
		time_t timestamp{};

};

class trashCan : public filterFile {
	public:
		trashCan(scfg_t* cfg, const char* name) {
			trashcan_fname(cfg, name, filterFile::fname, sizeof filterFile::fname);
			filterFile::fchk_interval = cfg->cache_filter_files;
		}
};

#endif  /* Don't add anything after this line */
