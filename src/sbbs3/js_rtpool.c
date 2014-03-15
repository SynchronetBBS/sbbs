/* $Id$ */

#include "js_rtpool.h"
#include <threadwrap.h>		/* Must be included after jsapi.h */
#include <genwrap.h>		/* SLEEP() */
#include <link_list.h>

#ifdef DLLCALL
#undef DLLCALL
#endif
#ifdef _WIN32
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else	/* !_WIN32 */
	#define DLLCALL
#endif

#define RT_UNIQUE	0
#define RT_SHARED	1
#define RT_SINGLE	2
#define RT_TYPE		RT_UNIQUE

#if (RT_TYPE != RT_UNIQUE)
#if (RT_TYPE == RT_SINGLE)
#define JSRT_QUEUE_SIZE		1
#else
#define JSRT_QUEUE_SIZE		256
#endif
struct jsrt_queue {
	JSRuntime       *rt;
	int			created;
#if (RT_TYPE==RT_SHARED)
	const char*	file;
	long		line;
#endif
};

struct jsrt_queue jsrt_queue[JSRT_QUEUE_SIZE];
#endif
static pthread_mutex_t		jsrt_mutex;
static int			initialized=0;
static link_list_t	rt_list;

#define TRIGGER_THREAD_STACK_SIZE	(256*1024)
static void trigger_thread(void *args)
{
	int	i;

	SetThreadName("JSRT Trigger");
	for(;;) {
		list_node_t *node;
		pthread_mutex_lock(&jsrt_mutex);
#if (RT_TYPE==RT_UNIQUE)
		for(node=listFirstNode(&rt_list); node; node=listNextNode(node)) {
			JS_TriggerAllOperationCallbacks(node->data);
		}
#else
		for(i=0; i<JSRT_QUEUE_SIZE; i++) {
			if(jsrt_queue[i].created)
				JS_TriggerAllOperationCallbacks(jsrt_queue[i].rt);
		}
#endif
		pthread_mutex_unlock(&jsrt_mutex);
		SLEEP(100);
	}
}

JSRuntime * DLLCALL jsrt_GetNew(int maxbytes, unsigned long timeout, const char *filename, long line)
{
#if (RT_TYPE==RT_SINGLE)
	if(!initialized) {
		initialized=TRUE;
		pthread_mutex_init(&jsrt_mutex, NULL);
		jsrt_queue[0].rt=JS_NewRuntime(128*1024*1024 /* 128 MB total for all scripts? */);
		jsrt_queue[0].created=1;
		_beginthread(trigger_thread, TRIGGER_THREAD_STACK_SIZE, NULL);
	}

	return jsrt_queue[0].rt;
#elif (RT_TYPE==RT_SHARED)
	int	i;

	if(!initialized) {
		initialized=TRUE;
		pthread_mutex_init(&jsrt_mutex, NULL);
		_beginthread(trigger_thread, TRIGGER_THREAD_STACK_SIZE, NULL);
	}

	pthread_mutex_lock(&jsrt_mutex);
	for(i=0; i<JSRT_QUEUE_SIZE; i++) {
		if(!jsrt_queue[i].created) {
			jsrt_queue[i].rt=JS_NewRuntime(maxbytes);
			if(jsrt_queue[i].rt != NULL) {
				jsrt_queue[i].file=filename;
				jsrt_queue[i].line=line;
				jsrt_queue[i].created=1;
			}
		}
		if(jsrt_queue[i].created && jsrt_queue[i].file == filename && jsrt_queue[i].line == line) {
			pthread_mutex_unlock(&jsrt_mutex);
			return(jsrt_queue[i].rt);
		}
	}
	pthread_mutex_unlock(&jsrt_mutex);

	return(NULL);
#elif (RT_TYPE==RT_UNIQUE)
	JSRuntime *ret;

	if(!initialized) {
		initialized=TRUE;
		pthread_mutex_init(&jsrt_mutex, NULL);
		listInit(&rt_list, 0);
		_beginthread(trigger_thread, TRIGGER_THREAD_STACK_SIZE, NULL);
	}
	pthread_mutex_lock(&jsrt_mutex);
	ret=JS_NewRuntime(maxbytes);
	listPushNode(&rt_list, ret);
	pthread_mutex_unlock(&jsrt_mutex);
	return ret;
#endif
}

void DLLCALL jsrt_Release(JSRuntime *rt)
{
#if (RT_TYPE==RT_UNIQUE)
	list_node_t	*node;

	pthread_mutex_lock(&jsrt_mutex);
	node = listFindNode(&rt_list, rt, 0);
	if(node)
		listRemoveNode(&rt_list, node, FALSE);
	JS_DestroyRuntime(rt);
	pthread_mutex_unlock(&jsrt_mutex);
#endif
}
