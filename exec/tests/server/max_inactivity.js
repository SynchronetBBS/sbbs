// Test the server.max_inactivity property
// @format.tab-size 4, @format.use-tabs true

// jsexec (the usual runner) and jsdoor create no server object: it must
// stay that way, so a script's "typeof server" test keeps working there.
// Run from a server (e.g. ;EXEC ?tests/test.js server on the Terminal
// Server), the property is checked for its type and read-only status.
var srv = js.global.server;

if (srv === undefined) {
	if (typeof server != 'undefined')
		throw new Error("server is visible but not a global property");
	if (js.global.max_inactivity !== undefined)
		throw new Error("max_inactivity leaked into the global object");
}
else {
	var val = srv.max_inactivity;
	if (typeof val != 'number')
		throw new Error("server.max_inactivity is a " + typeof val);
	if (val < 0 || Math.floor(val) != val)
		throw new Error("server.max_inactivity is not a whole number: " + val);
	var found = false;
	for (var p in srv) {
		if (p == 'max_inactivity')
			found = true;
	}
	if (!found)
		throw new Error("server.max_inactivity is not enumerable");
	srv.max_inactivity = val + 1;
	if (srv.max_inactivity != val)
		throw new Error("server.max_inactivity is writable");
}
