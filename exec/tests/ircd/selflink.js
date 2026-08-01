/*
 * exec/tests/ircd/selflink.js
 *
 * IRCd regression tests: a server must never link to itself.
 *
 * A [Server] section naming the running server produced a link whose nick and
 * linkparent were both our own name.  netsplit() quits every server whose
 * linkparent matches, so that link quit itself, netsplit again, forever --
 * killing the IRCd with "InternalError: too much recursion" the moment the
 * link dropped.
 *
 * Starts one IRCd on port 19710 configured to auto-connect to itself.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var SELF = { name: "selflink", sname: "selflink.ircd.local", port: 19710 };

var net = new IrcdTestNetwork();

/* Connect + register a client; proves the IRCd is still serving. */
function assert_alive(why) {
	var c;
	try {
		c = new IrcdClient(IRCD_TEST_HOST, SELF.port);
		c.register("alivechk" + (Date.now() % 10000));
		c.quit();
	} catch(e) {
		throw new Error("IRCd is not serving clients " + why + ": " + e);
	}
}

/* Count the 364 (RPL_LINKS) replies to a LINKS query. */
function count_links() {
	var c, raw, line, deadline, found;
	c = new IrcdClient(IRCD_TEST_HOST, SELF.port);
	c.register("lnkchk" + (Date.now() % 10000));
	c.send("LINKS");
	found    = 0;
	deadline = Date.now() + 3000;
	while (Date.now() < deadline) {
		raw = c.sock.recvline(512, 1);
		if (!raw || raw === "")
			continue;
		line = raw.replace(/\r?\n?$/, "");
		if (line.indexOf(" 364 ") >= 0)
			found++;
		if (line.indexOf(" 365 ") >= 0)
			break;
	}
	c.quit();
	return found;
}

function log_contains(str) {
	var f, text;
	f = new File(net.logpaths[SELF.name]);
	if (!f.open("r"))
		throw new Error("Cannot read IRCd log: " + net.logpaths[SELF.name]);
	text = f.read();
	f.close();
	return text.indexOf(str) >= 0;
}

try {
	/*
	 * The [Server] section names this very server, on its own listening port,
	 * in an auto-connecting class (Class:30, ConnectFrequency=3) -- exactly the
	 * misconfiguration that crashed a production server.
	 */
	net._start_one(SELF, [
		{ sname: SELF.sname, port: SELF.port, is_hub: true }
	]);

	/* Give auto-connect time to fire (and, before the fix, to crash us). */
	mswait(6000);

	run_tests({

		"self-referencing [Server] section does not crash the IRCd": function() {
			assert_alive("after the self-connect window");
		},

		"self-referencing [Server] section is reported to the sysop": function() {
			if (!log_contains("link to itself"))
				throw new Error("no self-link warning in "
					+ net.logpaths[SELF.name]);
		},

		"IRCd has not linked itself": function() {
			var n = count_links();
			if (n !== 1)
				throw new Error("expected 1 server in LINKS, got " + n);
		},

		"inbound link claiming our own server name is refused": function() {
			var fake, refused;
			fake    = new FakeLeafServer(SELF.port, SELF.sname);
			refused = false;
			try {
				fake.connect(5);
			} catch(e) {
				refused = true;
			}
			fake.close();
			if (!refused)
				throw new Error("IRCd accepted a link using its own name");
			assert_alive("after refusing a self-named link");
		},

		"IRCd survives the refused link dropping": function() {
			mswait(1000);
			assert_alive("after the refused link closed");
			if (count_links() !== 1)
				throw new Error("stray server left in LINKS");
		}

	});
} finally {
	if (net.pids[SELF.name]) {
		system.popen("kill " + net.pids[SELF.name] + " 2>/dev/null; true");
		mswait(200);
	}
	if (net.cfgpaths[SELF.name] && file_exists(net.cfgpaths[SELF.name]))
		file_remove(net.cfgpaths[SELF.name]);
}
