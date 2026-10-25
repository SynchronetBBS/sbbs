/* Live clock admission, drift and rolling-upgrade compatibility checks. */
"use strict";
load(js.exec_dir + "lib.sjs");

var net = new IrcdTestNetwork();
var node = {name: "hub1", sname: "clock.ircd.local", port: 19720, capab_enforce: true};

function check(cond, message) { if (!cond) throw new Error(message); }

function peer(name, legacy, skew) {
	var p = new FakeLeafServer(node.port, name);
	p.legacy = legacy; p.clock_skew = skew || 0;
	return p;
}

function pump(p, seconds) {
	var until = Date.now() + seconds * 1000, lines = [], next_clock = 0;
	while (Date.now() < until && p.sock.is_connected) {
		if (p.periodic_clock && Date.now() >= next_clock) {
			p.send("CLOCK -1 " + (Date.now() / 1000 + p.clock_skew).toFixed(3));
			next_clock = Date.now() + 15000;
		}
		var raw = p.sock.recvline(512, 0.2);
		if (!raw) continue;
		var line = raw.replace(/\r?\n?$/, ""); lines.push(line);
		if (/^CLOCK [0-9]+$/.test(line))
			p.send(line + " " + (Date.now() / 1000 + p.clock_skew).toFixed(3));
		if (/^PING /.test(line)) p.send("PONG " + line.substr(5));
	}
	return lines;
}

try {
	net._start_one(node, [
		{sname: "fakeleaf.ircd.local", is_hub: false},
		{sname: "fakehub.ircd.local", is_hub: true},
		{sname: "oldhub.ircd.local", is_hub: true},
		{sname: "services.ircd.local", is_hub: true, is_services: true}
	]);
	run_tests({
		"drifting leaf stays linked and receives the hub's canonical timestamp": function() {
			var p = peer("fakeleaf.ircd.local", false, 86400), c;
			try {
				p.connect(); c = net.client(node, "driftobserver");
				p.send("NICK DriftNick 1 1000000 + a drift fakeleaf.ircd.local 0 0.0.0.0 :Drifting leaf");
				c.wait_for_nick("DriftNick");
				p.send("SJOIN 1 #drift + :@DriftNick");
				var lines = pump(p, 16).join("\n");
				check(/ CSYNC [0-9]{10} #drift BEGIN /.test(lines), "missing timestamp correction: " + lines);
				check(p.sock.is_connected && !/^ERROR /m.test(lines), "drifting leaf disconnected");
			} finally { if (c) c.quit(); p.close(); }
		},
		"clock-skewed hubs are refused before state exchange": function() {
			[-31, 31].forEach(function(skew) {
				var p = peer("fakehub.ircd.local", false, skew), error = "";
				try { p.connect(); } catch(e) { error = String(e); }
				finally { p.close(); }
				check(/Clock skew/.test(error), "wrong rejection: " + error);
				check(!/^BURST /m.test(p.received.join("\n")), "state escaped before admission");
			});
		},
		"old leaves with and without TSJOIN survive existing capability enforcement": function() {
			["TSJOIN TSMODE", ""].forEach(function(capabilities) {
				var p = peer("fakeleaf.ircd.local", true), c;
				p.capabilities = capabilities;
				try {
					p.connect(); c = net.client(node, "oldleafcheck"); c.join("#oldleaf");
					p.send("NICK OldNick 1 1000000 + a old fakeleaf.ircd.local 0 0.0.0.0 :Old leaf");
					c.wait_for_nick("OldNick");
					p.send(":OldNick JOIN #oldleaf"); p.send("BURST 0");
					p.send(":fakeleaf.ircd.local MODE #oldleaf +o OldNick");
					var lines = p.received.concat(pump(p, 1)).join("\n");
					check(p.sock.is_connected, "legacy leaf disconnected");
					check(!/^(?:CLOCK |(?::[^ ]+ )?(?:CSYNC|TSMAP) )/m.test(lines), "extension sent to legacy leaf");
					check(/MODE #oldleaf (?:0 )?-ov OldNick OldNick/.test(lines), "missing corrective MODE");
				} finally { if (c) c.quit(); p.close(); }
			});
		},
		"trusted hub without TSJOIN survives existing capability enforcement": function() {
			var p = peer("oldhub.ircd.local", true);
			try { p.connect(); check(p.sock.is_connected, "old hub rejected"); }
			finally { p.close(); }
		},
		"old trusted hub stays linked without CLOCK or CSYNC": function() {
			var p = peer("oldhub.ircd.local", true), c;
			p.capabilities = "TSJOIN TSMODE";
			try {
				p.connect();
				c = net.client(node, "oldhubcheck"); c.join("#oldhub");
				var lines = pump(p, 1).join("\n");
				check(/SJOIN [0-9]+ #oldhub \+ :@oldhubcheck/.test(lines),
					"legacy hub missed founder membership: " + lines);
				check(!/^CLOCK /m.test(lines), "legacy hub was probed");
				check(p.sock.is_connected, "legacy hub was disconnected");
			} finally { if (c) c.quit(); p.close(); }
		},
		"legacy services keep TS-zero authority and remain exempt from clock probes": function() {
			var p = peer("services.ircd.local", true), c;
			try {
				p.connect(); c = net.client(node, "servicecheck"); c.join("#services");
				p.send("NICK ChanServ 1 1000000 +S s services services.ircd.local 0 0.0.0.0 :Services");
				c.wait_for_nick("ChanServ");
				p.send(":ChanServ JOIN 0 #services");
				p.send(":services.ircd.local SJOIN 0 #services +t :@ChanServ");
				c.expect("+o ChanServ", 3);
				p.send(":ChanServ MODE #services 0 +v servicecheck");
				c.expect("+v servicecheck", 3);
				var lines = pump(p, 16).join("\n");
				check(!/^(?:CLOCK |(?::[^ ]+ )?CSYNC )/m.test(lines), "new protocol sent to services");
				check(p.sock.is_connected, "legacy services disconnected");
			} finally { if (c) c.quit(); p.close(); }
		},
		"admitted hub is disconnected when its clock later drifts": function() {
			var p = peer("fakehub.ircd.local", false);
			try {
				p.connect(); p.clock_skew = 31;
				var lines = pump(p, 20).join("\n");
				check(/ERROR .*Clock skew/.test(lines), "missing drift rejection: " + lines);
				check(!p.sock.is_connected, "drifted peer still connected");
			} finally { p.close(); }
		},
		"upgraded hubs announce time without periodic request-response probes": function() {
			var p = peer("fakehub.ircd.local", false);
			p.capabilities = "TSJOIN TSMODE CLOCK CLOCKPUSH CSYNC TSMAP";
			p.periodic_clock = true;
			try {
				p.connect();
				check(/^CLOCK [0-9]+$/m.test(p.received.join("\n")), "admission did not request a clock sample");
				var lines = pump(p, 34).join("\n");
				check(p.sock.is_connected && !/^ERROR /m.test(lines), "periodic hub disconnected: " + lines);
				check((lines.match(/^CLOCK -1 [0-9]+[.][0-9]{3}$/gm) || []).length >= 2,
					"missing periodic announcements: " + lines);
				check(!/^CLOCK [0-9]+$/m.test(lines), "periodic mode requested another sample: " + lines);
			} finally { p.close(); }
		},
		"periodic announcements disconnect a hub that drifts in either direction": function() {
			[-31, 31].forEach(function(skew) {
				var p = peer("fakehub.ircd.local", false);
				p.capabilities = "TSJOIN TSMODE CLOCK CLOCKPUSH CSYNC TSMAP";
				p.periodic_clock = true;
				try {
					p.connect(); p.clock_skew = skew;
					var lines = pump(p, 3).join("\n");
					check(/ERROR .*Clock skew/.test(lines), "missing periodic drift rejection: " + lines);
					check(!p.sock.is_connected, "drifted periodic peer still connected");
				} finally { p.close(); }
			});
		},
		"a hub cannot remain linked while omitting negotiated announcements": function() {
			var p = peer("fakehub.ircd.local", false);
			p.capabilities = "TSJOIN TSMODE CLOCK CLOCKPUSH CSYNC TSMAP";
			try {
				p.connect();
				var lines = pump(p, 34).join("\n");
				check(/ERROR .*Periodic clock announcement timed out/.test(lines), "missing periodic timeout: " + lines);
				check(!p.sock.is_connected, "silent periodic hub still connected");
			} finally { p.close(); }
		}
	});
} finally { net.stop(); }
