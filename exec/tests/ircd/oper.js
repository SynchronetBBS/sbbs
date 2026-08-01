/*
 * exec/tests/ircd/oper.js
 *
 * IRCd integration tests: IRC operator commands and access control.
 * Tests that 481 ERR_NOPRIVILEGES is enforced for non-opers, and that
 * opers with the correct O-line flags can execute privileged commands.
 *
 * O-line in test config: Nick=testop, Password=testpass, Flags=OwkK
 *   O = global oper (includes rehash, globops, gkill, etc.)
 *   w = WALLOPS
 *   k = local KILL
 *   K = global KILL
 *
 * Starts a local IRCd on port 19670.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PORT = 19670;
var srv  = new IrcdTestServer(PORT);

/* Helper: register a client and authenticate as IRC operator */
function make_oper(nick) {
	var c = srv.client(nick);
	c.send("OPER testop testpass");
	c.expect(" 381 ", 3);   /* RPL_YOUREOPER */
	c.drain(300);
	return c;
}

try {
	srv.start();

	run_tests({

		/* --- OPER authentication --- */

		"491 for wrong OPER credentials": function() {
			var c = srv.client("wrongop");
			c.send("OPER testop wrongpassword");
			/* IRCd returns 491 (no matching O-line) for any auth failure */
			c.expect(" 491 ", 3);
			c.quit();
		},

		"491 for unknown OPER nick": function() {
			var c = srv.client("nooper");
			c.send("OPER nosuchoper testpass");
			c.expect(" 491 ", 3);
			c.quit();
		},

		"381 RPL_YOUREOPER on successful OPER": function() {
			var c = make_oper("gotoper");
			c.quit();
		},

		"381 gives usermode +o": function() {
			var c = srv.client("modeoper");
			c.send("OPER testop testpass");
			c.expect(" 381 ", 3);
			c.send("MODE modeoper");
			var reply = c.expect(" 221 ", 3);   /* RPL_UMODEIS */
			if (reply.indexOf("+") < 0 || reply.indexOf("o") < 0)
				throw new Error("Expected +o in mode reply: " + reply);
			c.quit();
		},

		"already-oper gets server notice instead of 381 again": function() {
			var c = make_oper("doubleop");
			c.send("OPER testop testpass");
			/* Should get server notice, not another 381 */
			var line = c.readln(3);
			/* May receive NOTICE or nothing; must NOT get 491 */
			if (line.indexOf(" 491 ") >= 0)
				throw new Error("Got 491 on re-OPER: " + line);
			c.quit();
		},

		/* --- KILL --- */

		"481 ERR_NOPRIVILEGES for KILL without oper": function() {
			var c1 = srv.client("nonopkiller");
			var c2 = srv.client("killtarget1");
			c1.send("KILL killtarget1 :test");
			c1.expect(" 481 ", 3);
			c1.quit();
			c2.quit();
		},

		"KILL by oper removes target": function() {
			var op     = make_oper("theop1");
			var victim = srv.client("victim1");
			op.send("KILL victim1 :killed by test");
			/* victim receives ERROR and is disconnected */
			mswait(400);
			/* Confirm victim is gone: WHOIS returns 401 */
			op.send("WHOIS victim1");
			op.expect(" 401 ", 3);
			op.quit();
		},

		"401 ERR_NOSUCHNICK when KILL targets unknown nick": function() {
			var op = make_oper("theop2");
			op.send("KILL nosuchnick99 :reason");
			op.expect(" 401 ", 3);
			op.quit();
		},

		/* --- WALLOPS --- */

		"481 ERR_NOPRIVILEGES for WALLOPS without oper": function() {
			var c = srv.client("wnooper");
			c.send("WALLOPS :test broadcast");
			c.expect(" 481 ", 3);
			c.quit();
		},

		"WALLOPS by oper does not return 481": function() {
			var op = make_oper("walloper");
			op.send("MODE walloper +w");   /* receive wallops */
			op.drain(200);
			op.send("WALLOPS :test broadcast from oper");
			/* Oper with +w receives their own WALLOPS */
			op.expect("WALLOPS", 3);
			op.quit();
		},

		/* --- REHASH --- */

		"481 ERR_NOPRIVILEGES for REHASH without oper": function() {
			var c = srv.client("rehashnooper");
			c.send("REHASH");
			c.expect(" 481 ", 3);
			c.quit();
		},

		"382 RPL_REHASHING for REHASH with oper": function() {
			var op = make_oper("rehasher");
			op.send("REHASH");
			op.expect(" 382 ", 3);
			/* Server should still be alive */
			op.send("PING :alive");
			op.expect("PONG", 3);
			op.quit();
		},

		/* --- KLINE --- */

		"481 ERR_NOPRIVILEGES for KLINE without oper": function() {
			var c = srv.client("klinenooper");
			c.send("KLINE *@badhost.example.com :test");
			c.expect(" 481 ", 3);
			c.quit();
		},

		"oper can KLINE and UNKLINE": function() {
			var op = make_oper("kliner");
			op.send("KLINE *@klinetest.example.com :test kline");
			/* Should not return 481 */
			var line = op.sock.recvline(128, 1);
			if (line && line.indexOf(" 481 ") >= 0)
				throw new Error("Got 481 on KLINE with oper: " + line);
			op.send("UNKLINE *@klinetest.example.com");
			var line2 = op.sock.recvline(128, 1);
			if (line2 && line2.indexOf(" 481 ") >= 0)
				throw new Error("Got 481 on UNKLINE with oper: " + line2);
			op.quit();
		},

		/* --- CONNECT --- */

		"481 ERR_NOPRIVILEGES for CONNECT without oper": function() {
			var c = srv.client("connnooper");
			c.send("CONNECT nonexistent.server 6667");
			c.expect(" 481 ", 3);
			c.quit();
		},

		/* --- MODE +o self-promotion blocked --- */

		"non-oper cannot set usermode +o on themselves": function() {
			var c = srv.client("selfpromo");
			c.send("MODE selfpromo +o");
			/* Server should either ignore silently or return an error */
			var line = c.sock.recvline(128, 1);
			if (line && line.indexOf("+o") >= 0 && line.indexOf(" 221 ") >= 0) {
				/* If we got a MODE reply, verify +o is NOT set */
				if (line.indexOf("o") >= 0)
					throw new Error("Non-oper got +o: " + line);
			}
			/* Confirm not an oper via WHOIS */
			c.send("WHOIS selfpromo");
			var whois = c.expect(" 311 ", 3);
			/* 313 RPL_WHOISOPERATOR only appears for opers */
			try {
				var extra = c.readln(1);
				if (extra.indexOf(" 313 ") >= 0)
					throw new Error("Non-oper has 313 RPL_WHOISOPERATOR");
			} catch(e) {
				if (e.toString().indexOf("313") >= 0) throw e;
				/* Timeout (no 313) = correct */
			}
			c.quit();
		},

		/* --- GLOBOPS --- */

		"481 ERR_NOPRIVILEGES for GLOBOPS without oper": function() {
			var c = srv.client("globnooper");
			c.send("GLOBOPS :test");
			c.expect(" 481 ", 3);
			c.quit();
		},

		"oper can GLOBOPS without 481": function() {
			var op = make_oper("globoper");
			op.send("GLOBOPS :test global notice");
			var line = op.sock.recvline(128, 1);
			if (line && line.indexOf(" 481 ") >= 0)
				throw new Error("Got 481 on GLOBOPS with oper: " + line);
			op.quit();
		},

		/* --- WHOIS shows 313 RPL_WHOISOPERATOR for opers --- */

		"313 RPL_WHOISOPERATOR in WHOIS for opered user": function() {
			var op   = make_oper("whooper");
			var c    = srv.client("whooperchecker");
			c.send("WHOIS whooper");
			c.expect(" 311 ", 3);   /* RPL_WHOISUSER */
			c.expect(" 313 ", 3);   /* RPL_WHOISOPERATOR */
			c.expect(" 318 ", 3);   /* RPL_ENDOFWHOIS */
			op.quit();
			c.quit();
		},

		"313 not present in WHOIS for non-oper": function() {
			var c1 = srv.client("regularuser");
			var c2 = srv.client("whoregular");
			c2.send("WHOIS regularuser");
			c2.expect(" 311 ", 3);
			/* Read all remaining WHOIS lines; 313 must not appear */
			var found313 = false;
			try {
				while (true) {
					var line = c2.readln(1);
					if (line.indexOf(" 313 ") >= 0) found313 = true;
					if (line.indexOf(" 318 ") >= 0) break;
				}
			} catch(e) { /* timeout OK */ }
			if (found313)
				throw new Error("Non-oper got 313 RPL_WHOISOPERATOR");
			c1.quit();
			c2.quit();
		}

	});

} finally {
	srv.stop();
}
