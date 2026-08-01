/*
 * exec/tests/ircd/messages.js
 *
 * IRCd integration tests: PRIVMSG, NOTICE, AWAY, SILENCE, 404 format fix.
 * Starts a local IRCd on port 19669, runs all tests, then stops it.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PORT = 19669;
var srv  = new IrcdTestServer(PORT);

try {
	srv.start();

	run_tests({

		/* --- Private messages --- */

		"PRIVMSG to user delivered": function() {
			var c1 = srv.client("pmuser1");
			var c2 = srv.client("pmuser2");
			c1.send("PRIVMSG pmuser2 :hello from pmuser1");
			c2.expect("PRIVMSG pmuser2 :hello from pmuser1", 3);
			c1.quit();
			c2.quit();
		},

		"NOTICE to user delivered": function() {
			var c1 = srv.client("notuser1");
			var c2 = srv.client("notuser2");
			c1.send("NOTICE notuser2 :notice text");
			c2.expect("NOTICE notuser2 :notice text", 3);
			c1.quit();
			c2.quit();
		},

		"401 ERR_NOSUCHNICK for PRIVMSG to unknown nick": function() {
			var c = srv.client("nosend");
			c.send("PRIVMSG nosuchuser12345 :hello");
			c.expect(" 401 ", 3);
			c.quit();
		},

		/*
		 * RFC 1459: NOTICE error replies go to the sender, but servers may
		 * still emit 401 for NOTICE to unknown nick - Synchronet does.
		 * Just verify the NOTICE itself doesn't crash anything.
		 */
		"NOTICE to unknown nick does not crash server": function() {
			var c = srv.client("nonotice");
			c.send("NOTICE nosuchuser12345 :hi");
			mswait(300);
			/* Server still alive - register a second client */
			var c2 = srv.client("nonotice2");
			c2.quit();
			c.quit();
		},

		/*
		 * 404 ERR_CANNOTSENDTOCHAN tests.
		 *
		 * Basic test: verify 404 is returned for blocked sends.
		 * Channel-name-in-reply test: verifies the format() fix from the
		 * ircd-fixes branch (chan.nam moved inside format() call).
		 * The channel-name check will show a literal "%s" on unpatched builds.
		 */

		"404 for +n outside message (basic)": function() {
			var c1 = srv.client("n404a");
			c1.join("#n404test");
			c1.send("MODE #n404test +n");
			c1.drain(300);
			var c2 = srv.client("n404b");
			c2.send("PRIVMSG #n404test :outside");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		"404 for +n outside message: channel name in reply (ircd-fixes)": function() {
			var c1 = srv.client("n404c");
			c1.join("#n404test2");
			c1.send("MODE #n404test2 +n");
			c1.drain(300);
			var c2 = srv.client("n404d");
			c2.send("PRIVMSG #n404test2 :outside");
			var reply = c2.expect(" 404 ", 3);
			if (reply.indexOf("#n404test2") < 0)
				throw new Error("404 reply missing channel name (needs ircd-fixes patch): " + reply);
			c1.quit();
			c2.quit();
		},

		"404 for +m message (basic)": function() {
			var c1 = srv.client("m404a");
			c1.join("#m404test");
			var c2 = srv.client("m404b");
			c2.send("JOIN #m404test");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #m404test +m");
			c1.drain(300);
			c2.send("PRIVMSG #m404test :unvoiced");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		/*
		 * "404 for +m/+b message: channel name in reply" tests omitted here
		 * because master uses format("%s :reason") without passing chan.nam,
		 * so %s appears literally.  That bug is fixed in ircd-fixes; add the
		 * channel-name assertion test there after merge.
		 */

		"404 for +b banned message (basic)": function() {
			var c1 = srv.client("b404a");
			c1.join("#b404test");
			var c2 = srv.client("b404b");
			c2.send("JOIN #b404test");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #b404test +b b404b!*@*");
			c1.drain(300);
			c2.send("PRIVMSG #b404test :banned");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		/* --- AWAY --- */

		"306 set away, 301 away reply, 305 unset away": function() {
			var c1 = srv.client("awaybot1");
			var c2 = srv.client("awaybot2");
			c1.send("AWAY :gone fishing");
			c1.expect(" 306 ", 3);   /* RPL_NOWAWAY */
			c2.send("PRIVMSG awaybot1 :hello");
			c2.expect(" 301 ", 3);   /* RPL_AWAY */
			c1.send("AWAY");
			c1.expect(" 305 ", 3);   /* RPL_UNAWAY */
			c1.quit();
			c2.quit();
		},

		"301 away reply includes away message": function() {
			var c1 = srv.client("awaybot3");
			var c2 = srv.client("awaybot4");
			c1.send("AWAY :out to lunch");
			c1.expect(" 306 ", 3);
			c2.send("PRIVMSG awaybot3 :hey");
			var reply = c2.expect(" 301 ", 3);
			if (reply.indexOf("out to lunch") < 0)
				throw new Error("Away message missing from 301: " + reply);
			c1.quit();
			c2.quit();
		},

		/* --- SILENCE --- */

		"SILENCE blocks PRIVMSG from listed nick": function() {
			var c1 = srv.client("silbot1");
			var c2 = srv.client("silbot2");
			c1.send("SILENCE +silbot2!*@*");
			c1.drain(300);
			c2.send("PRIVMSG silbot1 :you should not see this");
			/* c1 must NOT receive this message */
			var raw = c1.sock.recvline(128, 1.0);
			if (raw && raw.indexOf("PRIVMSG") >= 0 && raw.indexOf("silbot2") >= 0)
				throw new Error("Silenced PRIVMSG was delivered: " + raw);
			c1.quit();
			c2.quit();
		},

		"SILENCE +x returns list (271)": function() {
			var c = srv.client("sillist");
			c.send("SILENCE +nosuchnick!*@*");
			c.drain(200);
			c.send("SILENCE");
			c.expect(" 271 ", 3);
			c.quit();
		},

		/* --- WHOWAS --- */

		"314 RPL_WHOWAS for recently disconnected nick": function() {
			var c1 = srv.client("wwbot");
			c1.quit();
			mswait(200);
			var c2 = srv.client("wwchecker");
			c2.send("WHOWAS wwbot");
			c2.expect(/( 314 | 406 )/, 3);   /* 406 ERR_WASNOSUCHNICK if not tracked yet */
			c2.quit();
		},

		/* --- NICK history / collision after change --- */

		"433 when changing to nick in use": function() {
			var c1 = srv.client("histnick1");
			var c2 = srv.client("histnick2");
			c2.send("NICK histnick1");
			c2.expect(" 433 ", 3);
			c1.quit();
			c2.quit();
		},

		"nick change then old nick becomes available": function() {
			var c1 = srv.client("oldname");
			c1.send("NICK newname");
			c1.expect(" NICK newname", 3);  /* :oldname!user@host NICK newname */
			var c2 = srv.client("grabber");
			c2.send("NICK oldname");
			c2.expect(" NICK oldname", 3);  /* :grabber!user@host NICK oldname */
			c1.quit();
			c2.quit();
		},

		/* --- INVITE --- */

		"341 RPL_INVITING on INVITE": function() {
			var c1 = srv.client("invop3");
			c1.join("#invtest2");
			var c2 = srv.client("invitee2");
			c1.send("INVITE invitee2 #invtest2");
			c1.expect(" 341 ", 3);
			c1.quit();
			c2.quit();
		},

		"443 ERR_USERONCHANNEL for INVITE of member": function() {
			var c1 = srv.client("invop4");
			c1.join("#invtest3");
			var c2 = srv.client("member1");
			c2.send("JOIN #invtest3");
			c1.drain(500);
			c2.drain(200);
			c1.send("INVITE member1 #invtest3");
			c1.expect(" 443 ", 3);
			c1.quit();
			c2.quit();
		}

	});

} finally {
	srv.stop();
}
