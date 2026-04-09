#ifndef JSDEBUG_H
#define JSDEBUG_H

#ifdef __cplusplus
#include <jsapi.h>

enum debug_action {
	DEBUG_CONTINUE,
	DEBUG_EXIT
};

extern "C" {

void setup_debugger(void);
BOOL init_debugger(JSContext *cx, void (*puts)(const char *), char *(*getline)(void));
enum debug_action debug_prompt(JSContext *cx, JSScript *script);
void end_debugger(JSContext *cx);

} /* extern "C" */
#endif /* __cplusplus */

#endif
