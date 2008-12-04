/* $Id$ */

#include <threadwrap.h>
#include "js_rtpool.h"

struct jsrt_queue {
        JSRuntime       *rt;
        int             maxbytes;
	int		used;
	int		created;
};

#define JSRT_QUEUE_SIZE		128
struct jsrt_queue jsrt_queue[JSRT_QUEUE_SIZE];
static pthread_mutex_t		jsrt_mutex;
static int			initialized=0;

JSRuntime *jsrt_GetNew(int maxbytes)
{
	int	i;
	int	last_unused=-1;

	if(!initialized) {
		pthread_mutex_init(&jsrt_mutex, NULL);
		initialized=TRUE;
	}
	pthread_mutex_lock(&jsrt_mutex);
	for(i=0; i<JSRT_QUEUE_SIZE; i++) {
		if(!jsrt_queue[i].created) {
			jsrt_queue[i].rt=JS_NewRuntime(maxbytes);
			if(jsrt_queue[i].rt != NULL) {
				jsrt_queue[i].maxbytes=maxbytes;
				jsrt_queue[i].created=1;
				jsrt_queue[i].used=0;
			}
		}
		if(!jsrt_queue[i].used) {
			last_unused=i;
			if(jsrt_queue[i].created && jsrt_queue[i].maxbytes == maxbytes) {
				jsrt_queue[i].used=1;
				pthread_mutex_unlock(&jsrt_mutex);
				return(jsrt_queue[i].rt);
			}
		}
	}

	if(last_unused != -1) {
		jsrt_queue[last_unused].used=1;
		pthread_mutex_unlock(&jsrt_mutex);
		return(jsrt_queue[last_unused].rt);
	}

	pthread_mutex_unlock(&jsrt_mutex);
	return(NULL);
}

void jsrt_Release(JSRuntime *rt)
{
	int	i;

	for(i=0; i<JSRT_QUEUE_SIZE; i++) {
		if(rt==jsrt_queue[i].rt) {
			pthread_mutex_lock(&jsrt_mutex);
			jsrt_queue[i].used=0;
			/* TODO: Clear "stuff"? */
			pthread_mutex_unlock(&jsrt_mutex);
		}
	}
}
