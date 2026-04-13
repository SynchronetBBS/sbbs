/* Verify the JS interrupt callback re-arms after firing.
 *
 * Regression test for a bug in the SM128 JS_SetOperationCallback shim
 * where JS_ResetInterruptCallback was passed the wrong enable flag,
 * causing the callback to self-disable after its first invocation.
 * With the bug, js.time_limit never trips and tight loops hang forever.
 */

"use strict";

js.auto_terminate = true;
js.time_limit = 1;

var start = time();
var deadline = start + 5;
try {
	while (time() < deadline) {
		for (var i = 0; i < 100000; i++) {}
	}
} catch (e) {
	/* Expected: "Terminated" via HandleInterrupt */
}
var elapsed = time() - start;
if (elapsed >= 5)
	throw new Error("time_limit failed to terminate loop — "
		+ "interrupt callback not re-arming (elapsed=" + elapsed + "s)");
