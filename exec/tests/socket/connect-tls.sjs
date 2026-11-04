/* Driven by ircd/tls-network.py against a private TLS endpoint. */
"use strict";
var mode = argv[1], returned = false, ticked = false, calls = 0;
var sock = new Socket(); sock.array_buffer = false;
if (!sock.connect("localhost", Number(argv[0]))) throw new Error("Cannot connect TLS fixture");
if (mode == "client-auth") sock.tls_client_auth = true;
if (mode == "arguments") {
	var missing = false;
	try { sock.connectTLS(); } catch (e) { missing = true; }
	if (!missing || !sock.is_connected) throw new Error("Missing callback changed socket");
	[0, -1, 4, 301].forEach(function(timeout) {
		var rejected = false;
		try { sock.connectTLS(function() {}, timeout); } catch (e) { rejected = true; }
		if (!rejected || !sock.is_connected) throw new Error("Invalid timeout changed socket");
	});
	[undefined, null, 1, "callback"].forEach(function(callback) {
		var rejected = false;
		try { sock.connectTLS(callback); } catch (e) { rejected = true; }
		if (!rejected || !sock.is_connected) throw new Error("Invalid callback changed socket");
	});
	sock.close(); print("PASS: arguments");
} else {
	var began = system.timer, token = {value:mode};
	if (!sock.connectTLS(function() {
		calls++;
		if (token.value !== mode) throw new Error("TLS callback lost captured locals");
		if (!returned || this !== sock || (mode == "timeout" && !ticked)) throw new Error("TLS callback did not run asynchronously");
		if (mode == "cancel" || mode == "exit") throw new Error("Cancelled callback ran");
		if (mode == "success" || mode == "client-auth") {
			if (!this.ssl_session || this.ssl_server || !this.is_connected || !this.tls_remote_cert) throw new Error("Missing client TLS session or peer certificate");
			this.send("TLS ready\r\n");
		} else if (this.ssl_session || this.is_connected) throw new Error("Failed handshake retained live socket");
		if (mode == "timeout" && system.timer - began > 8) throw new Error("TLS ignored timeout");
		this.close(); print("PASS: " + mode);
	}, mode == "timeout" ? 5 : 10)) throw new Error("Cannot start client TLS");
	returned = true;
	js.setTimeout(function() { ticked = true; }, 10);
	js.gc(true);
	if (mode == "cancel") {
		sock.close(); sock = null; js.gc(true);
		js.setTimeout(function() { if (calls) throw new Error("Cancelled callback ran"); print("PASS: cancel"); }, 100);
	}
	if (mode == "exit") js.setTimeout(function() { js.do_callbacks = false; print("PASS: exit"); }, 50);
}
js.do_callbacks = true;
