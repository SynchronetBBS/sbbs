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
