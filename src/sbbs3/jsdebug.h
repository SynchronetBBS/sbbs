#ifndef JSDEBUG_H
#define JSDEBUG_H

#include <jsapi.h>

enum debug_action {
	DEBUG_CONTINUE,
	DEBUG_EXIT
};

void setup_debugger(void);
BOOL init_debugger(JSRuntime *rt, JSContext *cx,void (*puts)(const char *), char *(*getline)(void));
enum debug_action debug_prompt(JSContext *cx, JSObject *script);
void end_debugger(JSRuntime *rt, JSContext *cx);

#endif
