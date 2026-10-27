/* TLS configuration and credential-release regressions; no network needed. */
"use strict";
load(js.exec_dir + "lib.sjs");

function check(value, message) { if (!value) throw new Error(message || "Assertion failed"); }
function state() {
	var s = {system:system, log:function() {}, IRCDCFG_Editor:true,
		Servers:{}, Local_Users:{}, Unregistered:{}, Assigned_IDs:{}, Outbound_Connect_in_Progress:true,
		CLines:[], NLines:[], HLines:[], ILines:[], KLines:[], OLines:[], PLines:[],
		QLines:[], ULines:[], YLines:[], ZLines:[], WLines:[], RBL:[],
		Default_Port:6667, Admin1:"", Admin2:"", Admin3:"", Die_Password:"", Restart_Password:"",
		Config_Filename:"", Time_Config_Read:0, CapabEnforce:false, USERMODE_ROUTING:8192,
		ServerName:"hub.test", ServerDesc:"TLS test", SERVER_CAPAB:"TSJOIN",
		js:{clearTimeout:function() {}}};
	["irclib.js", "ircd/config.js", "ircd/core.js", "ircd/tls.js", "ircd/unregistered.js"].forEach(function(f) { load(s, f); });
	s.Clear_Config_Globals();
	s.YLines[1] = new s.YLine(120, 0, 10, 1000000);
	s.Unregistered_Client = function() {};
	s.umode_notice = function() {};
	return s;
}
function cline(s, tls, port) { return new s.CLine("peer.test", "private-password", "peer.test", port || 6667, 1, tls); }
function socket(c) {
	return {cline:c, is_connected:true, remote_port:c.port, output:[],
		send:function(line) { this.output.push(line); },
		close:function() { this.is_connected = false; }, on:function() { return 1; }};
}
function incoming(s, tls, name) {
	var client = {nick:"*", hostname:"127.0.0.1", password:"private-password", socket:{ssl_session:tls},
		quit:function(reason) { this.rejected = reason; }};
	s.Register_Unregistered_Local_Server = function(c) { c.registered = true; };
	s.Unregistered_Commands.call(client, "SERVER " + (name || "peer.test") + " 1 :Peer");
	return client;
}

run_tests({
	"server TLS requirements include standard TLS ports and preserve legacy plaintext": function() {
		var s = state();
		s.ini_Server("peer", {Hostname:"peer.test", Servername:"peer.test", Password:"test", Class:"1", Port:"6667"});
		check(!s.CLines[0].tls && !s.NLines[0].tls);
		["6697", "994"].forEach(function(port) {
			s.ini_Server(port, {Hostname:"peer.test", Servername:port + ".test", Password:"test", Class:"1", Port:port, TLS:false});
			check(s.CLines[s.CLines.length - 1].tls && s.NLines[s.NLines.length - 1].tls, "standard TLS port downgraded");
		});
		s.ini_Server("custom", {Hostname:"peer.test", Servername:"custom.test", Password:"test", Class:"1", Port:"17000", TLS:true});
		check(s.CLines[3].tls && s.NLines[3].tls);
		check(s.ini_tls("typo", false), "invalid TLS value silently allowed plaintext");
		s.TLS_Ports[17001] = true;
		check(!s.IRC_tls_required(cline(s, false, 17001), 17001), "local listening port changed remote policy");
	},
	"TLS configuration survives save, reload, and removal on rehash": function() {
		var s = state(), path = "/tmp/ircd-tls-config-" + random(0x7fffffff) + ".ini";
		try {
			s.load_config_defaults(); s.TLS_Config.ca_file = "test-ca.pem";
			s.ini_Port("17001", {TLS:true});
			s.CLines.push(cline(s, true, 17002));
			s.NLines.push(new s.NLine("peer.test", "private-password", "peer.test", 0, 1, true));
			s.NLines.push(new s.NLine("incoming.test", "private-password", "incoming.test", 0, 1, true));
			check(s.Write_Config_File(path)); s.Config_Filename = path; s.Read_Config_File();
			check(s.TLS_Config.ca_file == "test-ca.pem" && s.TLS_Ports[17001]);
			check(s.Find_CLine("peer.test").tls && s.Find_NLine("peer.test").tls);
			check(s.Find_NLine("incoming.test").tls && !s.Find_CLine("incoming.test").port);
			s.Clear_Config_Globals(); check(!s.TLS_Config.ca_file && !s.TLS_Ports[17001]);
		} finally { file_remove(path); }
	},
	"validated TLS precedes every outbound IRC credential": function() {
		var s = state(), sock = socket(cline(s, true));
		s.IRC_tls_connect = function(c) { check(!c.output.length, "credentials preceded validation"); c.verified = true; };
		check(s.handle_outbound_server_connect.call(sock));
		check(sock.verified && /^PASS private-password/.test(sock.output[0]));
	},
	"validation failure closes without credentials and schedules reconnect": function() {
		var s = state(), sock = socket(cline(s, true));
		s.YLines[1].connfreq = 60;
		s.IRC_tls_connect = function() { throw new Error("Untrusted"); };
		s.Reset_Autoconnect = function(c, delay) { s.delay = delay; };
		check(!s.handle_outbound_server_connect.call(sock));
		check(!sock.output.length && !sock.is_connected && s.delay == 60000 && !s.Outbound_Connect_in_Progress);
	},
	"plaintext links retain the existing handshake": function() {
		var s = state(), sock = socket(cline(s, false));
		s.IRC_tls_connect = function() { throw new Error("Plaintext attempted TLS"); };
		check(s.handle_outbound_server_connect.call(sock) && /^PASS /.test(sock.output[0]));
	},
	"unsupported TLS runtime fails before releasing credentials": function() {
		var s = state(), sock = socket(cline(s, true));
		check(!s.handle_outbound_server_connect.call(sock) && !sock.output.length && !sock.is_connected);
	},
	"TLS-required inbound names cannot bypass policy through a wildcard password or QWK entry": function() {
		[0, 1, 4].forEach(function(flags) {
			var s = state();
			s.NLines = [new s.NLine("*", "private-password", "*", flags, 1),
				new s.NLine("peer.test", "private-password", "peer.test", 0, 1, true)];
			var c = incoming(s, false);
			check(!c.registered && /TLS is required/.test(c.rejected));
		});
	},
	"TLS-required inbound links and plaintext legacy leaves register": function() {
		var s = state();
		s.NLines.push(new s.NLine("*", "private-password", "peer.test", 0, 1, true));
		check(incoming(s, true).registered);
		s.NLines.push(new s.NLine("*", "private-password", "leaf.test", 0, 1));
		check(incoming(s, false, "leaf.test").registered);
	},
	"verified TLS peer cannot introduce another configured server": function() {
		var s = state(), c = {nick:"*", hostname:"peer.test", cline:cline(s, true), socket:{ssl_session:true, irc_tls_verified:true},
			quit:function(reason) { this.rejected = reason; }};
		s.Unregistered_Commands.call(c, "SERVER other.test 1 :Different server");
		check(/unexpected server name/.test(c.rejected));
	},
	"manual CONNECT port override cannot disable TLS": function() {
		var s = state(), connected;
		s.Outbound_Connect_in_Progress = false;
		s.CLines.push(cline(s, true));
		s.Socket = function() { this.connect = function(host, port, callback) { this.cline = s.CLines[0]; connected = this; this.remote_port = port; }; };
		var oper = {nick:"Oper", uprefix:"oper", hostname:"test", server_notice:function() {}};
		check(s.IRCClient_do_connect.call(oper, "peer.test", "16667"));
		check(s.IRC_tls_required(connected.cline, connected.remote_port));
	}
});
