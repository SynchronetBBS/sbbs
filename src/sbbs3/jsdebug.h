#ifndef JSDEBUG_H
#define JSDEBUG_H

#include <jsapi.h>
#include "dllexport.h"

enum debug_action {
	DEBUG_CONTINUE,
	DEBUG_EXIT
};

DLLEXPORT void setup_debugger(void);
DLLEXPORT BOOL init_debugger(JSRuntime *rt, JSContext *cx,void (*puts)(const char *), char *(*getline)(void));
DLLEXPORT enum debug_action debug_prompt(JSContext *cx, JSObject *script);
DLLEXPORT void end_debugger(JSRuntime *rt, JSContext *cx);

#endif
