#include "sbbs.h"
#include "js_rtpool.h"
#include <js/Initialization.h>
#include <js/GCAPI.h>
#include <unordered_map>
#include <time.h>

static pthread_mutex_t jsrt_mutex;
static link_list_t     cx_list;

/* SM128: emulate JS_SetRuntimePrivate / JS_GetRuntimePrivate */
static pthread_mutex_t jsrt_private_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::unordered_map<JSRuntime*, void*> jsrt_private_map;

/* SM128: track heap-allocated AutoDisableGenerationalGC guards per context */
static std::unordered_map<JSContext*, JS::AutoDisableGenerationalGC*> jsrt_nogen_map;

void jsrt_SetRuntimePrivate(JSRuntime* rt, void* data) {
    pthread_mutex_lock(&jsrt_private_mutex);
    jsrt_private_map[rt] = data;
    pthread_mutex_unlock(&jsrt_private_mutex);
}
void* jsrt_GetRuntimePrivate(JSRuntime* rt) {
    pthread_mutex_lock(&jsrt_private_mutex);
    auto it = jsrt_private_map.find(rt);
    void* result = (it != jsrt_private_map.end()) ? it->second : nullptr;
    pthread_mutex_unlock(&jsrt_private_mutex);
    return result;
}
void jsrt_ClearRuntimePrivate(JSRuntime* rt) {
    pthread_mutex_lock(&jsrt_private_mutex);
    jsrt_private_map.erase(rt);
    pthread_mutex_unlock(&jsrt_private_mutex);
}

/* trigger_thread shutdown state */
static volatile bool   trigger_stop = false;
static pthread_mutex_t trigger_done_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  trigger_done_cond  = PTHREAD_COND_INITIALIZER;
static bool            trigger_done_flag  = false;

#define TRIGGER_THREAD_STACK_SIZE       (256 * 1024)
static void trigger_thread(void *args)
{
	SetThreadName("sbbs/jsRTtrig");
	while (!trigger_stop) {
		list_node_t *node;
		pthread_mutex_lock(&jsrt_mutex);
		for (node = listFirstNode(&cx_list); node; node = listNextNode(node))
			JS_RequestInterruptCallback(static_cast<JSContext *>(node->data));
		pthread_mutex_unlock(&jsrt_mutex);
		SLEEP(100);
	}
	pthread_mutex_lock(&trigger_done_mutex);
	trigger_done_flag = true;
	pthread_cond_signal(&trigger_done_cond);
	pthread_mutex_unlock(&trigger_done_mutex);
}

/* Called via atexit() — stops trigger_thread then shuts down SpiderMonkey.
 * libsbbs unloads before libmozjs at process exit (reverse load order), so
 * this handler runs before SM128's own static destructors. */
static void jsrt_atexit_shutdown(void)
{
	trigger_stop = true;

	/* Wait up to 1 second for trigger_thread to acknowledge and exit */
	pthread_mutex_lock(&trigger_done_mutex);
	if (!trigger_done_flag) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 1;
		pthread_cond_timedwait(&trigger_done_cond, &trigger_done_mutex, &ts);
	}
	pthread_mutex_unlock(&trigger_done_mutex);

	JS_ShutDown();
}

static void
jsrt_init(void)
{
	JS_Init();
	pthread_mutex_init(&jsrt_mutex, NULL);
	pthread_mutex_lock(&jsrt_mutex);
	listInit(&cx_list, 0);
	pthread_mutex_unlock(&jsrt_mutex);
	atexit(jsrt_atexit_shutdown);
	_beginthread(trigger_thread, TRIGGER_THREAD_STACK_SIZE, NULL);
}

static pthread_once_t jsrt_once = PTHREAD_ONCE_INIT;

/* SM128: each "runtime" is now a JSContext (which contains a JSRuntime).
 * For background threads, pass parent_cx to share GC with the parent context. */
JSContext * jsrt_GetNew(int maxbytes, unsigned long timeout, const char *filename, long line, JSContext* parent_cx)
{
	JSContext *ret;

	pthread_once(&jsrt_once, jsrt_init);
	/* Create and initialize the context outside the mutex (heavyweight operations) */
	ret = JS_NewContext(maxbytes);
	if (ret != NULL) {
		if (!JS::InitSelfHostedCode(ret)) {
			JS_DestroyContext(ret);
			ret = NULL;
		}
	}
	if (ret != NULL) {
		/* Diagnostic: shrink nursery to catch GC rooting bugs sooner */
		const char* nursery_env = getenv("SBBS_SMALL_NURSERY");
		if (nursery_env != NULL && *nursery_env != '0') {
			JS_SetGCParameter(ret, JSGC_MAX_NURSERY_BYTES, 256 * 1024);
			JS_SetGCParameter(ret, JSGC_MIN_NURSERY_BYTES, 256 * 1024);
		}
		/* SM128: disable compacting GC by default. */
		const char* compact_env = getenv("SBBS_COMPACTING_GC");
		if (compact_env == NULL || *compact_env == '0') {
			JS_SetGCParameter(ret, JSGC_COMPACTING_ENABLED, 0);
		}
		/* SM128: disable generational (nursery) GC by default.
		 * The codebase stores raw JSObject* pointers in C++ heap structures
		 * and passes them through function parameters without proper write
		 * barriers.  The nursery store buffer accumulates entries that
		 * reference these untracked pointers, causing crashes during minor
		 * GC when the entries are processed.
		 * Re-enable with SBBS_GENERATIONAL_GC=1 after full audit. */
		const char* gen_env = getenv("SBBS_GENERATIONAL_GC");
		if (gen_env == NULL || *gen_env == '0') {
			auto* guard = new JS::AutoDisableGenerationalGC(ret);
			pthread_mutex_lock(&jsrt_private_mutex);
			jsrt_nogen_map[ret] = guard;
			pthread_mutex_unlock(&jsrt_private_mutex);
		}
		/* Diagnostic: disable JIT to rule out JIT write barrier bugs */
		const char* nojit_env = getenv("SBBS_NO_JIT");
		if (nojit_env != NULL && *nojit_env != '0') {
			JS_SetGlobalJitCompilerOption(ret, JSJITCOMPILER_ION_ENABLE, 0);
			JS_SetGlobalJitCompilerOption(ret, JSJITCOMPILER_BASELINE_ENABLE, 0);
			JS_SetGlobalJitCompilerOption(ret, JSJITCOMPILER_BASELINE_INTERPRETER_ENABLE, 0);
		}
		pthread_mutex_lock(&jsrt_mutex);
		listPushNode(&cx_list, ret);
		pthread_mutex_unlock(&jsrt_mutex);
	}
	return ret;
}

void jsrt_Release(JSContext *cx)
{
	list_node_t *node;

	/* Re-enable generational GC (if disabled) before destroying context */
	pthread_mutex_lock(&jsrt_private_mutex);
	auto it = jsrt_nogen_map.find(cx);
	if (it != jsrt_nogen_map.end()) {
		delete it->second;
		jsrt_nogen_map.erase(it);
	}
	pthread_mutex_unlock(&jsrt_private_mutex);

	pthread_mutex_lock(&jsrt_mutex);
	node = listFindNode(&cx_list, cx, 0);
	if (node)
		listRemoveNode(&cx_list, node, FALSE);
	jsrt_ClearRuntimePrivate(JS_GetRuntime(cx));
	pthread_mutex_unlock(&jsrt_mutex);
	/* JS_DestroyContext runs finalizers (e.g. js_global_finalize, which waits
	 * on background threads).  A background thread draining itself calls
	 * jsrt_Release and needs jsrt_mutex — so destruction must happen outside
	 * the lock to avoid deadlock. */
	JS_DestroyContext(cx);
	/* Do NOT call JS_ShutDown() here — the engine must remain initialized for
	 * future contexts (e.g. new node connections).  JS_ShutDown() is a
	 * process-lifetime call; it is deferred to jsrt_atexit_shutdown(). */
}
