/* Verify the JS interrupt/operation callback re-arms after firing.
 *
 * Regression test for a bug in the SM128 JS_SetOperationCallback shim
 * where JS_ResetInterruptCallback was passed the wrong enable flag,
 * causing the callback to self-disable after its first invocation.
 * When that happens, js.counter stops advancing, js.auto_terminate
 * becomes unresponsive, and servers hang on shutdown because running
 * scripts never notice the terminated flag.
 *
 * The interrupt callback is driven by a background trigger thread that
 * fires roughly every 100ms. Spinning for ~1 second should produce
 * several callback invocations; with the bug, only one ever fires.
 */

"use strict";

/* The shim bug this test guards against only exists in SM128+. Under
 * SM185 the operation callback is driven by the engine's own bytecode
 * counter (no background trigger thread), so a 1-second spin can
 * legitimately produce only a single callback invocation — which would
 * look identical to the SM128 buggy state. Skip on older engines. */
if (js.version.indexOf("C128") < 0 && js.version.indexOf("C 128") < 0)
	exit(0);

var saved_auto = js.auto_terminate;
js.auto_terminate = false;

var initial = js.counter;
var start = time();
while (time() - start < 1) {}
var delta = js.counter - initial;

js.auto_terminate = saved_auto;

if (delta < 2)
	throw new Error("interrupt callback did not re-arm: "
		+ "js.counter advanced by only " + delta
		+ " across " + (time() - start) + "s of spinning");
