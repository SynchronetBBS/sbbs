/* Real services -> edge -> hub -> leaf routing using Bahamut wire forms. */
"use strict";
load(js.exec_dir + "lib.sjs");

var net = new IrcdTestNetwork(), services, owner, target;
var edge = {name:"hub1", sname:"edge.services.test", port:19730};
var hub = {name:"hub2", sname:"hub.services.test", port:19731};
var home = {name:"leaf1a", sname:"home.services.test", port:19732};
var authority = {sname:"services.test", is_hub:false, is_services:true, protocol:"bahamut"};

function check(ok, message) { if (!ok) throw new Error(message); }
function services_expect(pattern) {
	var until = Date.now() + 5000, seen = [];
	while (Date.now() < until) {
		var line = services.sock.recvline(512, 0.2);
		if (!line) continue;
		seen.push(line);
		if (line.indexOf("PING ") == 0) services.send("PONG " + line.substr(5));
		if (pattern.test(line)) return seen;
	}
	throw new Error("Services did not receive " + pattern + ": " + seen.join(" | "));
}
function status(client) {
	client.send("NAMES #service");
	return client.expect(" 353 ", 5).split(" :")[1];
}

try {
	net._start_one(edge, [{sname:hub.sname, is_hub:true}, authority]);
	net._start_one(hub, [{sname:edge.sname, port:edge.port, is_hub:true},
		{sname:home.sname, is_hub:false}, authority]);
	net._start_one(home, [{sname:hub.sname, port:hub.port, is_hub:true}, authority]);
	check(net._wait_links(edge.port, 3, 20), "Three-server network did not link");
	services = new FakeLeafServer(edge.port, authority.sname);
	services.legacy = true;
	services.capabilities = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT";
	services.connect();
	services.send("NICK ChanServ 1 1800000000 +i services services.test services.test 0 0 :Channel Service");
	services.send("BURST 0");
	owner = net.client(edge, "Owner"); target = net.client(home, "Target");
	target.wait_for_nick("ChanServ");
	owner.join("#service"); target.join("#service");
	owner.expect("Target!", 5);

	run_tests({
		"services SJOIN reaches a local leaf user without resetting existing ops": function() {
			services.send(":services.test SJOIN 0 #service +mt :@+Target");
			target.expect("+", 5);
			var names = status(target);
			check(names.indexOf("@Target") >= 0 && names.indexOf("@Owner") >= 0, "Services SJOIN lost new or existing ops: " + names);
		},
		"services TS-zero MODE reaches the far leaf": function() {
			services.send(":ChanServ MODE #service 0 +v Owner");
			target.expect("+v Owner", 5);
		},
		"Bahamut join-rate modes preserve following key and status arguments": function() {
			services.send(":ChanServ MODE #service 0 +jko 2:30 servicekey Target");
			target.expect("+kj servicekey 2:30", 5);
			check(status(target).indexOf("@Target") >= 0, "Later op argument was lost");
			services.send(":ChanServ MODE #service 0 -kj servicekey"); target.expect("-kj servicekey", 5);
		},
		"Bahamut services can add and clear ordinary exceptions across both hubs": function() {
			services.send(":ChanServ MODE #service 0 +e *!*@trusted.invalid");
			target.expect("+e *!*@trusted.invalid", 5);
			target.send("MODE #service +e"); target.expect(" 348 Target #service *!*@trusted.invalid", 5); target.expect(" 349 ", 5);
			services.send(":ChanServ MODE #service 0 -e *!*@trusted.invalid");
			target.expect("-e *!*@trusted.invalid", 5);
		},
		"ordinary exception updates reach services while extended masks stay on capable links": function() {
			owner.send("MODE #service +ee *!*@visible.invalid ~m:external:Target");
			var seen = services_expect(/ MODE #service .*\+e \*!\*@visible[.]invalid/);
			services.send("PING :exception-barrier");
			seen = seen.concat(services_expect(/PONG .*exception-barrier/));
			check(!seen.some(function(line) { return line.indexOf("~msgbypass:") >= 0; }), "Extended masks leaked to Bahamut");
			owner.send("MODE #service -ee *!*@visible.invalid ~m:external:Target");
			target.expect("-ee *!*@visible.invalid ~msgbypass:external:Target!*@*", 5);
		},
		"services restore an older saved topic through both hubs": function() {
			owner.send("TOPIC #service :Unwanted topic");
			target.expect("TOPIC #service :Unwanted topic", 5);
			services.send(":services.test TOPIC #service Founder 100 :Locked topic");
			target.expect("TOPIC #service :Locked topic", 5);
			target.send("TOPIC #service");
			target.expect(" 332 Target #service :Locked topic", 5);
		},
		"services without user mode +o can announce across both hubs": function() {
			services.send(":ChanServ NOTICE $*.test :Network maintenance");
			owner.expect("NOTICE $*.test :Network maintenance", 5);
			target.expect("NOTICE $*.test :Network maintenance", 5);
		},
		"services KICK and INVITE reach the far leaf without joining ChanServ": function() {
			services.send(":ChanServ KICK #service Target :Policy");
			target.expect("KICK #service Target :Policy", 5);
			services.send(":ChanServ INVITE Target #service");
			target.expect(/INVITE Target :?#service$/, 5);
			target.join("#service");
			owner.expect("Target!", 5);
		},
		"SVSNICK reaches the owner and acknowledges a complete rename/hold/release lifecycle": function() {
			var stamp = Math.floor(Date.now() / 1000);
			services.send(":services.test SVSNICK Target Guest211 :" + stamp);
			target.expect(/ NICK :?Guest211$/, 5); owner.expect(/:Target!.* NICK :?Guest211$/, 5);
			services_expect(/:Target NICK Guest211 :[0-9]+/);
			services.send("NICK Target 1 " + stamp + " +i enforcer services.test services.test 0 0 :Nickname enforcement");
			owner.wait_for_nick("Target");
			services.send(":Target QUIT :Released");
			services.send(":services.test SVSNICK Guest211 Target :" + stamp);
			target.expect(/ NICK :?Target$/, 5); owner.expect(/:Guest211!.* NICK :?Target$/, 5);
			services_expect(/:Guest211 NICK Target :[0-9]+/);
			check(status(target).indexOf("Target") >= 0, "Rename lost channel membership");
		},
		"services KILL reaches another route without disconnecting services": function() {
			services.send(":services.test KILL Target :Nickname reclaimed");
			target.expect("KILLED by services.test (Nickname reclaimed)", 5);
			owner.expect("QUIT :", 5);
			owner.wait_for_nick("ChanServ");
			check(services.sock.is_connected, "KILL disconnected services");
		},
		"services JUPE replaces the requested server without a name collision": function() {
			services.send(":services.test SQUIT " + home.sname + " :Juped");
			services.send("SERVER " + home.sname + " 2 :Juped");
			var until = Date.now() + 5000, replaced = false;
			while (Date.now() < until && !replaced) {
				owner.send("LINKS");
				for (;;) {
					var line = owner.readln(2);
					if (line.indexOf(" 364 ") >= 0 && line.indexOf(home.sname + " " + authority.sname) >= 0) replaced = true;
					if (line.indexOf(" 365 ") >= 0) break;
				}
			}
			check(replaced, "JUPE did not replace the server behind services");
			owner.wait_for_nick("ChanServ");
		}
	});
} finally {
	if (target) target.quit();
	if (owner) owner.quit();
	if (services) services.close();
	net.stop();
}
