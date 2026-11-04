/* Regressions from the fresh review of the unpublished 2.11 commit. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function services(s) {
	var peer = new_peer(s, "services.test", false);
	peer.uline = true; s.ULines = [peer.nick];
	return peer;
}
function timestamp_network() {
	var hub = new_state(), service = services(hub), leaf = new_peer(hub, "leaf.test", false);
	leaf.their_capab.push("SVCSTATE");
	leaf.work("NICK Reader 1 1799999950 +i user host.test leaf.test 0 0 :Reader");
	var home = new_state(); home.ServerName = "leaf.test"; home.ULines = [service.nick];
	var uplink = new_peer(home, hub.ServerName, true); uplink.their_capab.push("SVCSTATE");
	var authority = new home.IRC_Server();
	authority.nick = service.nick; authority.parent = uplink.nick; authority.local = false; authority.uline = true;
	home.Servers[authority.nick] = authority;
	var user = new_user(home, "Reader"); user.created = 1799999950;
	return {hub:hub, home:home, service:service, leaf:leaf, uplink:uplink, user:user};
}

run_tests({
	"denied private-channel joins correct the old leaf without exposing the channel": function() {
		["CHANMODE_PRIVATE", "CHANMODE_SECRET"].forEach(function(flag) {
			["JOIN #hidden", "SJOIN 1800000000 #hidden + :@Reader"].forEach(function(command) {
				var s = new_state(), leaf = new_peer(s, "old.test", false), owner = new_user(s, "Owner");
				leaf.their_capab = [];
				var chan = join_user(s, owner, "#hidden"); chan.mode |= s[flag] | s.CHANMODE_SECURE;
				var user = new_user(s, "Reader", leaf.nick);
				leaf.work(":" + (command.indexOf("SJOIN") == 0 ? leaf.nick : user.nick) + " " + command);
				check(!chan.users[user.id] && !user.channels["#HIDDEN"]);
				check(leaf.sent.length == 1 && leaf.sent[0].indexOf("KICK #hidden Reader :") >= 0, leaf.sent.join(" | "));
				check(!chan.leaf_views || !chan.leaf_views[leaf.nick] || !chan.leaf_views[leaf.nick].visible);
			});
		});
	},
	"remote FileServ keeps its channel-policy exemptions without gaining identity or TLS": function() {
		var s = new_state(), leaf = new_peer(s, "leaf.test", false), owner = new_user(s, "Owner");
		leaf.their_capab.push("CHANPOLICY", "CLIENTTLS");
		var chan = join_user(s, owner, "#files");
		chan.mode |= s.CHANMODE_REGISTEREDONLY | s.CHANMODE_OPERONLY | s.CHANMODE_SECURE | s.CHANMODE_REGMODERATED | s.CHANMODE_NOCOLOR;
		leaf.work("NICK FileServ_HOME 1 1800000000 +i bbs+ircd+files leaf.test leaf.test 0 0 :Synchronet embedded services");
		var bot = s.Users.FILESERV_HOME;
		leaf.work(":leaf.test CLIENTTLS FileServ_HOME INTERNAL");
		leaf.work(":FileServ_HOME JOIN #files");
		check(chan.users[bot.id] === bot, leaf.sent.join(" | "));
		check(!bot.uline && !bot.services_internal && !s.IRC_services_identified(bot) && !bot.services_identity);
		check(s.IRC_client_tls_status(bot) == "INTERNAL");
		bot.do_msg("#files", "PRIVMSG", "\x02File listing");
		check(owner.received.some(function(line) { return line.indexOf("File listing") >= 0; }));
	},
	"SGLINE preserves raw UTF-8 bytes through matching, replay and exact removal": function() {
		var s = new_state(), service = services(s), next = new_peer(s, "next.test", true);
		next.their_capab.push("SLINES");
		var name = "Jos" + String.fromCharCode(0xc3, 0xa9), mask = "*" + name + "*";
		service.work(":services.test SGLINE " + mask.length + " :" + mask + ":Realname policy");
		check(s.IRC_match_sline("G", name), "UTF-8 name escaped the ban");
		check(s.Services_Lines[0].mask == "*jos" + String.fromCharCode(0xc3, 0xa9) + "*");
		check(next.sent[0].indexOf(String.fromCharCode(0xc3, 0xa9)) >= 0);
		service.work(":services.test UNSGLINE :" + mask); check(!s.Services_Lines.length);
	},
	"timestamped services grants reach the owner despite leaf clock normalization": function() {
		var t = timestamp_network();
		t.service.work(":services.test SVSMODE Reader 1800000000 +dr 123");
		t.leaf.sent.forEach(function(line) { t.uplink.work(line); });
		check(t.hub.IRC_services_identified(t.hub.Users.READER));
		check(t.home.IRC_services_identified(t.user), t.leaf.sent.join(" | "));
		check(t.hub.Users.READER.created == 1800000000 && t.user.created == 1799999950);
		check(t.user.external_services.stamp == 123);
	},
	"corrective KICK visibility is scoped to its exact peer, channel and rejected user": function() {
		var s = new_state(), leaf = new_peer(s, "old.test", false), other = new_peer(s, "other.test", false);
		leaf.their_capab = other.their_capab = [];
		var owner = new_user(s, "Owner"), user = new_user(s, "Reader", leaf.nick);
		var chan = join_user(s, owner, "#hidden"); chan.mode |= s.CHANMODE_PRIVATE | s.CHANMODE_SECURE;
		var send = leaf.ircout;
		leaf.ircout = function(line) {
			this.rawout(":" + s.ServerName + " MODE #hidden +o Owner");
			this.rawout(":" + s.ServerName + " KICK #hidden Owner :unrelated");
			other.rawout(":" + s.ServerName + " " + line);
			send.call(this, line);
		};
		check(!s.IRC_channel_remote_join(leaf, user, chan));
		check(leaf.sent.length == 1 && !other.sent.length && !leaf.rejected_join);
		leaf.sent = []; leaf.ircout = function() { throw new Error("send failure"); };
		try { s.IRC_channel_remote_join(leaf, user, chan); } catch (e) { check(e.message == "send failure"); }
		check(!leaf.rejected_join);
		leaf.rawout(":" + s.ServerName + " KICK #hidden Reader :later"); check(!leaf.sent.length);
	},
	"FileServ declarations fail closed on identity, owner and route mismatches": function() {
		var changes = [function(u) { u.nick = "FileServ_TOOLONGID"; },
			function(u) { u.nick = "Reader"; }, function(u) { u.uprefix = "bbs+files"; },
			function(u) { u.local = true; }, function(u) { u.servername = "unknown.test"; },
			function(u, p) { p.admitted = false; }, function(u, p) { p.quitting = true; },
			function(u, p, s) { u.servername = new_peer(s, "wrong.test", false).nick; }];
		changes.forEach(function(change) {
			var s = new_state(), peer = new_peer(s, "leaf.test", false), u = new_user(s, "FileServ_HOME", peer.nick);
			u.uprefix = "bbs+ircd+files"; check(s.IRC_is_internal_service(u));
			change(u, peer, s); check(!s.IRC_is_internal_service(u));
		});
		var s = new_state(), u = new_user(s, "FileServ_HOME");
		check(s.parse_username("bbs+ircd+files") != "bbs+ircd+files");
		check(!s.IRC_is_internal_service(u));
	},
	"FileServ works through an old hub without asserting TLS and conflicts revoke policy exemptions": function() {
		var s = new_state(), hub = new_peer(s, "oldhub.test", true), owner = new s.IRC_Server();
		owner.nick = "home.test"; owner.parent = hub.nick; owner.local = false; s.Servers[owner.nick] = owner;
		var u = new_user(s, "FileServ_HOME", hub.nick); u.servername = owner.nick; u.uprefix = "bbs+ircd+files";
		check(s.IRC_is_internal_service(u) && s.IRC_client_tls_status(u) == "UNKNOWN");
		var chan = join_user(s, new_user(s, "Owner"), "#files");
		chan.mode |= s.CHANMODE_SECURE | s.CHANMODE_JOINRATE; chan.arg[s.CHANMODE_JOINRATE] = "1:30";
		check(!s.IRC_channel_join_policy(u, chan, true)); s.IRC_channel_joined(u, chan);
		check(!chan.joinrate);
		hub.their_capab.push("CLIENTTLS");
		hub.work(":oldhub.test CLIENTTLS FileServ_HOME INTERNAL"); check(!u.client_tls);
		hub.work(":home.test CLIENTTLS FileServ_HOME INTERNAL"); check(s.IRC_client_tls_status(u) == "INTERNAL");
		hub.work(":home.test CLIENTTLS FileServ_HOME TLS");
		check(!s.IRC_is_internal_service(u) && s.IRC_client_tls_status(u) != "TLS");
		check(s.IRC_channel_join_policy(u, chan, true));
		u.client_tls = null; u.nick = "Other"; check(!s.IRC_is_internal_service(u));
	},
	"S-line canonicalization folds only ASCII across all high wire bytes": function() {
		var s = new_state(), bytes = "";
		for (var i = 128; i <= 255; i++) bytes += String.fromCharCode(i);
		check(s.IRC_sline_mask("G", "A**Z" + bytes) == "a*z" + bytes);
		check(s.IRC_sline_mask("Q", "Ab**Cd") == "ab*cd");
	},
	"conditional revocations and SVCSTATE snapshots use the owning leaf timestamp": function() {
		var t = timestamp_network();
		t.user.mode |= t.home.USERMODE_OPER; t.hub.Users.READER.mode |= t.hub.USERMODE_OPER;
		t.service.work(":services.test SVSMODE Reader 1800000000 +dra 42");
		t.leaf.sent.forEach(function(line) { t.uplink.work(line); });
		check(t.home.IRC_services_admin(t.user)); t.leaf.sent = [];
		t.hub.IRC_send_svcstate(t.leaf, t.hub.Users.READER);
		check(t.leaf.sent[0] == ":hub.test SVCSTATE Reader 1799999950 services.test 42 ra");
		delete t.user.external_services; t.uplink.work(t.leaf.sent[0]); check(t.home.IRC_services_admin(t.user));
		t.leaf.sent = []; t.service.work(":services.test SVSMODE Reader 1800000000 -dra 0");
		t.leaf.sent.forEach(function(line) { t.uplink.work(line); });
		check(!t.home.IRC_services_identified(t.user) && !t.home.IRC_services_admin(t.user) && !t.user.services_stamp);
	},
	"invalid or stale leaf timestamp mappings reject before any grant or relay": function() {
		[function(t, u) { delete u.leaf_nick_view; },
		 function(t, u) { u.leaf_nick_view.peer = {}; },
		 function(t, u) { u.leaf_nick_view.nick = "OTHER"; },
		 function(t, u) { u.leaf_nick_view.created++; },
		 function(t, u) { u.leaf_nick_view.wire = null; },
		 function(t, u) { t.hub.Local_Servers[t.leaf.nick] = {}; }].forEach(function(change) {
			var t = timestamp_network(), u = t.hub.Users.READER; change(t, u);
			t.service.work(":services.test SVSMODE Reader 1800000000 +r");
			check(!u.external_services && !t.leaf.sent.length);
		});
		["0", "oops", "4294967296"].forEach(function(ts) {
			var t = timestamp_network(); t.leaf.work("NICK Missing 1 " + ts + " +i user host.test leaf.test 0 0 :Missing");
			t.service.work(":services.test SVSMODE Missing 1800000000 +r");
			check(!t.hub.Users.MISSING.external_services && !t.leaf.sent.length);
		});
	},
	"case-only and real renames refresh leaf addressing without trusting leaf time": function() {
		var t = timestamp_network(), u = t.hub.Users.READER;
		u.services_created = 777; var proof = u.services_identity = {id:"BBS:1:2"};
		t.leaf.work(":Reader NICK reader 1799999960");
		check(u.created == 1800000000 && u.leaf_nick_view.wire == 1799999960);
		t.service.work(":services.test SVSMODE reader 1800000000 +r"); check(t.hub.IRC_services_identified(u));
		t.leaf.work(":reader NICK Other 1799999970");
		check(!t.hub.IRC_services_identified(u) && u.created == 1800000000 && u.leaf_nick_view.nick == "OTHER");
		t.leaf.sent = []; t.service.work(":services.test SVSMODE Other 1800000000 +r");
		check(t.leaf.sent[0].indexOf("SVSMODE Other 1799999970 +r") >= 0);
		check(u.services_created == 777 && u.services_identity === proof);
	},
	"unconditional services commands keep their existing omitted or zero timestamp": function() {
		["+r", "0 +r"].forEach(function(fields) {
			var t = timestamp_network(); delete t.hub.Users.READER.leaf_nick_view;
			t.service.work(":services.test SVSMODE Reader " + fields);
			check(t.hub.IRC_services_identified(t.hub.Users.READER));
			check(t.leaf.sent[0] == ":services.test SVSMODE Reader " + fields);
		});
	},
	"leaf-side services conditions translate back to canonical time for other hubs": function() {
		var t = timestamp_network(), s = t.hub, next = new_peer(s, "next.test", true);
		next.their_capab.push("SVCSTATE");
		// A separately configured services authority is on the owner's branch.
		var authority = new s.IRC_Server(); authority.nick = "local-services.test";
		authority.local = false; authority.parent = t.leaf.nick; authority.uline = true;
		s.Servers[authority.nick] = authority; s.ULines.push(authority.nick);
		t.leaf.work(":local-services.test SVSMODE Reader 1799999950 +dr 91");
		check(s.IRC_services_identified(s.Users.READER));
		check(next.sent[0] == ":local-services.test SVSMODE Reader 1800000000 +dr 91");
		check(!t.leaf.sent.length);
		t.leaf.work(":local-services.test SVSMODE Reader 1799999951 -r");
		check(s.IRC_services_identified(s.Users.READER) && next.sent.length == 1);
	},
	"missing snapshot addresses and replacement users cannot reuse an earlier incarnation": function() {
		var t = timestamp_network(), u = t.hub.Users.READER;
		t.service.work(":services.test SVSMODE Reader +r"); t.leaf.sent = [];
		delete u.leaf_nick_view; t.hub.IRC_send_svcstate(t.leaf, u); check(!t.leaf.sent.length);
		u.quit("gone", true);
		t.leaf.work("NICK Reader 1 1799999990 +i user host.test leaf.test 0 0 :Replacement");
		var replacement = t.hub.Users.READER;
		check(replacement !== u && replacement.leaf_nick_view.wire == 1799999990 && !replacement.external_services);
		t.service.work(":services.test SVSMODE Reader 1799999950 +r"); check(!replacement.external_services);
		t.leaf.sent = []; t.service.work(":services.test SVSMODE Reader 1800000000 +r");
		check(t.leaf.sent[0].indexOf("Reader 1799999990 +r") >= 0);
	}
});
