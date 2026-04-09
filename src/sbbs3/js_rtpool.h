#ifndef _JS_RTPOOL_H_
#define _JS_RTPOOL_H_

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif
#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT   __declspec(dllexport)
	#else
		#define DLLEXPORT   __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL
	#else
		#define DLLCALL
	#endif
#else   /* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#if defined(__cplusplus)
extern "C" {
#endif
/* SM128: returns JSContext* (which IS the runtime in SM128).
 * parent_cx: optional parent context to share GC with (for background threads). */
DLLEXPORT JSContext * DLLCALL jsrt_GetNew(int maxbytes, unsigned long timeout, const char *filename, long line, JSContext* parent_cx = nullptr);
DLLEXPORT void DLLCALL jsrt_Release(JSContext *);
/* SM128: JS_SetRuntimePrivate / JS_GetRuntimePrivate emulation */
DLLEXPORT void DLLCALL jsrt_SetRuntimePrivate(JSRuntime* rt, void* data);
DLLEXPORT void * DLLCALL jsrt_GetRuntimePrivate(JSRuntime* rt);
DLLEXPORT void DLLCALL jsrt_ClearRuntimePrivate(JSRuntime* rt);
#if defined(__cplusplus)
}
#endif

#endif
