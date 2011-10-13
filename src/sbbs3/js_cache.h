#ifndef _JS_CACHE_H_
#define _JS_CACHE_H_

#include "link_list.h"
#include "jscntxt.h"
#include "jsscript.h"

/* JS Compile Cache Stuff */
struct context_cache {
	JSContext	*cx;
	link_list_t	cache;
};

struct cache_data {
	char	filename[MAX_PATH+1];
	time_t	mtime,ctime;		/* st_mtime/st_ctime from last stat() call */
	off_t	size;				/* File size when loaded */
	ulong	runcount;			/* Number of times this script has been used */
	time_t	lastrun;			/* Time script was last ran */
	time_t	laststat;			/* Time of last call to stat() */
	JSObject	*script;
	int		running;			/* Count of currently running scripts */
	int		expired;			/* TRUE if last stat() makes this as stale and
								 * running was non-zero */
};

typedef struct {
	DWORD   size;				/* sizeof(js_cache_startup_t) */
	time_t	max_age;			/* Max age in seconds for lastrun */
	ulong	max_scripts;		/* Max number of scripts to hold in cache */
	time_t	stale_timeout;		/* Minimum time between calls to stat() */
	ulong	expire_period;		/* Time between servicing the exipry cache... milliseconds */
} js_cache_startup_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Gets the compiled script from the cache if available and
 * laststat is less than stale time or a new stat() matches
 * the mtime, ctime, and size.
 * Compiles it and adds to the cache if not.
 * Increments running value.
 */
JSObject *js_get_compiled_script(JSContext *cx, char *filename);

/*
 * Starts the caching thread.  The caching thread periodically
 * checks the caches for entries to be expired.  They are expired
 * by weigth where weight=(age in seconds)/(number of times ran)
 * does NOT check the laststat nor does it do a stat().
 * It also does not expire an entry for which running is non-zero.
 */
void js_cache_thread(void *args);

/*
 * Decrements the running value, increments runcount, and updates
 * lastrun
 */
void js_script_done(struct cached_data *entry);

/*
 * Sets the expired member for any existing cached copies of filename
 * Required for self-modifying scripts.
 */
void js_cache_expire(char *filename);

/*
 * Creates a new cached runtime and context
 */
JSContext *js_get_cached_context(size_t maxbytes, size_t stacksize);
#ifdef __cplusplus
}
#endif

#endif
