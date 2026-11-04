/* Services authority and forwarding, including the guarantees from 6bd095d0. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function compatibility_network() {
	var states = [new_state(), new_state(), new_state()];
	var names = ["edge.test", "hub.test", "home.test"];
	states.forEach(function(s, i) { s.ServerName = names[i]; s.KLines = []; s.ZLines = []; });
	var first = new_peer(states[0], "services.test", false); first.uline = true; first.their_capab = [];
	var output0 = new_peer(states[0], names[1], true), input1 = new_peer(states[1], names[0], true);
	var output1 = new_peer(states[1], names[2], false), input2 = new_peer(states[2], names[1], true);
	var remote_home = new states[0].IRC_Server();
	remote_home.nick = remote_home.id = names[2]; remote_home.parent = output0.nick;
	remote_home.linkparent = names[1]; remote_home.local = false;
	states[0].Servers[names[2]] = remote_home;
	var nodes = [
		{s:states[0], input:first, output:output0},
		{s:states[1], input:input1, output:output1},
		{s:states[2], input:input2}
	];
	nodes.forEach(function(node, i) {
		var s = node.s;
		if (i) {
			var origin = new s.IRC_Server();
			origin.nick = origin.id = "services.test"; origin.parent = node.input.nick;
			origin.linkparent = names[0]; origin.local = false; origin.uline = true;
			s.Servers[origin.nick] = origin;
		}
		["ChanServ", "NickServ", "OperServ"].forEach(function(nick) {
			var bot = new_user(s, nick, node.input.nick);
			bot.servername = "services.test"; bot.uline = true;
		});
		node.owner = new_user(s, "Owner", i ? node.input.nick : undefined);
		node.target = new_user(s, "Target", i < 2 ? node.output.nick : undefined);
		node.target.servername = names[2];
		node.target.socket = {is_connected:false, clearOn:function() {}, close:function() {}};
		node.channel = join_user(s, node.owner, "#service");
		node.channel.modelist[s.CHANMODE_OP][node.owner.id] = node.owner;
		node.channel.topictime = s.now;
		node.channel.topicchangedby = "Owner";
		node.logs = []; node.scans = 0;
		s.log = function(level, message) { node.logs.push(message); };
		s.Scan_For_Banned_Clients = function() { node.scans++; };
	});
	return nodes;
}

function relay_command(nodes, line, verb, verify) {
	nodes.forEach(function(node) {
		if (node.output) node.output.sent = [];
		node.input.work(line);
		verify(node);
		check(!node.input.quitting, "Services command disconnected its incoming link");
		if (node.output) {
			var sent = node.output.sent.filter(function(wire) { return node.s.IRC_parse(wire).verb == verb; });
			check(sent.length == 1, verb + " did not traverse " + node.s.ServerName + ": " + node.output.sent.join(" | "));
			line = sent[0];
		}
	});
}

run_tests({
	"non-hub services can introduce reserved bot names while ordinary leaves cannot": function() {
		var node = compatibility_network()[0], s = node.s;
		s.QLines = [new s.QLine("*Serv", "Reserved for Services")]; s.ULines = ["services.test"];
		node.input.work("NICK HelpServ 1 1800000000 +i services services.test services.test 0 0 :Help Service");
		check(s.Users.HELPSERV && s.Users.HELPSERV.uline && (s.Users.HELPSERV.mode & s.USERMODE_INVISIBLE), "Non-hub services could not introduce their reserved bot");
		var leaf = new_peer(s, "ordinary.test", false);
		leaf.work("NICK BogusServ 1 1800000000 +i services services.test services.test 0 0 :Impersonator");
		check(!s.Users.BOGUSSERV && leaf.sent.some(function(line) { return line.indexOf("KILL BogusServ") >= 0; }), "An ordinary leaf acquired reserved nicknames");
		node.input.work("NICK Bad.Nick 1 1800000000 +i services services.test services.test 0 0 :Invalid");
		check(!s.Users["BAD.NICK"], "Services nickname exemption bypassed syntax validation");
	},
	"services global notices cross hubs without requiring operator user modes": function() {
		["services.test", "OperServ"].forEach(function(source) {
			var nodes = compatibility_network();
			relay_command(nodes, ":" + source + " NOTICE $*.test :Network maintenance", "NOTICE", function(node) {
				if (node.target.local)
					check(node.target.received.some(function(line) { return line.indexOf("NOTICE $*.test :Network maintenance") >= 0; }), "Global services notice did not reach local users");
			});
		});
	},
	"ordinary users do not gain global notices through a trusted services transport": function() {
		var node = compatibility_network()[0], user = new_user(node.s, "Ordinary", node.input.nick);
		node.input.work(":Ordinary NOTICE $*.test :Unauthorized announcement");
		check(!node.output.sent.length && !node.owner.received.length, "An ordinary user acquired services announcement privileges");
	},
	"remote operators retain ordinary global notice routing": function() {
		var nodes = compatibility_network();
		nodes.forEach(function(node) {
			var oper = node.s.Users.OPERSERV;
			oper.uline = false; oper.mode |= node.s.USERMODE_OPER;
		});
		relay_command(nodes, ":OperServ NOTICE $*.test :Operator announcement", "NOTICE", function(node) {
			if (node.target.local)
				check(node.target.received.some(function(line) { return line.indexOf(":Operator announcement") >= 0; }), "Remote operator announcement stopped at a hub");
		});
	},
	"services MODE corrections cross hubs whose local modes already match": function() {
		["services.test", "ChanServ"].forEach(function(source) {
			[0, 1799999980, 1800000020].forEach(function(ts) {
				var nodes = compatibility_network();
				nodes.forEach(function(node, i) {
					join_user(node.s, node.target, "#service");
					if (i < 2) {
						node.channel.modelist[node.s.CHANMODE_OP][node.target.id] = node.target;
						node.channel.modelist[node.s.CHANMODE_VOICE][node.target.id] = node.target;
					}
				});
				relay_command(nodes, ":" + source + " MODE #service " + ts + " +ov Target Target", "MODE", function(node) {
					check(node.channel.modelist[node.s.CHANMODE_OP][node.target.id] && node.channel.modelist[node.s.CHANMODE_VOICE][node.target.id], "Services correction did not apply");
					check(node.channel.created == node.s.now && node.channel.modelist[node.s.CHANMODE_OP][node.owner.id], "Services MODE reset channel age or existing ops");
				});
			});
		});
	},
	"services MODE relays all requested modes when only part changes at the first hub": function() {
		var nodes = compatibility_network();
		nodes.forEach(function(node, i) {
			join_user(node.s, node.target, "#service");
			if (!i) node.channel.modelist[node.s.CHANMODE_OP][node.target.id] = node.target;
		});
		relay_command(nodes, ":ChanServ MODE #service 0 +ov Target Target", "MODE", function(node) {
			check(node.channel.modelist[node.s.CHANMODE_OP][node.target.id] && node.channel.modelist[node.s.CHANMODE_VOICE][node.target.id], "Partial local delta lost a requested status");
		});
	},
	"services JOINs with zero or older timestamps preserve existing channel ops across hubs": function() {
		[0, 1799999980].forEach(function(ts) {
			var nodes = compatibility_network();
			relay_command(nodes, ":ChanServ JOIN " + ts + " #service", "JOIN", function(node) {
				check(node.channel.users[node.s.Users.CHANSERV.id], "ChanServ JOIN was lost");
				check(node.channel.created == node.s.now && node.channel.modelist[node.s.CHANMODE_OP][node.owner.id], "Services JOIN wiped channel age or ops");
			});
		});
	},
	"services JOIN and PART corrections cross hubs with matching membership": function() {
		["JOIN", "PART"].forEach(function(verb) {
			var nodes = compatibility_network();
			nodes.forEach(function(node, i) {
				if ((verb == "JOIN" && i < 2) || (verb == "PART" && i == 2))
					join_user(node.s, node.s.Users.CHANSERV, "#service");
			});
			relay_command(nodes, ":ChanServ " + verb + (verb == "JOIN" ? " 0" : "") + " #service", verb, function(node) {
				check(!!node.s.Users.CHANSERV.channels["#SERVICE"] == (verb == "JOIN"), "Services membership correction did not reach " + node.s.ServerName);
				check(node.channel.modelist[node.s.CHANMODE_OP][node.owner.id], "Services correction removed existing ops");
			});
		});
	},
	"saved services topics traverse matching hubs and override newer downstream topics": function() {
		["services.test", "ChanServ"].forEach(function(source) {
			var nodes = compatibility_network();
			nodes.forEach(function(node, i) {
				node.channel.topic = i < 2 ? "Locked topic" : "Unwanted topic";
				node.channel.topicchangedby = "Founder";
				node.channel.topictime = i < 2 ? node.s.now - 20 : node.s.now + 20;
			});
			relay_command(nodes, ":" + source + " TOPIC #service Founder 1799999980 :Locked topic", "TOPIC", function(node) {
				check(node.channel.topic == "Locked topic" && node.channel.topictime == node.s.now - 20 && node.channel.topicchangedby == "Founder", "Services topic restoration was lost");
				if (node.output) check(node.output.sent.some(function(line) { return line.indexOf(":" + source + " TOPIC ") == 0; }), "Services topic origin was replaced by the relay");
			});
		});
	},
	"services topic clears with zero timestamps remain authoritative": function() {
		var nodes = compatibility_network();
		nodes.forEach(function(node) { node.channel.topic = "Locked topic"; });
		relay_command(nodes, ":ChanServ TOPIC #service Founder 0 :", "TOPIC", function(node) {
			check(node.channel.topic === "" && node.channel.topicchangedby == "Founder", "Services topic clear was ignored");
		});
	},
	"ordinary topics retain timestamp ordering even over a trusted services connection": function() {
		var node = compatibility_network()[0], s = node.s;
		var user = new_user(s, "Ordinary", node.input.nick);
		user.servername = "ordinary.test";
		node.channel.topic = "Newer topic";
		node.input.work(":Ordinary TOPIC #service Ordinary " + (s.now - 20) + " :Old topic");
		check(node.channel.topic == "Newer topic" && !node.output.sent.length, "A trusted transport conferred topic authority on an ordinary user");
	},
	"services KILL reaches a remote user through its non-hub services uplink": function() {
		["services.test", "NickServ"].forEach(function(source) {
			var nodes = compatibility_network();
			nodes.forEach(function(node) { join_user(node.s, node.target, "#service"); });
			relay_command(nodes, ":" + source + " KILL Target :Nickname reclaimed", "KILL", function(node) {
				check(!node.s.Users.TARGET && !node.channel.users[node.target.id], "Services KILL left the target present");
				check(node.s.Users.CHANSERV && node.s.Users.NICKSERV, "KILL removed services instead of its target");
			});
		});
	},
	"services INVITE reaches users elsewhere without ChanServ channel membership": function() {
		var nodes = compatibility_network(), wire = ":ChanServ INVITE Target #service";
		nodes.forEach(function(node) {
			node.input.work(wire);
			check(node.target.invited == "#SERVICE" && !node.s.Users.CHANSERV.channels["#SERVICE"], "Services INVITE was rejected or joined ChanServ");
			check(node.target.received.length == 1, "INVITE was not forwarded to the target's route");
			wire = node.target.received[0];
		});
	},
	"services INVITE crosses hubs that already see the recipient joined": function() {
		var nodes = compatibility_network(), wire = ":ChanServ INVITE Target #service";
		nodes.forEach(function(node, i) { if (i < 2) join_user(node.s, node.target, "#service"); });
		nodes.forEach(function(node) {
			node.input.work(wire);
			check(node.target.received.length == 1 && node.target.invited == "#SERVICE", "Matching membership swallowed a services INVITE");
			wire = node.target.received[0];
		});
	},
	"services KICK crosses hubs without ChanServ membership or ops": function() {
		var nodes = compatibility_network();
		nodes.forEach(function(node) { join_user(node.s, node.target, "#service"); });
		relay_command(nodes, ":ChanServ KICK #service Target :Policy", "KICK", function(node) {
			check(!node.channel.users[node.target.id] && !node.target.channels["#SERVICE"], "Services KICK was lost");
			check(node.channel.users[node.owner.id] && node.s.Users.TARGET, "KICK removed unrelated state");
		});
	},
	"services KICK crosses hubs that already see the target absent": function() {
		var nodes = compatibility_network();
		join_user(nodes[2].s, nodes[2].target, "#service");
		relay_command(nodes, ":ChanServ KICK #service Target :Policy", "KICK", function(node) {
			check(!node.target.channels["#SERVICE"] && node.s.Users.TARGET, "Services KICK did not repair membership");
		});
	},
	"services AKILL and RAKILL retain their source and update every hub": function() {
		var nodes = compatibility_network();
		relay_command(nodes, ":services.test AKILL blocked.test bad 3600 OperServ 1800000000 :Policy", "AKILL", function(node) {
			check(node.s.isklined("bad@blocked.test") && node.scans == 1, "Services AKILL did not apply");
		});
		relay_command(nodes, ":services.test RAKILL blocked.test bad", "RAKILL", function(node) {
			check(!node.s.isklined("bad@blocked.test"), "Services RAKILL did not apply");
		});
	},
	"privileged services prefixes arriving on the wrong route cannot be forged": function() {
		["INVITE Target #service", "KILL Target :Forged", "AKILL blocked.test bad 3600 OperServ 1800000000 :Forged",
		 "RAKILL blocked.test bad", "SQUIT victim.test :Forged", "NOTICE $*.test :Forged"].forEach(function(command) {
			var node = compatibility_network()[0], s = node.s, rogue = new_peer(s, "rogue.test", false);
			var victim = new_peer(s, "victim.test", false);
			s.KLines.push(new s.KLine("bad@blocked.test", "Policy", "A"));
			var source = command.indexOf("INVITE") == 0 ? "ChanServ" : "services.test";
			rogue.work(":" + source + " " + command);
			check(s.Users.TARGET && !node.target.invited && s.isklined("bad@blocked.test"), "Forged services command changed user/ban state: " + command);
			check(!victim.quitting && !rogue.quitting && !node.input.quitting, "Forged SQUIT disconnected a link");
			check(!node.output.sent.length && !node.scans, "Forged services command was relayed or scanned bans: " + command);
			check(node.logs.some(function(line) { return line.indexOf("is on another link") >= 0; }), "Origin route check did not reject " + command);
		});
	},
	"ordinary leaves cannot gain cross-route KILL authority": function() {
		var node = compatibility_network()[0], s = node.s;
		node.input.uline = false;
		node.input.work(":services.test KILL Target :Untrusted");
		check(s.Users.TARGET && !node.output.sent.length, "Untrusted leaf killed a user on another route");
	},
	"services JUPE removes the requested subtree before introducing its replacement": function() {
		var nodes = compatibility_network(), edge = nodes[0], hub = nodes[1];
		var old_home = hub.s.Servers["home.test"], closes = 0;
		old_home.quit = hub.s.Server_Quit;
		old_home.socket = {is_connected:false, close:function() { closes++; }};
		hub.s.YLines[0] = new hub.s.YLine(90, 0, 10, 1000000);
		edge.input.work(":services.test SQUIT home.test :Juped");
		check(!edge.input.quitting && edge.s.Servers["services.test"], "Services SQUIT disconnected its own uplink");
		check(!edge.s.Servers["home.test"] && !edge.s.Users.TARGET, "SQUIT left a conflicting server subtree at the uplink");
		var split = edge.output.sent.filter(function(line) { return edge.s.IRC_parse(line).verb == "SQUIT"; });
		check(split.length == 1 && split[0] == ":services.test SQUIT home.test :Juped", "SQUIT was not relayed with its services origin");
		hub.input.work(split[0]);
		check(!hub.s.Servers["home.test"] && !hub.s.Users.TARGET && closes == 1, "Services SQUIT did not close the requested branch");
		edge.output.sent = [];
		edge.input.work(":services.test SERVER home.test 2 :Juped");
		check(!edge.input.quitting && edge.s.Servers["home.test"].linkparent == "services.test", "Non-hub services could not introduce the JUPE");
		var introductions = edge.output.sent.filter(function(line) { return edge.s.IRC_parse(line).verb == "SERVER"; });
		check(introductions.length == 1, "JUPE server introduction was not relayed");
		hub.input.work(introductions[0]);
		check(!hub.input.quitting && hub.s.Servers["home.test"].linkparent == "services.test");
		check(!edge.s.Servers["home.test"].uline && !hub.s.Servers["home.test"].uline, "JUPE server inherited services trust");
	},
	"an untrusted origin behind a services connection cannot introduce a server": function() {
		var node = compatibility_network()[0], s = node.s, origin = new s.IRC_Server();
		origin.nick = origin.id = "ordinary.test"; origin.parent = node.input.nick; origin.local = false;
		s.Servers[origin.nick] = origin;
		node.input.work(":ordinary.test SERVER fake.test 2 :Untrusted");
		check(!s.Servers["fake.test"] && !node.output.sent.length, "Services transport conferred server-introduction authority");
	},
	"ordinary duplicate membership commands do not acquire corrective relay semantics": function() {
		["JOIN #service", "PART #service", "KICK #service Target :Policy", "INVITE Target #service"].forEach(function(command) {
			var node = compatibility_network()[0], s = node.s, bot = s.Users.CHANSERV;
			bot.uline = false;
			if (command.indexOf("JOIN") == 0 || command.indexOf("INVITE") == 0) join_user(s, bot, "#service");
			if (command.indexOf("INVITE") == 0) {
				node.channel.modelist[s.CHANMODE_OP][bot.id] = bot;
				join_user(s, node.target, "#service");
			}
			node.input.work(":ChanServ " + command);
			check(!node.output.sent.length && !node.target.received.length, "Ordinary duplicate was relayed as a services correction: " + command);
		});
	}
});
