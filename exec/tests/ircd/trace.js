/* TRACE must report the route and leave completion to the destination. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function trace_state(name) {
	var s = new_state();
	s.ServerName = name;
	s.VERSION = "SynchronetIRCd-test";
	return s;
}

function trace_server(s, name, uplink) {
	var remote = new s.IRC_Server();
	remote.nick = name;
	remote.local = false;
	remote.parent = uplink.nick;
	remote.rawout = function(line) { uplink.rawout(line); };
	s.Servers[name.toLowerCase()] = remote;
	return remote;
}

run_tests({
	"a three-hop server trace reports each next hop and completes only at its destination": function() {
		var names = ["leaf.test", "hub1.test", "hub2.test", "dr.synchro.net"];
		var states = names.map(trace_state), users = [], incoming = [], outgoing = [];
		for (var i = 0; i < states.length; i++) {
			var s = states[i];
			if (i) incoming[i] = new_peer(s, names[i - 1], true);
			if (i < states.length - 1) {
				outgoing[i] = new_peer(s, names[i + 1], true);
				if (i < states.length - 2) trace_server(s, names[3], outgoing[i]);
			}
			users[i] = new_user(s, "Viewer", i ? incoming[i].nick : undefined);
		}
		users[0].work("TRACE dr.*");
		for (var hop = 0; hop < states.length - 1; hop++) {
			check(users[hop].received.length == 1
				&& users[hop].received[0] == ":" + names[hop]
				+ " 200 Viewer Link SynchronetIRCd-test dr.* " + names[hop + 1],
				"incorrect next hop or premature completion: " + users[hop].received.join(" | "));
			check(outgoing[hop].sent.length == 1 && outgoing[hop].sent[0] == ":Viewer TRACE dr.*",
				"TRACE was not forwarded once");
			incoming[hop + 1].work(outgoing[hop].sent[0]);
		}
		check(users[3].received.length == 1
			&& users[3].received[0] == ":dr.synchro.net 262 Viewer dr.* :End of /TRACE.",
			"destination did not complete TRACE");
	},
	"a local wildcard match terminates even when a peer also matches": function() {
		var s = trace_state("dr.synchro.net"), peer = new_peer(s, "dr.other.test", true);
		var viewer = new_user(s, "Viewer"), oper = new_user(s, "Operator");
		oper.mode |= s.USERMODE_OPER;
		viewer.work("TRACE dr.*");
		check(!peer.sent.length, "local wildcard TRACE was forwarded");
		check(viewer.received.length == 2 && / 204 Viewer Oper \d+ Operator$/.test(viewer.received[0])
			&& / 262 Viewer dr\.\* :End of \/TRACE\.$/.test(viewer.received[1]), "local operator trace failed");
	},
	"local nicknames produce user and operator trace replies": function() {
		var s = trace_state("hub.test"), viewer = new_user(s, "Viewer");
		var user = new_user(s, "Someone"), oper = new_user(s, "Operator");
		oper.mode |= s.USERMODE_OPER;
		[["someone", "205", "User", "Someone"], ["OPERATOR", "204", "Oper", "Operator"]].forEach(function(test) {
			viewer.received = [];
			viewer.work("TRACE " + test[0]);
			check(viewer.received.length == 2 && viewer.received[0].indexOf(" " + test[1] + " Viewer " + test[2] + " ") >= 0
				&& viewer.received[0].slice(-test[3].length) == test[3] && / 262 /.test(viewer.received[1]),
				"nickname trace listed the server's operators: " + viewer.received.join(" | "));
		});
	},
	"remote nicknames forward without an early end-of-trace": function() {
		var s = trace_state("hub.test"), peer = new_peer(s, "uplink.test", true);
		var viewer = new_user(s, "Viewer"), remote = new_user(s, "Remote", peer.nick);
		remote.rawout = function(line) { peer.rawout(line); };
		viewer.work("TRACE Remote");
		check(viewer.received.length == 1 && / 200 Viewer Link \S+ Remote uplink\.test$/.test(viewer.received[0]),
			"remote nickname TRACE completed before reaching its destination");
		check(peer.sent.length == 1 && peer.sent[0] == ":Viewer TRACE Remote", "nickname TRACE was not forwarded");
	},
	"an omitted target traces locally and an unknown target returns no-such-server": function() {
		var s = trace_state("hub.test"), viewer = new_user(s, "Viewer");
		viewer.work("TRACE");
		check(viewer.received.length == 1 && / 262 Viewer hub\.test /.test(viewer.received[0]), "default TRACE failed");
		viewer.received = [];
		viewer.work("TRACE missing.test");
		check(viewer.received.length == 1 && / 402 Viewer missing\.test /.test(viewer.received[0]), "missing target did not return 402");
	}
});
