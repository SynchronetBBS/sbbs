/* $Id$ */

#include "js_rtpool.h"

struct jsrt_queue {
        JSRuntime       *rt;
        int             maxbytes;
	int		used;
	int		created;
};

#define JSRT_QUEUE_SIZE		128
struct jsrt_queue jsrt_queue[JSRT_QUEUE_SIZE];

JSRuntime *jsrt_GetNew(int maxbytes)
{
	int	i;

	for(i=0; i<JSRT_QUEUE_SIZE; i++) {
		if(!jsrt_queue[i].created) {
			jsrt_queue[i].rt=JS_NewRuntime(maxbytes);
			if(jsrt_queue[i].rt != NULL) {
				jsrt_queue[i].maxbytes=maxbytes;
				jsrt_queue[i].created=1;
				jsrt_queue[i].used=0;
			}
		}
		if(jsrt_queue[i].created && jsrt_queue[i].maxbytes == maxbytes && jsrt_queue[i].used == 0) {
			jsrt_queue[i].used=1;
			return(jsrt_queue[i].rt);
		}
	}

	return(NULL);
}

void jsrt_Release(JSRuntime *rt)
{
	int	i;

	for(i=0; i<JSRT_QUEUE_SIZE; i++) {
		if(rt==jsrt_queue[i].rt) {
			jsrt_queue[i].used=0;
			/* TODO: Clear "stuff"? */
		}
	}
}
