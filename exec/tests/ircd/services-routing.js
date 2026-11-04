/* Legacy network services must remain reachable with embedded services off. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function load_services(s) {
	var callbacks = s.js;
	s.js = js;
	try {
		load(s, "ircd/services-common.js");
		load(s, "ircd/services.js");
	} finally { s.js = callbacks; }
	s.js.setInterval = function() { return 1; };
	s.system.qwk_id = "TEST";
	s.system.data_dir = IRCD_TEST_TEMP_DIR;
	s.DNS_Resolver = {};
}

function service_network(manager) {
	var s = new_state(), relay = new_peer(s, "relay.synchro.net", true);
	var services = new s.IRC_Server();
	services.nick = "services.synchro.net";
	services.parent = relay.nick;
	services.local = false;
	services.uline = true;
	s.Servers[services.nick] = services;
	s.ULines = [services.nick];
	["NickServ", "ChanServ", "MemoServ", "OperServ", "HelpServ"].forEach(function(nick) {
		var bot = new_user(s, nick, relay.nick);
		bot.servername = services.nick;
		bot.uline = true;
	});
	if (manager) {
		load_services(s);
		s.IRC_Services = new s.IRC_Services_Manager(s.EmbeddedServices_Config);
	}
	s.visitor = new_user(s, "Visitor");
	return s;
}

function rehash_services(s, body, editing) {
	var path = IRCD_TEST_TEMP_DIR + "ircd-service-routing-" + random(0x7fffffff) + ".ini";
	var f = new File(path);
	check(f.open("w"), "Cannot create routing test config: " + path);
	f.write("[Info]\nServername=" + s.ServerName + "\n[Services:network]\n" + body);
	f.close();
	s.Config_Filename = path;
	s.IRCDCFG_Editor = !!editing;
	s.system.qwk_id = "TEST";
	s.Scan_For_Banned_Clients = function() {};
	try { s.Read_Config_File(); }
	finally { file_remove(path); }
}

run_tests({
	"service aliases forward complete commands when built-in services are disabled": function() {
		[false, true].forEach(function(manager) {
			var s = service_network(manager);
			[["cs", "ChanServ"], ["chanserv", "ChanServ"], ["ns", "NickServ"], ["nickserv", "NickServ"],
			 ["ms", "MemoServ"], ["memoserv", "MemoServ"], ["os", "OperServ"], ["operserv", "OperServ"],
			 ["hs", "HelpServ"], ["helpserv", "HelpServ"], ["help", "HelpServ"]].forEach(function(alias) {
				var bot = s.Users[alias[1].toUpperCase()];
				bot.received = [];
				s.visitor.work(alias[0] + " HELP test topic");
				check(bot.received.join("\n") == ":Visitor PRIVMSG " + alias[1] + " :HELP test topic", alias[0]);
			});
			check(!s.visitor.received.length, "Aliases reported unavailable services");
		});
	},
	"IDENTIFY preserves legacy NickServ and ChanServ routing": function() {
		var s = service_network(true);
		s.visitor.work("IDENTIFY test-password");
		s.visitor.work("IDENTIFY #test test-password");
		check(s.Users.NICKSERV.received[0] == ":Visitor PRIVMSG NickServ :IDENTIFY test-password");
		check(s.Users.CHANSERV.received[0] == ":Visitor PRIVMSG ChanServ :IDENTIFY #test test-password");
		check(!s.visitor.received.length);
	},
	"enabling FileServ alone does not intercept OperServ commands": function() {
		var s = service_network(true);
		s.IRC_Services.config.fileserv = true;
		s.IRC_Services.fileserv = {command:function() { throw new Error("OperServ reached FileServ"); }};
		s.visitor.work("operserv HELP");
		check(s.Users.OPERSERV.received[0] == ":Visitor PRIVMSG OperServ :HELP");
		check(!s.visitor.received.length);
	},
	"alias trust rejection explains why direct messaging can still work": function() {
		var s = service_network(true);
		s.Servers["services.synchro.net"].uline = false;
		s.visitor.work("operserv HELP");
		check(!s.Users.OPERSERV.received.length);
		check(s.visitor.received.join("\n").indexOf(" 440 ") >= 0);
		s.visitor.work("PRIVMSG OperServ :HELP");
		check(s.Users.OPERSERV.received[0] == ":Visitor PRIVMSG OperServ :HELP");
	},
	"rehash grants and revokes trust for connected services without reconnecting": function() {
		[false, true].forEach(function(manager) {
			var s = service_network(manager), service = s.Servers["services.synchro.net"], bot = s.Users.OPERSERV;
			rehash_services(s, "");
			check(!service.uline && !bot.uline, "Removing U-lines retained live services trust");
			s.visitor.work("operserv HELP");
			check(!bot.received.length && s.visitor.received.join("\n").indexOf(" 440 ") >= 0);
			s.visitor.received = [];
			rehash_services(s, "Servername=Services.Synchro.Net\n");
			check(service.uline && bot.uline && !s.visitor.uline, "Added trust did not reach the existing server and bot");
			check(!s.EmbeddedServices_Config.enabled && !s.EmbeddedServices_Config.nickserv && !s.EmbeddedServices_Config.memoserv);
			s.visitor.work("operserv HELP");
			check(bot.received[0] == ":Visitor PRIVMSG OperServ :HELP" && !s.visitor.received.length);
			bot.received = [];
			rehash_services(s, "Disabled=true\nServername=services.synchro.net\n");
			check(!service.uline && !bot.uline);
			s.visitor.work("operserv HELP");
			check(!bot.received.length && s.visitor.received.join("\n").indexOf(" 440 ") >= 0);
			check(s.Servers["services.synchro.net"] === service && s.Users.OPERSERV === bot, "Rehash replaced connected services");
		});
	},
	"rehash updates WHOIS service status along with alias routing": function() {
		var s = service_network(true);
		rehash_services(s, "");
		s.visitor.work("WHOIS OperServ");
		check(s.visitor.received.join("\n").indexOf("is a Network Service") < 0);
		s.visitor.received = [];
		rehash_services(s, "Servername=services.synchro.net\n");
		s.visitor.work("WHOIS OperServ");
		check(s.visitor.received.join("\n").indexOf("is a Network Service") >= 0);
	},
	"trust revocation removes service mode authority without undoing prior channel state": function() {
		var s = service_network(true), relay = s.Servers["relay.synchro.net"];
		var chan = join_user(s, s.visitor, "#test");
		rehash_services(s, "Servername=services.synchro.net\n");
		relay.work(":ChanServ MODE #test " + (s.now + 20) + " +v Visitor");
		check(!!chan.modelist[s.CHANMODE_VOICE][s.visitor.id], "Trusted service mode was rejected");
		rehash_services(s, "");
		relay.work(":ChanServ MODE #test " + (s.now + 20) + " -v Visitor");
		check(!!chan.modelist[s.CHANMODE_VOICE][s.visitor.id], "Revoked service retained timestamp authority");
		check(chan.created == s.now && s.Channels["#TEST"] === chan, "Rehash reset existing channel state");
	},
	"remote service servers and users match configured trust without case sensitivity": function() {
		var s = new_state(), relay = new_peer(s, "relay.synchro.net", true);
		s.ULines = ["Services.Synchro.Net"];
		relay.work(":relay.synchro.net SERVER services.synchro.net 2 :Network services");
		check(s.Servers["services.synchro.net"].uline, "Remote service server lost trust due to case");
		relay.work("NICK OperServ 2 1800000000 +i services synchro.net services.synchro.net 0 0 :Operator Service");
		check(s.Users.OPERSERV && s.Users.OPERSERV.uline, "Remote service nickname lost trust due to case");
	},
	"direct service admission uses the same case-insensitive trust lookup": function() {
		var s = new_state(), Original_Server = s.IRC_Server;
		load(s, "ircd/unregistered.js");
		s.IRC_Server = function() {
			var peer = new Original_Server();
			peer.finalize_server_connect = function() { this.admitted = true; };
			return peer;
		};
		s.js.setInterval = function() { return 1; };
		s.ULines = ["Services.Synchro.Net"];
		s.YLines[0] = new s.YLine(90, 0, 10, 1000000);
		var unreg = {id:1, socket:{}, hostname:"services.test"};
		unreg.recvq = new s.IRC_Queue(unreg);
		unreg.sendq = new s.IRC_Queue(unreg);
		s.Register_Unregistered_Local_Server(unreg, ["services.synchro.net", 1, "Network services"], {flags:0,ircclass:0});
		check(s.Servers["services.synchro.net"].uline && s.Local_Servers["services.synchro.net"].admitted);
	},
	"configuration editor reads do not alter live trust flags": function() {
		var s = service_network(false);
		rehash_services(s, "", true);
		check(!s.ULines.length && s.Servers["services.synchro.net"].uline && s.Users.OPERSERV.uline);
	},
	"removing trust shuts down the embedded authority and removes its virtual users": function() {
		var s = new_state(), relay = new_peer(s, "relay.synchro.net", true);
		load_services(s);
		s.ULines = ["services.synchro.net"];
		s.EmbeddedServices_Config.enabled = true;
		var manager = s.IRC_Services = new s.IRC_Services_Manager(s.EmbeddedServices_Config);
		manager.localhost = true;
		manager.snapshot = function() {};
		manager.introduce();
		check(manager.central && s.Servers["services.synchro.net"].uline && s.Users.CHANSERV.uline);
		var visitor = new_user(s, "Visitor"), chan = join_user(s, visitor, "#test");
		manager.chanserv.do_join("#test");
		relay.sent = [];
		rehash_services(s, "[EmbeddedServices]\nEnabled=true\n");
		check(manager.stopped && !manager.central && !s.IRC_Services.central);
		check(!s.Servers["services.synchro.net"] && !s.Users.NICKSERV && !s.Users.CHANSERV);
		check(s.Channels["#TEST"] === chan && chan.users[visitor.id] === visitor, "Services shutdown removed an ordinary user's channel");
		var wire = relay.sent.join("\n");
		check(wire.indexOf(":NickServ QUIT :Services stopped") >= 0 && wire.indexOf(":ChanServ QUIT :Services stopped") >= 0);
		check(wire.indexOf(" SQUIT services.synchro.net :Services stopped") > wire.indexOf(":ChanServ QUIT"), "Virtual server disappeared before its service users");
		var pending = manager.queue.inputs.length;
		manager.stop();
		check(manager.queue.inputs.length == pending, "Repeated services shutdown was not idempotent");
	},
	"missing services and empty commands retain their numeric errors": function() {
		var s = service_network(true);
		delete s.Users.OPERSERV;
		s.visitor.work("operserv HELP");
		check(s.visitor.received.join("\n").indexOf(" 440 ") >= 0);
		s.visitor.received = [];
		s.visitor.services_msg("OperServ", "");
		check(s.visitor.received.join("\n").indexOf(" 412 ") >= 0);
	}
});
