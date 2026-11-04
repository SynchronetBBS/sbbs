/* Driven by connect-cancel.py with private resolver/fault interposition. */
"use strict";
js.global.exit_code = 1;
var mode = argv[0], port = Number(argv[1]), dir = backslash(argv[2]);
var original = new Socket(), saved = original.descriptor, calls = 0, replacement;
original.array_buffer = false;
function check(ok, message) { if (!ok) throw new Error(message); }
function write(name, value) {
	var file = new File(dir + name); check(file.open("w"), "Cannot write " + name);
	file.write(value || "ready"); file.close();
}
function wait(name) {
	var until = system.timer + 6;
	while (!file_exists(dir + name) && system.timer < until) mswait(10);
	check(file_exists(dir + name), "Missing " + name);
}
function complete() { calls++; throw new Error("Closed connection invoked JS callback"); }

if (mode == "pair-failure" || mode == "thread-failure") {
	write("arm");
	var rejected = false;
	try { original.connect("127.0.0.1", port, complete); } catch (e) { rejected = true; }
	check(rejected && original.descriptor == saved && !original.is_connected, "Start failure damaged the socket");
	file_remove(dir + "arm");
	check(original.connect("127.0.0.1", port, function() {
		check(this === original && this.is_connected, "Retry after startup failure did not complete");
		this.send("RECOVERED\r\n"); this.close();
		js.global.exit_code = 0; print("PASS: " + mode);
	}), "Start failure retained pending flag");
	js.do_callbacks = true;
} else {
	write("tracked", String(saved));
	check(original.connect("socket-delay.invalid", port, complete), "Cannot start delayed connection");
	wait("ready");
	if (mode == "close-ready" || mode == "teardown-ready") {
		write("release"); wait("notified"); // Native completion is queued; JS has not dispatched it.
	}
	if (mode != "teardown-open" && mode != "teardown-ready" && mode != "exception") {
		original.close(); original.close();
		check(original.descriptor == -1 && !original.is_connected, "JS close was not immediate");
	}
	if (mode != "close-ready" && mode != "teardown-ready")
		check(!file_exists(dir + "closed"), "Native close occurred before DNS completed");
	replacement = new Socket(); replacement.array_buffer = false;
	check(replacement.descriptor != saved, "A live worker's descriptor was recycled");
	write("script-ready");
	if (mode.indexOf("teardown") == 0 || mode == "exception") {
		replacement.close();
		js.global.exit_code = mode == "exception" ? 7 : 0;
		js.do_callbacks = false;
		print("RETURNING: " + mode);
		if (mode == "exception") throw new Error("EXPECTED_CONNECT_TEARDOWN");
	} else {
		original = null; js.gc(true); // The completion event must retain its socket.
		var began = system.timer;
		var timer = js.setInterval(function() {
			if (!file_exists(dir + "closed")) {
				check(system.timer - began < 7, "Deferred native close did not occur"); return;
			}
			js.clearInterval(timer);
			check(!calls && !replacement.is_connected, "Late completion changed replacement socket");
			check(!replacement.send("UNRELATED SOCKET\r\n"), "Replacement was connected by the old worker");
			replacement.close();
			js.global.exit_code = 0; print("PASS: " + mode);
		}, 10);
		js.do_callbacks = true;
	}
}
