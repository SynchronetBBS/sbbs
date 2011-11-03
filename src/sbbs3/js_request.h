#ifndef _JS_REQUEST_H_
#define _JS_REQUEST_H_

//#define DEBUG_JS_REQUESTS

#ifdef DEBUG_JS_REQUESTS
#ifdef __cplusplus
extern "C" {
#endif
void js_debug_beginrequest(JSContext *cx, const char *file, unsigned long line);
void js_debug_endrequest(JSContext *cx, const char *file, unsigned long line);
jsrefcount js_debug_suspendrequest(JSContext *cx, const char *file, unsigned long line);
void js_debug_resumerequest(JSContext *cx, jsrefcount rc, const char *file, unsigned long line);
#ifdef __cplusplus
}
#endif

#define JS_BEGINREQUEST(cx)			js_debug_beginrequest(cx, __FILE__, __LINE__)
#define JS_ENDREQUEST(cx)			js_debug_endrequest(cx, __FILE__, __LINE__)
#define JS_SUSPENDREQUEST(cx)		js_debug_suspendrequest(cx, __FILE__, __LINE__)
#define JS_RESUMEREQUEST(cx, rf)	js_debug_resumerequest(cx, rf, __FILE__, __LINE__)
#else
#define JS_BEGINREQUEST(cx)	JS_BeginRequest(cx)
#define JS_ENDREQUEST(cx)	JS_EndRequest(cx)
#define JS_SUSPENDREQUEST(cx)	JS_SuspendRequest(cx)
#define JS_RESUMEREQUEST(cx, rf)	JS_ResumeRequest(cx, rf)
#endif

#endif
