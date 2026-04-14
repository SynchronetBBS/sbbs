#include "sbbs.h"
#include "js_rtpool.h"
#include "semwrap.h"
#include <js/Initialization.h>
#include <js/GCAPI.h>
#include <js/Realm.h>
#include <unordered_map>

static pthread_mutex_t jsrt_mutex;
static link_list_t     cx_list;

/* SM128: emulate JS_SetRuntimePrivate / JS_GetRuntimePrivate */
static pthread_mutex_t jsrt_private_mutex;
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
static sem_t           trigger_done_sem;

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
	sem_post(&trigger_done_sem);
}

/* Called via atexit() — stops trigger_thread then shuts down SpiderMonkey.
 * libsbbs unloads before libmozjs at process exit (reverse load order), so
 * this handler runs before SM128's own static destructors. */
static void jsrt_atexit_shutdown(void)
{
	trigger_stop = true;
	sem_trywait_block(&trigger_done_sem, 1000);	/* wait up to 1 second */
	JS_ShutDown();
}

static void
jsrt_init(void)
{
	/* SM128: pass isDebugBuild=true when using the debug mozjs-128.dll.
	 * JS_Init() uses #ifdef DEBUG to select, but defining DEBUG globally
	 * causes debug-only DLL imports; call directly using _DEBUG instead. */
#ifdef _DEBUG
	JS::detail::InitWithFailureDiagnostic(true);
#else
	JS::detail::InitWithFailureDiagnostic(false);
#endif
	pthread_mutex_init(&jsrt_mutex, NULL);
	pthread_mutex_init(&jsrt_private_mutex, NULL);
	sem_init(&trigger_done_sem, 0, 0);
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

	/* SM128: wait for any background threads before destroying the context.
	 * js_global_finalize (called during GC inside JS_DestroyContext) cannot
	 * reliably wait: in SM128's release build the GC may not collect the global
	 * during JS_DestroyContext, leaving bg threads running when JS_ShutDown()
	 * is called at exit.  Do the wait here, on the context's owning thread,
	 * before the context is torn down.
	 *
	 * JS_GetGlobalObject uses the current realm, so call it before LeaveRealm. */
	{
		JSObject* glob = JS_GetGlobalObject(cx);
		if (glob != NULL) {
			global_private_t* p = (global_private_t*)JS_GetPrivate(glob);
			if (p != NULL) {
				while (p->bg_count) {
					if (sem_wait(&p->bg_sem) == 0)
						p->bg_count--;
				}
			}
		}
	}

	/* SM128: leave the initial realm before destroying the context.
	 * JS_NewCompartmentAndGlobalObject calls JS::EnterRealm (discarding the
	 * return value which was nullptr — no prior realm) to enter the newly
	 * created global's realm.  LeaveRealm with nullptr correctly un-roots the
	 * global (JS::Realm.h: "Entering a realm roots the realm and its global
	 * object until the matching LeaveRealm call").
	 *
	 * Any extra EnterRealm calls made during the context's lifetime (e.g. by
	 * js_execxtrn for door-game realms) must be balanced by matching LeaveRealm
	 * calls before jsrt_Release is called, so that only the original EnterRealm
	 * (which returned nullptr) remains unbalanced here.  After this LeaveRealm,
	 * all realm globals are un-rooted and the pre-destroy GC can collect them.
	 * (debug assertion: "Shouldn't destroy context with active realm") */
	if (JS::GetCurrentRealmOrNull(cx) != nullptr)
		JS::LeaveRealm(cx, nullptr);

	pthread_mutex_lock(&jsrt_mutex);
	node = listFindNode(&cx_list, cx, 0);
	if (node)
		listRemoveNode(&cx_list, node, FALSE);
	jsrt_ClearRuntimePrivate(JS_GetRuntime(cx));
	pthread_mutex_unlock(&jsrt_mutex);

	/* SM128: destroy context OUTSIDE jsrt_mutex. JS_DestroyContext triggers a
	 * final GC which calls js_global_finalize. The finalizer's bg_count wait
	 * has already been done above, so js_global_finalize just frees p.
	 *
	 * Delete the AutoDisableGenerationalGC guard (if any) BEFORE destroying the
	 * context so its destructor can properly re-enable the nursery.  The nursery
	 * was disabled for the entire lifetime of this context, so no allocations
	 * ever went to the nursery and the nursery store buffer is empty.
	 * Re-enabling the nursery here is therefore safe — there are no nursery
	 * objects with untracked raw C++ pointers to worry about at teardown.
	 * Keeping the guard alive through JS_DestroyContext causes SM128 debug builds
	 * to crash in AssertNoRootsTracer::onChild (checkNoRuntimeRoots) because the
	 * disabled nursery causes destroyRuntime to take a different GC code path. */
	pthread_mutex_lock(&jsrt_private_mutex);
	auto it = jsrt_nogen_map.find(cx);
	if (it != jsrt_nogen_map.end()) {
		delete it->second;   /* dtor re-enables nursery; safe because nursery was never used */
		jsrt_nogen_map.erase(it);
	}
	pthread_mutex_unlock(&jsrt_private_mutex);

	/* SM128: force a full shrinking GC before JS_DestroyContext so all realm
	 * globals are collected (globalObj_ → null) before destroyRuntime's final
	 * GC runs checkNoRuntimeRoots.  After LeaveRealm above, all globals are
	 * unreachable from C++ (no Rooted<T>, no PersistentRooted<T>, no
	 * current-realm trace), so the GC can finalize them.
	 *
	 * GCOptions::Shrink is used (vs. Normal / JS_GC) because it guarantees
	 * "all unreferenced objects are removed from the system" — the explicit
	 * assurance we need before destroyRuntime's checkNoRuntimeRoots runs.
	 * Neither PrepareForFullGC nor NonIncrementalGC invokes checkNoRuntimeRoots
	 * (GCReason != DESTROY_RUNTIME), so this is safe even in debug builds. */
	JS::PrepareForFullGC(cx);
	JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

	JS_DestroyContext(cx);

	/* Do NOT call JS_ShutDown() here — the engine must remain initialized for
	 * future contexts (e.g. new node connections).  JS_ShutDown() is a
	 * process-lifetime call; it is deferred to jsrt_atexit_shutdown(). */
}
