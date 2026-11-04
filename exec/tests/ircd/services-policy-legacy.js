/* New policy boundaries with actual old leaves and optional old hubs. */
"use strict";
load(js.exec_dir + "lib.sjs");
var legacy_source = "", legacy_hub = false;
argv.forEach(function(arg) {
	if (arg.indexOf("--legacy-source=") == 0) legacy_source = backslash(arg.substr(16));
	if (arg == "--legacy-hub") legacy_hub = true;
});
if (!legacy_source || !file_exists(legacy_source + "ircd.js")) throw new Error("Supply --legacy-source=/previous/exec/");
var net = new IrcdTestNetwork(), services, owner, target;
var edge = {name:"hub1", sname:"edge.policy.test", port:19754};
var hub = {name:"hub2", sname:"hub.policy.test", port:19755};
var home = {name:"leaf1a", sname:"old.policy.test", port:19756, source_dir:legacy_source};
var authority = {sname:"services.test", is_hub:false, is_services:true, protocol:"bahamut"};
if (legacy_hub) hub.source_dir = legacy_source;
function check(ok, text) { if (!ok) throw new Error(text); }
function mode(channel, fields) { services.send(":ChanServ MODE " + channel + " 0 " + fields); owner.expect(" MODE " + channel + " ", 5); }
try {
	net._start_one(edge, [{sname:hub.sname, is_hub:true}, authority]);
	net._start_one(hub, [{sname:edge.sname, port:edge.port, is_hub:true}, {sname:home.sname, is_hub:false}, authority]);
	net._start_one(home, [{sname:hub.sname, port:hub.port, is_hub:true}, authority]);
	check(net._wait_links(edge.port, 3, 20), "Legacy policy network did not link");
	services = new FakeLeafServer(edge.port, authority.sname); services.legacy = true;
	services.capabilities = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT"; services.connect();
	services.send("NICK ChanServ 1 1800000000 +i services services.test services.test 0 0 :Channel Service"); services.send("BURST 0");
	owner = net.client(edge, "Owner"); target = net.client(home, "OldTarget"); owner.wait_for_nick("OldTarget"); target.wait_for_nick("ChanServ");
	run_tests({
		"new gateways reject unidentified old-leaf joins and admit an actual services grant": function() {
			owner.join("#identified"); mode("#identified", "+R");
			target.send("JOIN #identified"); target.expect("KICK #identified OldTarget", 5);
			owner.send("NAMES #identified"); check(owner.expect(" 353 ", 5).indexOf("OldTarget") < 0, "Rejected membership entered canonical state");
			services.send(":services.test SVSMODE OldTarget +r"); owner.send("WHOIS OldTarget"); owner.expect(" 307 ", 5);
			target.send("JOIN #identified"); owner.expect(":OldTarget!", 5);
		},
		"old-leaf messages cannot bypass newer moderation or color policy": function() {
			mode("#identified", "+Mc");
			target.send("PRIVMSG #identified :\x0304red"); target.expect(" 404 ", 5);
			target.send("PRIVMSG #identified :identified legacy speech"); owner.expect("identified legacy speech", 5);
			services.send(":services.test SVSMODE OldTarget -r");
			services.send(":ChanServ PRIVMSG OldTarget :revocation barrier"); target.expect("revocation barrier", 5);
			target.send("PRIVMSG #identified :unidentified legacy speech"); target.expect(" 477 ", 5);
		},
		"unknown old-leaf TLS cannot bypass secure-only channels": function() {
			owner.join("#secure"); mode("#secure", "+S"); target.send("JOIN #secure"); target.expect("KICK #secure OldTarget", 5);
			owner.send("NAMES #secure"); check(owner.expect(" 353 ", 5).indexOf("OldTarget") < 0, "Unknown TLS was accepted");
		},
		"private and secret rejected joins leave no ghost membership on an old leaf": function() {
			["p", "s"].forEach(function(flag) {
				var channel = "#hidden" + flag;
				owner.join(channel); mode(channel, "+" + flag + "S");
				target.send("JOIN " + channel); target.expect("KICK " + channel + " OldTarget", 5);
				target.send("NAMES " + channel);
				var lines = "", line;
				do { line = target.readln(5); lines += line + "\n"; } while (line.indexOf(" 366 ") < 0);
				// Pre-2.5 reply filtering can expose other members through an old
				// hub; the correction must still remove the rejected local user.
				lines.split("\n").forEach(function(reply) {
					if (reply.indexOf(" 353 ") >= 0)
						check(reply.split(" :")[1].indexOf("OldTarget") < 0, "Rejected leaf membership survived: " + reply);
				});
				target.send("PART " + channel); target.expect(/ (403|442) /, 5);
				owner.send("NAMES " + channel);
				check(owner.expect(" 353 ", 5).indexOf("OldTarget") < 0, "Rejected canonical membership survived");
			});
		},
		"the newer gateway enforces join-rate limits for live old-leaf joins": function() {
			owner.join("#rate"); mode("#rate", "+j 1:30"); target.send("JOIN #rate"); owner.expect(":OldTarget!", 5);
			target.send("PART #rate"); target.expect(" PART #rate", 5); target.send("JOIN #rate"); target.expect("KICK #rate OldTarget", 5);
		},
		"native S-lines stop introductions from old leaves while their links remain usable": function() {
			services.send(":services.test SQLINE BadLegacy* :legacy reservation"); services.send(":services.test SGLINE 10 :Bad Real *:legacy realname");
			services.send(":ChanServ PRIVMSG OldTarget :sline barrier"); target.expect("sline barrier", 5);
			var bad = net.client(home, false);
			try {
				bad.send("NICK BadLegacy211"); bad.send("USER user 0 * :Ordinary"); bad.expect_disconnect(5);
				owner.send("WHOIS BadLegacy211"); owner.expect(" 401 ", 5);
			} finally { bad.quit(); }
			bad = net.client(home, false);
			try {
				bad.send("NICK BadReal211"); bad.send("USER user 0 * :Bad Real Name"); bad.expect_disconnect(5);
				owner.send("WHOIS BadReal211"); owner.expect(" 401 ", 5);
			} finally { bad.quit(); }
			check(net._wait_links(edge.port, 4, 5) && services.sock.is_connected, "Policy enforcement split a legacy link");
		}
	});
} finally {
	if (target) target.quit(); if (owner) owner.quit(); if (services) services.close(); net.stop();
}
