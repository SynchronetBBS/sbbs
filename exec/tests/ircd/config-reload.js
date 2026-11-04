/* A failed live REHASH must retain the active policy, objects and callbacks. */
"use strict";
load(js.exec_dir + "lib.sjs");
function check(value, message) { if (!value) throw new Error(message); }
var paths = [];
function config_state() {
	var s = {IRCDCFG_Editor:false, system:system, Servers:{}, Local_Servers:{}, Users:{}, log:function() {}};
	load(s, "ircd/config.js");
	s.effects = [];
	["IRC_cancel_outbound_connect", "IRC_tls_clear_cache", "Scan_For_Banned_Clients", "IRC_services_rehash_state", "IRC_reconcile_slines", "IRC_services_start"].forEach(function(key) {
		s[key] = function() { s.effects.push(key); };
	});
	s.js = {clearTimeout:function(id) { s.effects.push("cancel:" + id); }};
	s.IRC_Services = {};
	s.Epoch = function() { return 1000; };
	s.Reset_Autoconnect = function() { s.effects.push("autoconnect"); };
	return s;
}
function write_config(s, name) {
	var path = IRCD_TEST_TEMP_DIR + "reload-" + random(0x7fffffff) + ".ini";
	paths.push(path);
	var f = new File(path); check(f.open("w"), "open fixture");
	f.write("[Info]\nServername=" + name + "\n[Services:empty]\nDisabled=true\n[Allow]\nMask=*@*\nClass=1\n[Class:1]\nConnectFrequency=0\n[Server:peer]\nServername=peer.test\nHostname=127.0.0.1\nPassword=fixture-only\nPort=19988\nClass=1\n[TLS]\nCAFile=private.pem\n"); f.close();
	s.Config_Filename = path;
}
try {
	run_tests({
		"open, read and parser failures retain every active configuration reference and side effect": function() {
			["missing", "unreadable", "read", "parser"].forEach(function(mode) {
				var s = config_state(); write_config(s, "active.test");
				check(s.Read_Config_File(), "initial load");
				s.CLines[0].next_connect = 42;
				s.Servers["services.synchro.net"] = {nick:"services.synchro.net", uline:false};
				var active = s.IRC_config_capture(), parser = s.read_ini_config;
				s.effects = [];
				if (mode == "missing") check(file_remove(s.Config_Filename), "remove fixture");
				if (mode == "unreadable") s.File = function() { return {open:function() { return false; }}; };
				if (mode == "read") {
					s.File = function() { return {is_open:true, error:5, open:function() { return true; }, close:function() { this.is_open = false; }}; };
					s.read_ini_config = function() { s.ServerName = "partial.test"; };
				}
				if (mode == "parser") s.read_ini_config = function() { s.ServerName = "partial.test"; s.ULines.push("services.synchro.net"); throw new Error("Parser failed"); };
				check(!s.Read_Config_File(), "failed reload reported success: " + mode);
				var retained = s.IRC_config_capture();
				Object.keys(active).forEach(function(key) { check(retained[key] === active[key], "lost active " + key + ": " + mode); });
				check(!s.effects.length && s.CLines[0].next_connect == 42, "failed reload touched callbacks/cache/services: " + mode);
				check(!s.Servers["services.synchro.net"].uline && !s.ULines.length, "failed reload granted service authority");
				delete s.File; s.read_ini_config = parser;
				write_config(s, "replacement.test");
				check(s.Read_Config_File() && s.ServerName == "replacement.test", "retry failed");
				check(s.effects.indexOf("cancel:42") >= 0 && !active.CLines[0].next_connect, "successful reload retained obsolete timer");
				check(s.effects.indexOf("IRC_cancel_outbound_connect") >= 0 && s.effects.indexOf("IRC_tls_clear_cache") >= 0, "successful reload retained obsolete TLS attempt/cache");
			});
		},
		"first-load and editor parser failures still propagate after restoring their globals": function() {
			[false, true].forEach(function(editor) {
				var s = config_state(); s.IRCDCFG_Editor = editor; write_config(s, "invalid.test");
				var original = s.IRC_config_capture(), expected = new Error("Parser failed"), caught;
				s.read_ini_config = function() { s.ServerName = "partial.test"; throw expected; };
				try { s.Read_Config_File(); } catch (error) { caught = error; }
				check(caught === expected && !s.IRC_Config_Loaded, "initial parse failure was swallowed");
				var restored = s.IRC_config_capture();
				Object.keys(original).forEach(function(key) { check(restored[key] === original[key], "initial failure retained partial " + key); });
				check(!s.effects.length, "initial parse failure touched runtime");
			});
		},
		"first startup and editor still support defaults when the file is missing": function() {
			[false, true].forEach(function(editor) {
				var s = config_state(); s.IRCDCFG_Editor = editor;
				s.Config_Filename = IRCD_TEST_TEMP_DIR + "absent-" + random(0x7fffffff) + ".ini";
				check(s.Read_Config_File() && s.YLines[0] && s.ULines.length, "initial defaults unavailable");
			});
		}
	});
} finally { paths.forEach(function(path) { file_remove(path); }); }
