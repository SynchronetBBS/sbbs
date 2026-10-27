/* Native identity, SQLite policy, and legacy-safe transport tests.
 * No live BBS accounts, telegrams, or IRC listeners are modified.
 */
"use strict";
load(js.exec_dir + "lib.sjs");
load("sbbsdefs.js"); load("irclib.js");
load("ircd/services-common.js"); load("ircd/services-store.js"); load("ircd/services-core.js");
load("ircd/services.js");
function check(ok, message) { if (!ok) throw new Error(message || "Assertion failed"); }
function throws(fn) { var caught = false; try { fn(); } catch (_) { caught = true; } check(caught, "Expected rejection"); }
function identity(number, name) { return {id:"TEST:" + number + ":100", name:name, home:"test.example", qwk:"TEST", number:number, created:100, username:IRC_services_escape(name) + "+TEST"}; }
var paths = [], stores = [];
function store() { var path = "/tmp/ircd-services-" + IRC_services_random() + ".sqlite"; paths.push(path); var db = new IRC_Services_Store(path); stores.push(db); return db; }
function scenario() {
	var output = [], db = store(), engine = new IRC_Services_Engine(db, {}, function(a) { output.push(a); });
	var user = {ref:"session-1", nick:"Alice", rev:"Alice", server:"test.example", created:100, account:null};
	engine.event({type:"user", user:user}); engine.event({type:"identify", ref:user.ref, identity:identity(1,"Some User")});
	return {db:db, engine:engine, user:user, output:output};
}
try {
run_tests({
	"aliases use dots for spaces and escape delimiters": function() {
		check(IRC_services_escape("Some User+%@!") == "Some.User%2B%25%40%21");
		check(IRC_services_unescape("Some%20User") == "Some User");
		throws(function() { IRC_services_unescape("bad%xx"); });
	},
	"account ID survives alias changes and distinguishes record reuse": function() {
		var u = {number:7, alias:"Some User", name:"Real Name", stats:{firston_date:100}};
		var a = IRC_services_identity(u,"TEST","test.example"); u.alias = "Renamed";
		var b = IRC_services_identity(u,"TEST","test.example"); check(a.id == b.id && a.username != b.username);
		u.stats.firston_date++; check(IRC_services_identity(u,"TEST","test.example").id != a.id);
		u.alias = ""; check(IRC_services_identity(u,"TEST","test.example").username == "Real.Name+TEST");
	},
	"BBS authentication checks password and account eligibility": function() {
		var record = {number:1, alias:"Some User", name:"Real", stats:{firston_date:100}, settings:0, security:{password:"SECRET", restrictions:0}};
		var scope = {system:{find_login_id:function(name) { return name == "Some.User" ? 1 : 0; }}, User:function() { return record; }, USER_DELETED:USER_DELETED, USER_INACTIVE:USER_INACTIVE, UFLAG_G:UFLAG_G};
		load(scope, "ircd/services-common.js");
		check(scope.IRC_services_authenticate("Some.User","secret","TEST","test.example").id == "TEST:1:100");
		check(!scope.IRC_services_authenticate("Some.User","wrong","TEST","test.example"));
		record.settings = USER_INACTIVE; check(!scope.IRC_services_authenticate("Some.User","secret","TEST","test.example"));
		record.settings = 0; record.security.restrictions = UFLAG_G; check(!scope.IRC_services_authenticate("Some.User","secret","TEST","test.example"));
	},
	"native SQLite stores accounts without passwords and persists ownership": function() {
		var s = scenario(); s.engine.command("NickServ", s.user, "REGISTER");
		check(s.db.data.nicks.$ALICE == s.db.data.memberships["TEST:1:100"]);
		check(s.db.db.columns("accounts").every(function(c) { return c.name != "password"; }));
		var path = paths[paths.length - 1]; s.db.close(); stores.pop();
		var reopened = new IRC_Services_Store(path); stores.push(reopened); check(reopened.data.nicks.$ALICE == reopened.data.memberships["TEST:1:100"]);
	},
	"SQLite rejects a competing owner and rolls back failed changes": function() {
		var s = scenario(), path = paths[paths.length - 1]; throws(function() { new IRC_Services_Store(path); });
		var invalid = IRC_services_copy(s.db.data); invalid.nicks.$BAD = "missing"; invalid.nick_last_used.$BAD = Date.now(); invalid.memo_accounts.$BAD = "TEST:1:100";
		throws(function() { s.db.save(invalid); }); check(!s.db.data.nicks.$BAD); check(s.db.db.query("SELECT * FROM nicks").length == 0);
	},
	"identifying does not register nicknames; REGISTER requires identification": function() {
		var s = scenario(); check(Object.keys(s.db.data.nicks).length == 0);
		s.engine.event({type:"identify", ref:s.user.ref, identity:null}); throws(function() { s.engine.command("NickServ",s.user,"REGISTER"); });
	},
	"GHOST authorizes by BBS account, not by matching visible nickname": function() {
		var s = scenario(); s.engine.command("NickServ",s.user,"REGISTER");
		var ghost = {ref:"ghost",nick:"Alice",rev:"Alice"}; s.engine.event({type:"user", user:ghost});
		s.user.nick = "Other"; s.user.rev = "Other";
		s.engine.command("NickServ",s.user,"GHOST Alice"); check(s.output.some(function(a) { return a.type == "kill" && a.target.ref == "ghost"; }));
		s.engine.event({type:"identify",ref:s.user.ref,identity:identity(2,"Impostor")});
		throws(function() { s.engine.command("NickServ",s.user,"GHOST Alice"); });
	},
	"registered nicknames get a grace period and logout restarts enforcement": function() {
		var s = scenario(); s.engine.command("NickServ",s.user,"REGISTER");
		s.engine.event({type:"identify",ref:s.user.ref,identity:null}); check(s.engine.deadlines[s.user.ref]);
		s.engine.event({type:"tick"}); check(!s.output.some(function(a) { return a.type == "rename"; }));
		s.engine.deadlines[s.user.ref].at = 0; s.engine.event({type:"tick"}); check(s.output.some(function(a) { return a.type == "rename"; }));
	},
	"periodic identity checks cannot extend another account's nickname grace": function() {
		var s = scenario(); s.engine.command("NickServ",s.user,"REGISTER");
		s.engine.event({type:"identify",ref:s.user.ref,identity:identity(2,"Other User")});
		s.engine.deadlines[s.user.ref].at = 0;
		s.engine.event({type:"identify",ref:s.user.ref,identity:identity(2,"Other User")});
		s.engine.event({type:"tick"}); check(s.output.some(function(a) { return a.type == "rename"; }));
	},
	"ChanServ founder access follows accounts across nickname changes": function() {
		var s = scenario(), c = {ref:"chan",name:"#test",flags:"+nt",key:"",limit:0,topic:"",users:[{ref:s.user.ref,op:true,voice:false}]};
		s.engine.event({type:"channel",channel:c}); s.engine.command("ChanServ",s.user,"REGISTER #test Description");
		s.user.nick = "NewNick"; s.user.rev = "NewNick"; check(s.engine.level(s.user,c) == 100);
		s.engine.command("ChanServ",s.user,"SET #test MLOCK +nt-k");
		check(s.db.data.channels["$#TEST"].founder == s.db.data.memberships["TEST:1:100"]);
		s.engine.event({type:"identify",ref:s.user.ref,identity:identity(2,"Different")}); check(s.engine.level(s.user,c) == 0);
	},
	"ChanServ access entries accept qualified BBS aliases": function() {
		var s = scenario(), c = {ref:"chan",name:"#test",flags:"+nt",key:"",limit:0,topic:"",users:[{ref:s.user.ref,op:true,voice:false}]};
		s.engine.event({type:"channel",channel:c}); s.engine.command("ChanServ",s.user,"REGISTER #test Description");
		s.engine.event({type:"user",user:{ref:"other",nick:"Other",rev:"Other"}});
		s.engine.event({type:"identify",ref:"other",identity:identity(2,"Another User")});
		s.engine.command("ChanServ",s.user,"ACCESS #test ADD Another.User+TEST OP");
		check(s.db.data.channels["$#TEST"].access[s.db.data.memberships["TEST:2:100"]] == 50);
	},
	"mode locks reject conflicting modes and invalid arguments": function() {
		throws(function() { IRC_services_mlock(["+sp"]); }); throws(function() { IRC_services_mlock(["+l","NaN"]); });
		check(IRC_services_mlock(["+nt-kl"]).off == "" && IRC_services_mlock(["+nt-kl"]).key === "");
	},
	"transport completes mutual authentication and segments messages": function() {
		var wire = [], received = [], a = new IRC_Services_Transport("A",function(peer,text) { wire.push(["A",peer,text]); },function(peer,m) { received.push(m); });
		var b = new IRC_Services_Transport("B",function(peer,text) { wire.push(["B",peer,text]); },function(peer,m) { received.push(m); });
		a.write("B",{text:new Array(1501).join("x")});
		while (wire.length) { var m = wire.shift(); (m[1] == "A" ? a : b).read(m[0],m[2]); }
		check(received.length == 1 && received[0].text.length == 1500);
	},
	"transport handles simultaneous opens and rejects replay or modification": function() {
		var wire = [], received = [], last;
		var a = new IRC_Services_Transport("A",function(p,t) { wire.push(["A",p,t]); },function(p,m) { received.push(m); });
		var b = new IRC_Services_Transport("B",function(p,t) { wire.push(["B",p,t]); },function(p,m) { received.push(m); });
		a.write("B",{n:1}); b.write("A",{n:2});
		for (var loops = 0; wire.length && loops < 100; loops++) { var m = wire.shift(); if (m[2].indexOf("SVC1 D") == 0) last = m; (m[1] == "A" ? a : b).read(m[0],m[2]); }
		check(received.length == 2); (last[1] == "A" ? a : b).read(last[0],last[2]); check(received.length == 2);
		a.write("B",{n:3}); var changed = wire.shift(); b.read("A",changed[2] + "x"); check(received.length == 2);
		b.read("A",changed[2]); check(received.length == 3);
	},
	"forged handshake cannot authenticate a service agent": function() {
		var wire = [], received = [], b = new IRC_Services_Transport("B",function(p,t) { wire.push(t); },function(p,m) { received.push(m); });
		b.read("A","SVC1 H " + new Array(65).join("a"));
		b.read("A","SVC1 A " + new Array(65).join("b")); check(!b.peers.$A.key && !received.length);
	},
	"service credentials and internal RPC payloads are redacted": function() {
		["PASS secret", "NS IDENTIFY Alice secret", ":alice PRIVMSG NickServ@test.example :IDENTIFY Alice secret", ":agent PRIVMSG BBSsvc_test_abcd :secret"].forEach(function(line) { check(IRC_services_redact(line).indexOf("secret") < 0); });
		check(IRC_services_redact("PRIVMSG #test :public text") == "PRIVMSG #test :public text");
	},
	"queue and malformed-request errors cannot escape socket callbacks": function() {
		var replies = [], manager = Object.create(IRC_Services_Manager.prototype);
		manager.intercept_command = manager.receive_message = function() { throw new Error("Simulated queue full"); };
		manager.notice = function(service,user,text) { replies.push(text); };
		manager.peer = function() { return {}; }; manager.reply = function(peer,id,result) { replies.push(result.error); };
		check(manager.intercept({local:true},"NickServ","REGISTER"));
		manager.receive("peer",{id:"request"});
		check(replies.length == 2 && replies.every(function(text) { return text.indexOf("busy or unavailable") >= 0; }));
	},
	"local account revalidation revokes invalid sessions without overwriting newer logins": function() {
		var manager = Object.create(IRC_Services_Manager.prototype), pending;
		var user = {services_identity:identity(1,"Original")};
		manager.config = {server:"test.example"}; manager.jobs = {};
		manager.describe = function() { return {ref:"session"}; }; manager.find = function() { return user; };
		manager.central_peer = function() { return null; }; manager.sync_session = function() {};
		manager.apply_identity = function(u,account) { u.services_identity = account; };
		manager.job = function(job,callback) { pending = callback; };
		manager.refresh_local_identity(user,100); pending({error:"Account no longer exists"}); check(!user.services_identity);
		manager.add_login(user,identity(1,"Original"),true); manager.refresh_local_identity(user,131);
		var newer = identity(2,"New login"); manager.add_login(user,newer,true);
		pending({error:"Obsolete failure"}); check(user.services_identity === newer);
	},
	"ordinary users cannot become agents by taking an old server's helper nick": function() {
		var scope = {Users:{}, Servers:{"legacy.test":{}}, ServerName:"central.test"};
		load(scope,"ircd/services.js");
		var name = scope.IRC_services_agent("legacy.test"), u = {nick:name, servername:"legacy.test", local:false, uprefix:"~bbsircdag"};
		scope.Users[name.toUpperCase()] = u;
		var manager = Object.create(scope.IRC_Services_Manager.prototype);
		check(!manager.peer(name));
		u.uprefix = "Some.User+LEGACY"; check(!manager.peer(name));
		u.uprefix = "bbs+ircd+agent"; check(manager.peer(name) === u);
	},
	"central enable setting defaults off, resets on rehash, and survives config writes": function() {
		var scope = {ServerName:"hub.test", ServerDesc:"Test", Default_Port:19667, CapabEnforce:false};
		load(scope,"ircd/config.js"); scope.Clear_Config_Globals();
		check(scope.EmbeddedServices_Config.enabled === false);
		scope.ini_EmbeddedServices("",{Host:"services.test"}); check(!scope.EmbeddedServices_Config.enabled);
		scope.ini_EmbeddedServices("",{Enabled:"true"}); check(scope.EmbeddedServices_Config.enabled === true);
		var path = "/tmp/ircd-services-config-" + IRC_services_random() + ".ini"; paths.push(path);
		check(scope.Write_Config_File(path));
		var file = new File(path); check(file.open("r"));
		var saved; try { saved = file.iniGetObject("EmbeddedServices"); } finally { file.close(); }
		check(saved.Enabled === true);
		scope.Clear_Config_Globals(); check(scope.EmbeddedServices_Config.enabled === false);
		scope.ini_EmbeddedServices("",saved); check(scope.EmbeddedServices_Config.enabled === true);
		scope.ini_EmbeddedServices("",{Enabled:"false"}); check(scope.EmbeddedServices_Config.enabled === false);
	},
	"central activation and queued readiness both require explicit enablement": function() {
		var scope = {Users:{}, Servers:{}, Unregistered:{}, Local_Servers:{}, ULine_Exists:function() { return true; }};
		load(scope,"ircd/services.js");
		var jobs = [], manager = Object.create(scope.IRC_Services_Manager.prototype);
		manager.config = {host:"services.test", server:"hub.test"}; manager.localhost = true;
		manager.ensure_agent = function() { return true; }; manager.queue = {write:function(job) { jobs.push(job.type); return true; }};
		manager.start_central(); check(!manager.activating && !jobs.length);
		manager.config.enabled = true; manager.start_central(); check(manager.activating && jobs[0] == "activate");
		manager.config.enabled = false; manager.introduce();
		check(!manager.central && !manager.activating && jobs[1] == "deactivate" && !Object.keys(scope.Servers).length);
	},
	"disabled services create no timer and local BBS opt-in remains independent": function() {
		var timers = 0, helpers = 0, scope = {ServerName:"hub.test", server:{interface_ip_addr_list:["127.0.0.1"]}, DNS_Resolver:{},
			js:{exec_dir:js.exec_dir, load_path_list:js.load_path_list, setInterval:function() { timers++; return 1; }}};
		load(scope,"ircd/services.js");
		scope.IRC_Services_Manager.prototype.ensure_agent = function() { helpers++; return true; };
		var options = {enabled:false, nickserv:false, memoserv:false, host:"services.test", database:""};
		var manager = new scope.IRC_Services_Manager(options);
		check(!timers && !helpers && manager.interval === undefined);
		options.nickserv = true; manager = new scope.IRC_Services_Manager(options);
		check(timers == 1 && helpers == 1 && !manager.config.enabled);
	}
});
} finally {
	stores.forEach(function(s) { try { s.close(); } catch (_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p + "-journal"); });
}
