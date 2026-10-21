/* IRCd state regressions. No sockets or daemon processes are started. */
"use strict";

load(js.exec_dir + "lib.sjs");

var next_id = 0;

function new_state() {
	var s = {
		system: {timer: 1000}, now: 1800000000,
		ServerName: "hub.test", ServerDesc: "State tests", server: {},
		Users: {}, Servers: {}, Channels: {}, Local_Users: {}, Local_Servers: {},
		Assigned_IDs: {}, Unregistered: {}, ULines: [], QLines: [], Profile: {},
		WhoWas: {}, WhoWasMap: [], NickHistory: [],
		MAX_CHANLEN: 100, MAX_USER_CHANS: 100, MAX_MODES: 6, MAX_BANS: 25,
		MAX_NICKLEN: 30, MAX_NICKHISTORY: 1000, MAX_WHOWAS: 1000,
		MAX_TOPICLEN: 307, Enforcement: true, YLines: [{pingfreq: 90}],
		log: function() {},
		js: {setImmediate: function() {}, setTimeout: function() {}, clearInterval: function() {}},
		Generate_ID: function() { return ++next_id; }
	};
	["irclib.js", "ircd/core.js", "ircd/user.js", "ircd/channel.js",
	 "ircd/server.js", "ircd/config.js", "ircd/timestamps.js"].forEach(function(f) { load(s, f); });
	s.Epoch = function() { return s.now; };
	s.IRC_clock_now = function() { return s.now; };
	return s;
}

function new_peer(s, name, hub) {
	var p = new s.IRC_Server();
	p.nick = p.parent = p.id = name;
	p.hub = hub;
	p.admitted = true;
	p.clock_role_received = true; p.clock_enforce = true;
	p.their_capab = ["TSJOIN", "TSMODE", "CSYNC"];
	p.sent = [];
	p.rawout = function(line) { this.sent.push(line); };
	p.ircout = function(line) { this.rawout(":" + s.ServerName + " " + line); };
	p.originatorout = function(line, origin) { this.rawout(":" + origin.nick + " " + line); };
	p.quit = function(reason) { this.quitting = true; this.quit_reason = reason; };
	s.Servers[name.toLowerCase()] = s.Local_Servers[name.toLowerCase()] = p;
	return p;
}

function new_user(s, nick, parent) {
	var u = new s.IRC_User(++next_id);
	u.nick = nick;
	u.parent = parent || 0;
	u.local = !parent;
	u.servername = parent || s.ServerName;
	u.received = [];
	u.rawout = function(line) { this.received.push(line); };
	u.ircout = function(line) { this.rawout(":" + s.ServerName + " " + line); };
	u.originatorout = function(line, origin) { this.rawout(":" + origin.nick + " " + line); };
	s.Users[nick.toUpperCase()] = u;
	if (u.local) s.Local_Users[u.id] = u;
	return u;
}

function join_user(s, u, name, ts) {
	var c = s.Channels[name.toUpperCase()];
	if (!c)
		c = s.Channels[name.toUpperCase()] = new s.Channel(name);
	c.created = ts === undefined ? s.now : ts;
	c.users[u.id] = u;
	u.channels[name.toUpperCase()] = c;
	return c;
}

function check(cond, message) {
	if (!cond)
		throw new Error(message);
}

function merge_sides(ts_a, ts_b) {
	var a = new_state(), b = new_state();
	a.ServerName = "a.test"; b.ServerName = "b.test";
	var ab = new_peer(a, "b.test", true), ba = new_peer(b, "a.test", true);
	var alice_a = new_user(a, "Alice"), bob_a = new_user(a, "Bob", ab.nick);
	var alice_b = new_user(b, "Alice", ba.nick), bob_b = new_user(b, "Bob");
	var ca = join_user(a, alice_a, "#merge", ts_a), cb = join_user(b, bob_b, "#merge", ts_b);
	ca.mode |= a.CHANMODE_KEY | a.CHANMODE_LIMIT | a.CHANMODE_PRIVATE;
	cb.mode |= b.CHANMODE_KEY | b.CHANMODE_LIMIT | b.CHANMODE_SECRET;
	ca.arg[a.CHANMODE_KEY] = "keyA"; ca.arg[a.CHANMODE_LIMIT] = 10;
	cb.arg[b.CHANMODE_KEY] = "keyB"; cb.arg[b.CHANMODE_LIMIT] = 20;
	ca.modelist[a.CHANMODE_OP][alice_a.id] = alice_a;
	cb.modelist[b.CHANMODE_OP][bob_b.id] = bob_b;
	ab.server_chan_info(ca); ba.server_chan_info(cb);
	var from_a = ab.sent.slice(), from_b = ba.sent.slice();
	from_a.forEach(function(line) { ba.work(line); });
	from_b.forEach(function(line) { ab.work(line); });
	return {a:a, b:b, ca:ca, cb:cb, alice_a:alice_a, bob_b:bob_b};
}

run_tests({
	"legacy services TS-zero JOIN and SJOIN retain authority without resetting channel age": function() {
		var s = new_state(), p = new_peer(s, "services.test", false); p.uline = true;
		var local = new_user(s, "Local"), service = new_user(s, "ChanServ", p.nick); service.uline = true;
		var c = join_user(s, local, "#service"); c.modelist[s.CHANMODE_OP][local.id] = local;
		p.work(":ChanServ JOIN 0 #service");
		p.work(":services.test SJOIN 0 #service +t :@ChanServ");
		check(!p.quitting && c.created == s.now && c.modelist[s.CHANMODE_OP][local.id]
			&& c.modelist[s.CHANMODE_OP][service.id] && (c.mode & s.CHANMODE_TOPIC), "services TS-zero state lost");
		p.work(":services.test SJOIN 0 #newservice + :@ChanServ");
		check(s.Channels["#NEWSERVICE"].created == s.now, "services introduced epoch-zero channel");
	},
	"unqualified NAMES also bounds the list of users outside channels by wire length": function() {
		var s = new_state(), observer = new_user(s, "Observer"), expected = [];
		for (var i = 0; i < 80; i++) {
			var nick = "Nick" + ("000" + i).slice(-3) + "abcdefghijklmnopqrstuvw";
			new_user(s, nick); expected.push(nick);
		}
		observer.work("NAMES");
		observer.received.forEach(function(line) { check(line.length <= 510, "oversized unqualified NAMES"); });
		expected.forEach(function(n) { check(observer.received.join(" ").indexOf(n) >= 0, "omitted nick " + n); });
	},
	"QUIT precedes retirement of the last local user's channel mapping": function() {
		var s = new_state(), p = new_peer(s, "upstream.test", true);
		p.their_capab.push("TSMAP"); p.clock_enforce = false;
		var u = new_user(s, "Local"); u.do_join("#quit"); p.sent = [];
		u.socket = {is_connected: false, clearOn: function() {}, close: function() {}};
		u.quit("leaving");
		var quit = p.sent.map(function(l) { return l.indexOf(" QUIT ") >= 0; }).indexOf(true);
		var drop = p.sent.map(function(l) { return l.indexOf("TSMAP DROP ") >= 0; }).indexOf(true);
		check(quit >= 0 && drop > quit, "mapping DROP preceded QUIT");
	},
	"a delayed channel correction cannot resurrect an in-flight local PART": function() {
		var s = new_state(), p = new_peer(s, "upstream.test", true);
		p.their_capab.push("TSMAP"); p.clock_enforce = false;
		var u = new_user(s, "Local"), remote = new_user(s, "Remote", p.nick);
		u.do_join("#race"); join_user(s, remote, "#race");
		p.work("CSYNC " + s.now + " #race BEGIN +");
		p.work("CSYNC " + s.now + " #race MORE :Local Remote");
		u.do_part("#race"); var token = u.pending_channel_parts["#RACE"].token;
		p.work("CSYNC " + s.now + " #race END");
		check(!u.channels["#RACE"] && !s.Channels["#RACE"].users[u.id], "snapshot resurrected departed user");
		p.work("TSMAP PARTACK #race " + token);
		check(!u.pending_channel_parts["#RACE"], "PART acknowledgment leaked pending state");
	},
	"local departure reaches the hub before its ended channel mapping is retired": function() {
		var s = new_state(), p = new_peer(s, "upstream.test", true);
		p.their_capab.push("TSMAP"); p.clock_enforce = false;
		var u = new_user(s, "Local"); u.do_join("#ended"); p.sent = [];
		u.do_part("#ended");
		var part = p.sent.map(function(l) { return l.indexOf(" PART ") >= 0; }).indexOf(true);
		var drop = p.sent.map(function(l) { return l.indexOf("TSMAP DROP ") >= 0; }).indexOf(true);
		check(part >= 0 && drop > part, "mapping DROP preceded PART");
	},
	"channel correction carries a maximum-length topic without exceeding the wire limit": function() {
		var s = new_state(); s.ServerName = new Array(64).join("s");
		var p = new_peer(s, "leaf.test", false), u = new_user(s, "Local", p.nick);
		var c = join_user(s, u, "#" + new Array(100).join("c"));
		c.topic = new Array(308).join("t"); c.topictime = s.now;
		s.IRC_resync_leaf(p, c);
		p.sent.forEach(function(line) { check(line.length <= 510, "oversized correction: " + line.length); });
		check(p.sent.some(function(line) { return line.indexOf(c.topic) >= 0; }), "topic was truncated");
	},
	"an old hub snapshot cannot resurrect mappings for ended channel objects": function() {
		var a = new_state(), b = new_state(); b.ServerName = "b.test";
		var ab = new_peer(a, "b.test", true), ba = new_peer(b, "hub.test", true);
		ab.their_capab.push("TSMAP"); ba.their_capab.push("TSMAP");
		var p = new_peer(a, "leaf.test", false); p.their_capab.push("TSMAP");
		var id = "0123456789abcdef0123456789abcdef";
		p.work("TSMAP BEGIN " + id); new_user(a, "Alice", p.nick);
		p.work("TSMAP CLAIM " + id + " 1 1 #old"); p.work("SJOIN 1 #old + :Alice");
		p.work("TSMAP END " + id);
		ab.sent.forEach(function(l) { if (/ TSMAP /.test(l)) ba.work(l); });
		ba.sent = []; b.IRC_tsmap_burst(ba); var stale = ba.sent.slice();
		p.work(":Alice PART #old"); p.work("TSMAP DROP " + id + " 1");
		stale.forEach(function(l) { ab.work(l); });
		check(!Object.keys(a.Leaf_Timestamps["leaf.test"].objects).length,
			"stale burst resurrected a retired mapping");
	},
	"returning leaf waits for the hub's cached mapping burst to finish": function() {
		var s = new_state(), hub = new_peer(s, "otherhub.test", true);
		hub.their_capab.push("TSMAP"); hub.bursting = true;
		var leaf = new_peer(s, "leaf.test", false); leaf.admitted = false;
		leaf.clock_verified = true; leaf.clock_ready_received = true;
		leaf.synchronize = function() { this.bursts = 1; };
		s.IRC_finish_clock_handshake(leaf);
		check(!leaf.admitted && leaf.awaiting_hub_maps, "leaf raced incoming hub mappings");
		check(!leaf.clock_ready_sent, "READY let the leaf send state before mappings arrived");
		hub.work("BURST 0");
		check(leaf.admitted && leaf.bursts, "leaf not released after mapping burst");
	},
	"leaf clocks outside the wire epoch range still emit valid channel identities": function() {
		[-86400, 5000000000].forEach(function(now) {
			var s = new_state(), p = new_peer(s, "hub.test", true);
			p.their_capab.push("TSMAP"); p.clock_enforce = false; s.now = now;
			new_user(s, "Local").do_join("#farclock");
			var c = s.Channels["#FARCLOCK"];
			check(c.leaf_timestamp > 0 && c.leaf_timestamp <= 4294967295, "invalid raw identity");
			check(p.sent.some(function(l) { return l.indexOf("JOIN " + c.leaf_timestamp + " #farclock") >= 0; }),
				"leaf emitted a negative or overflowing JOIN timestamp");
		});
	},
	"hub-issued mapping follows a leaf process to another hub after its channel disappears": function() {
		var a = new_state(), b = new_state();
		var ab = new_peer(a, "b.test", true), ba = new_peer(b, "hub.test", true);
		ab.their_capab.push("TSMAP"); ba.their_capab.push("TSMAP");
		var leaf = new_peer(a, "leaf.test", false); leaf.their_capab.push("TSMAP");
		var id = "0123456789abcdef0123456789abcdef";
		leaf.work("TSMAP BEGIN " + id);
		new_user(a, "Alice", leaf.nick);
		leaf.work("TSMAP CLAIM " + id + " 1 1 #moving");
		leaf.work("SJOIN 1 #moving +m :@Alice");
		leaf.work("TSMAP END " + id);
		var ts = a.Channels["#MOVING"].created;
		ab.sent.forEach(function(line) { if (/ TSMAP /.test(line)) ba.work(line); });
		check(b.Leaf_Timestamps["leaf.test"].objects["1"].net == ts, "mapping was not replicated");
		/* No Channel or Server object survives the simulated SQUIT. */
		delete a.Channels["#MOVING"]; delete a.Servers["leaf.test"];
		b.now += 3600;
		var moved = new_peer(b, "leaf.test", false); moved.their_capab.push("TSMAP");
		moved.work("TSMAP BEGIN " + id); new_user(b, "Alice", moved.nick);
		moved.work("TSMAP CLAIM " + id + " 1 1 #moving");
		moved.work("SJOIN 1 #moving +m :@Alice"); moved.work("TSMAP END " + id);
		check(b.Channels["#MOVING"].created == ts, "handoff assigned a fresh timestamp");
		check(b.Channels["#MOVING"].created != b.now, "reconnect time replaced original mapping");
	},
	"timestamp cache retires ended objects and replacement leaf processes": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false); p.their_capab.push("TSMAP");
		var id = "0123456789abcdef0123456789abcdef";
		p.work("TSMAP BEGIN " + id); new_user(s, "Alice", p.nick);
		for (var i = 0; i < 100; i++) p.work("TSMAP CLAIM " + id + " " + i + " 1 #unused");
		check(!Object.keys(s.Leaf_Timestamps[p.nick].objects).length, "claims alone grew cache");
		p.work("TSMAP CLAIM " + id + " 1 1 #old"); p.work("SJOIN 1 #old + :Alice");
		p.work("TSMAP END " + id);
		var q = new_peer(s, p.nick, false); q.their_capab.push("TSMAP");
		q.work("TSMAP BEGIN " + id); q.work("TSMAP END " + id);
		check(!Object.keys(s.Leaf_Timestamps[p.nick].objects).length, "absent objects not retired");
		var r = new_peer(s, p.nick, false); r.their_capab.push("TSMAP");
		r.work("TSMAP BEGIN fedcba9876543210fedcba9876543210");
		check(s.Leaf_Timestamps[p.nick].instance != id, "replacement process reused old identity");
	},
	"leaf cannot forge replicated mappings or replace an established hub channel age": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false); p.their_capab.push("TSMAP");
		var id = "0123456789abcdef0123456789abcdef";
		p.work("TSMAP RESET victim.test " + id + " 1 hub.test");
		check(!s.Leaf_Timestamps["victim.test"], "leaf forged a hub record");
		p.work("TSMAP BEGIN " + id);
		var u = new_user(s, "Alice", p.nick), c = join_user(s, new_user(s, "HubOp"), "#existing");
		p.work("TSMAP CLAIM " + id + " 1 1 #existing");
		p.work("SJOIN 1 #existing +m :@Alice");
		check(c.created == s.now && !c.modelist[s.CHANMODE_OP][u.id], "mapping bypassed hub authority");
		p.work("TSMAP CLAIM " + id + " 2 1 #existing");
		p.work("SJOIN 1 #existing +m :@Alice");
		check(p.quitting && Object.keys(s.Leaf_Timestamps[p.nick].objects).length == 1,
			"leaf could allocate unlimited identities for an active channel");
	},
	"leaf link negotiates correction without clock-skew enforcement at either end": function() {
		[false, true].forEach(function(upstream) {
			var s = new_state(), p = new_peer(s, "peer.test", upstream);
			p.admitted = false; p.clock_role_received = false;
			p.synchronize = function() { this.bursts = 1; };
			s.IRC_clock_tick.call(p);
			check(!p.clock_probe, "clock probed before role negotiation");
			p.work("CLOCK ROLE " + (upstream ? "LEAF" : "HUB"));
			p.work("CLOCK READY");
			check(p.admitted && p.bursts && !p.clock_enforce && !p.clock_probe,
				"leaf clock was required to agree with hub");
			s.system.timer += 100; s.now += 86400; s.IRC_clock_tick.call(p);
			check(!p.quitting && !p.clock_probe, "drifting leaf disconnected");
		});
	},
	"leaf channel ops cannot backdate MODE to reset hub authority": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false);
		var hubop = new_user(s, "HubOp"), leafop = new_user(s, "LeafOp", p.nick);
		var c = join_user(s, hubop, "#guard"); join_user(s, leafop, "#guard");
		c.modelist[s.CHANMODE_OP][hubop.id] = hubop;
		c.modelist[s.CHANMODE_OP][leafop.id] = leafop;
		p.work(":LeafOp MODE #guard 1 +m");
		check(c.created == s.now && c.modelist[s.CHANMODE_OP][hubop.id]
			&& c.modelist[s.CHANMODE_OP][leafop.id] && (c.mode & s.CHANMODE_MODERATED),
			"leaf MODE changed hub timestamp or erased legitimate ops");
	},
	"local founder reaches legacy hubs through their existing SJOIN dialect": function() {
		var s = new_state(), p = new_peer(s, "oldhub.test", true);
		p.their_capab = ["TSJOIN", "TSMODE"];
		var services = new_peer(s, "services.test", true); services.uline = true;
		services.their_capab = [];
		var u = new_user(s, "Founder"); u.do_join("#new");
		check(p.sent.join("\n").indexOf("SJOIN " + s.now + " #new + :@Founder") >= 0,
			"old hub missed founder snapshot");
		check(!/ MODE /.test(p.sent.join("\n")), "redundant founder MODE to legacy hub");
		check(/:Founder JOIN #new/.test(services.sent.join("\n"))
			&& /MODE #new \+o Founder/.test(services.sent.join("\n")), "services dialect changed");
	},
	"peer cannot hold admission forever by omitting reciprocal READY": function() {
		var s = new_state(), p = new_peer(s, "peer.test", true);
		p.admitted = false; s.IRC_clock_tick.call(p);
		p.work("CLOCK " + p.clock_probe.nonce + " " + s.now);
		s.system.timer += 15; s.IRC_clock_tick.call(p);
		check(p.quitting && !p.admitted, "missing READY held link open");
	},
	"legacy trusted hubs remain linkable during the rolling upgrade": function() {
		var s = new_state(), p = new_peer(s, "oldhub.test", true);
		p.admitted = false; p.their_capab = [];
		p.socket = {outbound: true, remote_ip_address: "127.0.0.1", remote_port: 1};
		p.synchronize = function() { this.bursts = 1; }; s.HCC_Counter = 0;
		p.finalize_server_connect("TS");
		check(p.admitted && p.bursts && !p.quitting && !p.clock_probe, "legacy hub rejected");
	},
	"legacy untrusted leaves remain linkable without extensions": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.admitted = false; p.their_capab = [];
		p.socket = {outbound: true, remote_ip_address: "127.0.0.1", remote_port: 1};
		p.synchronize = function() { this.bursts = 1; }; s.HCC_Counter = 0;
		p.finalize_server_connect("TS");
		check(!p.quitting && p.admitted && p.bursts && !p.clock_probe, "legacy leaf rejected");
		check(!/CLOCK |CSYNC |TSMAP /.test(p.sent.join("\n")), "extensions sent to legacy leaf");
	},
	"legacy burst preserves new exclusive channels but cannot override hub channels": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.their_capab = ["TSJOIN", "TSMODE"]; p.bursting = true;
		var u = new_user(s, "OldUser", p.nick), local = new_user(s, "Local");
		var existing = join_user(s, local, "#existing");
		existing.modelist[s.CHANMODE_OP][local.id] = local;
		p.work(":OldUser JOIN 1 #fresh");
		p.work(":oldleaf.test MODE #fresh 0 +o OldUser");
		p.work(":oldleaf.test MODE #fresh +mtkl key 25");
		p.work(":oldleaf.test MODE #fresh +b *!*@banned");
		p.work(":oldleaf.test MODE #fresh +o Local");
		p.work(":oldleaf.test TOPIC #fresh OldUser 2000000000 :burst topic");
		var fresh = s.Channels["#FRESH"];
		check(fresh.modelist[s.CHANMODE_OP][u.id] && fresh.arg[s.CHANMODE_KEY] == "key"
			&& fresh.arg[s.CHANMODE_LIMIT] == 25 && fresh.topic == "burst topic", "burst state lost");
		check(fresh.modelist[s.CHANMODE_BAN].length == 1, "burst ban lost");
		check(!fresh.modelist[s.CHANMODE_OP][local.id], "burst granted status to a nonmember on another route");
		p.work(":OldUser JOIN 1 #existing");
		p.work(":oldleaf.test MODE #existing 0 +o OldUser");
		p.work(":oldleaf.test MODE #existing 0 +b *!*@forged");
		p.work(":oldleaf.test TOPIC #existing OldUser 2000000000 :forged topic");
		check(!existing.modelist[s.CHANMODE_OP][u.id] && !existing.topic
			&& !existing.modelist[s.CHANMODE_BAN].length, "leaf overrode established channel");
		p.work("BURST 0");
		check(!p.quitting && !fresh.pending_burst, "burst completion failed");
		check(/MODE #existing 0 -b \*!\*@forged/.test(p.sent.join("\n")), "rejected ban not undone");
		check(!/CSYNC |TSMAP |CLOCK /.test(p.sent.join("\n")), "legacy correction used extensions");
		p.work(":oldleaf.test MODE #fresh 0 +s");
		check(!(fresh.mode & s.CHANMODE_SECRET), "burst privilege survived BURST 0");
	},
	"a local member permanently ends the leaf's exclusive burst exemption": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.their_capab = ["TSJOIN"]; p.bursting = true;
		new_user(s, "OldUser", p.nick); p.work(":OldUser JOIN 1 #fresh");
		var c = s.Channels["#FRESH"], local = new_user(s, "Local");
		join_user(s, local, "#fresh"); p.work(":oldleaf.test MODE #fresh 0 +s");
		check(!(c.mode & s.CHANMODE_SECRET) && !c.pending_burst, "exclusive exemption retained");
	},
	"legacy leaf corrections bypass its stale channel and topic timestamps": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.their_capab = ["TSJOIN", "TSMODE"];
		var u = new_user(s, "OldUser", p.nick), c = join_user(s, u, "#legacy");
		c.modelist[s.CHANMODE_OP][u.id] = u;
		c.topic = "canonical"; c.topicchangedby = "HubUser"; c.topictime = s.now;
		s.IRC_note_legacy_topic(p, c, s.now + 86400);
		s.IRC_resync_leaf(p, c);
		check(p.sent.indexOf(":hub.test MODE #legacy 0 -l") >= 0
			&& p.sent.indexOf(":hub.test MODE #legacy 0 +o OldUser") >= 0, "MODE not unconditional");
		check(p.sent.join("\n").indexOf(" " + (s.now + 86401) + " :canonical") >= 0,
			"old leaf rejected newer topic correction");
		check(c.topictime == s.now && c.created == s.now, "presentation changed network timestamps");
	},
	"legacy leaf JOINs omit timestamps and limit removals preserve nickname arguments": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.their_capab = ["TSJOIN", "TSMODE"];
		var local = new_user(s, "Local"), c = join_user(s, local, "#legacy");
		s.IRC_bcast_join(local, c, c.created);
		s.IRC_bcast_chanmode(local, c, "-l+ov Local Local", c.created);
		check(p.sent[0] == ":Local JOIN #legacy", "JOIN can reset drifting leaf status");
		check(p.sent[1] == ":Local MODE #legacy 0 -l"
			&& p.sent[2] == ":Local MODE #legacy 0 -+ov Local Local", "legacy -l misaligns nickname arguments");
	},
	"legacy leaf correction does not resurrect its departed users": function() {
		var s = new_state(), p = new_peer(s, "oldleaf.test", false);
		p.their_capab = ["TSJOIN", "TSMODE"];
		var u = new_user(s, "OldUser", p.nick), local = new_user(s, "Local");
		var c = join_user(s, u, "#legacy"); join_user(s, local, "#legacy");
		s.IRC_resync_leaf(p, c);
		check(!/:OldUser JOIN /.test(p.sent.join("\n")), "own user may have PART in flight");
		check(/:Local JOIN #legacy/.test(p.sent.join("\n")), "missing hub user not repaired");
	},
	"clock handshake accepts either 30-second boundary before admitting state": function() {
		[-30, 30].forEach(function(skew) {
			var s = new_state(), p = new_peer(s, "peer.test", true);
			p.admitted = false; p.synchronize = function() { this.bursts = (this.bursts || 0) + 1; };
			s.IRC_clock_tick.call(p);
			p.work("CLOCK " + p.clock_probe.nonce + " " + (s.now + skew));
			check(!p.admitted && !p.bursts, "state admitted before reciprocal check");
			p.work("CLOCK READY");
			check(p.admitted && p.bursts == 1 && !p.quitting, "boundary rejected");
		});
	},
	"clock handshake refuses clocks beyond either boundary without a burst": function() {
		[-31, 31].forEach(function(skew) {
			var s = new_state(), p = new_peer(s, "peer.test", true);
			p.admitted = false; p.synchronize = function() { this.bursts = 1; };
			s.IRC_clock_tick.call(p);
			p.work("CLOCK " + p.clock_probe.nonce + " " + (s.now + skew));
			check(p.quitting && !p.bursts && !p.admitted, "bad clock admitted");
		});
	},
	"ongoing clock probe disconnects a link that starts drifting": function() {
		var s = new_state(), p = new_peer(s, "peer.test", true);
		p.clock_verified = true; p.last_clock_check = s.system.timer - 15;
		s.IRC_clock_tick.call(p);
		p.work("CLOCK " + p.clock_probe.nonce + " " + (s.now + 31));
		check(p.quitting && /Clock skew/.test(p.quit_reason), "drift did not close link");
	},
	"stale probes after a callback stall are retried, not treated as skew": function() {
		var s = new_state(), p = new_peer(s, "peer.test", true);
		s.IRC_clock_tick.call(p); var old_nonce = p.clock_probe.nonce;
		s.system.timer += 120; s.now += 120; s.Stall_Generation++;
		s.IRC_clock_tick.call(p);
		check(!p.quitting && p.clock_probe.nonce != old_nonce, "stall closed link or reused probe");
		p.work("CLOCK " + old_nonce + " " + (s.now - 120));
		check(!p.quitting && !!p.clock_probe, "stale reply used");
	},
	"long RTT is retried and an unanswered clock probe times out": function() {
		var s = new_state(), p = new_peer(s, "peer.test", true);
		s.IRC_clock_tick.call(p); var old_nonce = p.clock_probe.nonce;
		s.system.timer += 3; s.now += 3;
		p.work("CLOCK " + old_nonce + " " + (s.now - 3));
		check(!p.quitting && p.clock_probe.nonce != old_nonce, "lag mistaken for clock skew");
		s.system.timer += 15; s.now += 15; s.IRC_clock_tick.call(p);
		check(p.quitting && /timed out/.test(p.quit_reason), "unanswered probe kept link alive");
	},
	"legacy services bypass new clock capabilities and admission gating": function() {
		var s = new_state(), p = new_peer(s, "services.test", false);
		p.uline = true; p.admitted = false; p.their_capab = [];
		p.socket = {outbound: true, remote_ip_address: "127.0.0.1", remote_port: 1};
		p.synchronize = function() { this.bursts = 1; }; s.HCC_Counter = 0;
		p.finalize_server_connect("TS");
		check(p.admitted && p.bursts == 1 && !p.clock_probe && !p.quitting, "legacy services rejected");
	},
	"backdated leaf SJOIN cannot replace hub modes or steal ops": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), local = new_user(s, "Local");
		var attacker = new_user(s, "Attacker", p.nick), c = join_user(s, local, "#test", s.now - 100);
		c.mode |= s.CHANMODE_SECRET; c.modelist[s.CHANMODE_OP][local.id] = local;
		p.work(":leaf.test SJOIN 1 #test +m :@+Attacker");
		check(c.created == s.now - 100 && c.mode == (s.CHANMODE_NONE | s.CHANMODE_SECRET), "hub state replaced");
		check(!!c.modelist[s.CHANMODE_OP][local.id] && !c.modelist[s.CHANMODE_OP][attacker.id]
			&& !c.modelist[s.CHANMODE_VOICE][attacker.id], "attacker gained status");
		check(p.sent.some(function(l) { return l.indexOf(" CSYNC ") >= 0; }), "leaf correction missing");
	},
	"server-prefixed leaf MODE cannot bypass existing channel authority": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), local = new_user(s, "Local");
		var u = new_user(s, "Alice", p.nick), c = join_user(s, local, "#test"); join_user(s, u, "#test");
		p.work(":leaf.test MODE #test +o Alice");
		check(!c.modelist[s.CHANMODE_OP][u.id], "leaf server stole ops");
	},
	"new leaf channel gets one hub timestamp and only one founder grant": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), u = new_user(s, "Alice", p.nick);
		p.work(":Alice JOIN 1 #new"); var c = s.Channels["#NEW"];
		check(c.created == s.now, "leaf backdated a new network channel");
		p.work(":leaf.test MODE #new +o Alice");
		check(!!c.modelist[s.CHANMODE_OP][u.id], "founder op rejected");
		p.work(":Alice MODE #new -o Alice");
		p.work(":leaf.test MODE #new +o Alice");
		check(!c.modelist[s.CHANMODE_OP][u.id], "founder grant replayed");
	},
	"invalid or foreign SJOIN members cannot mutate a channel": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), local = new_user(s, "Local");
		var c = join_user(s, local, "#test"); c.modelist[s.CHANMODE_OP][local.id] = local;
		p.work(":leaf.test SJOIN 1 #test +m :@Local @Missing");
		p.work(":leaf.test SJOIN 1 #empty +m :");
		check(c.created == s.now && !(c.mode & s.CHANMODE_MODERATED), "invalid members changed state");
		check(!s.Channels["#EMPTY"], "empty channel retained");
	},
	"only configured direct hubs can apply an authoritative CSYNC": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), local = new_user(s, "Local");
		var c = join_user(s, local, "#test"); c.modelist[s.CHANMODE_OP][local.id] = local;
		["BEGIN +m", "MORE :Local", "END"].forEach(function(l) { p.work(":leaf.test CSYNC 1 #test " + l); });
		check(c.created == s.now && !!c.modelist[s.CHANMODE_OP][local.id], "untrusted correction accepted");
		p.hub = true;
		["BEGIN +m", "MORE :+Local", "END"].forEach(function(l) { p.work(":leaf.test CSYNC " + (s.now + 10) + " #test " + l); });
		check(c.created == s.now + 10 && !(c.modelist[s.CHANMODE_OP][local.id])
			&& !!c.modelist[s.CHANMODE_VOICE][local.id] && !!(c.mode & s.CHANMODE_MODERATED), "hub correction not applied");
	},
	"equal-TS split snapshots converge on keys, limits, secret mode and ops": function() {
		var m = merge_sides(1799999900, 1799999900);
		check(m.ca.chanmode(true) == m.cb.chanmode(true), "channel flags/args differ");
		check(m.ca.chanmode(true) == "+kls keyB 20", "unexpected merge rule");
		check(m.ca.occupants().split(" ").sort().join(" ") == "@Alice @Bob", "side A ops wrong");
		check(m.cb.occupants().split(" ").sort().join(" ") == "@Alice @Bob", "side B ops wrong");
	},
	"older snapshot wins identically on each side and notifies losing clients": function() {
		var m = merge_sides(1799999800, 1799999900);
		check(m.ca.created == m.cb.created && m.ca.created == 1799999800, "TS differs");
		check(m.ca.chanmode(true) == m.cb.chanmode(true), "losing flags survived");
		check(m.cb.occupants().split(" ").sort().join(" ") == "@Alice Bob", "losing op survived");
		check(m.bob_b.received.some(function(l) { return / MODE #merge -.*o/.test(l); }), "client not deopped");
	},
	"timestamp reset in JOIN sends local mode removals": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", true);
		var local = new_user(s, "Local"), remote = new_user(s, "Remote", p.nick);
		var c = join_user(s, local, "#test");
		c.mode |= s.CHANMODE_MODERATED; c.modelist[s.CHANMODE_VOICE][local.id] = local;
		p.work(":Remote JOIN 1799999900 #test");
		check(!(c.mode & s.CHANMODE_MODERATED) && !c.modelist[s.CHANMODE_VOICE][local.id], "state not reset");
		check(local.received.some(function(l) { return l.indexOf("MODE #test -mv Local") >= 0; }), "removals missing");
	},
	"burst flags and member status retain their timestamp through another hop": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", true), q = new_peer(s, "other.test", true);
		var local = new_user(s, "Local"), remote = new_user(s, "Alice", p.nick);
		var c = join_user(s, local, "#test", 1799999800);
		c.modelist[s.CHANMODE_OP][local.id] = local;
		p.work(":leaf.test SJOIN 1799999900 #test +m :@Alice");
		check(!(c.mode & s.CHANMODE_MODERATED) && !c.modelist[s.CHANMODE_OP][remote.id], "newer state applied");
		check(q.sent.indexOf(":leaf.test SJOIN 1799999900 #test +m :@Alice") >= 0, "original burst rewritten");
	},
	"secret state stays hidden until a leaf joins, then is synchronized": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), u = new_user(s, "Alice");
		var c = join_user(s, u, "#hidden"); c.mode |= s.CHANMODE_SECRET;
		c.topic = "private topic"; c.topictime = s.now;
		p.server_chan_info(c);
		check(p.sent.length == 0, "secret channel disclosed before a member joined");
		p.synchronize();
		check(p.sent.join("\n").indexOf("#hidden") < 0, "topic burst disclosed a hidden channel");
		p.sent = [];
		var remote = new_user(s, "Bob", p.nick);
		p.work(":Bob JOIN #hidden");
		check(p.sent.some(function(l) { return l.indexOf(" CSYNC ") >= 0; }), "missing authoritative catch-up");
		check(p.sent.some(function(l) { return l.indexOf("MORE :Alice Bob") >= 0; }), "catch-up missing members");
	},
	"single-user SJOIN relays the user with its original timestamp": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", true), q = new_peer(s, "other.test", true);
		new_user(s, "Alice", p.nick);
		p.work(":Alice SJOIN 1799999900 #test");
		check(q.sent.indexOf(":Alice JOIN 1799999900 #test") >= 0, "user or TS lost in relay");
	},
	"combined SJOIN statuses supply both nickname arguments": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", true), local = new_user(s, "Local");
		new_user(s, "Alice", p.nick); join_user(s, local, "#test");
		p.work(":leaf.test SJOIN " + s.now + " #test + :@+Alice");
		check(local.received.indexOf(":leaf.test MODE #test +ov Alice Alice") >= 0, "malformed +ov");
	},
	"-l takes no argument and leaves following nick arguments aligned": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), u = new_user(s, "Alice", p.nick);
		var c = join_user(s, u, "#test"); c.mode |= s.CHANMODE_LIMIT; c.arg[s.CHANMODE_LIMIT] = 10;
		c.modelist[s.CHANMODE_OP][u.id] = u;
		p.set_chanmode(c, ["-lo", "Alice"]);
		check(!(c.mode & s.CHANMODE_LIMIT) && !c.modelist[s.CHANMODE_OP][u.id], "-lo misparsed");
	},
	"NAMES and SJOIN bursts fit wire length and contain every nick": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), observer = new_user(s, "Observer");
		var c = join_user(s, observer, "#" + new Array(100).join("x")), expected = [];
		for (var i = 0; i < 80; i++) {
			var nick = "Nick" + ("000" + i).slice(-3) + "abcdefghijklmnopqrstuvw";
			var u = new_user(s, nick); join_user(s, u, c.nam); expected.push(nick);
		}
		observer.names(c); p.server_chan_info(c);
		observer.received.concat(p.sent).forEach(function(l) { check(l.length + 2 <= 512, "oversized line: " + l.length); });
		expected.forEach(function(n) {
			check(observer.received.join(" ").indexOf(n) >= 0 && p.sent.join(" ").indexOf(n) >= 0, "missing nick " + n);
		});
	},
	"legacy services retain JOIN/MODE bursts and authoritative timestamped ops": function() {
		var s = new_state(), p = new_peer(s, "services.test", false), local = new_user(s, "Local");
		p.uline = true; p.their_capab = [];
		var c = join_user(s, local, "#test"); c.modelist[s.CHANMODE_OP][local.id] = local;
		p.server_chan_info(c);
		check(p.sent.indexOf(":Local JOIN #test") >= 0, "legacy JOIN missing");
		check(p.sent.indexOf(":hub.test MODE #test +o Local") >= 0, "legacy MODE missing");
		var remote = new_user(s, "ChanServ", p.nick); remote.uline = true; join_user(s, remote, "#test");
		p.work(":ChanServ MODE #test " + (s.now + 20) + " +v Local");
		check(!!c.modelist[s.CHANMODE_VOICE][local.id] && !!c.modelist[s.CHANMODE_OP][local.id], "services authority lost");
	},
	"sequential same-second topic changes advance timestamps and clear topics": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false), u = new_user(s, "Alice");
		var c = join_user(s, u, "#test");
		u.work("TOPIC #test :first"); u.work("TOPIC #test :second");
		check(c.topic == "second" && c.topictime == s.now + 1, "topic timestamp not advanced");
		u.work("TOPIC #test :");
		check(c.topic === "" && c.topictime == s.now + 2, "topic clear ignored");
		p.synchronize();
		check(p.sent.some(function(l) { return l.indexOf("TOPIC #test Alice " + (s.now + 2) + " :") >= 0; }), "clear tombstone omitted from burst");
	},
	"equal timestamp topics converge regardless of arrival order": function() {
		function side(reverse) {
			var s = new_state(), p = new_peer(s, "leaf.test", true), local = new_user(s, "Local");
			join_user(s, local, "#test");
			var lines = [":leaf.test TOPIC #test Alice " + s.now + " :a", ":leaf.test TOPIC #test Bob " + s.now + " :b"];
			if (reverse) lines.reverse();
			lines.forEach(function(l) { p.work(l); });
			return s.Channels["#TEST"].topic;
		}
		check(side(false) === "b" && side(true) === "b", "topic tie differs by arrival order");
	},
	"trusted timestamps survive clock differences and JOIN relays": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", true);
		var q = new_peer(s, "other.test", true);
		new_user(s, "Alice", p.nick);
		p.work(":Alice JOIN 1799999900 #old");
		check(s.Channels["#OLD"].created === 1799999900, "leaf TS replaced");
		check(q.sent.indexOf(":Alice JOIN 1799999900 #old") >= 0, "relay lost TS");
		q.their_capab = [];
		p.work(":Alice JOIN 1799999900 #legacy");
		check(q.sent.indexOf(":Alice JOIN #legacy") >= 0, "legacy JOIN format changed");
	},
	"malformed SJOIN timestamp cannot create or modify a channel": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false);
		new_user(s, "Alice", p.nick);
		p.work(":leaf.test SJOIN 1800000000oops #bad + :@Alice");
		check(p.quitting && !s.Channels["#BAD"], "malformed TS accepted");
	},
	"leaf founder op, following modes, and KICK propagate": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false);
		var u = new_user(s, "Founder", p.nick);
		p.work(":Founder JOIN #new");
		var c = s.Channels["#NEW"];
		p.work(":leaf.test MODE #new +o Founder");
		check(!!c.modelist[s.CHANMODE_OP][u.id], "founder op rejected");
		p.work(":Founder MODE #new +m");
		check(!!(c.mode & s.CHANMODE_MODERATED), "founder MODE rejected");
		var victim = new_user(s, "Victim", p.nick);
		join_user(s, victim, "#new");
		p.work(":Founder KICK #new Victim :test");
		check(!c.users[victim.id] && !victim.channels["#NEW"], "KICK did not remove membership");
	},
	"non-op leaf users cannot MODE or KICK": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false);
		var u = new_user(s, "Alice", p.nick), victim = new_user(s, "Bob", p.nick);
		var c = join_user(s, u, "#test");
		join_user(s, victim, "#test");
		p.work(":Alice MODE #test +m");
		p.work(":Alice KICK #test Bob :test");
		check(!(c.mode & s.CHANMODE_MODERATED) && !!c.users[victim.id], "non-op request accepted");
	},
	"leaf cannot forge another link's server or opped user": function() {
		var s = new_state(), p = new_peer(s, "leaf.test", false);
		var q = new_peer(s, "other.test", true);
		var u = new_user(s, "Alice", q.nick), victim = new_user(s, "Bob", p.nick);
		var c = join_user(s, u, "#test");
		join_user(s, victim, "#test");
		c.modelist[s.CHANMODE_OP][u.id] = u;
		p.work(":other.test MODE #test +m");
		p.work(":Alice MODE #test +m");
		p.work(":Alice KICK #test Bob :test");
		check(!(c.mode & s.CHANMODE_MODERATED) && !!c.users[victim.id], "forged origin accepted");
	},
	"services MODE with TS zero remains authoritative": function() {
		var s = new_state(), p = new_peer(s, "services.test", false);
		p.uline = true;
		var u = new_user(s, "Alice", p.nick), c = join_user(s, u, "#test");
		p.work(":services.test MODE #test 0 +o Alice");
		check(!!c.modelist[s.CHANMODE_OP][u.id] && c.created == s.now, "services TS zero broken");
	}
});
