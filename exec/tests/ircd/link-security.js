/* Live-edge security reports, trust boundaries, and TLSROUTE regressions. */
"use strict";
load(js.exec_dir + "lib.sjs");

function check(value, message) { if (!value) throw new Error(message || "Assertion failed"); }
function state() {
	var s = {system:{timer:1000}, ServerName:"local.test", ServerDesc:"Tests", server:{},
		Users:{}, Servers:{}, Channels:{}, Local_Users:{}, Local_Servers:{}, Unregistered:{},
		Assigned_IDs:{}, WhoWas:{}, WhoWasMap:[], NickHistory:[], ULines:[], QLines:[], Profile:{},
		MAX_NICKLEN:30, Enforcement:true, YLines:[{pingfreq:90, connfreq:0, active:0}],
		log:function() {}, js:{setImmediate:function() {}, clearInterval:function() {}},
		Generate_ID:function() { return 1; }};
	["irclib.js", "ircd/link-security.js", "ircd/core.js", "ircd/user.js", "ircd/channel.js",
	 "ircd/server.js", "ircd/config.js", "ircd/timestamps.js"].forEach(function(f) { load(s, f); });
	s.Epoch = function() { return 1800000000; };
	return s;
}
function peer(s, name, hub, caps) {
	var p = new s.IRC_Server();
	p.nick = p.id = p.parent = name; p.linkparent = s.ServerName;
	p.hub = !!hub; p.admitted = true; p.their_capab = caps || ["LINKSEC"];
	p.sent = []; p.rawout = function(line) { this.sent.push(line); };
	p.quit = function(reason) { this.quitting = true; this.reason = reason; };
	s.Servers[name] = s.Local_Servers[name] = p;
	return p;
}
function remote(s, name, parent, via) {
	var p = new s.IRC_Server();
	p.nick = p.id = name; p.linkparent = parent; p.parent = via; p.local = false;
	s.Servers[name] = p;
	return p;
}
function client(s) {
	var u = new s.IRC_User(1);
	u.nick = "Viewer"; u.lines = [];
	u.rawout = function(line) { this.lines.push(line); };
	u.ircout = function(line) { this.rawout(":" + s.ServerName + " " + line); };
	return u;
}
function query(s, name) {
	var u = client(s); u.work("TLSROUTE" + (name ? " " + name : ""));
	return u.lines.join("\n");
}
function receive(s, p, origin, neighbor, transport, direction, validation) {
	p.work(":" + origin + " LINKSEC " + neighbor + " " + transport + " " + direction + " " + validation);
}
function tls(s, p, outbound, verified) {
	p.socket = {ssl_session:true, outbound:outbound, irc_tls_verified:verified, close:function() {}};
	s.IRC_linksec_admit(p);
}

run_tests({
	"outbound socket validation is visible to ordinary registered clients": function() {
		var s = state(), p = peer(s, "hub.test", true);
		tls(s, p, true, true);
		var output = query(s, "HUB.TEST");
		check(output.indexOf("TLS; certificate validation VERIFIED by local.test") >= 0, output);
		check(output.indexOf("all network hops TLS; certificate validation verified") >= 0, output);
	},
	"configuration and incoming TLS do not imply certificate verification": function() {
		var s = state(), p = peer(s, "hub.test", true);
		p.tls = true;
		check(query(s, p.nick).indexOf("unknown; certificate validation unknown") >= 0);
		tls(s, p, false, true);
		check(query(s, p.nick).indexOf("all network hops TLS; certificate validation unknown") >= 0);
		receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		check(query(s, p.nick).indexOf("VERIFIED by hub.test") >= 0);
	},
	"outbound TLS without the successful validation marker is unverified": function() {
		var s = state(), p = peer(s, "hub.test", true);
		tls(s, p, true, false);
		check(query(s, p.nick).indexOf("all network hops TLS; certificate validation unverified") >= 0);
	},
	"multi-hop routes combine trusted endpoint observations and direction": function() {
		var s = state(), p = peer(s, "hub.test", true);
		remote(s, "far.test", p.nick, p.nick); tls(s, p, true, true);
		receive(s, p, "far.test", p.nick, "TLS", "OUT", "VERIFIED");
		receive(s, p, p.nick, "far.test", "TLS", "IN", "NA");
		var output = query(s, "far.test");
		check(output.indexOf("VERIFIED by far.test") >= 0 && output.indexOf("all network hops TLS; certificate validation verified") >= 0, output);
	},
	"plaintext anywhere dominates the route summary": function() {
		var s = state(), p = peer(s, "hub.test", true);
		remote(s, "far.test", p.nick, p.nick); tls(s, p, true, true);
		receive(s, p, p.nick, "far.test", "PLAIN", "IN", "NA");
		check(query(s, "far.test").indexOf("plaintext present; certificate validation not applicable") >= 0);
	},
	"legacy intermediaries leave remote links unknown without receiving LINKSEC": function() {
		var s = state(), old = peer(s, "old.test", true, ["TSJOIN"]), observer = peer(s, "observer.test", true);
		remote(s, "far.test", old.nick, old.nick); tls(s, old, true, true);
		s.IRC_linksec_burst(old);
		check(!old.sent.length, old.sent.join("\n"));
		check(observer.sent.join("\n").indexOf("LINKSEC old.test TLS OUT VERIFIED") >= 0);
		check(query(s, "far.test").indexOf("unknown; certificate validation unknown") >= 0);
		receive(s, old, old.nick, "far.test", "TLS", "OUT", "VERIFIED");
		check(!s.Servers["far.test"].link_security, "incapable peer provided metadata");
	},
	"untrusted leaves and U:Lines cannot vouch for security": function() {
		var s = state(), p = peer(s, "leaf.test", false); p.uline = true;
		tls(s, p, false, false);
		receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		check(query(s, p.nick).indexOf("all network hops TLS; certificate validation unknown") >= 0);
		receive(s, p, p.nick, s.ServerName, "PLAIN", "IN", "NA");
		check(!s.IRC_linksec_status(p).conflict, "leaf overrode local socket facts");
	},
	"spoofed prefixes, user origins, and nonadjacent claims are ignored": function() {
		var s = state(), p = peer(s, "hub.test", true), other = peer(s, "other.test", true);
		var far = remote(s, "far.test", p.nick, p.nick);
		receive(s, p, other.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		receive(s, p, far.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		var u = client(s); s.Users.VIEWER = u;
		receive(s, p, u.nick, p.nick, "TLS", "OUT", "VERIFIED");
		check(!other.link_security && !far.link_security && !p.link_security);
	},
	"malformed and inapplicable certificate assertions are ignored": function() {
		var s = state(), p = peer(s, "hub.test", true);
		["TLS IN VERIFIED", "TLS OUT NA", "PLAIN OUT VERIFIED", "INTERNAL NONE NA", "TLS NONE VERIFIED", "BOGUS OUT VERIFIED", "TLS OUT VERIFIED extra"].forEach(function(fields) {
			p.work(":" + p.nick + " LINKSEC " + s.ServerName + " " + fields);
		});
		check(!p.link_security, "invalid report was retained");
	},
	"contradictory directions and transport cannot yield a verified route": function() {
		var s = state(), p = peer(s, "hub.test", true); tls(s, p, true, true);
		receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		check(query(s, p.nick).indexOf("conflicting reports") >= 0);
		check(query(s, p.nick).indexOf("END hub.test: unknown; certificate validation unknown") >= 0);
		receive(s, p, p.nick, s.ServerName, "PLAIN", "IN", "NA");
		check(query(s, p.nick).indexOf("plaintext present") >= 0);
	},
	"equivocation is bounded, replayed, and retains plaintext evidence": function() {
		var s = state(), p = peer(s, "hub.test", true), observer = peer(s, "observer.test", true);
		receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		receive(s, p, p.nick, s.ServerName, "TLS", "IN", "NA");
		receive(s, p, p.nick, s.ServerName, "PLAIN", "IN", "NA");
		for (var i = 0; i < 100; i++) receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "UNVERIFIED");
		observer.sent = []; s.IRC_linksec_burst(observer);
		check(observer.sent.length == 2 && observer.sent.join("\n").indexOf("PLAIN") >= 0);
		check(query(s, p.nick).indexOf("plaintext present") >= 0);
	},
	"snapshot follows topology introductions and avoids reflecting source prefixes": function() {
		var s = state(), p = peer(s, "hub.test", true), observer = peer(s, "observer.test", true);
		remote(s, "far.test", p.nick, p.nick); tls(s, p, true, true);
		receive(s, p, "far.test", p.nick, "TLS", "OUT", "VERIFIED");
		observer.sent = []; observer.synchronize();
		var wire = observer.sent.join("\n");
		check(wire.indexOf("SERVER far.test") < wire.indexOf(":far.test LINKSEC"), wire);
		p.sent = []; s.IRC_linksec_burst(p);
		check(p.sent.join("\n").indexOf(":far.test LINKSEC") < 0);
	},
	"admission announces the server before publishing socket observations": function() {
		var s = state(), observer = peer(s, "observer.test", true), p = peer(s, "hub.test", true);
		p.admitted = false; p.socket = {ssl_session:true, outbound:true, irc_tls_verified:true};
		s.IRC_admit_server(p);
		var wire = observer.sent.join("\n");
		check(wire.indexOf("SERVER hub.test") >= 0 && wire.indexOf("SERVER hub.test") < wire.indexOf("LINKSEC hub.test"), wire);
		check(p.sent.join("\n").indexOf("LINKSEC hub.test TLS OUT VERIFIED") >= 0);
	},
	"netsplit and same-name reconnect discard every old observation": function() {
		var s = state(), p = peer(s, "hub.test", true);
		var far = remote(s, "far.test", p.nick, p.nick); tls(s, p, true, true);
		receive(s, p, far.nick, p.nick, "TLS", "OUT", "VERIFIED");
		s.Server_Quit.call(p, "Split", true);
		check(!s.Servers[p.nick] && !s.Servers[far.nick]);
		p = peer(s, "hub.test", true); p.socket = {ssl_session:false, outbound:false, close:function() {}}; s.IRC_linksec_admit(p);
		remote(s, "far.test", p.nick, p.nick);
		check(query(s, p.nick).indexOf("plaintext present") >= 0 && !s.Servers[far.nick].link_security);
		s.Server_Quit.call(p, "Split again", true);
		p = peer(s, "hub.test", true); tls(s, p, true, true);
		check(query(s, p.nick).indexOf("all network hops TLS; certificate validation verified") >= 0);
	},
	"embedded service edges are explicitly internal and have no physical TLS hop": function() {
		var s = state(), observer = peer(s, "observer.test", true);
		var service = remote(s, "services.test", s.ServerName, 0); service.uline = true;
		check(query(s, service.nick).indexOf("UNKNOWN") >= 0, "missing socket inferred internal");
		s.IRC_linksec_internal(service);
		check(query(s, service.nick).indexOf("INTERNAL; certificate validation NA") >= 0);
		check(query(s, service.nick).indexOf("no network hops; certificate validation not applicable") >= 0);
		delete s.Servers[service.nick];
		service = remote(s, "services.test", s.ServerName, 0);
		check(!service.link_security);
		check(observer.sent.join("\n").indexOf("LINKSEC services.test INTERNAL NONE NA") >= 0);
	},
	"trusted remote internal services do not degrade an otherwise verified TLS path": function() {
		var s = state(), p = peer(s, "hub.test", true); tls(s, p, true, true);
		remote(s, "services.test", p.nick, p.nick);
		receive(s, p, p.nick, "services.test", "INTERNAL", "NONE", "NA");
		check(query(s, "services.test").indexOf("all network hops TLS; certificate validation verified") >= 0);
	},
	"embedded service introduction and shutdown publish and retire their internal edge": function() {
		var s = state(), observer = peer(s, "observer.test", true);
		var callbacks = s.js; delete s.js;
		load(s, "ircd/services.js");
		s.js = callbacks;
		var manager = Object.create(s.IRC_Services_Manager.prototype);
		manager.config = {enabled:true, host:"services.test"}; manager.localhost = true;
		manager.queue = {write:function() { return true; }};
		manager.snapshot = function() {};
		manager.fake_user = function(nick) {
			var bot = {nick:nick, quit:function() { delete s.Users[nick.toUpperCase()]; }};
			s.Users[nick.toUpperCase()] = bot;
			return bot;
		};
		manager.introduce();
		var wire = observer.sent.join("\n");
		check(wire.indexOf("SERVER services.test") >= 0 && wire.indexOf("SERVER services.test") < wire.indexOf("LINKSEC services.test INTERNAL"), wire);
		check(query(s, "services.test").indexOf("no network hops") >= 0);
		manager.stop_central();
		check(!s.Servers["services.test"] && !manager.service_server);
		observer.sent = []; s.IRC_linksec_burst(observer);
		check(!observer.sent.length, "retired services observation survived shutdown");
		manager.introduce();
		check(query(s, "services.test").indexOf("no network hops") >= 0);
	},
	"missing, unknown, self, and broken topology queries terminate clearly": function() {
		var s = state();
		check(query(s).indexOf(" 461 Viewer TLSROUTE ") >= 0);
		check(query(s, "missing.test").indexOf(" 402 Viewer missing.test ") >= 0);
		check(query(s, "constructor").indexOf(" 402 Viewer constructor ") >= 0);
		check(query(s, s.ServerName).indexOf("no network hops; certificate validation not applicable") >= 0);
		var a = remote(s, "a.test", "b.test", "b.test"), b = remote(s, "b.test", "a.test", "a.test");
		check(query(s, a.nick).indexOf("END a.test: unknown; certificate validation unknown") >= 0);
		b.linkparent = "missing.test";
		check(query(s, a.nick).indexOf("END a.test: unknown; certificate validation unknown") >= 0);
	},
	"pre-admission peers cannot query or send security state": function() {
		var s = state(), p = peer(s, "hub.test", true); p.admitted = false;
		s.IRC_linksec_burst(p); check(!p.sent.length);
		receive(s, p, p.nick, s.ServerName, "TLS", "OUT", "VERIFIED");
		check(p.quitting && !p.link_security);
	}
});
