/*
 * exec/tests/ircd/webirc.js
 *
 * IRCd integration tests: WEBIRC gateway handling.  A WEBIRC that is
 * malformed or not accepted must close the connection, and bans must apply
 * to the client IP the gateway passes, not just to the gateway's own IP.
 * (Numerics sent just before a quit are purged with the sendq, so these
 * check the ERROR line.)  Starts a local IRCd on port 19668, runs all tests, then stops it.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PORT = 19668;
var BANNED_IP = "203.0.113.7";	/* TEST-NET-3 */
/* nip.io answers a.b.c.d.nip.io with a.b.c.d, so as an RBL it "lists" every
   IPv4 address with its own reversal; BadResponses picks out one of them. */
var RBL_HOST = "nip.io";
var RBL_LISTED_IP = "203.0.113.9";
var rbl_reachable = resolve_ip("9.113.0.203." + RBL_HOST) == "9.113.0.203";
var srv  = new IrcdTestServer(PORT, [
	"[WebIRC:1]",
	"Hostname=" + IRCD_TEST_HOST,
	"Password=gwpass",
	"",
	"[Ban:1]",
	"Mask=*@" + BANNED_IP,
	"Reason=Test ban",
	"",
	"[RBL:1]",
	"Hostname=" + RBL_HOST,
	"BadResponses=9.113.0.203",
	""
]);

function expect_closed(c, timeout_sec) {
	var deadline = Date.now() + (timeout_sec * 1000);
	while (Date.now() < deadline) {
		if (!c.sock.is_connected)
			return;
		var raw = c.sock.recvline(512, 0.5);
		if (raw === null && !c.sock.is_connected)
			return;
	}
	throw new Error("connection still open after " + timeout_sec + "s");
}

try {
	srv.start();

	run_tests({

		"accepted WEBIRC registers with the client's host": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("WEBIRC gwpass testgw client.example.com 198.51.100.1");
			c.send("NICK wirc1");
			c.send("USER wirc1 0 * :Test");
			c.expect(" 001 ", 5);
			c.drain(300);
			c.send("WHOIS wirc1");
			c.expect(/ 311 \S+ wirc1 \S+ client\.example\.com /, 3);
			c.quit();
		},

		"bad WEBIRC password closes the connection": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("WEBIRC wrongpass testgw client.example.com 198.51.100.2");
			c.send("NICK wirc2");
			c.send("USER wirc2 0 * :Test");
			c.expect(/^ERROR :WEBIRC rejected/, 5);
			expect_closed(c, 3);
			c.quit();
		},

		"malformed WEBIRC closes the connection": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("WEBIRC gwpass testgw client.example.com");
			c.expect(/^ERROR :WEBIRC rejected/, 5);
			expect_closed(c, 3);
			c.quit();
		},

		"K:Line on the WEBIRC IP applies despite a resolved hostname": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("WEBIRC gwpass testgw banned.example.com " + BANNED_IP);
			c.send("NICK wirc3");
			c.send("USER wirc3 0 * :Test");
			c.expect(/^ERROR :You've been K:Lined/, 5);
			expect_closed(c, 3);
			c.quit();
		},

		"RBL listing of the WEBIRC IP closes the connection": function() {
			if (!rbl_reachable) {
				write("(skipped: " + RBL_HOST + " not resolvable) ");
				return;
			}
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("WEBIRC gwpass testgw listed.example.com " + RBL_LISTED_IP);
			c.send("NICK wirc5");
			c.send("USER wirc5 0 * :Test");
			c.expect(" 463 ", 15);
			c.expect(/^ERROR :Your IP address is on an RBL/, 3);
			expect_closed(c, 3);
			c.quit();
		},

		"gateway's own clients still register without WEBIRC": function() {
			var c = srv.client("wirc4");
			c.quit();
		}
	});
} finally {
	srv.stop();
}
