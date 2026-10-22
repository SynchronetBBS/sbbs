/* Actual old/new processes, including both directions of hub/leaf links.
 * Supply --legacy-source=/path/to/previous/exec/ (including its load tree).
 * The fixture is external so this test never silently substitutes new code.
 */
"use strict";
load(js.exec_dir + "lib.sjs");

var legacy_source = "";
argv.forEach(function(arg) {
	if (arg.indexOf("--legacy-source=") == 0) legacy_source = backslash(arg.substr(16));
});
if (!legacy_source) {
	writeln("SKIP: rolling upgrade test requires --legacy-source=/previous/exec/");
	exit(0);
}
if (!file_exists(legacy_source + "ircd.js")) throw new Error("Missing legacy IRCd fixture");

var net = new IrcdTestNetwork(), clients = [];
var hub = {name: "hub1", sname: "newhub.ircd.local", port: 19760, capab_enforce: true};
var oldhub = {name: "hub2", sname: "oldhub.ircd.local", port: 19761, source_dir: legacy_source};
var oldleaf = {name: "leaf1a", sname: "oldleaf.ircd.local", port: 19762, source_dir: legacy_source};
var leaf = {name: "leaf2a", sname: "newleaf.ircd.local", port: 19763};
var nodes = [hub, oldhub, oldleaf, leaf], users, founder, oper;

function check(cond, message) { if (!cond) throw new Error(message); }
function client(node, nick) {
	var c = net.client(node, nick); clients.push(c); return c;
}
function names(c, channel) {
	c.drain(100); c.send("NAMES " + channel); return c.expect(" 353 ", 5).split(" :")[1];
}
function mode(c, channel) {
	c.drain(100); c.send("MODE " + channel); return c.expect(" 324 ", 5);
}
function settle() { mswait(500); clients.forEach(function(c) { c.drain(100); }); }

try {
	/* Seed channel state before the old leaf can reach the new hub. */
	net._start_one(oldleaf, [{sname: hub.sname, port: hub.port, is_hub: true}]);
	founder = client(oldleaf, "oldFounder"); founder.join("#oldburst");
	founder.send("MODE #oldburst +mnt"); founder.expect("+mnt", 5);
	founder.send("MODE #oldburst +kl burstkey 25"); founder.expect("+kl", 5);
	founder.send("MODE #oldburst +b *!*@blocked.invalid"); founder.expect("+b", 5);
	founder.send("TOPIC #oldburst :old burst topic"); founder.expect("TOPIC #oldburst :old burst topic", 5);
	net._start_one(hub, [{sname: oldleaf.sname, is_hub: false}, {sname: oldhub.sname, is_hub: true}]);
	net._start_one(oldhub, [{sname: hub.sname, port: hub.port, is_hub: true},
		{sname: leaf.sname, is_hub: false}]);
	net._start_one(leaf, [{sname: oldhub.sname, port: oldhub.port, is_hub: true}]);
	check(net._wait_links(hub.port, 4, 20), "mixed network did not link");
	users = nodes.map(function(n, i) { return client(n, "rollUser" + i); });
	oper = users[0]; oper.send("OPER testop testpass"); oper.expect(" 381 ", 5);
	settle();
	run_tests({
		"legacy leaf burst retains modes, founder, bans and topic throughout the mixed network": function() {
			users.forEach(function(c, i) {
				c.send("JOIN #oldburst burstkey"); c.expect(" 366 ", 5);
				check(names(c, "#oldburst").indexOf("@oldFounder") >= 0, "founder lost on " + nodes[i].sname);
				var modes = mode(c, "#oldburst");
				check(/burstkey 25|25 burstkey/.test(modes) && /m/.test(modes.split(" #oldburst ")[1]), "burst modes lost: " + modes);
				c.send("MODE #oldburst +b"); check(c.expect(" 367 ", 5).indexOf("blocked.invalid") >= 0, "ban lost");
				c.send("TOPIC #oldburst"); check(c.expect(" 332 ", 5).indexOf("old burst topic") >= 0, "topic lost");
			});
		},
		"new and old leaves create channels with working founder ops, MODE and messages": function() {
			[2, 3].forEach(function(index) {
				var channel = "#runtime" + index, owner = users[index];
				owner.join(channel); settle();
				users.forEach(function(c) { if (c !== owner) c.join(channel); }); settle();
				owner.send("MODE " + channel + " +mt");
				users[0].expect("MODE " + channel + " +mt", 5); settle();
				users.forEach(function(c) {
					check(names(c, channel).indexOf("@" + owner.nick) >= 0, "runtime founder lost");
					check(/m/.test(mode(c, channel).split(" " + channel + " ")[1]), "runtime mode lost");
				});
				owner.send("PRIVMSG " + channel + " :mixed message");
				users[0].expect("PRIVMSG " + channel + " :mixed message", 5);
			});
		},
		"hub topics reach an old leaf after successive changes": function() {
			users[2].send("TOPIC #runtime2 :from old leaf"); users[0].expect("TOPIC #runtime2 :from old leaf", 5);
			users[2].send("MODE #runtime2 +o " + users[0].nick); users[0].expect("+o " + users[0].nick, 5);
			users[0].send("TOPIC #runtime2 :hub update one"); users[2].expect("TOPIC #runtime2 :hub update one", 5);
			users[0].send("TOPIC #runtime2 :hub update two"); users[2].expect("TOPIC #runtime2 :hub update two", 5);
		},
		"KICK and PART remove presence across old/new hub and leaf links": function() {
			users[3].send("KICK #runtime3 " + users[2].nick + " :mixed kick");
			users[2].expect("KICK #runtime3 " + users[2].nick, 5); settle();
			users.forEach(function(c) { check(names(c, "#runtime3").indexOf(users[2].nick) < 0, "kicked user retained"); });
			users[2].send("PART #runtime2"); users[2].expect("PART #runtime2", 5); settle();
			users.forEach(function(c) { check(names(c, "#runtime2").indexOf(users[2].nick) < 0, "parted user retained"); });
		},
		"rapid legacy JOIN/PART cannot be undone by hub corrections": function() {
			oper.join("#oldpartrace");
			for (var i = 0; i < 10; i++) {
				users[2].send("JOIN #oldpartrace"); users[2].send("PART #oldpartrace");
			}
			/* Older generations throttle this burst more slowly than 2.3.
			   Wait for its final command before inspecting channel state. */
			users[2].send("PING :oldpartbarrier"); users[2].expect("oldpartbarrier", 45);
			settle();
			users.forEach(function(c, index) {
				var list = names(c, "#oldpartrace");
				check(list.indexOf(users[2].nick) < 0, "PART was undone on " + nodes[index].sname + ": " + list);
			});
		},
		"combined limit removal and status change reaches legacy peers": function() {
			oper.join("#oldlimit"); users[2].join("#oldlimit"); users[3].join("#oldlimit");
			oper.send("MODE #oldlimit +l 10"); oper.expect("+l 10", 5); settle();
			oper.send("MODE #oldlimit -l+v " + users[2].nick); oper.expect("+v", 5); settle();
			users.forEach(function(c) {
				check(names(c, "#oldlimit").indexOf("+" + users[2].nick) >= 0, "voice argument consumed by -l");
				check(!/l/.test(mode(c, "#oldlimit").split(" #oldlimit ")[1]), "limit not removed");
			});
		},
		"legacy leaf reconnect catches up secret channel state under hub authority": function() {
			oper.join("#secret-roll"); oper.send("MODE #secret-roll +smnt"); oper.expect(" MODE ", 5);
			oper.send("MODE #secret-roll +b *!*@denied.invalid"); oper.expect("+b", 5);
			oper.send("TOPIC #secret-roll :hub secret topic"); oper.expect("TOPIC #secret-roll", 5);
			users[2].join("#secret-roll"); settle();
			oper.send("SQUIT " + oldleaf.sname + " :rolling reconnect");
			mswait(1000);
			check(net._wait_links(hub.port, 4, 20), "old leaf did not reconnect"); settle();
			var list = names(users[2], "#secret-roll");
			check(list.indexOf("@" + oper.nick) >= 0 && list.indexOf("@" + users[2].nick) < 0, "hub authority lost: " + list);
			check(/s/.test(mode(users[2], "#secret-roll").split(" #secret-roll ")[1]), "secret mode lost");
			users[2].send("TOPIC #secret-roll"); check(users[2].expect(" 332 ", 5).indexOf("hub secret topic") >= 0, "topic lost");
			users[2].send("MODE #secret-roll +b"); check(users[2].expect(" 367 ", 5).indexOf("denied.invalid") >= 0, "ban lost");
		},
		"old and new hubs recover their link with both leaf generations still running": function() {
			oper.send("SQUIT " + oldhub.sname + " :mixed hub reconnect");
			mswait(1000);
			check(net._wait_links(hub.port, 4, 20), "old hub did not reconnect"); settle();
			users.forEach(function(c) {
				c.wait_for_nick(founder.nick);
				check(names(c, "#runtime3").indexOf("@" + users[3].nick) >= 0, "hub reconnect lost leaf founder");
				check(/m/.test(mode(c, "#runtime3").split(" #runtime3 ")[1]), "hub reconnect lost modes");
			});
		},
		"mixed network remains linked through a periodic clock check": function() {
			mswait(16000); check(net._wait_links(hub.port, 4, 5), "mixed network split later");
			users.forEach(function(c) { c.wait_for_nick(founder.nick); });
		}
	});
} finally {
	clients.forEach(function(c) { try { c.quit(); } catch(e) {} }); net.stop();
}
