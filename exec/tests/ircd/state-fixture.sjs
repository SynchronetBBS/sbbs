/* Shared in-memory IRCd network fixtures. */
"use strict";
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
		MAX_TOPICLEN: 307, MAX_WHO: 100, Enforcement: true, YLines: [{pingfreq: 90}],
		log: function() {},
		js: {setImmediate: function() {}, setTimeout: function() {}, clearInterval: function() {}},
		Generate_ID: function() { return ++next_id; }
	};
	["irclib.js", "ircd/exceptions.js", "ircd/link-security.js", "ircd/client-security.js", "ircd/core.js", "ircd/user.js", "ircd/channel.js",
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
	p.their_capab = ["TSJOIN", "TSMODE", "CSYNC", "FULLCHAN"];
	p.sent = [];
	p.rawout = function(line) { if (s.IRC_filter_leaf_output(this, line)) this.sent.push(line); };
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

function channel_sync(peer, ts, name, modes, members, bans) {
	var header = "CSYNC " + ts + " " + name + " ";
	peer.work(header + "BEGIN " + modes);
	if (members) peer.work(header + "MORE :" + members);
	if (bans) peer.work(header + "BANS :" + bans);
	peer.work(header + "END");
}

function mode_changes(s, user) {
	var changes = [];
	user.received.forEach(function(line) {
		var cmd = s.IRC_parse(line), add = true, arg = 2;
		if (cmd.verb != "MODE") return;
		for (var i = 0; i < cmd.params[1].length; i++) {
			var mode = cmd.params[1][i];
			if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
			changes.push((add ? "+" : "-") + mode
				+ ("bekov".indexOf(mode) >= 0 || (mode == "l" && add) ? ":" + cmd.params[arg++] : ""));
		}
	});
	return changes.sort().join(" ");
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
