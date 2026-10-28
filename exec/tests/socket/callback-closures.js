/* Standalone regression for #1294, including GC and independent closures. */
"use strict";
var count = 0, expected = 18;
function register(kind, value, explicitThis) {
	var local = {value:value}, context = {tag:value}, id;
	function makeCallback() {
		var nested = value + 100;
		return function() {
			if (local.value !== value || nested !== value + 100)
				throw new Error(kind + " lost its closure");
			if (explicitThis && this !== context)
				throw new Error(kind + " lost its this object");
			if (kind == "interval")
				js.clearInterval(id);
			count++;
		};
	}
	if (kind == "immediate") {
		if (explicitThis) js.setImmediate(makeCallback(), context);
		else js.setImmediate(makeCallback());
	}
	else if (kind == "timeout") {
		if (explicitThis) js.setTimeout(makeCallback(), 1, context);
		else js.setTimeout(makeCallback(), 1);
	}
	else {
		if (explicitThis) id = js.setInterval(makeCallback(), 1, context);
		else id = js.setInterval(makeCallback(), 1);
	}
}
["immediate", "timeout", "interval"].forEach(function(kind) {
	for (var i = 0; i < 3; i++) {
		register(kind, i, false);
		register(kind, i + 10, true);
	}
});
js.gc(true);
js.setTimeout(function() {
	if (count !== expected)
		throw new Error("Expected " + expected + " closure callbacks, got " + count);
	print("PASS: timer and immediate closures survive GC (#1294)");
}, 100);
js.do_callbacks = true;
