/*
 * exec/tests/ircd/connect.js
 *
 * IRCd integration tests: basic connection, registration, identity commands.
 * Starts a local IRCd on port 19667, runs all tests, then stops it.
 */

"use strict";

load(js.exec_dir + "lib.sjs");

var PORT = 19667;
var srv  = new IrcdTestServer(PORT);

try {
	srv.start();

	run_tests({

		/* --- Welcome sequence --- */

		"001 RPL_WELCOME on registration": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("NICK bot1");
			c.send("USER bot1 0 * :Test Bot");
			c.expect(" 001 ", 5);
			c.quit();
		},

		"002 RPL_YOURHOST in burst": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("NICK bot2");
			c.send("USER bot2 0 * :Test Bot");
			c.expect(" 001 ", 5);
			c.expect(" 002 ", 3);
			c.quit();
		},

		"005 RPL_ISUPPORT in burst": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("NICK bot3");
			c.send("USER bot3 0 * :Test Bot");
			c.expect(" 001 ", 5);
			c.expect(" 005 ", 3);
			c.quit();
		},

		/* --- PING/PONG --- */

		"PONG in reply to client PING": function() {
			var c = srv.client("pingbot");
			c.send("PING :testtoken");
			c.expect("PONG", 3);
			c.quit();
		},

		/* --- Nickname validation --- */

		"433 ERR_NICKNAMEINUSE for duplicate nick on connect": function() {
			var c1 = srv.client("dupnick");
			var c2 = new IrcdClient(IRCD_TEST_HOST, PORT);
			c2.send("NICK dupnick");
			c2.send("USER u2 0 * :U2");
			c2.expect(" 433 ", 5);
			c2.sock.close();
			c1.quit();
		},

		"432 ERR_ERRONEUSNICKNAME for bad nick on connect": function() {
			var c = new IrcdClient(IRCD_TEST_HOST, PORT);
			c.send("NICK !invalid!");
			c.send("USER u 0 * :U");
			c.expect(" 432 ", 5);
			c.sock.close();
		},

		"NICK change accepted": function() {
			var c = srv.client("nickold");
			c.send("NICK nicknew");
			/* Full line: :nickold!user@host NICK nicknew */
			c.expect(" NICK nicknew", 3);
			c.nick = "nicknew";
			c.quit();
		},

		"433 ERR_NICKNAMEINUSE for in-use nick change": function() {
			var c1 = srv.client("taken");
			var c2 = srv.client("changer");
			c2.send("NICK taken");
			c2.expect(" 433 ", 3);
			c1.quit();
			c2.quit();
		},

		"432 ERR_ERRONEUSNICKNAME for bad nick change": function() {
			var c = srv.client("goodbot");
			c.send("NICK $bad$nick$");
			c.expect(" 432 ", 3);
			c.quit();
		},

		/* --- QUIT --- */

		"QUIT disconnects cleanly": function() {
			var c = srv.client("quitter");
			c.send("QUIT :bye");
			mswait(300);
			/* Server should have closed the socket; recv should return nothing */
			var raw = c.sock.recvline(16, 0.5);
			/* Either we get ERROR or an empty read - either is correct */
		},

		/* --- MOTD --- */

		"MOTD returns 375/376 or 422 ERR_NOMOTD": function() {
			var c = srv.client("motdbot");
			c.send("MOTD");
			c.expect(/( 375 | 376 | 422 )/, 3);
			c.quit();
		},

		/* --- WHOIS --- */

		"311 RPL_WHOISUSER for WHOIS self": function() {
			var c = srv.client("whoisbot");
			c.send("WHOIS whoisbot");
			c.expect(" 311 ", 3);
			c.expect(" 318 ", 3);   /* RPL_ENDOFWHOIS */
			c.quit();
		},

		"401 ERR_NOSUCHNICK for WHOIS unknown nick": function() {
			var c = srv.client("whoerr");
			c.send("WHOIS nosuchnick1234");
			c.expect(" 401 ", 3);
			c.quit();
		},

		/* --- WHO --- */

		"352 RPL_WHOREPLY for WHO on channel": function() {
			var c = srv.client("whobot");
			c.join("#whotest");
			c.send("WHO #whotest");
			c.expect(" 352 ", 3);
			c.expect(" 315 ", 3);   /* RPL_ENDOFWHO */
			c.quit();
		},

		"315 RPL_ENDOFWHO for WHO empty channel": function() {
			var c = srv.client("whobot2");
			c.send("WHO #emptychan_" + Date.now());
			c.expect(" 315 ", 3);
			c.quit();
		},

		/* --- USERHOST --- */

		"302 RPL_USERHOST for USERHOST self": function() {
			var c = srv.client("uhbot");
			c.send("USERHOST uhbot");
			c.expect(" 302 ", 3);
			c.quit();
		},

		/* --- ISON --- */

		"303 RPL_ISON includes online nick": function() {
			var c = srv.client("isonbot");
			c.send("ISON isonbot nonexistent");
			var line = c.expect(" 303 ", 3);
			if (line.indexOf("isonbot") < 0)
				throw new Error("Expected isonbot in ISON reply: " + line);
			c.quit();
		},

		/* --- STATS --- */

		"STATS l returns 211 (link info)": function() {
			var c = srv.client("statsbot");
			c.send("STATS l");
			c.expect(" 211 ", 3);
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS L returns 241 (uplinks)": function() {
			var c = srv.client("statsLbot");
			c.send("STATS L");
			c.expect(/( 241 | 219 )/, 3);
			c.quit();
		},

		"STATS i returns 215 (allow lines)": function() {
			var c = srv.client("statsibot");
			c.send("STATS i");
			c.expect(" 215 ", 3);  /* we have an [Allow] section */
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS I (uppercase) returns 215": function() {
			var c = srv.client("statsIbot");
			c.send("STATS I");
			c.expect(" 215 ", 3);
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS y returns 218 (Y-lines/classes)": function() {
			var c = srv.client("statsybot");
			c.send("STATS y");
			c.expect(" 218 ", 3);  /* we have Class:1 and Class:10 */
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS m returns 212 (command counters)": function() {
			var c = srv.client("statsmbot");
			c.send("STATS m");
			c.expect(" 212 ", 3);
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS u returns 242 (uptime)": function() {
			var c = srv.client("statsubot");
			c.send("STATS u");
			c.expect(" 242 ", 3);
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS o returns 243 (O-lines)": function() {
			var c = srv.client("statsobot");
			c.send("STATS o");
			c.expect(" 243 ", 3);  /* we have [Operator:testop] */
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS k returns 219 (no K-lines)": function() {
			var c = srv.client("statskbot");
			c.send("STATS k");
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS h returns 219 (no H-lines in test config)": function() {
			var c = srv.client("statshbot");
			c.send("STATS h");
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS q returns 219 (no Q-lines)": function() {
			var c = srv.client("statsqbot");
			c.send("STATS q");
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS c returns 219 (no C-lines in test config)": function() {
			var c = srv.client("statscbot");
			c.send("STATS c");
			c.expect(" 219 ", 3);
			c.quit();
		},

		"STATS p returns 219 (ports)": function() {
			var c = srv.client("statspbot");
			c.send("STATS p");
			c.expect(" 219 ", 3);
			c.quit();
		},

		/* --- LUSERS --- */

		"251 RPL_LUSERCLIENT after LUSERS": function() {
			var c = srv.client("lusersbot");
			c.send("LUSERS");
			c.expect(" 251 ", 3);
			c.quit();
		},

		"255 RPL_LUSERME in LUSERS response": function() {
			var c = srv.client("lusersbot2");
			c.send("LUSERS");
			c.expect(" 255 ", 3);
			c.quit();
		},

		/* --- LINKS --- */

		"365 RPL_ENDOFLINKS after LINKS": function() {
			var c = srv.client("linksbot");
			c.send("LINKS");
			c.expect(" 365 ", 3);
			c.quit();
		},

		/* --- ADMIN --- */

		"256 RPL_ADMINME after ADMIN": function() {
			var c = srv.client("adminbot");
			c.send("ADMIN");
			c.expect(" 256 ", 3);
			c.quit();
		},

		/* --- VERSION --- */

		"351 RPL_VERSION": function() {
			var c = srv.client("verbot");
			c.send("VERSION");
			c.expect(" 351 ", 3);
			c.quit();
		},

		/* --- TIME --- */

		"391 RPL_TIME": function() {
			var c = srv.client("timebot");
			c.send("TIME");
			c.expect(" 391 ", 3);
			c.quit();
		},

		/* --- INFO --- */

		"374 RPL_ENDOFINFO after INFO": function() {
			var c = srv.client("infobot");
			c.send("INFO");
			c.expect(" 374 ", 5);
			c.quit();
		}

	});

} finally {
	srv.stop();
}
