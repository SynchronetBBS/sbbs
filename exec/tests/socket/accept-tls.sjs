/* Driven by ircd/tls-network.py with a private BBS certificate. */
"use strict";
var mode = argv[1], accepted, returned = false, ticked = false, calls = 0;
var listener = new Socket();
if (!listener.bind(Number(argv[0]), "127.0.0.1") || !listener.listen(1))
	throw new Error("Cannot bind TLS fixture");
listener.once("read", function() {
	accepted = this.accept();
	listener.close();
	accepted.array_buffer = false;
	if (mode == "arguments") {
		[0, -1, 4, 301].forEach(function(timeout) {
			var rejected = false;
			try { accepted.acceptTLS(function() {}, timeout); }
			catch (e) { rejected = true; }
			if (!rejected || !accepted.is_connected)
				throw new Error("Invalid timeout changed the socket");
		});
		accepted.close();
		print("PASS: arguments");
		return;
	}
	var began = system.timer, token = {value:mode};
	if (!accepted.acceptTLS(function() {
		calls++;
		if (token.value !== mode)
			throw new Error("TLS callback lost its captured local value (#1294)");
		if (!returned || this !== accepted || (mode == "timeout" && !ticked))
			throw new Error("TLS callback did not run asynchronously on its socket");
		if (mode == "cancel" || mode == "exit")
			throw new Error("Cancelled handshake invoked its callback");
		if (mode == "success") {
			if (!this.ssl_session || !this.ssl_server || !this.is_connected)
				throw new Error("TLS handshake did not establish the server session");
			this.send("TLS ready\r\n");
		}
		else if (this.ssl_session || this.is_connected)
			throw new Error("Failed handshake retained a live socket");
		if (mode == "timeout" && system.timer - began > 8)
			throw new Error("TLS handshake ignored its timeout");
		this.close();
		print("PASS: " + mode);
	}, mode == "timeout" ? 5 : 10))
		throw new Error("Could not start TLS handshake");
	returned = true;
	js.setTimeout(function() { ticked = true; }, 10);
	js.gc(true);
	if (mode == "cancel") {
		accepted.close();
		accepted = null;
		js.gc(true);
		js.setTimeout(function() {
			if (calls) throw new Error("Cancelled callback ran");
			print("PASS: cancel");
		}, 100);
	}
	if (mode == "exit") {
		js.setTimeout(function() {
			js.do_callbacks = false;
			print("PASS: exit");
		}, 50);
	}
});
var ready = new File(argv[2]);
if (!ready.open("w")) throw new Error("Cannot write fixture readiness");
ready.write("ready"); ready.close();
js.do_callbacks = true;
