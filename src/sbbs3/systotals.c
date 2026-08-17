/* Synchronet cached system-wide message and file totals */

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

/****************************************************************************/
/* Neither total can be maintained as a counter: messages and files enter    */
/* and leave the bases through too many independent paths (JS, smbutil, the  */
/* terminal server, FTP, mail, services, web) for an incremented count to    */
/* stay true. They can only be obtained by enumerating every base - one file */
/* operation per sub-board or directory. That is cheap locally, but on a     */
/* network-mounted data directory each one is a separate round-trip that     */
/* cannot be pipelined, so a system with thousands of bases spends seconds   */
/* here. The enumeration is therefore shared process-wide for                */
/* cfg->totals_interval seconds rather than repeated per caller.             */
/*                                                                          */
/* This lives apart from getstats.c, which holds the getposts()/getfiles()   */
/* primitives these build on, because getstats.c is also compiled into the   */
/* single-threaded stand-alone utilities. Those link neither the xpdev       */
/* thread wrappers nor, on some toolchains, any pthread implementation, so   */
/* the mutex cannot live there. The invalidation flags that the mutating     */
/* code paths set do stay in getstats.c, precisely because several of those  */
/* paths are in files the utilities compile too.                            */
/****************************************************************************/

#include "getstats.h"
#include "threadwrap.h"
#include "scfglib.h"

struct cached_total {
	time_t   last;
	uint64_t value;
};

static pthread_mutex_t     totals_mutex;
static pthread_once_t      totals_once = PTHREAD_ONCE_INIT;
static struct cached_total cached_msgs;
static struct cached_total cached_files;

static void init_totals_mutex(void)
{
	pthread_mutex_init(&totals_mutex, NULL);
}

static uint64_t sum_posts(scfg_t* cfg)
{
	uint64_t total = 0;

	for (int i = 0; i < cfg->total_subs; i++)
		total += getposts(cfg, i);
	return total;
}

static uint64_t sum_files(scfg_t* cfg)
{
	uint64_t total = 0;

	for (int i = 0; i < cfg->total_dirs; i++)
		total += getfiles(cfg, i);
	return total;
}

static uint64_t get_cached_total(scfg_t* cfg, struct cached_total* cache
                                 , bool invalidated, uint64_t (*sum)(scfg_t*))
{
	uint64_t result;
	time_t   now;

	if (cfg->totals_interval == 0)
		return sum(cfg);

	pthread_once(&totals_once, init_totals_mutex);
	pthread_mutex_lock(&totals_mutex);
	now = time(NULL);
	/* Held across the enumeration deliberately: concurrent callers finding
	   the cache stale should wait for one scan rather than each run their
	   own. A base changed while that scan is in progress leaves the flag set
	   again, so the next caller re-counts. */
	if (invalidated || cache->last == 0 || difftime(now, cache->last) >= cfg->totals_interval) {
		cache->value = sum(cfg);
		cache->last = now;
	}
	result = cache->value;
	pthread_mutex_unlock(&totals_mutex);
	return result;
}

uint64_t total_msgs(scfg_t* cfg)
{
	if (cfg == NULL)
		return 0;
	return get_cached_total(cfg, &cached_msgs, msg_total_invalidated(), sum_posts);
}

uint64_t total_files(scfg_t* cfg)
{
	if (cfg == NULL)
		return 0;
	return get_cached_total(cfg, &cached_files, file_total_invalidated(), sum_files);
}
