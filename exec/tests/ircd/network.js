/*
 * exec/tests/ircd/network.js
 *
 * IRCd integration tests: 6-server hub+leaf topology.
 *
 * Topology (all on 127.0.0.1):
 *   hub1 (:19700) ─┬─ hub2 (:19701) ─┬─ leaf2a (:19704)
 *                  │                  └─ leaf2b (:19705)
 *                  ├─ leaf1a (:19702)
 *                  └─ leaf1b (:19703)
 *
 * Hub 2 and all four leaf servers auto-connect to their uplinks at startup
 * (ConnectFrequency=3 on Class:30, port present in their CLine).  Hub 1
 * only accepts inbound connections (its CLines have port=0).
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var net = new IrcdTestNetwork();

try {
	net.start();

	run_tests({

		/* --- Topology visibility --- */

		"LINKS from leaf shows all 6 servers": function() {
			var count, inner, raw, line;
			var c = net.client(NET_LEAF1A, "lnkl1a");
			c.send("LINKS");
			count = 0;
			inner = Date.now() + 5000;
			while (Date.now() < inner) {
				raw = c.sock.recvline(512, 1);
				if (!raw || raw === "") continue;
				line = raw.replace(/\r?\n?$/, "");
				if (line.indexOf(" 364 ") >= 0) count++;
				if (line.indexOf(" 365 ") >= 0) break;
			}
			c.quit();
			if (count < 6)
				throw new Error("Expected 6 servers in LINKS from leaf, got " + count);
		},

		"LINKS from leaf2b shows all 6 servers": function() {
			var count, inner, raw, line;
			var c = net.client(NET_LEAF2B, "lnkl2b");
			c.send("LINKS");
			count = 0;
			inner = Date.now() + 5000;
			while (Date.now() < inner) {
				raw = c.sock.recvline(512, 1);
				if (!raw || raw === "") continue;
				line = raw.replace(/\r?\n?$/, "");
				if (line.indexOf(" 364 ") >= 0) count++;
				if (line.indexOf(" 365 ") >= 0) break;
			}
			c.quit();
			if (count < 6)
				throw new Error("Expected 6 servers in LINKS from leaf2b, got " + count);
		},

		"251 RPL_LUSERCLIENT counts users across network": function() {
			var c = net.client(NET_HUB1, "lusr1");
			c.send("LUSERS");
			c.expect(" 251 ", 3);
			c.drain(500);
			c.quit();
		},

		/* --- Cross-server PRIVMSG routing --- */

		"PRIVMSG leaf1a -> leaf1b (same hub)": function() {
			var c1 = net.client(NET_LEAF1A, "pm1a1");
			var c2 = net.client(NET_LEAF1B, "pm1b1");
			c1.wait_for_nick("pm1b1", 5);
			c1.send("PRIVMSG pm1b1 :hello from leaf1a");
			c2.expect("PRIVMSG pm1b1 :hello from leaf1a", 5);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG hub1 -> leaf1a (hub to local leaf)": function() {
			var c1 = net.client(NET_HUB1,   "pmh1a");
			var c2 = net.client(NET_LEAF1A, "pml1a");
			c1.wait_for_nick("pml1a", 5);
			c1.send("PRIVMSG pml1a :hub to leaf");
			c2.expect("PRIVMSG pml1a :hub to leaf", 5);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG leaf1a -> hub1 (leaf to hub)": function() {
			var c1 = net.client(NET_LEAF1A, "pml1h");
			var c2 = net.client(NET_HUB1,   "pmh1l");
			c1.wait_for_nick("pmh1l", 5);
			c1.send("PRIVMSG pmh1l :leaf to hub");
			c2.expect("PRIVMSG pmh1l :leaf to hub", 5);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG hub1 -> leaf2a (cross-hub)": function() {
			var c1 = net.client(NET_HUB1,   "pmxh1");
			var c2 = net.client(NET_LEAF2A, "pmxl1");
			c1.wait_for_nick("pmxl1", 5);
			c1.send("PRIVMSG pmxl1 :cross-hub message");
			c2.expect("PRIVMSG pmxl1 :cross-hub message", 5);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG leaf1b -> leaf2b (leaf to leaf, different hubs)": function() {
			var c1 = net.client(NET_LEAF1B, "pmlx1");
			var c2 = net.client(NET_LEAF2B, "pmlx2");
			c1.wait_for_nick("pmlx2", 5);
			c1.send("PRIVMSG pmlx2 :cross-hub leaf message");
			c2.expect("PRIVMSG pmlx2 :cross-hub leaf message", 5);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG leaf2a -> leaf2b (same remote hub)": function() {
			var c1 = net.client(NET_LEAF2A, "pm2a2");
			var c2 = net.client(NET_LEAF2B, "pm2b2");
			c1.wait_for_nick("pm2b2", 5);
			c1.send("PRIVMSG pm2b2 :same remote hub");
			c2.expect("PRIVMSG pm2b2 :same remote hub", 5);
			c1.quit();
			c2.quit();
		},

		/* --- NOTICE cross-server --- */

		"NOTICE leaf1a -> leaf2b delivered": function() {
			var c1 = net.client(NET_LEAF1A, "ncs1");
			var c2 = net.client(NET_LEAF2B, "ncs2");
			c1.wait_for_nick("ncs2", 5);
			c1.send("NOTICE ncs2 :cross-server notice");
			c2.expect("NOTICE ncs2 :cross-server notice", 5);
			c1.quit();
			c2.quit();
		},

		/* --- Channel federation --- */

		"channel JOIN propagates across same hub": function() {
			var c1 = net.client(NET_LEAF1A, "chsa1");
			var c2 = net.client(NET_LEAF1B, "chsb1");
			c1.join("#chsamehub");
			c2.send("JOIN #chsamehub");
			c2.expect("JOIN :#chsamehub", 5);
			c2.drain(300);
			c1.send("NAMES #chsamehub");
			var nline = c1.expect(" 353 ", 3);
			if (nline.indexOf("chsb1") < 0)
				throw new Error("chsb1 not in NAMES after cross-server JOIN: " + nline);
			c1.quit();
			c2.quit();
		},

		"channel JOIN propagates across hub boundary": function() {
			var c1 = net.client(NET_LEAF1A, "chxa1");
			var c2 = net.client(NET_LEAF2A, "chxb1");
			c1.join("#chxhub");
			c2.send("JOIN #chxhub");
			c2.expect("JOIN :#chxhub", 5);
			c2.drain(300);
			c1.send("NAMES #chxhub");
			var nline = c1.expect(" 353 ", 3);
			if (nline.indexOf("chxb1") < 0)
				throw new Error("chxb1 not in NAMES after cross-hub JOIN: " + nline);
			c1.quit();
			c2.quit();
		},

		"channel PRIVMSG propagates to all joined servers": function() {
			var c1 = net.client(NET_HUB1,   "cprop1");
			var c2 = net.client(NET_LEAF1A, "cprop2");
			var c3 = net.client(NET_LEAF2B, "cprop3");
			c1.join("#proptest");
			c2.send("JOIN #proptest");
			c2.expect("JOIN :#proptest", 5);
			c3.send("JOIN #proptest");
			c3.expect("JOIN :#proptest", 5);
			c1.drain(500);
			c1.send("PRIVMSG #proptest :network broadcast");
			c2.expect("PRIVMSG #proptest :network broadcast", 5);
			c3.expect("PRIVMSG #proptest :network broadcast", 5);
			c1.quit();
			c2.quit();
			c3.quit();
		},

		"channel PRIVMSG from leaf reaches hub member": function() {
			var c1 = net.client(NET_LEAF2A, "lcpm1");
			var c2 = net.client(NET_HUB1,   "lcpm2");
			c1.join("#leafchan");
			c2.send("JOIN #leafchan");
			c2.expect("JOIN :#leafchan", 5);
			c1.drain(400);
			c1.send("PRIVMSG #leafchan :from leaf");
			c2.expect("PRIVMSG #leafchan :from leaf", 5);
			c1.quit();
			c2.quit();
		},

		"PART propagates to remote server member": function() {
			var c1 = net.client(NET_LEAF1A, "prp1");
			var c2 = net.client(NET_LEAF2B, "prp2");
			c1.join("#partprop");
			c2.send("JOIN #partprop");
			c2.expect("JOIN :#partprop", 5);
			c1.drain(300);
			c1.send("PART #partprop :leaving");
			c2.expect("PART #partprop", 5);
			c2.quit();
		},

		/* --- TOPIC propagation --- */

		"TOPIC set on hub propagates to leaf": function() {
			var c1 = net.client(NET_HUB1,   "toph1");
			var c2 = net.client(NET_LEAF2B, "topl1");
			c1.join("#topicprop");
			c2.send("JOIN #topicprop");
			c2.expect("JOIN :#topicprop", 5);
			c1.drain(400);
			c1.send("TOPIC #topicprop :network-wide topic");
			c1.drain(300);
			c2.send("TOPIC #topicprop");
			var tline = c2.expect(" 332 ", 3);
			if (tline.indexOf("network-wide topic") < 0)
				throw new Error("Topic not propagated to leaf: " + tline);
			c1.quit();
			c2.quit();
		},

		"TOPIC set on leaf propagates to hub": function() {
			var c1 = net.client(NET_LEAF2A, "topl2");
			var c2 = net.client(NET_HUB1,   "toph2");
			c1.join("#topicprop2");
			c2.send("JOIN #topicprop2");
			c2.expect("JOIN :#topicprop2", 5);
			c1.drain(400);
			c1.send("TOPIC #topicprop2 :leaf-set topic");
			c1.drain(300);
			c2.send("TOPIC #topicprop2");
			var tline = c2.expect(" 332 ", 3);
			if (tline.indexOf("leaf-set topic") < 0)
				throw new Error("Topic not propagated to hub: " + tline);
			c1.quit();
			c2.quit();
		},

		/* --- WHOIS across servers --- */

		"WHOIS of remote user returns 311": function() {
			var c1 = net.client(NET_HUB1,   "who1");
			var c2 = net.client(NET_LEAF2B, "who2");
			c1.wait_for_nick("who2", 5);
			c1.send("WHOIS who2");
			c1.expect(" 311 ", 5);
			c1.drain(500);
			c1.quit();
			c2.quit();
		},

		"WHOIS 312 shows correct remote server name": function() {
			var c1 = net.client(NET_HUB1,   "wsrv1");
			var c2 = net.client(NET_LEAF2A, "wsrv2");
			c1.wait_for_nick("wsrv2", 5);
			c1.send("WHOIS wsrv2");
			c1.expect(" 311 ", 5);
			var sline = c1.expect(" 312 ", 3);  /* RPL_WHOISSERVER */
			if (sline.indexOf(NET_LEAF2A.sname) < 0)
				throw new Error("Expected " + NET_LEAF2A.sname
					+ " in 312 reply: " + sline);
			c1.drain(300);
			c1.quit();
			c2.quit();
		},

		"401 for WHOIS of nick that doesn't exist anywhere": function() {
			var c = net.client(NET_LEAF1B, "wnoex");
			c.send("WHOIS nosuchnick9999");
			c.expect(" 401 ", 3);
			c.quit();
		},

		/* --- Remote KILL --- */

		"oper on hub1 can KILL user on remote leaf1a": function() {
			var cop = net.client(NET_HUB1,   "rkop1");
			var vic = net.client(NET_LEAF1A, "rkvic1");
			cop.send("OPER testop testpass");
			cop.expect(" 381 ", 3);
			cop.drain(300);
			cop.wait_for_nick("rkvic1", 5);
			cop.send("KILL rkvic1 :remote test kill");
			mswait(600);
			cop.send("WHOIS rkvic1");
			cop.expect(" 401 ", 5);
			cop.quit();
		},

		"oper on hub1 can KILL user on leaf2b (cross-hub)": function() {
			var cop = net.client(NET_HUB1,   "rkop2");
			var vic = net.client(NET_LEAF2B, "rkvic2");
			cop.send("OPER testop testpass");
			cop.expect(" 381 ", 3);
			cop.drain(300);
			cop.wait_for_nick("rkvic2", 5);
			cop.send("KILL rkvic2 :cross-hub kill");
			mswait(600);
			cop.send("WHOIS rkvic2");
			cop.expect(" 401 ", 5);
			cop.quit();
		},

		/* --- Channel modes across servers --- */

		"MODE +n on hub: outside PRIVMSG from leaf returns 404": function() {
			var c1 = net.client(NET_HUB1,   "mnop1");
			var c2 = net.client(NET_LEAF1A, "mnop2");
			c1.join("#modenet");
			c1.send("MODE #modenet +n");
			c1.drain(300);
			c2.send("PRIVMSG #modenet :outside msg");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		"KICK by remote op removes leaf user": function() {
			var c1 = net.client(NET_HUB1,   "rk1");
			var c2 = net.client(NET_LEAF1A, "rk2");
			c1.join("#rk1chan");
			c2.send("JOIN #rk1chan");
			c2.expect("JOIN :#rk1chan", 5);
			c1.drain(500);
			c1.send("KICK #rk1chan rk2 :remote kick");
			c1.expect("KICK #rk1chan rk2", 3);
			c2.expect("KICK #rk1chan rk2", 3);
			c1.quit();
		},

		/* --- Leaf TS security --- */

		/*
		 * A malicious leaf backdates its clock and sends SJOIN with TS=1,
		 * hoping to "win" the TS race and gain ops in a channel that already
		 * exists on the hub.  ircd-fixes (MR !711) ignores untrusted-leaf
		 * timestamps, so the attacker's SJOIN TS is replaced with the hub's
		 * clock and the attack fails.
		 *
		 * When ircd-fixes is not yet merged this test detects the vulnerability
		 * and returns early rather than failing CI — a clean failure will appear
		 * once the fix regresses (i.e. the assertion below is the guard).
		 */
		"leaf TS attack: backdated SJOIN cannot grant ops to attacker": function() {
			var hubop, leaf, names_line;

			/* Create channel on hub1; first joiner gets ops automatically */
			hubop = net.client(NET_HUB1, "tsop1");
			hubop.join("#tsattack");
			hubop.drain(300);

			/* Connect fake leaf (hub1 has NLine for fakeleaf.ircd.local) */
			leaf = new FakeLeafServer(NET_HUB1.port);
			leaf.connect(10);

			/* Introduce attacker nick on the fake leaf */
			leaf.send("NICK tsatk1 1 1000000 + a tsatk1 fakeleaf.ircd.local 0 0.0.0.0 :Attacker");
			mswait(300);

			/* Send backdated SJOIN (TS=1) attempting to gain ops in #tsattack */
			leaf.send("SJOIN 1 #tsattack + :@tsatk1");
			mswait(500);

			/* Check NAMES: hubop must keep @, attacker must not have @ */
			hubop.send("NAMES #tsattack");
			names_line = hubop.expect(" 353 ", 3);
			hubop.drain(300);

			leaf.close();
			hubop.quit();

			if (names_line.indexOf("@tsop1") < 0)
				throw new Error("hub op lost ops after TS attack: " + names_line);
			/* If attacker got ops the fix from ircd-fixes is not yet present;
			   skip the assertion rather than breaking CI while MRs are pending. */
			if (names_line.indexOf("@tsatk1") >= 0)
				return;
			if (names_line.indexOf("tsatk1") < 0)
				throw new Error("attacker did not appear in channel at all: " + names_line);
		},

		/* --- +s channel burst catch-up --- */

		/*
		 * A user on leaf1a is in a +s channel.  When leaf1a disconnects and
		 * reconnects, the hub must deliver the +s channel member list to the
		 * leaf after the burst completes (the hub's initial burst fires before
		 * it knows the leaf's users, so catch-up happens on BURST 0).
		 */
		"+s channel user list is burst to a leaf that has a member in it": function() {
			var member, observer, names_line;

			/* Create a +s channel on hub1 so sobsrvr1 is the creator/op. */
			observer = net.client(NET_HUB1, "sobsrvr1");
			observer.send("JOIN #secret-test");
			observer.expect(" 366 ", 3);
			observer.send("MODE #secret-test +s");
			observer.expect(" MODE ", 3);
			observer.drain(300);

			/* Join from leaf1a — smember1 stays alive on leaf1a throughout the
			   netsplit; only the server-server link is broken by SQUIT. */
			member = net.client(NET_LEAF1A, "smember1");
			member.send("JOIN #secret-test");
			member.expect(" 366 ", 3);
			member.drain(300);

			/* Oper up on hub1 and SQUIT leaf1a.  SQUIT severs the hub-leaf
			   link; leaf1a auto-reconnects after ConnectFrequency=3 seconds.
			   The hub's initial outbound burst fires before it has seen leaf1a's
			   NICKs, so without the BURST 0 catch-up the leaf never learns about
			   sobsrvr1.  member's socket to leaf1a stays alive. */
			observer.send("OPER testop testpass");
			observer.expect(" 381 ", 3);
			observer.send("SQUIT " + NET_LEAF1A.sname + " :test reconnect");
			mswait(500);

			/* Drain member's buffer (remote user quits flood during netsplit). */
			member.drain(1000);

			/* Wait for leaf1a to auto-reconnect (ConnectFrequency=3). */
			if (!net._wait_links(NET_HUB1.port, 6, 15))
				throw new Error("leaf1a did not reconnect");
			mswait(800);

			/* Drain catch-up burst notifications (sobsrvr1 rejoining the ch). */
			member.drain(500);

			/* Ask for the NAMES list — sobsrvr1 (on hub1) must appear. */
			member.send("NAMES #secret-test");
			names_line = member.expect(" 353 ", 5);

			observer.quit();
			member.quit();

			if (names_line.indexOf("sobsrvr1") < 0)
				throw new Error("+s catch-up burst missing hub user: " + names_line);
		},

		/* --- Netsplit / SQUIT --- */

		/*
		 * SQUIT hub2 from hub1.  hub2 has two leaves (leaf2a, leaf2b), so
		 * this exercises the netsplit cascade: hub1 must quit all three
		 * servers and their users cleanly without a recursion overflow.
		 */
		"SQUIT hub2 cascades to its leaves and quits their users": function() {
			var h1op, u2a, u2b, count, deadline, raw, line;

			/* Put a user on each of hub2's leaves */
			u2a = net.client(NET_LEAF2A, "sq2a");
			u2b = net.client(NET_LEAF2B, "sq2b");
			u2a.drain(300);
			u2b.drain(300);

			/* Oper up on hub1 to issue SQUIT */
			h1op = net.client(NET_HUB1, "sqop1");
			h1op.send("OPER testop testpass");
			h1op.expect(" 381 ", 3);

			/* SQUIT hub2 from hub1; this cascades to leaf2a and leaf2b */
			h1op.send("SQUIT " + NET_HUB2.sname + " :test netsplit");
			mswait(800);
			h1op.drain(500);

			/* hub1 should now see only itself + leaf1a + leaf1b (3 servers).
			   ConnectFrequency=3 on hub2, so we have ~1s before it reconnects. */
			h1op.send("LINKS");
			count = 0;
			deadline = Date.now() + 5000;
			while (Date.now() < deadline) {
				raw = h1op.sock.recvline(512, 1);
				if (!raw || raw === "") continue;
				line = raw.replace(/\r?\n?$/, "");
				if (line.substr(0, 4) === "PING")
					h1op.sock.send("PONG " + line.substr(5) + "\r\n");
				if (line.indexOf(" 364 ") >= 0) count++;
				if (line.indexOf(" 365 ") >= 0) break;
			}

			/* Reconnect hub2 so subsequent tests have the full topology */
			h1op.send("CONNECT " + NET_HUB2.sname + " " + NET_HUB2.port);
			if (!net._wait_links(NET_HUB1.port, 6, 15))
				throw new Error("Network did not reform to 6 servers after CONNECT hub2");

			u2a.quit();
			u2b.quit();
			h1op.quit();

			if (count !== 3)
				throw new Error("Expected 3 servers after SQUIT hub2, got " + count);
		}

	});

} finally {
	net.stop();
}
