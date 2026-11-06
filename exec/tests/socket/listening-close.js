/* A ListeningSocket releases its port when closed or collected (#1296). Loopback only. */
function free_port() {
	var probe = new Socket();
	if (!probe.bind(0, "127.0.0.1"))
		throw new Error("Cannot find a free port: " + probe.error);
	var port = probe.local_port;
	probe.close();
	return port;
}
function listen(port) {
	return new ListeningSocket(["127.0.0.1"], port, "Test", {retry_count: 0});
}
function rebinds(port, why) {
	try {
		listen(port).close();
	} catch (e) {
		throw new Error("Port still held after " + why + ": " + e);
	}
}

var port = free_port();
var closed = listen(port);
closed.close();
rebinds(port, "close()");
closed.close(); /* A second close() is harmless */
if (closed.accept() !== undefined || closed.poll() !== -1)
	throw new Error("A closed ListeningSocket still accepts or polls");

/* The interface may be one string instead of an array */
port = free_port();
new ListeningSocket("127.0.0.1", port, "Test", {retry_count: 0}).close();
rebinds(port, "close() of a single-interface listener");

port = free_port();
(function() { listen(port); })();
js.gc(true);
rebinds(port, "garbage collection");

if (js.global.js && typeof closed.on == "function") {
	port = free_port();
	var watched = listen(port);
	var supported = true;
	try {
		watched.on("read", function() {});
	} catch (e) {
		supported = false; /* This host runs no event loop */
	}
	watched.close();
	if (supported)
		rebinds(port, "close() with a read handler installed");
}
