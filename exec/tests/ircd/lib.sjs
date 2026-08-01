/*
 * exec/tests/ircd/lib.sjs
 *
 * Shared helpers for IRCd integration tests.
 * Named .sjs so the test runner (which looks for .js) ignores it.
 * Load with: load(js.exec_dir + "lib.sjs");
 */

"use strict";

var IRCD_TEST_HOST = "127.0.0.1";

/* ---- Server lifecycle ---- */

function IrcdTestServer(port) {
	this.port = port || 19667;
	this.pid  = 0;
	this.config_path = "";
	this.log_path = system.temp_dir + "ircd_test_" + this.port + ".log";
}

IrcdTestServer.prototype._write_config = function() {
	var lines = [
		"[Info]",
		"Servername=test.ircd.local",
		"Description=IRCd Integration Test Server",
		"Admin1=Test Admin",
		"Admin2=localhost",
		"Admin3=testop@test.ircd.local",
		"",
		"[Port:" + this.port + "]",
		"Default=true",
		"",
		"[Class:1]",
		"PingFrequency=3600",
		"ConnectFrequency=0",
		"Maximum=100",
		"SendQ=1000000",
		"",
		"[Class:10]",
		"PingFrequency=3600",
		"ConnectFrequency=0",
		"Maximum=10",
		"SendQ=2000000",
		"",
		"[Allow]",
		"Mask=*@*",
		"Class=1",
		"",
		/* O flag = global oper (includes gkill, globops, rehash, etc.)
		 * w = wallops, k = local kill, K = global kill */
		"[Operator:testop]",
		"Nick=testop",
		"Mask=*@*",
		"Password=testpass",
		"Flags=OwkK",
		"Class=10",
		""
	];
	var f = new File(this.config_path);
	if (!f.open("w"))
		throw new Error("Cannot write IRCd test config: " + this.config_path);
	f.write(lines.join("\n"));
	f.close();
};

IrcdTestServer.prototype.start = function() {
	write("  [ircd] starting on port " + this.port + "... ");
	this.config_path = system.temp_dir
		+ "ircd_test_" + this.port + "_" + Date.now() + ".ini";
	this._write_config();

	var ircd   = js.exec_dir + "../../ircd.js";
	var jsexec = js.exec_dir + "../../jsexec";
	var ctrl   = system.ctrl_dir;

	/*
	 * Background the IRCd via the shell and capture its PID.
	 * system.popen() runs through /bin/sh, blocks until the command returns,
	 * and returns captured stdout lines.  The trailing "& echo $!" makes the
	 * shell return immediately after printing the child PID.
	 */
	/*
	 * -a is required: without it the IRCd binds both 0.0.0.0 and ::, which
	 * collide (EADDRINUSE) wherever net.ipv6.bindv6only is 0 -- the default
	 * on Linux.  Tests only ever talk to IRCD_TEST_HOST anyway.
	 */
	var cmd = jsexec
		+ " -c " + ctrl
		+ " " + ircd
		+ " -f " + this.config_path
		+ " -a " + IRCD_TEST_HOST
		+ " </dev/null >>" + this.log_path + " 2>&1"
		+ " & echo $!";

	var lines = system.popen(cmd);
	if (!lines || !lines.length)
		throw new Error("IRCd start failed: no PID returned (check " + this.log_path + ")");

	this.pid = parseInt(lines[0]);
	if (!this.pid || isNaN(this.pid))
		throw new Error("IRCd start failed: bad PID '" + lines[0] + "'");

	/* Wait up to 5 seconds for the listening port to appear */
	var deadline = Date.now() + 5000;
	var ready    = false;
	while (Date.now() < deadline) {
		var s = new Socket();
		if (s.connect(IRCD_TEST_HOST, this.port, 0.5)) {
			s.close();
			ready = true;
			break;
		}
		s.close();
		mswait(100);
	}
	if (!ready)
		throw new Error("IRCd port " + this.port
			+ " not ready after 5s — check " + this.log_path);
	writeln("PID " + this.pid + " ready");
};

IrcdTestServer.prototype.stop = function() {
	if (this.pid) {
		write("  [ircd] stopping PID " + this.pid + "... ");
		system.popen("kill " + this.pid + " 2>/dev/null; true");
		mswait(300);
		this.pid = 0;
		writeln("done");
	}
	if (this.config_path && file_exists(this.config_path))
		file_remove(this.config_path);
};

/* Create a connected (and optionally registered) client */
IrcdTestServer.prototype.client = function(nick) {
	var c = new IrcdClient(IRCD_TEST_HOST, this.port);
	if (nick !== false)
		c.register(nick);
	return c;
};

/* ---- IRC test client ---- */

function IrcdClient(host, port) {
	this.host = host;
	this.port = port;
	this.nick = "";
	this.sock = new Socket();

	if (!this.sock.connect(host, port, 5))
		throw new Error("IrcdClient: connect " + host + ":" + port
			+ " failed: " + this.sock.error);
}

/* Send a raw IRC line (CRLF appended) */
IrcdClient.prototype.send = function(line) {
	this.sock.send(line + "\r\n");
};

/*
 * Read one IRC line, waiting up to timeout_sec seconds.
 * Returns the line without trailing CRLF.
 */
IrcdClient.prototype.readln = function(timeout_sec) {
	var t   = (timeout_sec !== undefined) ? timeout_sec : 3;
	var raw = this.sock.recvline(512, t);
	if (!raw || raw === "")
		throw new Error("IRC read timeout (" + t + "s)");
	return raw.replace(/\r?\n?$/, "");
};

/*
 * Read lines until one contains pattern (string) or matches it (RegExp).
 * Returns the matching line.  Throws if timeout_sec elapses first.
 * Automatically handles PING requests while waiting.
 */
IrcdClient.prototype.expect = function(pattern, timeout_sec) {
	var deadline = Date.now() + ((timeout_sec !== undefined ? timeout_sec : 5) * 1000);
	var seen     = [];
	var sock     = this.sock;
	while (Date.now() < deadline) {
		var remaining_sec = (deadline - Date.now()) / 1000;
		if (remaining_sec <= 0) break;
		var raw = sock.recvline(512, Math.min(remaining_sec, 1));
		if (!raw || raw === "") continue;
		var line = raw.replace(/\r?\n?$/, "");
		/* Auto-PONG so keep-alive never breaks tests */
		if (line.substr(0, 4) === "PING")
			sock.send("PONG " + line.substr(5) + "\r\n");
		seen.push(line);
		if (typeof pattern === "string" && line.indexOf(pattern) >= 0)
			return line;
		if (pattern instanceof RegExp && pattern.test(line))
			return line;
	}
	throw new Error("Expected " + pattern + " but got:\n  " + seen.join("\n  "));
};

/*
 * Discard pending input for up to drain_ms milliseconds.
 * Also handles PINGs.
 */
IrcdClient.prototype.drain = function(drain_ms) {
	var deadline = Date.now() + (drain_ms || 400);
	while (Date.now() < deadline) {
		var remaining_sec = (deadline - Date.now()) / 1000;
		if (remaining_sec <= 0.05) break;
		var raw = this.sock.recvline(512, Math.min(remaining_sec, 0.2));
		if (!raw || raw === "") break;
		var line = raw.replace(/\r?\n?$/, "");
		if (line.substr(0, 4) === "PING")
			this.sock.send("PONG " + line.substr(5) + "\r\n");
	}
};

/*
 * Register as a user: send NICK + USER and wait for 001.
 * Also drains the remainder of the welcome burst.
 */
IrcdClient.prototype.register = function(nick, user, realname) {
	this.nick = nick || ("t" + (Date.now() % 100000));
	user      = user      || "u";
	realname  = realname  || "Test";
	this.send("NICK " + this.nick);
	this.send("USER " + user + " 0 * :" + realname);
	this.expect(" 001 ", 5);   /* RPL_WELCOME */
	this.drain(500);            /* consume rest of welcome burst */
};

/* JOIN a channel and wait for the server echo */
IrcdClient.prototype.join = function(chan) {
	this.send("JOIN " + chan);
	this.expect("JOIN", 3);
	this.drain();
};

/* Graceful disconnect */
IrcdClient.prototype.quit = function() {
	try { this.send("QUIT :done"); } catch(e) {}
	this.sock.close();
};

/* ---- Network test infrastructure (6-server hub+leaf topology) ---- */

/*
 * Topology:
 *   hub1 (19700) ─┬─ hub2 (19701) ─┬─ leaf2a (19704)
 *                 │                 └─ leaf2b (19705)
 *                 ├─ leaf1a (19702)
 *                 └─ leaf1b (19703)
 *
 * Hub 2 and the four leaf servers initiate outbound connections to their
 * respective uplinks.  Hub 1 only accepts inbound connections.
 * ConnectFrequency=3 on Class:30 causes Reset_Autoconnect to fire with
 * delay=1ms on first config load (config.js:336), so linking is nearly
 * instantaneous once all ports are ready.
 */

var IRCD_NET_LINK_PASS = "netpassword";

var NET_HUB1   = { name: "hub1",   sname: "hub1.ircd.local",   port: 19700 };
var NET_HUB2   = { name: "hub2",   sname: "hub2.ircd.local",   port: 19701 };
var NET_LEAF1A = { name: "leaf1a", sname: "leaf1a.ircd.local", port: 19702 };
var NET_LEAF1B = { name: "leaf1b", sname: "leaf1b.ircd.local", port: 19703 };
var NET_LEAF2A = { name: "leaf2a", sname: "leaf2a.ircd.local", port: 19704 };
var NET_LEAF2B = { name: "leaf2b", sname: "leaf2b.ircd.local", port: 19705 };
var NET_ALL    = [NET_HUB1, NET_HUB2, NET_LEAF1A, NET_LEAF1B, NET_LEAF2A, NET_LEAF2B];

function IrcdTestNetwork() {
	this.pids     = {};
	this.cfgpaths = {};
	this.logpaths = {};
}

/*
 * server_sections: array of { sname, port, is_hub }
 *   port=0 (or omitted) → no Port line → no auto-connect (accept-only)
 *   port>0              → auto-connects to that address on startup
 *   is_hub=true         → Hub=true in config (server may introduce others)
 */
IrcdTestNetwork.prototype._write_config = function(srv, server_sections) {
	var i, s, lines;
	lines = [
		"[Info]",
		"Servername=" + srv.sname,
		"Description=IRCd Network Test - " + srv.name,
		"Admin1=Test Admin",
		"Admin2=localhost",
		"Admin3=testop@" + srv.sname,
		"",
		"[Port:" + srv.port + "]",
		"Default=true",
		"",
		"[Class:1]",
		"PingFrequency=3600",
		"ConnectFrequency=0",
		"Maximum=100",
		"SendQ=1000000",
		"",
		"[Class:10]",
		"PingFrequency=3600",
		"ConnectFrequency=0",
		"Maximum=10",
		"SendQ=2000000",
		"",
		/* ConnectFrequency=3 activates auto-connect for CLines that have a port */
		"[Class:30]",
		"PingFrequency=60",
		"ConnectFrequency=3",
		"Maximum=10",
		"SendQ=15000000",
		"",
		"[Allow]",
		"Mask=*@*",
		"Class=1",
		"",
		"[Operator:testop]",
		"Nick=testop",
		"Mask=*@*",
		"Password=testpass",
		"Flags=OwkKcC",
		"Class=10",
		""
	];

	for (i = 0; i < server_sections.length; i++) {
		s = server_sections[i];
		lines.push("[Server:" + s.sname + "]");
		lines.push("Servername=" + s.sname);
		lines.push("Hostname=127.0.0.1");
		if (s.port)
			lines.push("Port=" + s.port);
		lines.push("InboundPassword=" + IRCD_NET_LINK_PASS);
		lines.push("OutboundPassword=" + IRCD_NET_LINK_PASS);
		lines.push("Class=30");
		lines.push("Hub=" + (s.is_hub ? "true" : "false"));
		lines.push("");
	}

	var path = system.temp_dir + "ircd_net_" + srv.name + "_" + Date.now() + ".ini";
	var f = new File(path);
	if (!f.open("w"))
		throw new Error("Cannot write network config for " + srv.name + ": " + path);
	f.write(lines.join("\n"));
	f.close();
	return path;
};

IrcdTestNetwork.prototype._start_one = function(srv, server_sections) {
	var pid, s, ready, deadline, out, cmd;
	write("  [net] " + srv.name + " (:" + srv.port + ")... ");

	var cfg  = this._write_config(srv, server_sections);
	var log  = system.temp_dir + "ircd_net_" + srv.name + ".log";
	this.cfgpaths[srv.name] = cfg;
	this.logpaths[srv.name] = log;

	var ircd   = js.exec_dir + "../../ircd.js";
	var jsexec = js.exec_dir + "../../jsexec";
	var ctrl   = system.ctrl_dir;
	cmd = jsexec + " -c " + ctrl + " " + ircd
		+ " -f " + cfg
		+ " -a " + IRCD_TEST_HOST	/* see IrcdTestServer.start() */
		+ " </dev/null >>" + log + " 2>&1"
		+ " & echo $!";

	out = system.popen(cmd);
	if (!out || !out.length)
		throw new Error("start failed for " + srv.name + " (no PID)");
	pid = parseInt(out[0]);
	if (!pid || isNaN(pid))
		throw new Error("start failed for " + srv.name + " (bad PID: '" + out[0] + "')");
	this.pids[srv.name] = pid;

	deadline = Date.now() + 8000;
	ready    = false;
	while (Date.now() < deadline) {
		s = new Socket();
		if (s.connect(IRCD_TEST_HOST, srv.port, 0.5)) {
			s.close();
			ready = true;
			break;
		}
		s.close();
		mswait(200);
	}
	if (!ready)
		throw new Error(srv.name + " port " + srv.port
			+ " not ready after 8s — check " + log);
	writeln("PID " + pid + " ready");
};

IrcdTestNetwork.prototype.start = function() {
	/* Root hub first; downstream servers auto-connect on startup */
	this._start_one(NET_HUB1, [
		{ sname: NET_HUB2.sname,   port: 0,              is_hub: true  },
		{ sname: NET_LEAF1A.sname, port: 0,              is_hub: false },
		{ sname: NET_LEAF1B.sname, port: 0,              is_hub: false },
		/* fakeleaf: no port (no auto-connect), no hub flag (leaf) */
		{ sname: "fakeleaf.ircd.local",  port: 0, is_hub: false }
	]);
	/* hub2 connects outbound to hub1 */
	this._start_one(NET_HUB2, [
		{ sname: NET_HUB1.sname,   port: NET_HUB1.port,  is_hub: true  },
		{ sname: NET_LEAF2A.sname, port: 0,              is_hub: false },
		{ sname: NET_LEAF2B.sname, port: 0,              is_hub: false }
	]);
	/* leaves connect outbound to their hub */
	this._start_one(NET_LEAF1A, [
		{ sname: NET_HUB1.sname, port: NET_HUB1.port, is_hub: true }
	]);
	this._start_one(NET_LEAF1B, [
		{ sname: NET_HUB1.sname, port: NET_HUB1.port, is_hub: true }
	]);
	this._start_one(NET_LEAF2A, [
		{ sname: NET_HUB2.sname, port: NET_HUB2.port, is_hub: true }
	]);
	this._start_one(NET_LEAF2B, [
		{ sname: NET_HUB2.sname, port: NET_HUB2.port, is_hub: true }
	]);

	write("  [net] waiting for all 6 servers to link (up to 20s)... ");
	if (!this._wait_links(NET_HUB1.port, 6, 20))
		throw new Error("Network did not fully link within 20s"
			+ " — check logs in " + system.temp_dir);
	writeln("linked");
};

/* Poll LINKS on port until count x 364 are returned, or timeout */
IrcdTestNetwork.prototype._wait_links = function(port, count, timeout_sec) {
	var found, c, inner, raw, line, deadline;
	deadline = Date.now() + timeout_sec * 1000;
	while (Date.now() < deadline) {
		found = 0;
		try {
			c = new IrcdClient(IRCD_TEST_HOST, port);
			c.register("_lk" + (Date.now() % 10000));
			c.send("LINKS");
			inner = Date.now() + 3000;
			while (Date.now() < inner) {
				raw = c.sock.recvline(512, 1);
				if (!raw || raw === "") continue;
				line = raw.replace(/\r?\n?$/, "");
				if (line.indexOf(" 364 ") >= 0) found++;
				if (line.indexOf(" 365 ") >= 0) break;
			}
			c.quit();
		} catch(e) { /* server may not be ready yet */ }
		if (found >= count) return true;
		mswait(1000);
	}
	return false;
};

IrcdTestNetwork.prototype.stop = function() {
	var i, n;
	for (i = 0; i < NET_ALL.length; i++) {
		n = NET_ALL[i].name;
		if (this.pids[n]) {
			write("  [net] stopping " + n + " (PID " + this.pids[n] + ")... ");
			system.popen("kill " + this.pids[n] + " 2>/dev/null; true");
			mswait(200);
			this.pids[n] = 0;
			writeln("done");
		}
		if (this.cfgpaths[n] && file_exists(this.cfgpaths[n]))
			file_remove(this.cfgpaths[n]);
	}
};

/* Connect a registered client to the given network node */
IrcdTestNetwork.prototype.client = function(srv, nick) {
	var c = new IrcdClient(IRCD_TEST_HOST, srv.port);
	if (nick !== false)
		c.register(nick);
	return c;
};

/*
 * Poll WHOIS until nick appears (311) or timeout.
 * Used after connecting a remote client to wait for nick propagation.
 */
IrcdClient.prototype.wait_for_nick = function(nick, timeout_sec) {
	var deadline, found, inner, raw, line;
	deadline = Date.now() + (timeout_sec || 5) * 1000;
	while (Date.now() < deadline) {
		this.send("WHOIS " + nick);
		found = false;
		inner = Date.now() + 2000;
		while (Date.now() < inner) {
			raw = this.sock.recvline(512, 1);
			if (!raw || raw === "") continue;
			line = raw.replace(/\r?\n?$/, "");
			if (line.substr(0, 4) === "PING")
				this.sock.send("PONG " + line.substr(5) + "\r\n");
			if (line.indexOf(" 311 ") >= 0) found = true;
			if (line.indexOf(" 318 ") >= 0 || line.indexOf(" 401 ") >= 0) break;
		}
		if (found) return;
		mswait(200);
	}
	throw new Error("Nick " + nick + " not visible after "
		+ (timeout_sec || 5) + "s");
};

/* ---- Fake leaf server for security tests ---- */

/*
 * Opens a raw TCP connection to a hub and completes the PASS/SERVER handshake
 * as an untrusted leaf.  Used to simulate a malicious leaf sending crafted
 * server-to-server messages (e.g. a backdated SJOIN to steal channel ops).
 *
 * The hub's config must have a [Server:] section for this.sname with a
 * matching InboundPassword and Hub=false (no H:Line).
 */
function FakeLeafServer(hub_port, sname, pass) {
	this.hub_port = hub_port;
	this.sname    = sname || "fakeleaf.ircd.local";
	this.pass     = pass  || IRCD_NET_LINK_PASS;
	this.sock     = null;
}

/*
 * Connect to the hub, complete the handshake, and drain the burst.
 * Returns after seeing "BURST 0" from the hub (end-of-burst).
 */
FakeLeafServer.prototype.connect = function(timeout_sec) {
	var deadline, bursted, raw, line;
	timeout_sec = timeout_sec || 10;
	this.sock = new Socket();
	if (!this.sock.connect(IRCD_TEST_HOST, this.hub_port, 5))
		throw new Error("FakeLeafServer: connect to " + this.hub_port
			+ " failed: " + this.sock.error);

	/* Present ourselves as a leaf with the shared link password */
	this.sock.send("PASS " + this.pass + " :TS\r\n");
	this.sock.send("SERVER " + this.sname + " 1 :Fake Leaf\r\n");

	/* Drain hub's burst; BURST 0 signals end of burst */
	deadline = Date.now() + timeout_sec * 1000;
	bursted  = false;
	while (Date.now() < deadline) {
		raw = this.sock.recvline(512, 1);
		if (!raw || raw === "") continue;
		line = raw.replace(/\r?\n?$/, "");
		if (line.indexOf("PING") === 0)
			this.sock.send("PONG " + line.substr(5) + "\r\n");
		if (line.indexOf("ERROR") === 0)
			throw new Error("FakeLeafServer: hub rejected: " + line);
		if (line.indexOf("BURST 0") >= 0) {
			bursted = true;
			break;
		}
	}
	if (!bursted)
		throw new Error("FakeLeafServer: hub burst did not complete within "
			+ timeout_sec + "s");
};

FakeLeafServer.prototype.send = function(line) {
	this.sock.send(line + "\r\n");
};

FakeLeafServer.prototype.close = function() {
	try { if (this.sock) this.sock.close(); } catch(e) {}
	this.sock = null;
};

/* ---- Test runner ---- */

/*
 * Run an object of named test functions.  Each function is called with no
 * arguments; it should throw on failure.  All tests run regardless of earlier
 * failures.  Throws a summary Error at the end if any failed.
 */
function run_tests(tests) {
	var failures = [];
	var keys     = [];
	for (var k in tests) {
		if (tests.hasOwnProperty(k))
			keys.push(k);
	}
	for (var i = 0; i < keys.length; i++) {
		var name = keys[i];
		write("  [" + (i + 1) + "/" + keys.length + "] " + name + "... ");
		try {
			tests[name]();
			writeln("ok");
		} catch(e) {
			writeln("FAIL: " + e);
			failures.push(name + ": " + e);
		}
	}
	if (failures.length > 0)
		throw new Error(failures.length + " test(s) failed:\n- "
			+ failures.join("\n- "));
}
