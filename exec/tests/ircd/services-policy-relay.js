/* Bahamut identity, policy and native S-lines across real native relays. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork(), services, owner, target, visitor;
var edge = {name:"hub1", sname:"edge.policy.test", port:19750};
var hub = {name:"hub2", sname:"hub.policy.test", port:19751};
var home = {name:"leaf1a", sname:"home.policy.test", port:19752};
var authority = {sname:"services.test", is_hub:false, is_services:true, protocol:"bahamut"};
function check(ok, text) { if (!ok) throw new Error(text); }
function identified(client, nick, expected) {
	var until = Date.now() + 6000, lines;
	do {
		client.send("WHOIS " + nick); lines = "";
		for (;;) { var line = client.readln(3); lines += line + "\n"; if (line.indexOf(" 318 ") >= 0) break; }
		if ((lines.indexOf(" 307 ") >= 0) == expected) return;
		mswait(100);
	} while (Date.now() < until);
	throw new Error("Wrong identification state: " + lines);
}
function mode(channel, fields, client) {
	services.send(":ChanServ MODE " + channel + " 0 " + fields);
	(client || owner).expect(" MODE " + channel + " ", 5);
}
function service_connect() {
	services = new FakeLeafServer(edge.port, authority.sname); services.legacy = true;
	services.capabilities = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT"; services.connect();
	services.send("NICK ChanServ 1 1800000000 +i services services.test services.test 0 0 :Channel Service"); services.send("BURST 0");
}
function nick_timestamp(nick) {
	var until = Date.now() + 6000;
	while (Date.now() < until) {
		var line = services.sock.recvline(512, 1);
		if (!line) continue;
		var fields = line.split(" ");
		if (fields[0] == "NICK" && fields[1] == nick) return fields[3];
	}
	throw new Error("Services did not receive NICK " + nick);
}
try {
	net._start_one(edge, [{sname:hub.sname, is_hub:true}, {sname:"snapshot.test", is_hub:true}, authority]);
	net._start_one(hub, [{sname:edge.sname, port:edge.port, is_hub:true}, {sname:home.sname, is_hub:false}, authority]);
	net._start_one(home, [{sname:hub.sname, port:hub.port, is_hub:true}, authority]);
	check(net._wait_links(edge.port, 3, 20), "Policy network did not link"); service_connect();
	owner = net.client(edge, "Owner"); target = net.client(home, "Target"); visitor = net.client(home, "Visitor");
	owner.wait_for_nick("Target"); target.wait_for_nick("ChanServ");
	run_tests({
		"SVSMODE identification and stamps cross two relays without granting native notice modes": function() {
			services.send(":services.test SVSMODE Target " + nick_timestamp("Target") + " +dr 1234567"); identified(owner, "Target", true); identified(target, "Target", true);
			target.send("MODE Target"); var line = target.expect(" 221 ", 5); check(!/[rd]/.test(line.split(" ").pop()), line);
		},
		"identified-only joins distinguish the services grant from an ordinary user": function() {
			owner.join("#identified"); mode("#identified", "+R"); target.join("#identified");
			visitor.send("JOIN #identified"); visitor.expect(" 477 ", 5);
			services.send(":services.test SVSMODE Visitor +r"); identified(visitor, "Visitor", true); visitor.join("#identified");
		},
		"nickname changes revoke identification and M blocks unidentified speech": function() {
			mode("#identified", "+M"); target.send("NICK Changed"); target.expect(" NICK Changed", 5); target.nick = "Changed";
			owner.wait_for_nick("Changed");
			identified(target, "Changed", false); target.send("PRIVMSG #identified :blocked message"); target.expect(" 477 ", 5);
			services.send(":services.test SVSMODE Changed +r"); identified(target, "Changed", true);
			target.send("PRIVMSG #identified :identified message"); owner.expect("identified message", 5);
			target.send("NICK Target"); target.expect(" NICK Target", 5); target.nick = "Target";
			owner.wait_for_nick("Target");
			services.send(":services.test SVSMODE Target +r"); identified(target, "Target", true);
		},
		"invite masks reach the leaf and services can clear the list": function() {
			owner.join("#invite"); mode("#invite", "+iI Target"); target.join("#invite");
			target.send("MODE #invite +I"); target.expect(" 346 Target #invite Target!*@*", 5); target.expect(" 347 ", 5);
			mode("#invite", "-I Target!*@*"); target.send("PART #invite"); target.expect(" PART #invite", 5);
			target.send("JOIN #invite"); target.expect(" 473 ", 5);
		},
		"control-code restrictions and join-rate limits are enforced at the leaf": function() {
			owner.join("#controls"); target.join("#controls"); mode("#controls", "+c");
			target.send("PRIVMSG #controls :\x0304red"); target.expect(" 404 ", 5);
			target.send("PRIVMSG #controls :plain message"); owner.expect("plain message", 5);
			mode("#controls", "+j 1:30"); visitor.join("#controls"); visitor.send("PART #controls"); visitor.expect(" PART #controls", 5);
			visitor.send("JOIN #controls"); visitor.expect(" 471 ", 5);
		},
		"secure-only policy rejects plaintext clients across native relays": function() {
			owner.join("#secure"); mode("#secure", "+S"); target.send("JOIN #secure"); target.expect(" 489 ", 5);
		},
		"SGLINE and SQLINE reach native admission and exact removals release only their own masks": function() {
			services.send(":services.test SQLINE Blocked* :Reserved nickname");
			services.send(":services.test SGLINE 10 :Bad Real *:Realname policy");
			// PING through the same route orders the policy before admission.
			services.send(":ChanServ PRIVMSG Target :sline barrier"); target.expect("sline barrier", 5);
			var bad = net.client(home, false);
			try { bad.send("NICK BlockedNick"); bad.expect(" 432 ", 5); } finally { bad.quit(); }
			bad = net.client(home, false);
			try { bad.send("NICK RealnameTarget"); bad.send("USER user 0 * :Bad Real Name"); bad.expect("SGLINE: Realname policy", 5); } finally { bad.quit(); }
			services.send(":services.test UNSQLINE Blocked*"); services.send(":services.test UNSGLINE :Bad Real *");
			services.send(":ChanServ PRIVMSG Target :unline barrier"); target.expect("unline barrier", 5);
			bad = net.client(home, false); try { bad.register("BlockedNick", "user", "Bad Real Name"); } finally { bad.quit(); }
		},
		"services reconnect receives the same stamp and identification in Bahamut NICK introductions": function() {
			services.send(":services.test SQLINE ReconnectStale* :Expired while disconnected");
			services.send(":ChanServ PRIVMSG Target :reconnect barrier"); target.expect("reconnect barrier", 5);
			services.close();
			var until = Date.now() + 6000, gone = false;
			while (!gone && Date.now() < until) {
				owner.send("WHOIS ChanServ");
				for (;;) {
					var line = owner.readln(3);
					if (line.indexOf(" 401 ") >= 0) { gone = true; break; }
					if (line.indexOf(" 318 ") >= 0) break;
				}
				if (!gone) mswait(100);
			}
			check(gone, "Services departure was not processed");
			service_connect(); owner.wait_for_nick("ChanServ");
			check(services.received.some(function(line) { return /^NICK Target [0-9]+ [0-9]+ \+[^ ]*r /.test(line) && line.split(" ")[8] == "1234567"; }), services.received.filter(function(line) { return line.indexOf("NICK Target ") >= 0; }).join(" | "));
			identified(target, "Target", true);
			var released = net.client(home, "ReconnectStale211"); released.quit();
		},
		"new hub bursts contain identity, invite masks, policies and owned S-lines": function() {
			owner.join("#snapshot"); target.join("#snapshot"); mode("#snapshot", "+RMcIj Target 3:10");
			services.send(":services.test SQLINE SnapshotReserved* :Snapshot policy");
			services.send(":ChanServ PRIVMSG Target :snapshot barrier"); target.expect("snapshot barrier", 5);
			var snapshot = new FakeLeafServer(edge.port, "snapshot.test");
			snapshot.capabilities = "TSJOIN TSMODE CLOCK CSYNC TSMAP FULLCHAN SVCSTATE INVMASK CHANPOLICY SLINES CLIENTTLS";
			try {
				snapshot.connect();
				check(snapshot.received.some(function(line) { return line.indexOf("SJOIN ") >= 0 && line.indexOf(" #snapshot ") >= 0 && line.indexOf("3:10") >= 0; }), snapshot.received.join(" | "));
				check(snapshot.received.some(function(line) { return line.indexOf("MODE #snapshot ") >= 0 && line.indexOf("+I Target!*@*") >= 0; }), "Invite mask missing from burst");
				check(snapshot.received.some(function(line) { return /SVCSTATE Target [0-9]+ services[.]test 1234567 r$/.test(line); }), "Services grant missing from burst");
				check(snapshot.received.some(function(line) { return line.indexOf(":services.test SQLINE snapshotreserved* :Snapshot policy") == 0; }), "S-line owner missing from burst");
			} finally { snapshot.close(); }
		}
	});
} finally {
	if (visitor) visitor.quit(); if (target) target.quit(); if (owner) owner.quit(); if (services) services.close(); net.stop();
}
