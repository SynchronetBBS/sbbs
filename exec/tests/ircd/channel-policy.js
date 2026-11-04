/* Policy enforcement at local clients and the untrusted leaf boundary. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(), owner = new_user(s, "Owner"), visitor = new_user(s, "Visitor"); s.IRC_Services = null;
	var service = new_peer(s, "services.test", false); service.uline = true; service.protocol = "bahamut"; s.ULines = [service.nick];
	var chan = join_user(s, owner, "#test"); chan.modelist[s.CHANMODE_OP][owner.id] = owner;
	visitor.created = s.now; visitor.socket = {ssl_session:false};
	return {s:s, owner:owner, visitor:visitor, chan:chan, service:service};
}
function mode(t, fields) { t.service.work(":services.test MODE #test 0 " + fields); }
function identify(t, user) { t.service.work(":services.test SVSMODE " + (user || t.visitor).nick + " +r"); }
function talk(t, text, type) {
	t.owner.received = []; t.visitor.do_msg("#test", type || "PRIVMSG", text);
	return t.owner.received.some(function(line) { return line.indexOf((type || "PRIVMSG") + " #test :" + text) >= 0; });
}
function remote(t, name) {
	var leaf = new_peer(t.s, name || "leaf.test", false), user = new_user(t.s, "Remote", leaf.nick);
	user.created = t.s.now;
	return {peer:leaf, user:user};
}

run_tests({
	"channel registration is server state and cannot be set by a channel operator or leaf": function() {
		var t = scenario(), leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("CHANPOLICY");
		t.owner.work("MODE #test +r"); check(!(t.chan.mode & t.s.CHANMODE_REGISTERED));
		leaf.work(":leaf.test MODE #test 0 +r"); check(!(t.chan.mode & t.s.CHANMODE_REGISTERED));
		mode(t, "+r"); check(t.chan.mode & t.s.CHANMODE_REGISTERED);
		mode(t, "-r"); check(!(t.chan.mode & t.s.CHANMODE_REGISTERED));
	},
	"R requires a services grant, independent of native notice modes and visible BBS usernames": function() {
		var t = scenario(); mode(t, "+R");
		t.visitor.mode |= t.s.USERMODE_OPER | t.s.USERMODE_REJECTED;
		t.visitor.uprefix = "Some.User+TEST"; check(!t.visitor.do_join("#test"));
		identify(t); check(t.visitor.do_join("#test"));
		t.visitor.do_part("#test"); t.service.work(":services.test SVSMODE Visitor -r"); check(!t.visitor.do_join("#test"));
	},
	"O admits only operators and ordinary channel ops cannot set it": function() {
		var t = scenario(); t.owner.work("MODE #test +O"); check(!(t.chan.mode & t.s.CHANMODE_OPERONLY));
		mode(t, "+O"); check(!t.visitor.do_join("#test"));
		t.visitor.mode |= t.s.USERMODE_OPER; check(t.visitor.do_join("#test"));
		t.owner.mode |= t.s.USERMODE_OPER; t.owner.work("MODE #test -O"); check(!(t.chan.mode & t.s.CHANMODE_OPERONLY));
	},
	"S requires known client TLS and personal invitations or masks cannot replace it": function() {
		var t = scenario(); mode(t, "+SI Visitor"); t.visitor.invited = "#TEST";
		check(!t.visitor.do_join("#test")); delete t.visitor.socket.ssl_session; check(!t.visitor.do_join("#test"));
		t.visitor.socket.ssl_session = true; check(t.visitor.do_join("#test"));
	},
	"M checks external identity for messages while channel ops and voices retain permission": function() {
		var t = scenario(); join_user(t.s, t.visitor, "#test"); mode(t, "+M");
		check(!talk(t, "unidentified") && !talk(t, "unidentified", "NOTICE"));
		identify(t); check(talk(t, "identified")); t.service.work(":services.test SVSMODE Visitor -r");
		mode(t, "+v Visitor"); check(talk(t, "voiced")); mode(t, "-v+o Visitor Visitor"); check(talk(t, "opped"));
	},
	"ordinary moderated exceptions do not silently bypass M": function() {
		var t = scenario(); join_user(t.s, t.visitor, "#test"); mode(t, "+M");
		t.owner.work("MODE #test +e ~m:moderated:Visitor"); check(!talk(t, "still unidentified"));
		identify(t); check(talk(t, "identified"));
	},
	"c rejects color, formatting and ANSI controls even from voiced members": function() {
		var t = scenario(); join_user(t.s, t.visitor, "#test"); mode(t, "+cv Visitor");
		["\x0304red", "\x04ff0000red", "\x02bold", "\x1b[31mred", "\x01ACTION \x0304red\x01"].forEach(function(text) { check(!talk(t, text), text); });
		check(talk(t, "plain text") && talk(t, "\x01ACTION waves\x01"));
		check(talk(t, "\x1b$Bcharset"));
	},
	"j enforces a replenishing join bucket using monotonic time": function() {
		var t = scenario(); mode(t, "+j 2:10");
		check(t.visitor.do_join("#test")); t.visitor.do_part("#test");
		check(t.visitor.do_join("#test")); t.visitor.do_part("#test"); check(!t.visitor.do_join("#test"));
		t.s.now += 10000; check(!t.visitor.do_join("#test"));
		t.s.system.timer += 5; check(t.visitor.do_join("#test"));
		t.visitor.do_part("#test"); mode(t, "-j"); check(t.visitor.do_join("#test"));
	},
	"j validates complete values without shifting following key or status arguments": function() {
		var t = scenario(); join_user(t.s, t.visitor, "#test");
		["bad", "-1:5", "1", "1:5x", "2147483648:1"].forEach(function(value) {
			mode(t, "+jkv " + value + " secret Visitor"); check(!(t.chan.mode & t.s.CHANMODE_JOINRATE));
			check(t.chan.arg[t.s.CHANMODE_KEY] == "secret" && t.chan.modelist[t.s.CHANMODE_VOICE][t.visitor.id]);
		});
		mode(t, "+j 0002:0010"); check(t.chan.arg[t.s.CHANMODE_JOINRATE] == "2:10");
		mode(t, "+j 0"); check(!(t.chan.mode & t.s.CHANMODE_JOINRATE));
	},
	"old leaf JOINs receive a KICK before denied membership or status enters the hub": function() {
		["R", "O", "S"].forEach(function(flag) {
			var t = scenario(), r = remote(t); mode(t, "+" + flag); r.peer.sent = [];
			r.peer.work(":Remote JOIN #test"); check(!t.chan.users[r.user.id] && !r.user.channels["#TEST"], flag);
			check(r.peer.sent.some(function(line) { return line.indexOf("KICK #test Remote :Cannot join channel (+" + flag) >= 0; }), r.peer.sent.join(" | "));
		});
	},
	"old leaf SJOINs cannot grant ops or membership past newer R and S policy": function() {
		var t = scenario(), r = remote(t); mode(t, "+RS");
		r.peer.work(":leaf.test SJOIN " + t.chan.created + " #test + :@Remote");
		check(!t.chan.users[r.user.id] && !t.chan.modelist[t.s.CHANMODE_OP][r.user.id]);
		identify(t, r.user); r.peer.their_capab.push("CLIENTTLS");
		r.peer.work(":leaf.test CLIENTTLS Remote TLS");
		r.peer.work(":leaf.test SJOIN " + t.chan.created + " #test + :@Remote");
		check(t.chan.users[r.user.id] && !t.chan.modelist[t.s.CHANMODE_OP][r.user.id]);
	},
	"remote admissions consume join-rate capacity and old live leaves cannot bypass it": function() {
		var t = scenario(), r = remote(t); mode(t, "+j 1:10");
		r.peer.work(":Remote JOIN #test"); check(t.chan.users[r.user.id]);
		check(!t.visitor.do_join("#test")); r.user.do_part("#test");
		r.peer.work(":Remote JOIN #test"); check(!t.chan.users[r.user.id]);
		t.s.system.timer += 10; r.peer.work(":Remote JOIN #test"); check(t.chan.users[r.user.id]);
	},
	"an incapable intermediate hub cannot bypass newer join policies": function() {
		var t = scenario(), r = remote(t, "oldhub.test"); r.peer.hub = true; mode(t, "+RS");
		r.peer.work(":Remote JOIN #test"); check(!t.chan.users[r.user.id]);
		r.peer.work(":oldhub.test SJOIN " + t.chan.created + " #test + :@Remote");
		check(!t.chan.users[r.user.id] && !t.chan.modelist[t.s.CHANMODE_OP][r.user.id]);
	},
	"legacy mode and snapshot representations omit new policies and their parameters": function() {
		var t = scenario(), old = new_peer(t.s, "old.test", true), fresh = new_peer(t.s, "fresh.test", true);
		fresh.their_capab.push("CHANPOLICY"); mode(t, "+rRMOcSjkl 5:10 key 20");
		check(t.chan.mode & t.s.CHANMODE_SECURE);
		old.sent = []; fresh.sent = []; old.server_chan_info(t.chan); fresh.server_chan_info(t.chan);
		check(!old.sent.some(function(line) { return line.indexOf("5:10") >= 0 || /\+[a-zA-Z]*[rRMOcSj]/.test(line); }), old.sent.join(" | "));
		check(fresh.sent.some(function(line) { return line.indexOf("5:10") >= 0; }));
		channel_sync(old, t.chan.created, "#test", "+kl key 20", "@Owner", "");
		check((t.chan.mode & t.s.CHANMODE_SECURE) && t.chan.arg[t.s.CHANMODE_JOINRATE] == "5:10");
	},
	"capable snapshots can clear policies and equal-TS join-rate merges converge": function() {
		var t = scenario(), hub = new_peer(t.s, "next.test", true); hub.their_capab.push("CHANPOLICY");
		mode(t, "+jRM 4:10"); channel_sync(hub, t.chan.created, "#test", "+j 2:10", "@Owner", "");
		check(!(t.chan.mode & t.s.CHANMODE_REGMODERATED) && t.chan.arg[t.s.CHANMODE_JOINRATE] == "2:10");
		hub.set_chanmode(t.chan, ["+j", "6:20"], t.chan.created, {merge:true}); check(t.chan.arg[t.s.CHANMODE_JOINRATE] == "2:10");
		hub.set_chanmode(t.chan, ["+j", "1:5"], t.chan.created, {merge:true}); check(t.chan.arg[t.s.CHANMODE_JOINRATE] == "1:5");
	},
	"internal services remain exempt from external client join policies": function() {
		var t = scenario(); mode(t, "+ROSMj 1:10"); t.visitor.services_internal = true;
		check(t.visitor.do_join("#test") && talk(t, "services message"));
	}
});
