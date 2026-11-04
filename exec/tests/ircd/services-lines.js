/* Native S-line ownership, enforcement and runtime lifecycle. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(), service = new_peer(s, "services.test", false), hub = new_peer(s, "next.test", true);
	service.uline = true; service.protocol = "bahamut"; s.ULines = [service.nick]; hub.their_capab.push("SLINES");
	return {s:s, service:service, hub:hub};
}
function send(t, command, from) { (from || t.service).work(":" + (from || t.service).nick + " " + command); }
function sg(t, mask, reason) { send(t, "SGLINE " + mask.length + " :" + mask + ":" + (reason || "Policy: reason")); }
function local(t, nick) {
	var u = new_user(t.s, nick); u.created = t.s.now; u.socket = {is_connected:false, clearOn:function() {}, close:function() {}}; return u;
}
function rehash(t, trust) {
	var path = IRCD_TEST_TEMP_DIR + "ircd-slines-" + random(2147483647) + ".ini", f = new File(path);
	check(f.open("w")); f.write("[Info]\nServername=hub.test\n[Services:network]\nServername=" + trust + "\n[Restrict:local]\nMask=Local*\nReason=Local policy\n"); f.close();
	t.s.Config_Filename = path; t.s.system.qwk_id = "TEST"; t.s.IRCDCFG_Editor = false;
	t.s.Scan_For_Banned_Clients = function() {};
	try { t.s.Read_Config_File(); } finally { file_remove(path); }
}

run_tests({
	"SGLINE uses its declared mask length, including embedded spaces and colons": function() {
		var t = scenario(); sg(t, "Bad: Real *", "Policy: details: here");
		var line = t.s.IRC_match_sline("G", "bad: real name");
		check(line && line.mask == "bad: real *" && line.reason == "Policy: details: here");
		check(t.hub.sent[0] == ":services.test SGLINE 11 :bad: real *:Policy: details: here");
	},
	"malformed commands cannot create ambiguous entries or wildcard removal requests": function() {
		["SGLINE 0 ::reason", "SGLINE 5 :Bad*:reason", "SGLINE -1 :Bad*:reason", "SGLINE 4 Bad*:reason trailing", "SQLINE #channel :reason", "SQLINE bad@host :reason", "SQLINE bad,nick :reason", "UNSQLINE 1 :*", "UNSGLINE 1 :*"].forEach(function(command) {
			var t = scenario(); send(t, command); check(!t.s.Services_Lines.length && !t.hub.sent.length, command);
		});
	},
	"owned exact removals preserve overlapping patterns and independent authorities": function() {
		var t = scenario(), second = new_peer(t.s, "second.test", false); second.uline = true; t.s.ULines.push(second.nick);
		send(t, "SQLINE Bad* :First"); send(t, "SQLINE BadNick :Specific"); send(t, "SQLINE Bad* :Second", second);
		send(t, "UNSQLINE BadNick"); check(t.s.Services_Lines.length == 2 && t.s.IRC_match_sline("Q", "BadNick"));
		send(t, "UNSQLINE **"); check(t.s.Services_Lines.length == 2);
		send(t, "UNSQLINE BAD**"); check(t.s.Services_Lines.length == 1 && t.s.Services_Lines[0].owner == second.nick);
		send(t, "UNSQLINE bad*", second); check(!t.s.Services_Lines.length);
	},
	"runtime reservations never replace or remove configured Q-lines": function() {
		var t = scenario(); t.s.QLines = [new t.s.QLine("Local*", "Configured")];
		send(t, "SQLINE LocalNick :Service"); send(t, "UNSQLINE LocalNick");
		check(t.s.QLines.length == 1 && t.s.QLines[0].reason == "Configured");
		var u = local(t, "Reader"); check(!u.check_nickname("LocalNick"));
	},
	"untrusted sources and wrong service routes cannot add or remove policy": function() {
		var t = scenario(); send(t, "SQLINE Bad* :Policy");
		t.hub.work(":services.test UNSQLINE Bad*"); send(t, "SQLINE Evil* :Untrusted", t.hub);
		t.service.uline = false; send(t, "UNSQLINE Bad*");
		check(t.s.Services_Lines.length == 1 && !t.s.IRC_match_sline("Q", "EvilNick"));
	},
	"redundant corrections relay through matching caches and omit incapable native peers": function() {
		var t = scenario(), old = new_peer(t.s, "old.test", true);
		send(t, "SQLINE Bad* :One"); send(t, "SQLINE BAD** :Two"); send(t, "UNSQLINE Gone");
		check(t.s.Services_Lines.length == 1 && t.s.Services_Lines[0].reason == "Two");
		check(t.hub.sent.length == 3 && !old.sent.length);
	},
	"reconnect bursts retain the actual authority and never forge it toward its own route": function() {
		var t = scenario(); sg(t, "Bad Real *"); send(t, "SQLINE Bad* :Policy"); t.hub.sent = [];
		t.s.IRC_burst_slines(t.hub); check(t.hub.sent.length == 2 && t.hub.sent[0].indexOf(":services.test SGLINE ") == 0);
		t.service.their_capab.push("SLINES"); t.service.sent = []; t.s.IRC_burst_slines(t.service); check(!t.service.sent.length);
	},
	"local nickname changes reject reservations without disconnecting the existing session": function() {
		var t = scenario(), u = local(t, "Reader"); send(t, "SQLINE Bad* :Reserved by services");
		u.work("NICK BadNick"); check(u.nick == "Reader" && t.s.Users.READER === u);
		check(u.received.some(function(line) { return line.indexOf(" 432 Reader BadNick :Reserved by services") >= 0; }));
		send(t, "UNSQLINE Bad*"); u.work("NICK BadNick"); check(t.s.Users.BADNICK === u);
	},
	"admission rechecks both S-line kinds after pending NICK and USER input": function() {
		var t = scenario(); load(t.s, "ircd/unregistered.js"); sg(t, "Bad Real *"); send(t, "SQLINE Bad* :Reserved");
		[{nick:"Reader", realname:"Bad Real Name"}, {nick:"BadNick", realname:"Ordinary"}].forEach(function(input) {
			var u = {nick:input.nick, realname:input.realname, lines:[], numeric:function(n, text) { this.lines.push(n + " " + text); }, quit:function(reason) { this.reason = reason; }};
			t.s.Unregistered_Welcome.call(u); check(u.reason && u.lines[0].indexOf("465 :S") == 0);
			check(!t.s.Users[input.nick.toUpperCase()]);
		});
	},
	"old leaves cannot introduce reserved nicknames or banned real names into canonical state": function() {
		var t = scenario(), leaf = new_peer(t.s, "old.test", false); send(t, "SQLINE Bad* :Reserved"); sg(t, "Bad Real *");
		leaf.work("NICK BadNick 1 1800000000 +i user host.test old.test 0 0 :Ordinary");
		leaf.work("NICK Reader 1 1800000000 +i user host.test old.test 0 0 :Bad Real Name");
		check(!t.s.Users.BADNICK && !t.s.Users.READER);
		check(leaf.sent.filter(function(line) { return line.indexOf("KILL ") >= 0; }).length == 2);
		check(!t.hub.sent.some(function(line) { return line.indexOf("NICK ") >= 0; }));
	},
	"trusted hubs also enforce incoming user policy and service bots remain exempt": function() {
		var t = scenario(); sg(t, "Bad Real *"); send(t, "SQLINE *Serv :Reserved");
		t.hub.work("NICK Reader 1 1800000000 +i user host.test next.test 0 0 :Bad Real Name"); check(!t.s.Users.READER);
		t.service.work("NICK OperServ 1 1800000000 +i services host.test services.test 0 0 :Bad Real Name");
		check(t.s.Users.OPERSERV && t.s.Users.OPERSERV.uline);
	},
	"adding policy does not retroactively kill existing users": function() {
		var t = scenario(), u = local(t, "BadNick"); u.realname = "Bad Real Name";
		sg(t, "Bad Real *"); send(t, "SQLINE Bad* :Reserved"); check(t.s.Users.BADNICK === u && !u.quitting);
	},
	"rehash retains trusted runtime entries and discards revoked authority without touching local policy": function() {
		var t = scenario(); sg(t, "Bad Real *"); send(t, "SQLINE Bad* :Reserved");
		rehash(t, "services.test"); check(t.s.Services_Lines.length == 2 && t.s.QLines.some(function(q) { return q.nick == "Local*"; }));
		rehash(t, "other.test"); check(!t.s.Services_Lines.length && t.s.QLines.some(function(q) { return q.nick == "Local*"; }));
	},
	"authority departure retires expiry-less policy so reconnect cannot resurrect expired entries": function() {
		var t = scenario(); sg(t, "Bad Real *"); send(t, "SQLINE Bad* :Reserved");
		delete t.s.Servers[t.service.nick]; t.s.IRC_reconcile_slines(); check(!t.s.Services_Lines.length);
		var returning = new_peer(t.s, "services.test", false); returning.uline = true;
		send(t, "SQLINE Active* :Current", returning);
		check(!t.s.IRC_match_sline("Q", "BadNick") && t.s.IRC_match_sline("Q", "ActiveNick"));
	}
});
