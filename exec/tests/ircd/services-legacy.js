/* Actual old leaf/hub processes: services additions must preserve old links. */
"use strict";
load(js.exec_dir + "lib.sjs");
var legacy_source = "", legacy_hub = false;
argv.forEach(function(arg) {
	if (arg.indexOf("--legacy-source=") == 0) legacy_source = backslash(arg.substr(16));
	if (arg == "--legacy-hub") legacy_hub = true;
});
if (!legacy_source || !file_exists(legacy_source + "ircd.js")) throw new Error("Supply --legacy-source=/previous/exec/");
var legacy_server = new File(legacy_source + (file_exists(legacy_source + "load/ircd_server.js")
	? "load/ircd_server.js" : "load/ircd/server.js"));
if (!legacy_server.open("r")) throw new Error("Cannot inspect legacy server handlers");
var legacy_rakill = /case\s+["']RAKILL["']/.test(legacy_server.read()); legacy_server.close();
IRCD_TEST_TEMP_DIR += "ircd-services-legacy-" + random(2147483647) + "/";
if (!mkdir(IRCD_TEST_TEMP_DIR)) throw new Error("Cannot create private fixture directory");
var net = new IrcdTestNetwork(), services, owner, target;
var edge = {name:"hub1", sname:"edge.services.test", port:19740};
var hub = {name:"hub2", sname:"hub.services.test", port:19741};
var home = {name:"leaf1a", sname:"old.services.test", port:19742, source_dir:legacy_source};
var authority = {sname:"services.test", is_hub:false, is_services:true, protocol:"bahamut"};
if (legacy_hub) hub.source_dir = legacy_source;
function check(ok, message) { if (!ok) throw new Error(message); }
function bans(server) {
	owner.send("STATS K " + server);
	var lines = [], until = Date.now() + 5000;
	while (Date.now() < until) {
		var line = owner.readln(2); lines.push(line);
		if (line.indexOf(" 219 ") >= 0) return lines.join("\n");
	}
	throw new Error("STATS K did not finish for " + server);
}
try {
	net._start_one(edge, [{sname:hub.sname, is_hub:true}, authority]);
	net._start_one(hub, [{sname:edge.sname, port:edge.port, is_hub:true}, {sname:home.sname, is_hub:false}, authority]);
	net._start_one(home, [{sname:hub.sname, port:hub.port, is_hub:true}, authority]);
	check(net._wait_links(edge.port, 3, 20), "Mixed network did not link");
	services = new FakeLeafServer(edge.port, authority.sname); services.legacy = true;
	services.capabilities = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT"; services.connect();
	services.send("NICK ChanServ 1 1800000000 +i services services.test services.test 0 0 :Channel Service");
	owner = net.client(edge, "Owner"); target = net.client(home, "OldTarget");
	owner.send("OPER testop testpass"); owner.expect(" 381 ", 5);
	target.wait_for_nick("ChanServ"); owner.join("#legacy"); target.join("#legacy"); owner.expect("OldTarget!", 5);
	run_tests({
		"ordinary old-leaf messages and relayed modes work while new exceptions stay protected": function() {
			owner.send("MODE #legacy +e *!*@trusted.invalid"); owner.expect("+e *!*@trusted.invalid", 5);
			target.send("PRIVMSG #legacy :legacy channel message"); owner.expect("legacy channel message", 5);
			owner.send("MODE #legacy +m"); target.expect("MODE #legacy +m", 5);
			owner.send("MODE #legacy +e"); owner.expect(" 348 Owner #legacy *!*@trusted.invalid", 5); owner.expect(" 349 ", 5);
		},
		"services ban expiry preserves old links and records the 1.3 RAKILL limitation": function() {
			var created = Math.floor(Date.now() / 1000);
			services.send(":services.test AKILL blocked.invalid bad 5 OperServ " + created + " :Timed policy");
			var until = Date.now() + 3000, installed = false;
			while (Date.now() < until && !installed) installed = bans(home.sname).indexOf("bad@blocked.invalid") >= 0;
			check(installed, "Old leaf did not receive services AKILL");
			until = Date.now() + 8000;
			while (Date.now() < until) {
				if (bans(legacy_rakill ? home.sname : edge.sname).indexOf("bad@blocked.invalid") < 0) {
					if (!legacy_rakill) check(bans(home.sname).indexOf("bad@blocked.invalid") >= 0,
						"The unmodified 1.3 fixture unexpectedly implements RAKILL");
					check(net._wait_links(edge.port, 4, 5), "Ban expiry split a legacy link"); return;
				}
				mswait(100);
			}
			throw new Error("Old peer retained an expired services ban");
		},
		"SVSNICK releases a protected nick on old paths without inventing a rename": function() {
			services.send(":services.test SVSNICK OldTarget Guest211 :" + Math.floor(Date.now() / 1000));
			target.expect_disconnect(5); owner.expect("QUIT :", 5);
			owner.send("WHOIS OldTarget"); owner.expect(" 401 ", 5);
			owner.send("WHOIS Guest211"); owner.expect(" 401 ", 5);
			owner.wait_for_nick("ChanServ");
			check(net._wait_links(edge.port, 4, 5), "Legacy fallback split the network");
		},
		"services can hold and release the freed nick across the old link": function() {
			services.send("NICK OldTarget 1 " + Math.floor(Date.now()/1000) + " +i enforcer services.test services.test 0 0 :Enforcement");
			owner.wait_for_nick("OldTarget");
			services.send(":OldTarget QUIT :Released"); owner.send("PING :release-barrier"); owner.expect("PONG ", 5);
			check(services.sock.is_connected && net._wait_links(edge.port, 4, 5), "Services lifecycle broke a legacy link");
		}
	});
} finally {
	if (target) target.quit(); if (owner) owner.quit(); if (services) services.close();
	net.stop();
}
