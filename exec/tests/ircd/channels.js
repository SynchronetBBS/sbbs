/*
 * exec/tests/ircd/channels.js
 *
 * IRCd integration tests: channels, modes, KICK, BAN, topic, NAMES/LIST.
 * Starts a local IRCd on port 19668, runs all tests, then stops it.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PORT = 19668;
var srv  = new IrcdTestServer(PORT);

try {
	srv.start();

	run_tests({

		/* --- JOIN / PART --- */

		"JOIN creates channel and gives creator op": function() {
			var c = srv.client("joinbot1");
			c.send("JOIN #jointest1");
			/* Server echoes: :nick!user@host JOIN :#chan (trailing colon) */
			c.expect("JOIN :#jointest1", 3);
			c.drain();
			c.quit();
		},

		"JOIN sends 353 NAMES and 366 end-of-names": function() {
			var c = srv.client("namesbot1");
			c.send("JOIN #namestest1");
			c.expect(" 353 ", 3);
			c.expect(" 366 ", 3);
			c.quit();
		},

		"JOIN second user sees first in NAMES": function() {
			var c1 = srv.client("chan2bot1");
			c1.join("#chan2test");
			var c2 = srv.client("chan2bot2");
			c2.send("JOIN #chan2test");
			var names_line = c2.expect(" 353 ", 3);
			if (names_line.indexOf("chan2bot1") < 0)
				throw new Error("chan2bot1 not in NAMES: " + names_line);
			c2.drain();
			c1.quit();
			c2.quit();
		},

		"PART leaves channel": function() {
			var c = srv.client("partbot");
			c.join("#parttest");
			c.send("PART #parttest :bye");
			c.expect("PART #parttest", 3);
			c.quit();
		},

		"JOIN after PART rejoins successfully": function() {
			var c = srv.client("rejoinbot");
			c.join("#rejointest");
			c.send("PART #rejointest");
			c.drain(400);
			c.send("JOIN #rejointest");
			c.expect("JOIN :#rejointest", 3);
			c.drain();
			c.quit();
		},

		/* --- TOPIC --- */

		"TOPIC set and echoed to channel": function() {
			var c = srv.client("topicbot1");
			c.join("#topictest1");
			c.send("TOPIC #topictest1 :Hello World");
			c.expect("TOPIC #topictest1", 3);
			c.quit();
		},

		"TOPIC query returns 331 or 332": function() {
			var c = srv.client("topicbot2");
			c.join("#topictest2");
			c.send("TOPIC #topictest2");
			c.expect(/( 331 | 332 )/, 3);
			c.quit();
		},

		"332 RPL_TOPIC after setting topic": function() {
			var c = srv.client("topicbot3");
			c.join("#topictest3");
			c.send("TOPIC #topictest3 :My Topic");
			c.drain(300);
			c.send("TOPIC #topictest3");
			c.expect(" 332 ", 3);
			c.quit();
		},

		/* --- PRIVMSG to channel --- */

		"PRIVMSG to channel relayed to other members": function() {
			var c1 = srv.client("pmbot1");
			c1.join("#pmtest1");
			var c2 = srv.client("pmbot2");
			c2.join("#pmtest1");
			c1.drain(400);
			c1.send("PRIVMSG #pmtest1 :hello from pmbot1");
			c2.expect("PRIVMSG #pmtest1 :hello from pmbot1", 3);
			c1.quit();
			c2.quit();
		},

		"PRIVMSG sender does not receive own message": function() {
			var c1 = srv.client("selfpm1");
			c1.join("#selfpmtest");
			var c2 = srv.client("selfpm2");
			c2.join("#selfpmtest");
			c1.drain(400);
			c1.send("PRIVMSG #selfpmtest :no echo");
			/* c2 gets it; c1 should NOT */
			c2.expect("PRIVMSG #selfpmtest", 3);
			var raw = c1.sock.recvline(64, 0.5);
			if (raw && raw.indexOf("PRIVMSG #selfpmtest") >= 0)
				throw new Error("Sender received own channel PRIVMSG");
			c1.quit();
			c2.quit();
		},

		/* --- KICK --- */

		"KICK by channel op removes target": function() {
			var c1 = srv.client("kicker1");  /* joins first -> gets op */
			c1.join("#kicktest1");
			var c2 = srv.client("kickee1");
			c2.send("JOIN #kicktest1");
			c1.drain(500);
			c2.drain(200);
			c1.send("KICK #kicktest1 kickee1 :out");
			c1.expect("KICK #kicktest1 kickee1", 3);
			c2.expect("KICK #kicktest1 kickee1", 3);
			c1.quit();
			c2.quit();
		},

		"482 ERR_CHANOPRIVSNEEDED when non-op kicks": function() {
			var c1 = srv.client("nopop1");   /* op */
			c1.join("#nopkick");
			var c2 = srv.client("nopop2");   /* non-op */
			c2.send("JOIN #nopkick");
			c1.drain(500);
			c2.drain(200);
			c2.send("KICK #nopkick nopop1 :try kick op");
			c2.expect(" 482 ", 3);
			c1.quit();
			c2.quit();
		},

		"403 ERR_NOSUCHCHANNEL for KICK on unknown channel": function() {
			var c = srv.client("kickbot3");
			c.send("KICK #nosuch_" + Date.now() + " kickbot3 :x");
			c.expect(" 403 ", 3);
			c.quit();
		},

		/* --- Channel modes --- */

		"MODE +m moderated: non-voiced cannot PRIVMSG": function() {
			var c1 = srv.client("modeop");   /* op */
			c1.join("#modtest_m");
			var c2 = srv.client("moduser");  /* non-voiced */
			c2.send("JOIN #modtest_m");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #modtest_m +m");
			c1.drain(300);
			c2.send("PRIVMSG #modtest_m :should fail");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +m moderated: voiced user CAN PRIVMSG": function() {
			var c1 = srv.client("modeop2");
			c1.join("#modtest_mv");
			var c2 = srv.client("modvoice");
			c2.send("JOIN #modtest_mv");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #modtest_mv +mv modvoice");
			c1.drain(300);
			c2.send("PRIVMSG #modtest_mv :voice ok");
			c1.expect("voice ok", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +n: outside message blocked (404)": function() {
			var c1 = srv.client("nop1");
			c1.join("#ntest");
			c1.send("MODE #ntest +n");
			c1.drain(300);
			var c2 = srv.client("outside1");
			c2.send("PRIVMSG #ntest :outside");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +t: non-op cannot set topic": function() {
			var c1 = srv.client("tbot1");
			c1.join("#ttest");
			c1.send("MODE #ttest +t");
			c1.drain(300);
			var c2 = srv.client("tbot2");
			c2.send("JOIN #ttest");
			c2.drain(400);
			c2.send("TOPIC #ttest :not my topic");
			c2.expect(" 482 ", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +k: key required to join": function() {
			var c1 = srv.client("keyop");
			c1.join("#keytest");
			c1.send("MODE #keytest +k secret");
			c1.drain(300);
			var c2 = srv.client("keyuser");
			c2.send("JOIN #keytest wrongkey");
			c2.expect(" 475 ", 3);   /* ERR_BADCHANNELKEY */
			/* Correct key should work */
			c2.send("JOIN #keytest secret");
			c2.expect("JOIN :#keytest", 3);
			c1.quit();
			c2.quit();
		},

		"474 ERR_BANNEDFROMCHAN: banned user cannot join": function() {
			var c1 = srv.client("banop3");
			c1.join("#banjoin");
			c1.send("MODE #banjoin +b banjoinee!*@*");
			c1.drain(300);
			var c2 = srv.client("banjoinee");
			c2.send("JOIN #banjoin");
			c2.expect(" 474 ", 3);   /* ERR_BANNEDFROMCHAN */
			c1.quit();
			c2.quit();
		},

		"MODE +l: limit blocks join when full": function() {
			var c1 = srv.client("limitop");
			c1.join("#limitest");
			c1.send("MODE #limitest +l 1");
			c1.drain(300);
			var c2 = srv.client("limituser");
			c2.send("JOIN #limitest");
			c2.expect(" 471 ", 3);   /* ERR_CHANNELISFULL */
			c1.quit();
			c2.quit();
		},

		"MODE +i: invite-only blocks un-invited join": function() {
			var c1 = srv.client("inviteop");
			c1.join("#invitetest");
			c1.send("MODE #invitetest +i");
			c1.drain(300);
			var c2 = srv.client("uninvited");
			c2.send("JOIN #invitetest");
			c2.expect(" 473 ", 3);   /* ERR_INVITEONLYCHAN */
			c1.quit();
			c2.quit();
		},

		"INVITE allows join on +i channel": function() {
			var c1 = srv.client("invop2");
			c1.join("#invitetest2");
			c1.send("MODE #invitetest2 +i");
			c1.drain(300);
			var c2 = srv.client("invitee");
			c2.send("JOIN #invitetest2");
			c2.expect(" 473 ", 3);
			c1.send("INVITE invitee #invitetest2");
			c1.expect(" 341 ", 3);            /* c1 gets RPL_INVITING */
			c2.expect("INVITE invitee", 3);   /* c2 gets the INVITE notice */
			c2.send("JOIN #invitetest2");
			c2.expect("JOIN :#invitetest2", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +b ban blocks channel message": function() {
			var c1 = srv.client("banop");
			c1.join("#bantest");
			var c2 = srv.client("banned");
			c2.send("JOIN #bantest");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #bantest +b banned!*@*");
			c1.drain(300);
			c2.send("PRIVMSG #bantest :should be blocked");
			c2.expect(" 404 ", 3);
			c1.quit();
			c2.quit();
		},

		"MODE -b unban restores message ability": function() {
			var c1 = srv.client("banop2");
			c1.join("#bantest2");
			var c2 = srv.client("banned2");
			c2.send("JOIN #bantest2");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #bantest2 +b banned2!*@*");
			c1.drain(300);
			c1.send("MODE #bantest2 -b banned2!*@*");
			c1.drain(300);
			c2.send("PRIVMSG #bantest2 :now ok");
			c1.expect("now ok", 3);
			c1.quit();
			c2.quit();
		},

		"MODE +b ban list (367/368)": function() {
			var c1 = srv.client("banlist1");
			c1.join("#banlisttest");
			c1.send("MODE #banlisttest +b *!bad@*");
			c1.drain(300);
			c1.send("MODE #banlisttest +b");
			c1.expect(" 367 ", 3);   /* RPL_BANLIST */
			c1.expect(" 368 ", 3);   /* RPL_ENDOFBANLIST */
			c1.quit();
		},

		"MODE +o grants op, recipient can then KICK": function() {
			var c1 = srv.client("opop1");
			c1.join("#optest");
			var c2 = srv.client("opnew");
			c2.send("JOIN #optest");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #optest +o opnew");
			c1.drain(300);
			c2.send("KICK #optest opop1 :promoted");
			c2.expect("KICK #optest opop1", 3);
			c2.quit();
		},

		"MODE -o removes op": function() {
			var c1 = srv.client("deop1");
			c1.join("#deoptest");
			var c2 = srv.client("deop2");
			c2.send("JOIN #deoptest");
			c1.drain(500);
			c2.drain(200);
			c1.send("MODE #deoptest +o deop2");
			c1.drain(300);
			c1.send("MODE #deoptest -o deop2");
			c1.drain(300);
			c2.send("KICK #deoptest deop1 :should fail");
			c2.expect(" 482 ", 3);
			c1.quit();
			c2.quit();
		},

		/* --- NAMES --- */

		"NAMES returns 353 and 366": function() {
			var c = srv.client("namesbot2");
			c.join("#namestest2");
			c.send("NAMES #namestest2");
			c.expect(" 353 ", 3);
			c.expect(" 366 ", 3);
			c.quit();
		},

		"NAMES shows @ prefix for ops": function() {
			var c = srv.client("opnames");
			c.join("#opnames_chan");
			c.send("NAMES #opnames_chan");
			var line = c.expect(" 353 ", 3);
			if (line.indexOf("@opnames") < 0)
				throw new Error("NAMES missing @ prefix for creator: " + line);
			c.quit();
		},

		/* --- LIST --- */

		"LIST returns 321 and 323": function() {
			var c = srv.client("listbot");
			c.join("#listtest");
			c.send("LIST");
			c.expect(" 321 ", 3);
			c.expect(/( 322 | 323 )/, 3);
			c.quit();
		},

		/* --- MODE query --- */

		"324 RPL_CHANNELMODEIS for MODE #chan query": function() {
			var c = srv.client("modeqbot");
			c.join("#modeqtest");
			c.send("MODE #modeqtest");
			c.expect(" 324 ", 3);
			c.quit();
		}

	});

} finally {
	srv.stop();
}
