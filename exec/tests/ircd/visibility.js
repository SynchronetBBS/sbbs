/* Hidden channel bursts and client privacy with uninvolved leaf servers.
 * Optionally supply --legacy-source=/previous/exec/ to run one real old leaf.
 */
"use strict";
load(js.exec_dir + "lib.sjs");

var legacy_source = "";
argv.forEach(function(arg) {
	if (arg.indexOf("--legacy-source=") == 0) legacy_source = backslash(arg.substr(16));
});
if (legacy_source && !file_exists(legacy_source + "ircd.js"))
	throw new Error("Missing legacy IRCd fixture");

var net = new IrcdTestNetwork(), clients = [];
var a = {name: "hub1", sname: "visibilitya.ircd.local", port: 19780};
var b = {name: "hub2", sname: "visibilityb.ircd.local", port: 19781};
var x = {name: "leaf1a", sname: "visibilityx.ircd.local", port: 19782};
var y = {name: "leaf2a", sname: "visibilityy.ircd.local", port: 19783};
if (legacy_source) y.source_dir = legacy_source;
var owner, guest, outsider, probes, channels = ["#secret-visible", "#private-visible"];

function check(cond, message) { if (!cond) throw new Error(message); }
function client(node, nick) {
	var c = net.client(node, nick); clients.push(c); return c;
}
function query(c, command, end, seconds, keep_pending) {
	if (!keep_pending) c.drain(100);
	c.send(command);
	var lines = [], until = Date.now() + (seconds || 8) * 1000;
	while (Date.now() < until) {
		var line = c.sock.recvline(512, 1);
		if (!line) continue;
		line = line.replace(/\r?\n$/, "");
		if (line.indexOf("PING ") == 0) { c.send("PONG " + line.substr(5)); continue; }
		lines.push(line);
		if (line.indexOf(" " + end + " ") >= 0) return lines;
	}
	throw new Error("No " + end + " after " + command + ": " + lines.join(" | "));
}
function oper(c) { c.send("OPER testop testpass"); c.expect(" 381 ", 5); }
function wait_names(c, channel, present) {
	var until = Date.now() + 8000, lines;
	do {
		lines = query(c, "NAMES " + channel, "366");
		if (lines.some(function(l) { return / 353 /.test(l); }) == present) return lines.join("\n");
		mswait(100);
	} while (Date.now() < until);
	throw new Error("Channel visibility did not become " + present + ": " + lines.join(" | "));
}
function no_rejections(c) {
	var lines = query(c, "PING :visibilitybarrier", "PONG", 8, true);
	/* Remote errors can arrive after the local PONG. Keep them rather than
	   discarding them while waiting for a later expected MODE response. */
	var until = Date.now() + 1000;
	while (Date.now() < until) {
		var line = c.sock.recvline(512, 0);
		if (line) lines.push(line);
		else mswait(20);
	}
	check(!lines.some(function(l) { return / 482 /.test(l); }), "remote leaf rejected an authorized MODE: " + lines.join(" | "));
}

try {
	net._start_one(a, [{sname: b.sname, is_hub: true}, {sname: x.sname, is_hub: false}]);
	owner = client(a, "hiddenOwner"); oper(owner);
	channels.forEach(function(channel, i) {
		owner.join(channel);
		owner.send("MODE " + channel + (i ? " +pt" : " +st")); owner.expect(" MODE ", 5);
		owner.send("MODE " + channel + " +b *!*@hidden.invalid"); owner.expect("+b", 5);
		owner.send("TOPIC " + channel + " :hidden topic"); owner.expect("TOPIC " + channel, 5);
	});
	net._start_one(b, [{sname: a.sname, port: a.port, is_hub: true}, {sname: y.sname, is_hub: false}]);
	net._start_one(x, [{sname: a.sname, port: a.port, is_hub: true}]);
	net._start_one(y, [{sname: b.sname, port: b.port, is_hub: true}]);
	check(net._wait_links(a.port, 4, 20), "visibility network did not link");
	probes = [client(x, "hiddenProbeX"), client(y, "hiddenProbeY")]; probes.forEach(oper);
	outsider = client(x, "hiddenOutsider"); guest = client(a, "hiddenGuest");
	run_tests({
		"burst and live nickname introductions preserve IP addresses across legacy links": function() {
			var old = probes[1], modern = probes[0], live = client(a, "ipLive");
			old.wait_for_nick(live.nick, 8); modern.wait_for_nick(old.nick, 8);
			[[old, owner.nick], [old, live.nick], [modern, old.nick]].forEach(function(pair) {
				var rows = query(pair[0], "WHO +In " + pair[1], "315").filter(function(l) { return / 352 /.test(l); });
				check(rows.some(function(l) { return / 127\.0\.0\.1 /.test(l); }), "nickname IP was corrupted: " + rows.join(" | "));
			});
		},
		"updated leaves receive hidden state and legacy leaves receive no hidden channel": function() {
			channels.forEach(function(channel) {
				probes.forEach(function(c, i) {
					var names = query(c, "NAMES " + channel, "366").join("\n");
					check(legacy_source && i == 1 ? names.indexOf(channel) < 0 || !/ 353 /.test(names)
						: names.indexOf("@hiddenOwner") >= 0, "incorrect initial leaf view: " + names);
				});
			});
		},
		"later remote JOINs and MODE toggles do not cause leaf non-op rejections": function() {
			channels.forEach(function(channel) {
				guest.join(channel); owner.drain(300);
				owner.send("MODE " + channel + " +n");
				owner.send("MODE " + channel + " -n");
				owner.send("MODE " + channel + " +v hiddenGuest");
				no_rejections(owner);
				probes.forEach(function(c, i) {
					var names = query(c, "NAMES " + channel, "366").join("\n");
					if (legacy_source && i == 1) {
						check(!/ 353 /.test(names), "runtime JOIN/MODE created a hidden legacy channel: " + names);
						return;
					}
					check(names.indexOf("@hiddenOwner") >= 0 && names.indexOf("+hiddenGuest") >= 0,
						"operator or voice state lost: " + names);
					var modes = query(c, "MODE " + channel, "324").join("\n").split(" " + channel + " ")[1];
					check(modes && modes.split(" ")[0].indexOf("n") < 0, "remote -n was not applied");
				});
			});
		},
		"ordinary outsiders cannot query hidden channel membership, modes or topics": function() {
			channels.forEach(function(channel) {
				[["NAMES ", "366"], ["WHO ", "315"], ["WHO +c ", "315"], ["WHO +C ", "315"],
				 ["MODE ", "403"], ["TOPIC ", "403"]].forEach(function(q) {
					var lines = query(outsider, q[0] + channel, q[1]);
					check(!lines.some(function(l) { return / (?:324|332|333|352|353) /.test(l); }),
						"hidden response leaked through " + q[0] + ": " + lines.join(" | "));
				});
			});
			var list = query(outsider, "LIST", "323").filter(function(l) { return / 322 /.test(l); }).join("\n");
			check(list.indexOf("#secret-visible") < 0 && list.indexOf("#private-visible") < 0
				&& list.indexOf("hidden topic") < 0, "LIST disclosed hidden channels");
			var whois = query(outsider, "WHOIS hiddenOwner", "318").filter(function(l) { return / 319 /.test(l); }).join("\n");
			check(whois.indexOf("#secret-visible") < 0 && whois.indexOf("#private-visible") < 0, "WHOIS disclosed hidden channels");
		},
		"leaf reconnect preserves its complete or hidden channel view": function() {
			owner.send("SQUIT " + y.sname + " :hidden channel reconnect"); mswait(1000);
			check(net._wait_links(a.port, 4, 20), "uninvolved leaf did not reconnect");
			channels.forEach(function(channel) {
				owner.send("MODE " + channel + " +n");
				owner.send("MODE " + channel + " -n"); no_rejections(owner);
				var names = query(probes[1], "NAMES " + channel, "366").join("\n");
				check(legacy_source ? !/ 353 /.test(names) : names.indexOf("@hiddenOwner") >= 0,
					"reconnect changed channel visibility");
			});
		},
		"first legacy leaf JOIN receives full ops, modes, topic and bans and last PART hides it": function() {
			if (!legacy_source) return;
			channels.forEach(function(channel) {
				var c = probes[1]; c.send("JOIN " + channel); c.expect(" 366 ", 5);
				c.expect(":hiddenOwner!", 8); c.drain(500);
				var names = query(c, "NAMES " + channel, "366").join("\n");
				check(names.indexOf("@hiddenOwner") >= 0 && names.indexOf("+hiddenGuest") >= 0,
					"first JOIN missing corrected member status: " + names);
				check(query(c, "TOPIC " + channel, "333").join("\n").indexOf("hidden topic") >= 0, "first JOIN lost topic");
				check(query(c, "MODE " + channel + " +b", "368").join("\n").indexOf("hidden.invalid") >= 0, "first JOIN lost bans");
				owner.send("MODE " + channel + " +n"); owner.send("MODE " + channel + " -n"); no_rejections(owner);
				c.send("PART " + channel); c.expect(" PART ", 5); mswait(500);
				check(!query(c, "NAMES " + channel, "366").some(function(l) { return / 353 /.test(l); }), "last PART retained hidden channel");
			});
		},
		"public and hidden mode transitions introduce and withdraw the legacy channel": function() {
			if (!legacy_source) return;
			channels.forEach(function(channel, i) {
				owner.send("MODE " + channel + (i ? " -p" : " -s"));
				check(wait_names(probes[1], channel, true).indexOf("@hiddenOwner") >= 0, "public transition lost ops");
				owner.send("MODE " + channel + (i ? " +p" : " +s"));
				wait_names(probes[1], channel, false);
				owner.send("MODE " + channel + " +n"); owner.send("MODE " + channel + " -n"); no_rejections(owner);
			});
		},
		"last local KICK and QUIT also withdraw hidden state from the legacy leaf": function() {
			if (!legacy_source) return;
			var c = probes[1], channel = channels[0];
			c.send("JOIN " + channel); c.expect(" 366 ", 5); c.expect(":hiddenOwner!", 8);
			owner.send("KICK " + channel + " " + c.nick + " :visibility cleanup"); c.expect(" KICK ", 5);
			wait_names(c, channel, false);
			var quitter = client(y, "hiddenQuitter");
			quitter.send("JOIN " + channel); quitter.expect(" 366 ", 5); quitter.expect(":hiddenOwner!", 8);
			quitter.quit(); wait_names(c, channel, false);
		},
		"JOIN automatically returns complete NAMES and topic replies to the new member": function() {
			channels.forEach(function(channel) {
				var lines = query(outsider, "JOIN " + channel, "366");
				var names = lines.filter(function(l) { return / 353 /.test(l); }).join("\n");
				check(names.indexOf("@hiddenOwner") >= 0 && names.indexOf("+hiddenGuest") >= 0
					&& names.indexOf("hiddenOutsider") >= 0, "JOIN lost member list: " + lines.join(" | "));
				check(lines.some(function(l) { return / 332 /.test(l) && l.indexOf("hidden topic") >= 0; }), "JOIN lost burst topic");
				check(query(outsider, "NAMES " + channel, "366").some(function(l) { return / 353 /.test(l); }), "member NAMES blocked");
				check(query(outsider, "MODE " + channel + " +b", "368").join("\n").indexOf("hidden.invalid") >= 0, "member lost burst bans");
			});
		}
	});
} finally {
	clients.forEach(function(c) { try { c.quit(); } catch(e) {} }); net.stop();
}
