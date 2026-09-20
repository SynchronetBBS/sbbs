/*
 * exec/tests/ircd/timeout.js
 *
 * Unit tests for IRCClient_check_timeout() (ircd/core.js).
 *
 * The daemon is a single-threaded callback engine, so anything that blocks --
 * a stat() of an unreachable ctrl_dir, a suspended machine, a very long GC --
 * freezes every ping timer at once and every peer looks silent when we come
 * back.  These tests pin down that we ping and close links on the usual
 * schedule, but never close one because WE were the thing that stopped.
 *
 * No server is started: core.js is loaded into a scope of its own so the
 * clock it reads (system.timer) can be wound forward at will.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PINGFREQ = 90;	/* seconds; matches a typical server class */

/* core.js reaches for a few globals, and a scope object lets us supply our
   own -- including a fake system.timer.  Loading it has no side effects. */
function new_core() {
	var scope = {
		system: { timer: 1000 },
		YLines: [ { pingfreq: PINGFREQ } ],
		ServerName: "test.ircd.local"
	};
	load(scope, "ircd/core.js");
	return scope;
}

function new_client(core) {
	return {
		ircclass: 0,
		idletime: core.system.timer,
		pinged: false,
		pings: 0,
		quits: 0,
		rawout: function() { this.pings++; },
		quit: function() { this.quits++; }
	};
}

function check(core, client) {
	return core.IRCClient_check_timeout.call(client);
}

function tick(core, secs) {
	core.system.timer += secs;
}

function assert(what, cond) {
	if (!cond)
		throw new Error(what);
}

run_tests({

	"an idle connection is PINGed, not closed": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		assert("returned non-zero", check(core, c) === 0);
		assert("no PING sent", c.pings == 1);
		assert("connection closed", c.quits === 0);
		assert("ping clock not started", c.pinged == core.system.timer);
	},

	"a connection that ignores the PING is closed": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		check(core, c);
		tick(core, PINGFREQ + 1);
		assert("returned zero", check(core, c) == 1);
		assert("connection not closed", c.quits == 1);
	},

	"a stall spanning an outstanding PING is forgiven": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		check(core, c);				/* PING goes out */
		core.Stall_Generation++;	/* watchdog saw a stalled callback engine */
		tick(core, 40 * 60);		/* ...for forty minutes */
		assert("returned non-zero", check(core, c) === 0);
		assert("closed for our own downtime", c.quits === 0);
		assert("ping clock not restarted", c.pinged == core.system.timer);
	},

	"a peer that is really gone is closed after the forgiven round": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		check(core, c);
		core.Stall_Generation++;
		tick(core, 40 * 60);
		check(core, c);				/* forgiven */
		tick(core, PINGFREQ + 1);
		assert("connection not closed", check(core, c) == 1);
		assert("connection not closed", c.quits == 1);
	},

	"forgiveness is spent once per stall, not once per check": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		check(core, c);
		core.Stall_Generation++;
		tick(core, 40 * 60);
		check(core, c);				/* forgiven */
		tick(core, PINGFREQ + 1);
		check(core, c);				/* same generation: usual rules */
		assert("connection not closed", c.quits == 1);
	},

	"an un-PINGed connection after a stall is PINGed, not closed": function() {
		var core = new_core();
		var c = new_client(core);

		core.Stall_Generation++;
		tick(core, 40 * 60);
		assert("returned non-zero", check(core, c) === 0);
		assert("no PING sent", c.pings == 1);
		assert("connection closed", c.quits === 0);
	},

	"the first check of a new connection is not treated as a stall": function() {
		var core = new_core();
		var c = new_client(core);

		tick(core, PINGFREQ + 1);
		check(core, c);
		assert("first PING was skipped", c.pings == 1);
	}

});
