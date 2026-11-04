/* Services protocol additions in 2.11; no production state or sockets. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(), input = new_peer(s, "services.test", false), output = new_peer(s, "hub2.test", true);
	input.uline = true; input.their_capab = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT".split(" ");
	s.KLines = []; s.ZLines = []; s.scans = 0; s.IRCDCFG_Editor = false;
	s.Scan_For_Banned_Clients = function() { s.scans++; };
	var owner = new_user(s, "Owner"), target = new_user(s, "Target", output.nick);
	var chan = join_user(s, owner, "#test"); join_user(s, target, "#test");
	chan.modelist[s.CHANMODE_OP][owner.id] = owner;
	return {s:s, input:input, output:output, owner:owner, target:target, chan:chan};
}
function bans(t) { return t.s.KLines.filter(function(b) { return !!b; }); }
function local_target(t) {
	t.target.local = true; t.target.parent = 0; t.target.servername = t.s.ServerName;
	t.target.socket = {is_connected:false, clearOn:function() {}, close:function() {}};
	t.s.Local_Users[t.target.id] = t.target;
}
function akill(t, mask, duration, created, source) {
	var split = mask.split("@");
	t.input.work(":" + (source || "services.test") + " AKILL " + split[1] + " " + split[0]
		+ " " + duration + " OperServ " + (created === undefined ? t.s.now : created) + " :Policy");
}
function rehash(t, trusted) {
	var path = IRCD_TEST_TEMP_DIR + "ircd-211-" + random(2147483647) + ".ini", f = new File(path);
	check(f.open("w"));
	f.write("[Info]\nServername=" + t.s.ServerName + "\n[Services:network]\nServername=" + trusted + "\n[Ban:local]\nMask=*@local.test\nReason=Local policy\n"); f.close();
	t.s.Config_Filename = path; t.s.system.qwk_id = "TEST";
	try { t.s.Read_Config_File(); } finally { file_remove(path); }
}

run_tests({
	"SVSNICK commits at the owning server with ordinary NICK acknowledgement and history": function() {
		var t = scenario(), s = t.s; local_target(t);
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(s.Users.GUEST123 === t.target && !s.Users.TARGET && t.target.created == 1800000001);
		check(t.target.received[0] == ":Target NICK Guest123" && t.owner.received[0] == ":Target NICK Guest123");
		check(t.input.sent[0] == ":Target NICK Guest123 :1800000001" && t.output.sent[0] == t.input.sent[0]);
		check(s.search_nickbuf("Target") === t.target && t.chan.users[t.target.id] === t.target);
	},
	"SVSNICK is routed toward a capable owner without changing intermediate caches": function() {
		var t = scenario(); t.output.their_capab.push("SVSNICK");
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(t.output.sent[0] == ":services.test SVSNICK Target Guest123 :1800000001");
		check(t.target.nick == "Target" && t.s.Users.TARGET === t.target && !t.input.sent.length);
	},
	"old downstream links free a protected nick using KILL without fictitious renames": function() {
		["synchronet", "bahamut"].forEach(function(profile) {
		var t = scenario(); t.output.protocol = profile;
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(!t.s.Users.TARGET && !t.s.Users.GUEST123);
		check(t.input.sent.some(function(line) { return line.indexOf(":" + t.s.ServerName + " KILL Target :") == 0; }));
		check(t.output.sent.some(function(line) { return line.indexOf(":services.test KILL Target :") == 0; }));
		check(!t.input.quitting && !t.output.quitting && !t.s.NickHistory.length);
		});
	},
	"untrusted, malformed and wrong-route SVSNICK requests cannot rename or kill users": function() {
		["Target Bad.Nick :1800000001", "Target Guest123 :0", "Target Guest123 :-1", "Target Guest123 :NaN",
		 "Target " + new Array(32).join("G") + " :1800000001", "Missing Guest123 :1800000001"].forEach(function(fields) {
			var t = scenario(); local_target(t); t.input.work(":services.test SVSNICK " + fields);
			check(t.s.Users.TARGET && !t.target.received.length && !t.output.sent.length);
		});
		var t = scenario(); local_target(t); t.input.uline = false;
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		t.input.uline = true; t.output.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(t.s.Users.TARGET && !t.target.received.length && !t.input.sent.length && !t.output.sent.length);
		t.s.QLines = [new t.s.QLine("Reserved*", "Reserved")];
		t.input.work(":services.test SVSNICK Target ReservedNick :1800000001");
		check(t.s.Users.TARGET);
	},
	"the owner resolves SVSNICK collisions without disturbing the existing nickname holder": function() {
		var t = scenario(); local_target(t);
		var existing = new_user(t.s, "Guest123"); t.s.random = function() { return 7; };
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(t.s.Users.GUEST123 === existing && t.s.Users["GUEST123-7"] === t.target && !t.s.Users.TARGET);
	},
	"exhausted SVSNICK collision attempts release only the enforcement target": function() {
		var t = scenario(); local_target(t); t.s.random = function() { return 7; };
		var existing = new_user(t.s, "Guest123"), alternate = new_user(t.s, "Guest123-7");
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(!t.s.Users.TARGET && t.s.Users.GUEST123 === existing && t.s.Users["GUEST123-7"] === alternate);
	},
	"SVSNICK never bounces a request toward its incoming link": function() {
		var t = scenario(); t.target.parent = t.input.nick;
		t.input.work(":services.test SVSNICK Target Guest123 :1800000001");
		check(t.s.Users.TARGET && !t.input.sent.length && !t.output.sent.length);
	},
	"Bahamut exceptions preserve plain masks and following arguments without accepting extensions": function() {
		var t = scenario(), s = t.s;
		t.input.protocol = "bahamut"; t.output.their_capab.push("CHANEXCEPT");
		t.input.work(":services.test MODE #test 0 +eeov *!*@trusted.test ~m:external:Target Target Owner");
		check(t.chan.modelist[s.CHANMODE_EXCEPT].join(" ") == "*!*@trusted.test");
		check(t.chan.modelist[s.CHANMODE_OP][t.target.id] && t.chan.modelist[s.CHANMODE_VOICE][t.owner.id]);
		check(t.output.sent[0] == ":services.test MODE #test 0 +eov *!*@trusted.test Target Owner", t.output.sent.join(" | "));
		t.input.work(":services.test MODE #test 0 -e *!*@trusted.test");
		check(!t.chan.count_modelist(s.CHANMODE_EXCEPT));
	},
	"Bahamut live changes and reconnect bursts contain only ordinary exceptions": function() {
		var t = scenario(); t.input.protocol = "bahamut";
		t.owner.set_chanmode(t.chan, ["+ee", "*!*@trusted.test", "~m:external:Target"], 0);
		check(t.input.sent.length == 1 && t.input.sent[0].indexOf("+e *!*@trusted.test") >= 0);
		t.input.sent = []; t.input.server_chan_info(t.chan);
		check(t.input.sent.some(function(line) { return line.indexOf("+e *!*@trusted.test") >= 0; }));
		check(!t.input.sent.some(function(line) { return line.indexOf("~msgbypass:") >= 0; }));
		t.owner.set_chanmode(t.chan, ["-e", "*!*@trusted.test"], 0);
		check(t.input.sent[t.input.sent.length - 1].indexOf("-e *!*@trusted.test") >= 0);
	},
	"SJOIN relays filter exception types independently for each recipient": function() {
		var t = scenario(); t.input.their_capab.push("CHANEXCEPT"); t.output.protocol = "bahamut";
		t.input.work(":services.test SJOIN 0 #test +e ~m:external:Target :@Target");
		check(t.chan.count_modelist(t.s.CHANMODE_EXCEPT) == 1);
		check(t.output.sent[0] == ":services.test SJOIN " + t.s.now + " #test + :@Target");
		t.output.their_capab.push("CHANEXCEPT"); t.output.sent = [];
		t.input.work(":services.test SJOIN 0 #test +e ~m:external:Target :@Target");
		check(t.output.sent[0].indexOf("+e ~m:external:Target") >= 0);
	},
	"the mode profile changes neither U-line authority nor extended snapshot capability": function() {
		var t = scenario(); t.input.protocol = "bahamut"; t.input.uline = false;
		t.input.work(":services.test MODE #test 0 +e *!*@trusted.test");
		check(!t.chan.count_modelist(t.s.CHANMODE_EXCEPT));
		check(!t.s.IRC_has_chanexcept(t.input));
	},
	"peer protocol configuration is case-insensitive, round-trips and applies on REHASH": function() {
		var t = scenario(), s = t.s;
		s.system.qwk_id = "TEST";
		s.Clear_Config_Globals(); s.load_config_defaults();
		s.ini_Server("external", {SERVERNAME:"services.test", HOSTNAME:"127.0.0.1", PASSWORD:"link", CLASS:"0", PROTOCOL:"BaHaMuT"});
		check(s.Find_NLine("services.test").protocol == "bahamut");
		var path = IRCD_TEST_TEMP_DIR + "ircd-profile-" + random(2147483647) + ".ini";
		s.system.qwk_id = "TEST";
		try {
			check(s.Write_Config_File(path)); s.Config_Filename = path; s.Read_Config_File();
			check(s.Find_NLine("services.test").protocol == "bahamut" && t.input.protocol == "bahamut");
		} finally { file_remove(path); }
	},
	"Bahamut extended modes retain their arguments while old native relays omit them": function() {
		var t = scenario(), s = t.s;
		t.input.protocol = "bahamut";
		t.input.work(":services.test MODE #test 0 +jko 5:10 secret Target");
		check(t.chan.arg[s.CHANMODE_KEY] == "secret" && t.chan.modelist[s.CHANMODE_OP][t.target.id]);
		check(t.chan.arg[s.CHANMODE_JOINRATE] == "5:10");
		check(t.output.sent[0] == ":services.test MODE #test 0 +ko secret Target", t.output.sent.join(" | "));
		t.input.work(":services.test MODE #test 0 -j+Iov *!*@trusted.test Target Owner");
		check(t.chan.modelist[s.CHANMODE_VOICE][t.owner.id] && !t.chan.modelist[s.CHANMODE_VOICE][t.target.id]);
		check(t.chan.count_modelist(s.CHANMODE_INVITEMASK) == 1);
		check(t.output.sent[1] == ":services.test MODE #test 0 +ov Target Owner");
	},
	"local mode errors cannot change the meaning of later supported modes": function() {
		var t = scenario();
		t.owner.work("MODE #test +jko invalid secret Target");
		check(t.chan.arg[t.s.CHANMODE_KEY] == "secret" && t.chan.modelist[t.s.CHANMODE_OP][t.target.id]);
		check(t.owner.received.some(function(line) { return line.indexOf(" 696 ") >= 0; }));
	},
	"ambiguous or incomplete network mode requests are rejected before mutation": function() {
		["MODE #test 0 +Zko unexpected secret Target", "MODE #test 0 +jko 5:10 secret",
		 "SJOIN 1799999999 #test +Zk unexpected secret :@Target"].forEach(function(command) {
			var t = scenario(); t.input.work(":services.test " + command);
			check(!t.chan.arg[t.s.CHANMODE_KEY] && !t.chan.modelist[t.s.CHANMODE_OP][t.target.id]
				&& t.chan.created == t.s.now && !t.output.sent.length, command);
		});
	},
	"SJOIN and outgoing batches retain arguments following unsupported modes": function() {
		var t = scenario();
		t.input.work(":services.test SJOIN 0 #test +jk 5:10 secret :@Target");
		check(t.chan.arg[t.s.CHANMODE_KEY] == "secret");
		check(t.output.sent[0] == ":services.test SJOIN " + t.s.now + " #test +k secret :@Target");
		check(t.s.IRC_chanmode_batches(["+jkov", "5:10", "secret", "Target", "Owner"], "MODE #test ", 2).join(" | ") == "+jk 5:10 secret | +ov Target Owner");
	},
	"services bans coexist with broader local policy and remove only their exact masks": function() {
		var t = scenario(), local = new t.s.KLine("*@*.test", "Local", "K");
		t.s.KLines.push(local); akill(t, "bad@bad.test", 60); akill(t, "*@bad.test", 60);
		check(bans(t).length == 3);
		t.input.work(":services.test RAKILL BAD.TEST BAD");
		check(bans(t).length == 2 && bans(t)[0] === local && t.s.isklined("other@other.test") === local);
		check(bans(t)[1].hostmask == "*@bad.test");
	},
	"different services owners retain independent entries for the same ban mask": function() {
		var t = scenario(), other = new t.s.IRC_Server();
		other.nick = "other.test"; other.parent = t.input.nick; other.local = false; other.uline = true;
		t.s.Servers[other.nick] = other;
		akill(t, "bad@bad.test", 60); akill(t, "bad@bad.test", 120, undefined, other.nick);
		t.input.work(":services.test RAKILL bad.test bad");
		check(bans(t).length == 1 && bans(t)[0].owner == other.nick);
	},
	"AKILL updates and expiry use source creation time without overwriting newer bans": function() {
		var t = scenario(); akill(t, "bad@bad.test", 60);
		t.s.now += 10; akill(t, "bad@bad.test", 120);
		akill(t, "bad@bad.test", 1, t.s.now - 100);
		akill(t, "bad@bad.test", 999, t.s.now - 1);
		check(bans(t).length == 1 && bans(t)[0].expires == t.s.now + 120);
		t.s.now += 120;
		check(!t.s.isklined("bad@bad.test") && !bans(t).length);
		akill(t, "forever@bad.test", 0); t.s.now += 10000; t.s.IRC_expire_akills();
		check(t.s.isklined("forever@bad.test"));
		akill(t, "long@bad.test", 4294967295);
		check(t.output.sent[t.output.sent.length - 1].indexOf("AKILL bad.test long 4294967295 ") >= 0, "Unsigned lifetime changed on relay");
	},
	"expiry sends legacy RAKILL only away from the services authority": function() {
		var t = scenario(); akill(t, "bad@bad.test", 1);
		t.output.sent = []; t.input.sent = []; t.s.now++;
		t.s.IRC_expire_akills(); t.s.IRC_expire_akills();
		check(t.output.sent.length == 1 && t.output.sent[0] == ":services.test RAKILL bad.test bad" && !t.input.sent.length);
		t.output.sent = []; akill(t, "bad@bad.test", 1, t.s.now - 10);
		check(!t.output.sent.length && !bans(t).length, "Expired requests reached legacy peers");
	},
	"invalid AKILL fields change no policy and are not relayed": function() {
		["bad.test bad -1 OperServ 1800000000 :Bad", "bad.test bad NaN OperServ 1800000000 :Bad",
		 "bad.test bad 60 OperServ 999999999999 :Bad", "bad@test bad 60 OperServ 1800000000 :Bad"].forEach(function(fields) {
			var t = scenario(); t.input.work(":services.test AKILL " + fields);
			check(!bans(t).length && !t.output.sent.length && !t.s.scans);
		});
	},
	"reconnect bursts preserve services-ban ownership and absolute expiry": function() {
		var t = scenario(); akill(t, "bad@bad.test", 60); t.s.now += 10;
		t.output.sent = []; t.s.IRC_burst_akills(t.output);
		check(t.output.sent[0] == ":services.test AKILL bad.test bad 60 OperServ 1800000000 :Policy");
		var remote = scenario(); remote.s.now = t.s.now;
		remote.input.work(t.output.sent[0]);
		check(bans(remote)[0].expires == 1800000060);
		t.input.sent = []; t.s.IRC_burst_akills(t.input); check(!t.input.sent.length);
		t.s.now = 1800000060; t.output.sent = []; t.s.IRC_burst_akills(t.output);
		check(t.output.sent.length == 1 && t.output.sent[0] == ":services.test RAKILL bad.test bad");
	},
	"full REHASH preserves unexpired trusted services bans independently of local policy": function() {
		var t = scenario(); akill(t, "bad@bad.test", 60); rehash(t, "SERVICES.TEST");
		check(bans(t).length == 2 && t.s.isklined("bad@bad.test") && t.s.isklined("other@local.test"));
		t.s.now += 60; rehash(t, "services.test"); check(bans(t).length == 1);
		akill(t, "bad@bad.test", 60); rehash(t, "different.test"); check(bans(t).length == 1);
	},
	"explicit REHASH AKILLS still clears services bans while preserving local bans": function() {
		var t = scenario(); akill(t, "bad@bad.test", 60);
		t.s.KLines.push(new t.s.KLine("*@local.test", "Local", "K"));
		t.owner.mode |= t.s.USERMODE_OPER; t.owner.flags = t.s.OLINE_CAN_REHASH;
		t.owner.work("REHASH AKILLS"); check(bans(t).length == 1 && bans(t)[0].type == "K");
	}
});
