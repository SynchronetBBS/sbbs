/* Trusted services SJOINs must cross hubs to users on other routes. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

var service_name = "services.synchro.net";

function remote_services(s, via) {
	var origin = new s.IRC_Server();
	origin.nick = origin.id = service_name;
	origin.parent = via.nick;
	origin.linkparent = "eotlbbs.synchro.net";
	origin.local = false;
	origin.uline = true;
	s.Servers[service_name] = origin;
	return origin;
}

function services_network() {
	var e = new_state(), h = new_state(), t = new_state();
	e.ServerName = "eotlbbs.synchro.net";
	h.ServerName = "hub.synchro.net";
	t.ServerName = "sestart.synchro.net";
	var service = new_peer(e, service_name, false);
	service.uline = true; service.their_capab = [];
	var eh = new_peer(e, h.ServerName, true), he = new_peer(h, e.ServerName, true);
	var ht = new_peer(h, t.ServerName, false), th = new_peer(t, h.ServerName, true);
	remote_services(h, he); remote_services(t, th);
	var owner_e = new_user(e, "Owner"), owner_h = new_user(h, "Owner", he.nick), owner_t = new_user(t, "Owner", th.nick);
	var user_e = new_user(e, "wkitty42", eh.nick), user_h = new_user(h, "wkitty42", ht.nick), user_t = new_user(t, "wkitty42");
	var nodes = [
		{s:e, input:service, output:eh, owner:owner_e, user:user_e},
		{s:h, input:he, output:ht, owner:owner_h, user:user_h},
		{s:t, input:th, owner:owner_t, user:user_t}
	];
	nodes.forEach(function(node) {
		node.channel = join_user(node.s, node.owner, "#service");
		node.channel.modelist[node.s.CHANMODE_OP][node.owner.id] = node.owner;
		node.logs = [];
		node.s.log = function(level, text) { node.logs.push(text); };
	});
	return nodes;
}

run_tests({
	"services SJOINs cross eotlbbs and another hub to a user local on sestart": function() {
		[0, 1800000000 - 20, 1800000000 + 20].forEach(function(ts) {
			var nodes = services_network();
			var wire = ":" + service_name + " SJOIN " + ts + " #service +mtk servicekey :@+wkitty42";
			nodes.forEach(function(node) {
				node.input.work(wire);
				var s = node.s, c = node.channel;
				check(c.users[node.user.id] === node.user && node.user.channels["#SERVICE"] === c,
					"Services membership did not reach " + s.ServerName);
				check(c.modelist[s.CHANMODE_OP][node.user.id] && c.modelist[s.CHANMODE_VOICE][node.user.id],
					"Services status did not reach " + s.ServerName);
				check((c.mode & s.CHANMODE_MODERATED) && (c.mode & s.CHANMODE_TOPIC) && c.arg[s.CHANMODE_KEY] == "servicekey",
					"Services channel modes did not reach " + s.ServerName);
				check(c.created == s.now && c.modelist[s.CHANMODE_OP][node.owner.id], "Services snapshot reset existing channel state");
				check(!node.logs.some(function(line) { return line.indexOf("Ignoring SJOIN member") >= 0; }), "Valid services member was rejected");
				if (node.output) {
					var forwarded = node.output.sent.filter(function(line) { return line.indexOf(" SJOIN ") >= 0; });
					check(forwarded.length == 1, "Services SJOIN stopped at " + s.ServerName);
					wire = forwarded[0];
					check(wire == ":" + service_name + " SJOIN " + (ts || s.now) + " #service +mtk servicekey :@+wkitty42",
						"Services source, timestamp, modes or member status was lost");
				}
			});
		});
	},
	"services snapshots can update existing members without duplicate joins": function() {
		var node = services_network()[0], s = node.s;
		join_user(s, node.user, "#service");
		node.owner.received = [];
		node.input.work(":" + service_name + " SJOIN 0 #service + :@wkitty42");
		check(node.channel.modelist[s.CHANMODE_OP][node.user.id]);
		check(!node.owner.received.some(function(line) { return line.indexOf(" JOIN ") >= 0; }), "Existing member joined twice");
		check(node.output.sent.some(function(line) { return line.indexOf(" :@wkitty42") >= 0; }), "Status update was not relayed");
	},
	"trusted services still reject unknown users and omit them from relays": function() {
		var node = services_network()[0], s = node.s;
		node.input.work(":" + service_name + " SJOIN 0 #service +m :@Missing");
		check(!(node.channel.mode & s.CHANMODE_MODERATED) && !node.output.sent.length, "Unknown member changed or relayed channel state");
		node.input.work(":" + service_name + " SJOIN 0 #service +m :@Missing @wkitty42");
		check(node.channel.modelist[s.CHANMODE_OP][node.user.id]);
		check(node.output.sent.some(function(line) { return / :@wkitty42$/.test(line); }), "Known services member was not relayed");
		check(!node.output.sent.some(function(line) { return line.indexOf("Missing") >= 0; }), "Unknown member escaped into the network");
	},
	"ordinary hubs and leaves cannot assert SJOIN members on other routes": function() {
		[false, true].forEach(function(hub) {
			var node = services_network()[0], s = node.s;
			node.input.uline = false; node.input.hub = hub;
			node.input.work(":" + service_name + " SJOIN " + s.now + " #service +m :@wkitty42 @Owner");
			check(!(node.channel.mode & s.CHANMODE_MODERATED) && !node.channel.users[node.user.id]);
			check(!node.output.sent.length, "An untrusted origin relayed another route's membership");
		});
	},
	"a trusted link does not grant services authority to an untrusted downstream origin": function() {
		var node = services_network()[0], s = node.s;
		var other = new s.IRC_Server();
		other.nick = other.id = "ordinary.test"; other.parent = node.input.nick; other.local = false;
		s.Servers[other.nick] = other;
		node.input.work(":ordinary.test SJOIN " + s.now + " #service +m :@wkitty42");
		check(!(node.channel.mode & s.CHANMODE_MODERATED) && !node.channel.users[node.user.id]);
		check(!node.output.sent.length, "Trusted transport promoted an untrusted SJOIN origin");
	},
	"a services prefix on the wrong link remains rejected": function() {
		var node = services_network()[1], s = node.s;
		node.output.work(":" + service_name + " SJOIN " + s.now + " #service +m :@wkitty42");
		check(!(node.channel.mode & s.CHANMODE_MODERATED) && !node.channel.users[node.user.id]);
		check(node.logs.some(function(line) { return line.indexOf("origin " + service_name + " is on another link") >= 0; }));
	}
});
