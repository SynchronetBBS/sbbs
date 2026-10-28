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

function accepted_socket() {
	return {local_port:6697, remote_port:45000, remote_ip_address:"192.0.2.1",
		ssl_session:false, is_connected:true,
		close:function() { this.is_connected = false; }};
}
function accept_state() {
	var s = state();
	s.warnings = [];
	s.log = function(level, message) { if (level == LOG_WARNING) s.warnings.push(message); };
	s.IP_Banned = function() { return false; };
	s.register_accepted_socket = function(sock) { sock.registered = true; return true; };
	return s;
}
function accept_socket(s, sock) {
	return s.accept_new_socket.call({accept:function() { return sock; }});
}

run_tests({
	"inbound TLS detects old hosts and warns once while retaining TLS": function() {
		var s = accept_state();
		[undefined, true].forEach(function(method) {
			var sock = accepted_socket();
			sock.acceptTLS = method;
			sock.__defineSetter__("ssl_server", function(value) {
				check(value && this.tls_minver == 102, "legacy TLS minimum was not set first");
				check(!this.registered, "IRC registration preceded TLS");
				this.ssl_session = true;
			});
			check(accept_socket(s, sock) && sock.registered && sock.ssl_session);
		});
		check(s.warnings.length == 1 && /synchronous TLS handshakes/.test(s.warnings[0]));
		check(s.Pending_TLS_Accepts == 0, "legacy handshakes changed the asynchronous count");
	},
	"legacy TLS failures close before IRC registration": function() {
		["throw", "disabled", "disconnected"].forEach(function(mode) {
			var s = accept_state(), sock = accepted_socket();
			sock.__defineSetter__("ssl_server", function() {
				if (mode == "throw") throw new Error("Handshake rejected");
				if (mode == "disconnected") { this.ssl_session = true; this.is_connected = false; }
			});
			check(!accept_socket(s, sock) && !sock.registered && !sock.is_connected, mode);
			check(s.Pending_TLS_Accepts == 0);
		});
	},
	"capable hosts use asynchronous TLS and defer registration until completion": function() {
		var s = accept_state(), sock = accepted_socket(), complete;
		sock.__defineSetter__("ssl_server", function() { throw new Error("Unexpected synchronous fallback"); });
		sock.acceptTLS = function(callback, timeout) {
			check(this.tls_minver == 102 && timeout == 10);
			complete = callback;
			return true;
		};
		check(accept_socket(s, sock) && !sock.registered && s.Pending_TLS_Accepts == 1);
		sock.ssl_session = true;
		complete.call(sock);
		check(sock.registered && s.Pending_TLS_Accepts == 0 && !s.warnings.length);
	},
	"asynchronous TLS failures never retry through the legacy fallback": function() {
		["throw", "rejected", "callback"].forEach(function(mode) {
			var s = accept_state(), sock = accepted_socket(), sync = 0, complete;
			sock.__defineSetter__("ssl_server", function() { sync++; });
			sock.acceptTLS = function(callback) {
				if (mode == "throw") throw new Error("Cannot start handshake");
				complete = callback;
				return mode == "callback";
			};
			var started = accept_socket(s, sock);
			if (mode == "callback") { check(started); complete.call(sock); }
			else check(!started);
			check(!sync && !sock.registered && !sock.is_connected && s.Pending_TLS_Accepts == 0, mode);
		});
	},
	"plaintext and established TLS sockets do not need the new host method": function() {
		[false, true].forEach(function(encrypted) {
			var s = accept_state(), sock = accepted_socket();
			if (encrypted) sock.ssl_session = true;
			else sock.local_port = 6667;
			check(accept_socket(s, sock) && sock.registered && !s.warnings.length);
			check(s.Pending_TLS_Accepts == 0);
		});
	},
	"services TLS requirements default off and survive save, reload, and rehash": function() {
		var s = state(), path = "/tmp/ircd-services-tls-" + random(0x7fffffff) + ".ini";
		try {
			check(!s.EmbeddedServices_Config.require_tls && !s.EmbeddedServices_Config.bbs_require_tls);
			s.load_config_defaults();
			s.ini_BBSServices("", {NickServ:true, RequireTLS:true});
			s.ini_EmbeddedServices("", {Enabled:true, RequireTLS:true});
			check(s.Write_Config_File(path)); s.Config_Filename = path; s.Read_Config_File();
			check(s.EmbeddedServices_Config.require_tls && s.EmbeddedServices_Config.bbs_require_tls);
			s.Clear_Config_Globals();
			check(!s.EmbeddedServices_Config.require_tls && !s.EmbeddedServices_Config.bbs_require_tls);
			s.ini_BBSServices("", {RequireTLS:"typo"}); s.ini_EmbeddedServices("", {RequireTLS:"typo"});
			check(s.EmbeddedServices_Config.require_tls && s.EmbeddedServices_Config.bbs_require_tls);
		} finally { file_remove(path); }
	},
	"BBS PASS authentication requires a TLS client socket only when configured": function() {
		[false,true].forEach(function(required) { [false,true].forEach(function(tls) {
			var s = state(), calls = 0;
			s.EmbeddedServices_Config.bbs_require_tls = required; s.EmbeddedServices_Config.nickserv = true;
			s.system = {matchuser:function() {}, qwk_id:"TEST"};
			s.IRC_services_authenticate = function() { calls++; return {number:1, username:"User+TEST"}; };
			s.User = function() { return {}; };
			var u = {socket:{ssl_session:tls}, uprefix:"user", nick:"User", hostname:"test", password:"example", ircout:function() {}, welcome:function() { this.welcomed = true; }};
			s.Unregistered_Check_User_Registration.call(u);
			check(u.welcomed && (required && !tls ? !calls && u.uprefix == "~user" : calls == 1 && u.uprefix == "User+TEST"));
		}); });
	},
	"missing optional certificate attributes accept both native error spellings": function() {
		var s = state();
		["CryptLib error -43", "Cryptlib error -43 (attribute absent)"].forEach(function(message) {
			var cert = {get_attribute:function() { throw new Error(message); }};
			check(s.IRC_tls_optional_attribute(cert, 1) === undefined, message);
		});
		["CryptLib error -20", "CryptLib error -430"].forEach(function(message) {
			var error = new Error(message), caught;
			try { s.IRC_tls_optional_attribute({get_attribute:function() { throw error; }}, 1); }
			catch (e) { caught = e; }
			check(caught === error, "unrelated certificate failure was ignored");
		});
	},
	"certificate chain exhaustion accepts both native error spellings and resets its cursor": function() {
		var s = state();
		s.CryptCert = {TYPE:{CERTCHAIN:2}, CURSOR:{FIRST:-1, NEXT:-2}};
		s.IRC_tls_check_certificate = function() {};
		["CryptLib error -43", "Cryptlib error -43 (end of chain)", "CryptLib error -20"].forEach(function(message) {
			var cert = {certtype:2}, cursor, error = new Error(message), caught;
			cert.__defineSetter__("current_certificate", function(value) {
				if (value == s.CryptCert.CURSOR.NEXT) throw error;
				cursor = value;
			});
			try { s.IRC_tls_check_chain_usage(cert); } catch (e) { caught = e; }
			check(cursor == s.CryptCert.CURSOR.FIRST, "chain cursor was not restored");
			check(message.indexOf("-20") >= 0 ? caught === error : !caught, message);
		});
	},
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
			s.ini_BBSServices("", {FileServ:true});
			s.ini_FileServ("", {TLS:"required",PublicDirs:"pub_*",ExcludeLibs:"staff_*",Channels:"#files",Address:"192.0.2.1",PortFirst:41000,PortLast:41007,JSexec:"/private/jsexec"});
			s.ini_Port("17001", {TLS:true});
			s.CLines.push(cline(s, true, 17002));
			s.NLines.push(new s.NLine("peer.test", "private-password", "peer.test", 0, 1, true));
			s.NLines.push(new s.NLine("incoming.test", "private-password", "incoming.test", 0, 1, true));
			check(s.Write_Config_File(path)); s.Config_Filename = path; s.Read_Config_File();
			check(s.TLS_Config.ca_file == "test-ca.pem" && s.TLS_Ports[17001]);
			check(s.EmbeddedServices_Config.fileserv && s.EmbeddedServices_Config.files.tls=="required" && s.EmbeddedServices_Config.files.public_dirs=="pub_*" && s.EmbeddedServices_Config.files.port_last==41007 && s.EmbeddedServices_Config.files.jsexec=="/private/jsexec");
			check(s.Find_CLine("peer.test").tls && s.Find_NLine("peer.test").tls);
			check(s.Find_NLine("incoming.test").tls && !s.Find_CLine("incoming.test").port);
			s.Clear_Config_Globals(); check(!s.TLS_Config.ca_file && !s.TLS_Ports[17001]);
			check(!s.EmbeddedServices_Config.fileserv && !s.EmbeddedServices_Config.files.public_dirs && s.EmbeddedServices_Config.files.tls=="optional");
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
