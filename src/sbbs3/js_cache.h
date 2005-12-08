#ifndef _JS_CACHE_H_
#define _JS_CACHE_H_

/* JS Compile Cache Stuff */
struct cache_data {
	char	filename[MAX_PATH+1];
	time_t	mtime,ctime;
	off_t	size;
	ulong	runcount;
	time_t	lastrun;
	time_t	laststat;
	JSScript	*script;
	int		running;
};

typedef struct {
	DWORD   size;				/* sizeof(js_cache_startup_t) */
	time_t	max_age;			/* Max age in seconds for lastrun */
	ulong	max_scripts;		/* Max number of scripts to hold in cache */
	time_t	stale_timeout;		/* Minimum time between calls to stat() */
} js_cache_startup_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Gets the compiled script from the cache if available and
 * laststat is less than stale time or a new stat() matches
 * the mtime, ctime, and size.
 * Compiles it and adds to the cache if not.
 */
struct cached_data *js_get_compiled_script(char *filename);

/*
 * Starts the caching thread.  The caching thread periodically
 * checks the cache for entries to be expired.  They are expired
 * by weigth where weight=(age in seconds)/(number of times ran)
 * does NOT check the laststat nor does it do a stat().
 * It also does not expire an entry for which running is non-zero.
 */
void js_cache_thread(void *args);

/*
 * Decrements the running value, increments runcount, and updates
 * lastrun
 */
void js_script_done(struct cached_data entry);
#ifdef __cplusplus
}
#endif

#endif
