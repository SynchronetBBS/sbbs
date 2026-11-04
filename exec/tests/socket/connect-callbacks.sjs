/* Native callback results, GC roots and deferred JS close(). Loopback only. */
"use strict";
js.global.exit_code = 1;
function check(ok, message) { if (!ok) throw new Error(message); }
var listener = new Socket(), peers = [], clients = [], callbacks = 0, messages = {}, finishing = false;
check(listener.bind(0, "127.0.0.1") && listener.listen(), "Cannot listen");
var port = listener.local_port, listeners = [listener];
var timeout = js.setTimeout(function() { throw new Error("Connection callbacks did not finish"); }, 10000);
function finish() {
	if (callbacks != 5 || !messages.plain || !messages.bound || !messages.borrowed || !messages.synchronous || finishing) return;
	finishing = true;
	js.setTimeout(function() {
		listeners.forEach(function(l) { l.close(); }); peers.forEach(function(p) { p.close(); });
		clients.forEach(function(c) { c.close(); });
		js.clearTimeout(timeout);
		js.global.exit_code = 0;
		print("PASS: connect callbacks publish results, preserve binds/modes/closures and defer cancelled closure safely");
	}, 100);
}
function accept_peer() {
	var peer = this.accept(); peers.push(peer); peer.array_buffer = false;
	peer.on("read", function() {
		var line = this.recvline(100, 0);
		if (line) { messages[line] = true; this.close(); finish(); }
		else if (!this.is_connected) this.close();
	});
}
listener.on("read", accept_peer);
function start(name, host, target, expected, bind, borrowed) {
	var owner = borrowed ? new Socket() : null;
	var client = borrowed ? new Socket(true, owner.descriptor) : new Socket(), returned = false, captured = {name:name}, local;
	client.array_buffer = false; clients.push(client);
	if (owner) clients.push(owner);
	if (bind) { check(client.bind(0, "127.0.0.1"), "Cannot pre-bind"); local = client.local_port; client.nonblocking = true; }
	check(client.connect(host, target, function() {
		check(returned && this === client && captured.name == name, "Callback lost receiver, closure or asynchronous dispatch");
		check(this.is_connected === expected, name + ": incorrect connection status: " + this.error_str);
		if (expected) {
			check(this.remote_port == target, "Missing peer address");
			if (bind) check(this.local_port == local && this.nonblocking, "Lost bind or blocking configuration");
			this.descriptor = this.descriptor; // The pending flag must be clear before callback dispatch.
			this.send(name + "\r\n");
		} else check(this.error && this.error_str, "Missing failure details");
		this.close(); if (owner) owner.close(); callbacks++; finish();
	}), "Cannot start " + name);
	returned = true;
}
/* Drain the synchronous connection before starting workers: do not depend
   on a large listener backlog while the event loop is not running yet. */
var synchronous = new Socket(); synchronous.array_buffer = false;
check(synchronous.connect("127.0.0.1", port, 1), "Synchronous connect changed: " + synchronous.error_str);
synchronous.send("synchronous\r\n"); synchronous.close();
var syncpeer = listener.accept(); syncpeer.array_buffer = false;
check(syncpeer.recvline(100, 1) == "synchronous", "Synchronous payload missing"); syncpeer.close();
messages.synchronous = true;
start("plain", "127.0.0.1", port, true, false);
start("bound", "127.0.0.1", port, true, true);
var borrowed_listener = new Socket();
check(borrowed_listener.bind(0, "127.0.0.1") && borrowed_listener.listen(), "Cannot listen for borrowed socket");
listeners.push(borrowed_listener); borrowed_listener.on("read", accept_peer);
start("borrowed", "127.0.0.1", borrowed_listener.local_port, true, false, true);
var unused = new Socket(); check(unused.bind(0, "127.0.0.1"), "Cannot reserve refused port");
var refused = unused.local_port; unused.close();
start("refused", "127.0.0.1", refused, false, false);
start("invalid-host", "256.256.256.256", port, false, false);
var cancel_listener = new Socket();
check(cancel_listener.bind(0, "127.0.0.1") && cancel_listener.listen(), "Cannot listen for cancelled socket");
listeners.push(cancel_listener); cancel_listener.on("read", accept_peer);
var cancel_port = cancel_listener.local_port;
var cancelled = new Socket(), descriptor = cancelled.descriptor;
check(cancelled.connect("127.0.0.1", cancel_port, function() { throw new Error("Cancelled callback ran"); }), "Cannot start cancelled connection");
[function() { cancelled.connect("127.0.0.1", cancel_port, function() {}); },
 function() { cancelled.connect("127.0.0.1", cancel_port); },
 function() { cancelled.descriptor = descriptor; }].forEach(function(operation) {
	var rejected = false;
	try { operation(); } catch (e) { rejected = true; }
	check(rejected, "Competing operation was accepted");
});
cancelled.close(); cancelled.close();
check(cancelled.descriptor == -1 && !cancelled.is_connected, "JS close did not invalidate the socket immediately");
var replacement = new Socket();
check(replacement.descriptor != descriptor, "Descriptor reused before pending completion was collected"); replacement.close();
cancelled = null; js.gc(true);
js.do_callbacks = true;
