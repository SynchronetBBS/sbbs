/* Invite masks, wire compatibility and authoritative list reconciliation. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(), owner = new_user(s, "Owner"), visitor = new_user(s, "Visitor");
	var chan = join_user(s, owner, "#test"); chan.modelist[s.CHANMODE_OP][owner.id] = owner;
	visitor.uprefix = "reader"; visitor.hostname = "client.test";
	return {s:s, owner:owner, visitor:visitor, chan:chan};
}
function mode(t, fields) { t.owner.work("MODE #test " + fields); }
function masks(t) { return Object.keys(t.chan.modelist[t.s.CHANMODE_INVITEMASK]).map(function(i) { return t.chan.modelist[t.s.CHANMODE_INVITEMASK][i]; }); }

run_tests({
	"matching invite masks admit invite-only joins without granting a personal invitation": function() {
		var t = scenario(); mode(t, "+i"); check(!t.visitor.do_join("#test"));
		mode(t, "+I *!reader@*.test"); check(t.visitor.do_join("#test"));
		check(!t.visitor.invited && t.chan.isinvitematched(t.visitor.nuh));
	},
	"invite masks do not bypass keys, limits or bans": function() {
		var t = scenario(); mode(t, "+iIkl Visitor secret 1");
		check(!t.visitor.do_join("#test", "secret")); mode(t, "-l");
		check(!t.visitor.do_join("#test", "wrong")); mode(t, "+b Visitor");
		check(!t.visitor.do_join("#test", "secret")); mode(t, "-b Visitor!*@*");
		check(t.visitor.do_join("#test", "secret"));
	},
	"list replies, permissions, canonical duplicates and exact removals retain metadata": function() {
		var t = scenario(); t.visitor.work("MODE #test +I Visitor"); check(!masks(t).length);
		mode(t, "+I Visitor"); var stamp = t.chan.invitemasktime[0], setter = t.chan.invitemaskcreator[0];
		t.s.now++; mode(t, "+I visitor!*@*"); check(masks(t).length == 1);
		check(t.chan.invitemasktime[0] == stamp && t.chan.invitemaskcreator[0] == setter);
		t.visitor.received = []; t.visitor.work("MODE #test +I");
		check(t.visitor.received.some(function(line) { return line.indexOf(" 346 Visitor #test Visitor!*@*") >= 0; }));
		check(t.visitor.received.some(function(line) { return line.indexOf(" 347 ") >= 0; }));
		mode(t, "-I *!*@*"); check(masks(t).length == 1);
		mode(t, "-I visitor!*@*"); check(!masks(t).length && !Object.keys(t.chan.invitemasktime).length);
	},
	"invite masks accept ordinary identities and reject extended message exceptions": function() {
		var t = scenario(); mode(t, "+I ~m:external:Visitor"); check(!masks(t).length);
		mode(t, "+I *!Some%2B.User+VERT@2001:db8::1"); check(masks(t)[0] == "*!Some%2B.User+VERT@2001:db8::1");
	},
	"invite capacity is independent of bans and exceptions": function() {
		var t = scenario();
		for (var i = 0; i < 25; i++) { mode(t, "+I User" + i); mode(t, "+b Ban" + i); mode(t, "+e Except" + i); }
		mode(t, "+I Full"); check(masks(t).length == 25);
		check(t.chan.count_modelist(t.s.CHANMODE_BAN) == 25 && t.chan.count_modelist(t.s.CHANMODE_EXCEPT) == 25);
	},
	"mixed modes preserve arguments for old peers and Bahamut services": function() {
		var t = scenario(), fresh = new_peer(t.s, "fresh.test", true), old = new_peer(t.s, "old.test", true), service = new_peer(t.s, "services.test", false);
		fresh.their_capab.push("INVMASK"); service.protocol = "bahamut";
		join_user(t.s, t.visitor, "#test");
		mode(t, "+Ikov Visitor secret Visitor Owner");
		check(fresh.sent[0].indexOf("+kIov secret Visitor!*@* Visitor Owner") >= 0, fresh.sent.join(" | "));
		check(service.sent[0].indexOf("Visitor!*@*") >= 0);
		check(old.sent[0].indexOf("Visitor!*@*") < 0 && old.sent[0].indexOf("+kov secret Visitor Owner") >= 0, old.sent.join(" | "));
		fresh.sent = []; service.sent = []; old.sent = [];
		fresh.server_chan_info(t.chan); service.server_chan_info(t.chan); old.server_chan_info(t.chan);
		check(fresh.sent.some(function(line) { return line.indexOf("+I Visitor!*@*") >= 0; }));
		check(service.sent.some(function(line) { return line.indexOf("+I Visitor!*@*") >= 0; }));
		check(!old.sent.some(function(line) { return line.indexOf("Visitor!*@*") >= 0; }));
	},
	"services clear operations relay even through caches that already lack the masks": function() {
		var t = scenario(), service = new_peer(t.s, "services.test", false), hub = new_peer(t.s, "next.test", true);
		service.uline = true; service.protocol = "bahamut"; hub.their_capab.push("INVMASK");
		mode(t, "+II Visitor Owner"); hub.sent = [];
		service.work(":services.test MODE #test 0 -II Visitor!*@* Owner!*@*");
		service.work(":services.test MODE #test 0 -II Visitor!*@* Owner!*@*");
		check(!masks(t).length && hub.sent.length == 2);
	},
	"snapshots reconcile invite lists without churn and old snapshots cannot erase them": function() {
		var t = scenario(), hub = new_peer(t.s, "next.test", true); mode(t, "+II Keep Gone");
		channel_sync(hub, t.chan.created, "#test", "+", "@Owner", ""); check(masks(t).length == 2);
		hub.their_capab.push("INVMASK"); var stamp = t.chan.invitemasktime[0], setter = t.chan.invitemaskcreator[0];
		hub.work("CSYNC " + t.chan.created + " #test BEGIN +");
		hub.work("CSYNC " + t.chan.created + " #test INVITES :keep!*@* Added!*@*");
		hub.work("CSYNC " + t.chan.created + " #test MORE :@Owner"); hub.work("CSYNC " + t.chan.created + " #test END");
		check(masks(t).sort().join(" ") == "Added!*@* Keep!*@*");
		check(t.chan.invitemasktime[0] == stamp && t.chan.invitemaskcreator[0] == setter);
		channel_sync(hub, t.chan.created, "#test", "+", "@Owner", ""); check(!masks(t).length);
	},
	"untrusted leaf snapshots and server modes cannot change invite masks": function() {
		var t = scenario(), leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("INVMASK"); mode(t, "+I Keep");
		leaf.work(":leaf.test MODE #test 0 -I Keep!*@*"); channel_sync(leaf, t.chan.created, "#test", "+", "@Owner", "");
		check(masks(t).join(" ") == "Keep!*@*");
		t.s.Channel_reset_timestamp(t.chan, t.chan.created - 1); check(masks(t).length == 1);
	},
	"long invite lists fit MODE and snapshot framing": function() {
		var t = scenario(), leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("INVMASK");
		for (var i = 0; i < 12; i++) mode(t, "+I User" + i + "!" + new Array(81).join("u") + "@" + new Array(70).join("h"));
		leaf.sent = []; leaf.server_chan_info(t.chan); t.s.IRC_resync_leaf(leaf, t.chan);
		check(masks(t).length == 12 && leaf.sent.every(function(line) { return line.length <= 510; }));
		check(leaf.sent.some(function(line) { return line.indexOf(" INVITES :") >= 0; }));
	}
});
