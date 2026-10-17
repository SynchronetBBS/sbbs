/*
 * exec/tests/ircd/sendq.js
 *
 * Unit tests for the send queue drain, Queue_Send() (ircd/core.js).
 *
 * When a socket won't take more data, Queue_Send() re-arms itself on a
 * 100 ms timer until it drains.  A connection that closes with output still
 * queued must end that cycle: Socket.send() on a closed socket returns
 * nothing rather than throwing, so the drain would otherwise retry ten
 * times a second forever, and each such orphan adds to the load of the
 * single-threaded callback engine until it can no longer answer PINGs.
 *
 * No server is started: core.js is loaded into a scope of its own with a
 * stand-in `js` object that records what the drain schedules.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

function new_core() {
	var scope = {
		system: { timer: 1000 },
		YLines: [ { pingfreq: 90, sendq: 0 } ],
		ServerName: "test.ircd.local",
		scheduled: [],
		js: {
			setTimeout: function(func, msecs) {
				scope.scheduled.push({ func: func, msecs: msecs });
			},
			setImmediate: function(func) {
				scope.scheduled.push({ func: func, msecs: 0 });
			}
		}
	};
	load(scope, "ircd/core.js");
	return scope;
}

/* A socket that accepts `accept` bytes per send() call; a closed one
   accepts nothing and, like the real Socket, returns undefined. */
function new_socket(connected, accept) {
	return {
		is_connected: connected,
		send: function(str) {
			if (!this.is_connected)
				return undefined;
			return Math.min(accept, str.length);
		}
	};
}

function new_queue(core, sock) {
	var irc = { socket: sock, ircclass: 0, sent_msgs: 0, sent_bytes: 0,
		quits: 0, quit: function() { this.quits++; } };
	return new core.IRC_Queue(irc);
}

function assert(what, cond) {
	if (!cond)
		throw new Error(what);
}

run_tests({

	"a stalled but connected socket is retried after 100 ms": function() {
		var core = new_core();
		var sock = new_socket(true, 0);
		var q = new_queue(core, sock);

		q.add("PRIVMSG #test :hello");
		q.send(sock);
		assert("output was dropped", !q.empty);
		assert("retry not scheduled", core.scheduled.length == 1);
		assert("retry not on the 100 ms back-off",
			core.scheduled[0].msecs == 100);
	},

	"a partial send is continued immediately": function() {
		var core = new_core();
		var sock = new_socket(true, 5);
		var q = new_queue(core, sock);

		q.add("PRIVMSG #test :hello");
		q.send(sock);
		assert("continuation not scheduled", core.scheduled.length == 1);
		assert("continuation was delayed", core.scheduled[0].msecs === 0);
	},

	"a closed socket's unsent output is dropped, not retried": function() {
		var core = new_core();
		var sock = new_socket(true, 0);
		var q = new_queue(core, sock);

		q.add("PRIVMSG #test :hello");
		q.send(sock);				/* stalls: retry armed */
		core.scheduled = [];
		sock.is_connected = false;	/* the connection quits */
		q.send(sock);				/* the armed retry fires */
		assert("retried a closed socket", core.scheduled.length === 0);
		assert("unsent output kept for a closed socket", q.empty);
	},

	"output queued to a closed socket is dropped, not retried": function() {
		var core = new_core();
		var sock = new_socket(false, 0);
		var q = new_queue(core, sock);

		q.add("PRIVMSG #test :hello");
		q.send(sock);
		assert("retried a closed socket", core.scheduled.length === 0);
		assert("unsent output kept for a closed socket", q.empty);
	}

});
