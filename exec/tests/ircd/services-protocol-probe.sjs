/* Observational Bahamut compatibility probes, without sockets or live state.
 * Run manually with JSexec; output records behavior, not pass/fail results.
 * Keep these separate from regression suites while features are unsupported.
 */
"use strict";
load(js.exec_dir + "state-fixture.sjs");
function scenario() {
	var s = new_state(), input = new_peer(s, "services.test", false), output = new_peer(s, "downstream.test", true);
	input.uline = true;
	input.protocol = "bahamut";
	input.their_capab = "TS3 SSJOIN NICKIP NOQUIT TSMODE UNCONNECT".split(" ");
	output.their_capab.push("CHANEXCEPT");
	s.ULines = ["services.test"]; s.KLines = []; s.ZLines = []; s.logs = [];
	s.log = function(level, line) { s.logs.push(line); };
	s.Scan_For_Banned_Clients = function() {};
	var bot = new_user(s, "ChanServ", input.nick); bot.servername = input.nick; bot.uline = true;
	var target = new_user(s, "Target", output.nick), owner = new_user(s, "Owner");
	var chan = join_user(s, owner, "#audit"); join_user(s, target, chan.nam);
	chan.modelist[s.CHANMODE_OP][owner.id] = owner;
	return {s:s, input:input, output:output, bot:bot, target:target, owner:owner, chan:chan};
}
function result(name, value) { writeln(name + ": " + JSON.stringify(value)); }
var t = scenario();
t.input.work(":services.test SVSNICK Target Guest123 :1800000000");
result("SVSNICK", {nick:t.target.nick, relayed:t.output.sent, unrecognized:t.s.logs.some(function(l) { return l.indexOf("unrecognized command: SVSNICK") >= 0; })});
t = scenario();
t.input.work(":ChanServ SVSMODE Target :+r");
t.input.work(":services.test SVSMODE Target +d 123456");
result("SVSMODE", {mode:t.target.get_usermode(), servicestamp:t.target.servicestamp || null, relayed:t.output.sent});
t = scenario();
t.input.work(":ChanServ MODE #audit 0 +e *!*@trusted.test");
result("incoming ordinary +e", {exceptions:t.chan.modelist[t.s.CHANMODE_EXCEPT], relayed:t.output.sent});
t = scenario();
t.owner.set_chanmode(t.chan, ["+e", "*!*@trusted.test"], 0);
result("outgoing ordinary +e", {local:t.chan.modelist[t.s.CHANMODE_EXCEPT], services:t.input.sent, capableHub:t.output.sent});
t.input.sent = []; t.input.server_chan_info(t.chan);
result("ordinary +e reconnect burst", {services:t.input.sent.filter(function(l) { return / MODE /.test(l); })});
t = scenario();
t.input.work(":ChanServ MODE #audit 0 +jko 5:10 secret Target");
result("unsupported +j shifts following arguments", {key:t.chan.arg[t.s.CHANMODE_KEY], targetOp:!!t.chan.modelist[t.s.CHANMODE_OP][t.target.id], relayed:t.output.sent});
t = scenario();
t.input.work(":ChanServ MODE #audit 0 +Iov *!*@trusted.test Target Owner");
result("unsupported +I shifts following arguments", {targetOp:!!t.chan.modelist[t.s.CHANMODE_OP][t.target.id], targetVoice:!!t.chan.modelist[t.s.CHANMODE_VOICE][t.target.id], ownerVoice:!!t.chan.modelist[t.s.CHANMODE_VOICE][t.owner.id]});
t = scenario();
t.input.work(":services.test AKILL bad.test bad 1 OperServ 1800000000 :One second");
t.s.now += 10;
result("AKILL expiry", {stillBanned:!!t.s.isklined("bad@bad.test"), entry:t.s.KLines[0]});
t = scenario();
t.s.KLines.push(new t.s.KLine("*@*.test", "Local policy", "K"));
t.input.work(":services.test AKILL bad.test bad 60 OperServ 1800000000 :Services policy");
result("AKILL shadowed by broader local ban", {entries:t.s.KLines});
t.input.work(":services.test RAKILL bad.test bad");
result("RAKILL deletes broader local ban", {stillBanned:!!t.s.isklined("other@other.test"), entries:t.s.KLines});
t = scenario();
t.chan.mode |= t.s.CHANMODE_MODERATED;
t.input.work(":services.test SJOIN 1799999999 #audit + :");
result("empty services SJOIN", {moderated:!!(t.chan.mode & t.s.CHANMODE_MODERATED), ownerOp:!!t.chan.modelist[t.s.CHANMODE_OP][t.owner.id], created:t.chan.created, relayed:t.output.sent});
t = scenario();
t.input.work("NICK StampUser 1 1800000000 +i user host.test services.test 987654 0 :Stamped user");
t.output.sent = []; t.output.server_nick_info(t.s.Users.STAMPUSER);
result("services stamp across NICK relay", {receivedStamp:987654, nick:t.output.sent[0]});
t = scenario();
t.input.work(":services.test SGLINE 3 :Bad:Realname policy");
t.input.work(":services.test UNSGLINE :Bad");
t.input.work(":services.test SQLINE Bad* :Nickname policy");
t.input.work(":services.test UNSQLINE Bad*");
result("S-line commands", {unrecognized:t.s.logs.filter(function(l) { return l.indexOf("unrecognized command:") >= 0; }), relayed:t.output.sent});
t = scenario();
t.input.sent = []; t.input.server_chan_info(t.chan);
result("SSJOIN-capable services channel burst", {wire:t.input.sent});
t = scenario();
t.target.socket = {ssl_session:true}; t.target.local = true;
t.target.created = t.s.now; t.target.uprefix = "target"; t.target.hostname = "client.test";
t.target.servername = t.s.ServerName; t.target.realname = "TLS user"; t.target.ip = "192.0.2.1";
t.input.sent = []; t.input.server_nick_info(t.target);
result("TLS client presented to Bahamut", {knownTransport:t.s.IRC_client_tls_status(t.target), wire:t.input.sent});
result("mode-letter collisions", {r:t.s.USERMODE_CHAR.r === t.s.USERMODE_REJECTED, d:t.s.USERMODE_CHAR.d === t.s.USERMODE_DEBUG});
