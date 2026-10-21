/* Replicated timestamp mappings survive SQUIT and a move to another hub. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork();
var a = {name: "hub1", sname: "mapa.ircd.local", port: 19740};
var b = {name: "hub2", sname: "mapb.ircd.local", port: 19741};
var c = {name: "leaf1a", sname: "mapc.ircd.local", port: 19742};
var leaf;

function check(cond, message) { if (!cond) throw new Error(message); }

function correction(p, name) {
	var until = Date.now() + 5000;
	while (Date.now() < until) {
		var line = p.sock.recvline(512, 0.2);
		if (!line) continue;
		if (line.indexOf(" " + name + " BEGIN ") >= 0)
			return Number(line.split(" ")[2]);
		if (/^ERROR/.test(line)) throw new Error(line);
	}
	throw new Error("No correction for " + name);
}

function introduce(p) {
	p.send("NICK mapuser 1 1 + a drift fakeleaf.ircd.local 0 0.0.0.0 :Drifting leaf");
	p.send("SJOIN 1 #mapped +m :@mapuser");
	var ts = correction(p, "#mapped"); p.end_burst(); barrier(p); return ts;
}

function barrier(p) {
	var token = "mapbarrier" + Date.now(), until = Date.now() + 5000;
	p.send("PING :" + token);
	while (Date.now() < until) {
		var line = p.sock.recvline(512, 0.2);
		if (line && line.indexOf("PONG ") >= 0 && line.indexOf(token) >= 0) return;
		if (line && /^ERROR/.test(line)) throw new Error(line);
	}
	throw new Error("mapping barrier timed out");
}

function disappeared(port) {
	var observer = new IrcdClient(IRCD_TEST_HOST, port); observer.register("mapwatch");
	try {
		var until = Date.now() + 5000;
		while (Date.now() < until) {
			observer.send("WHOIS mapuser");
			try { observer.expect(" 401 ", 1); return; } catch(e) {}
		}
		throw new Error("split nick did not disappear");
	} finally { observer.quit(); }
}

try {
	net._start_one(a, [{sname: b.sname, is_hub: true}, {sname: "fakeleaf.ircd.local", is_hub: false}]);
	net._start_one(b, [{sname: a.sname, port: a.port, is_hub: true},
		{sname: c.sname, is_hub: true}, {sname: "fakeleaf.ircd.local", is_hub: false}]);
	check(net._wait_links(a.port, 2, 20), "hubs did not link");
	leaf = new FakeLeafServer(a.port); leaf.clock_skew = 86400; leaf.defer_map_end = true;
	leaf.connect(); var original = introduce(leaf);
	check(original > 1000000000, "hub used the raw leaf timestamp");
	leaf.close(); disappeared(b.port);
	/* A later hub must learn cached mappings in its burst, even though the
	   leaf's last user and the leaf-only channel are now absent everywhere. */
	net._start_one(c, [{sname: b.sname, port: b.port, is_hub: true},
		{sname: "fakeleaf.ircd.local", is_hub: false}]);
	check(net._wait_links(a.port, 3, 20), "third hub did not link");
	mswait(2100);
	leaf.hub_port = c.port; leaf.connect(); var moved = introduce(leaf);
	check(moved == original, "handoff re-aged channel: " + original + " -> " + moved);
	writeln("PASS: live hub replication, cached burst and same-process leaf handoff retain canonical timestamp");
	leaf.send(":mapuser PART #mapped");
	leaf.send("TSMAP DROP " + leaf.instance + " " + leaf.channel_claims["#MAPPED"].id);
	delete leaf.channel_claims["#MAPPED"];
	barrier(leaf);
	mswait(2100);
	leaf.send("SJOIN 1 #mapped + :@mapuser"); var replacement = correction(leaf, "#mapped");
	check(replacement > moved, "new channel object reused a retired timestamp");
	writeln("PASS: ending the object retires its mapping; same-second leaf timestamps do not alias");
} finally { if (leaf) leaf.close(); net.stop(); }
