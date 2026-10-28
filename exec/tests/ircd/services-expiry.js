/* Last-used tracking and central expiry policy; private SQLite files only. */
"use strict";
load(js.exec_dir + "lib.sjs");
load("sbbsdefs.js"); load("ircd/services-common.js");
load("ircd/services-store.js"); load("ircd/services-core.js"); load("ircd/services-backend.js");

var DAY = 86400000, START = 1800000000000, paths = [], stores = [];
function check(ok, message) { if (!ok) throw new Error(message || "Assertion failed"); }
function identity(number, name) { return {id:"TEST:" + number + ":100", name:name, home:"test.example", qwk:"TEST", number:number, created:100, username:IRC_services_escape(name) + "+TEST"}; }
function reopen(path) { var db = new IRC_Services_Store(path); stores.push(db); return db; }
function scenario(config) {
	var path = "/tmp/ircd-expiry-" + IRC_services_random() + ".sqlite"; paths.push(path);
	var s = {path:path, db:reopen(path), now:START, output:[]};
	s.engine = new IRC_Services_Engine(s.db, config || {}, function(a) { s.output.push(a); });
	s.engine.now = function() { return s.now; };
	s.user = {ref:"owner", nick:"Alice", rev:"Alice", server:"test.example", created:100};
	s.channel = {ref:"channel", name:"#test", flags:"+nt", key:"", limit:0, topic:"", users:[{ref:"owner", op:true, voice:false}]};
	s.engine.event({type:"user", user:s.user});
	s.engine.event({type:"identify", ref:"owner", identity:identity(1,"Some User")});
	s.engine.event({type:"channel", channel:s.channel});
	s.engine.command("NickServ",s.user,"REGISTER");
	s.engine.command("ChanServ",s.user,"REGISTER #test Test channel");
	return s;
}
function maintain(s) { s.engine.next_maintenance = 0; s.engine.event({type:"tick"}); }
function leave(s) {
	s.engine.event({type:"quit", ref:"owner"});
	s.engine.event({type:"channel_gone", ref:"channel"});
}
function absent(s) { check(!s.db.data.nicks.$ALICE && !s.db.data.channels["$#TEST"], "unused registrations survived"); }

try {
run_tests({
	"registration records initial usage and persists it across reopen": function() {
		var s = scenario();
		check(s.db.data.nick_last_used.$ALICE == START && s.db.data.channels["$#TEST"].last_used == START);
		check(s.db.db.query("PRAGMA user_version")[0].user_version == 3);
		s.db.close(); s.db = reopen(s.path);
		check(s.db.data.nick_last_used.$ALICE == START && s.db.data.channels["$#TEST"].last_used == START);
	},
	"expiry defaults off independently of last-used tracking": function() {
		var s = scenario(); leave(s); s.now += 1000 * DAY; maintain(s);
		check(s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"], "default expired a registration");
		check(s.db.data.nick_last_used.$ALICE == START, "absence counted as use");
	},
	"authenticated owner presence keeps idle nickname and channel alive": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:1});
		s.now += 2 * DAY; maintain(s);
		check(s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"]);
		check(s.db.data.nick_last_used.$ALICE == s.now && s.db.data.channels["$#TEST"].last_used == s.now);
		check(s.db.db.query("SELECT last_used FROM nicks")[0].last_used == s.now);
	},
	"nickname and channel expiry have independent exact day boundaries": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:2}); leave(s);
		s.now = START + DAY - 1; maintain(s); check(s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"]);
		s.now++; maintain(s); check(!s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"]);
		check(!s.db.data.nick_last_used.$ALICE && s.db.db.query("SELECT * FROM nicks").length == 0);
		s.now = START + 2 * DAY; maintain(s); absent(s);
		check(!s.output.some(function(a) { return a.type == "join" || a.type == "part"; }), "expiry changed services membership");
		check(Object.keys(s.db.data.accounts).length == 1, "nickname expiry deleted the BBS account");
	},
	"services, visitors and another account using the nick do not renew ownership": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:1}); leave(s);
		var other = {ref:"other",nick:"Alice",rev:"Alice",server:"test.example",created:200};
		s.engine.event({type:"user",user:other});
		s.engine.event({type:"identify",ref:"other",identity:identity(2,"Different")});
		s.engine.event({type:"user",user:{ref:"bot",nick:"ChanServ",rev:"ChanServ"}});
		var channel = IRC_services_copy(s.channel); channel.users = [{ref:"other",op:true},{ref:"bot",op:true}];
		s.engine.event({type:"channel",channel:channel});
		s.engine.deadlines.other.at = 0; s.now += DAY; maintain(s); absent(s);
		check(!s.output.some(function(a) { return a.type == "rename"; }), "expired nickname still enforced");
	},
	"identification on a different nick does not renew registered nicknames": function() {
		var s = scenario({nick_expire_days:1});
		var renamed = IRC_services_copy(s.user); renamed.nick = renamed.rev = "OtherNick";
		s.engine.event({type:"user",user:renamed}); s.now += DAY; maintain(s);
		check(!s.db.data.nicks.$ALICE, "account login renewed an unused nickname");
		check(s.db.data.channels["$#TEST"].last_used == s.now, "founder identity depended on nick registration");
	},
	"an authenticated access-list member keeps a channel after the founder nick expires": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:1});
		var other = {ref:"other",nick:"Bob",rev:"Bob",server:"test.example",created:200};
		s.engine.event({type:"user",user:other});
		s.engine.event({type:"identify",ref:"other",identity:identity(2,"Another User")});
		s.engine.command("ChanServ",s.user,"ACCESS #test ADD Another.User+TEST VOICE");
		s.engine.event({type:"quit",ref:"owner"});
		var channel = IRC_services_copy(s.channel); channel.users = [{ref:"other",op:false,voice:true}];
		s.engine.event({type:"channel",channel:channel});
		s.now += DAY; maintain(s);
		check(!s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"].last_used == s.now);
		check(s.db.db.query("SELECT * FROM access")[0].level == 10, "activity changed channel access");
		s.engine.event({type:"quit",ref:"other"}); s.now += DAY; maintain(s);
		absent(s); check(s.db.db.query("SELECT * FROM access").length == 0, "expired channel retained access rows");
	},
	"brief visits and departures between maintenance passes preserve last use": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:1}); leave(s);
		s.now += 90000;
		s.engine.event({type:"user",user:s.user});
		s.engine.event({type:"identify",ref:"owner",identity:identity(1,"Some User")});
		s.engine.event({type:"channel",channel:s.channel});
		s.now += 5000; leave(s);
		check(s.db.data.nick_last_used.$ALICE == START, "activity wrote without batching");
		s.now = START + DAY + 90000; maintain(s);
		check(s.db.data.nick_last_used.$ALICE == START + 95000 && s.db.data.channels["$#TEST"].last_used == START + 95000);
		s.now += 5000; maintain(s); absent(s);
	},
	"logout stops renewing registrations while the client remains connected": function() {
		var s = scenario({nick_expire_days:1, channel_expire_days:1});
		s.now += 5000; s.engine.event({type:"identify",ref:"owner",identity:null});
		s.now += DAY; maintain(s); absent(s);
	},
	"activity updates are batched and do not rewrite channel access lists": function() {
		var s = scenario(), writes = 0, update = s.db.update_usage;
		s.engine.event({type:"user",user:{ref:"other",nick:"Other",rev:"Other"}});
		s.engine.event({type:"identify",ref:"other",identity:identity(2,"Another User")});
		s.engine.commit(function(next) { next.channels["$#TEST"].access[next.memberships["TEST:2:100"]] = 50; });
		s.db.update_usage = function(n,c) { writes++; update.call(this,n,c); };
		s.db.db.exec("CREATE TRIGGER no_access_rewrite BEFORE DELETE ON access BEGIN SELECT RAISE(ABORT,'Access rewrite'); END;");
		for (var i = 0; i < 10; i++) {
			s.now += 1000; s.engine.event({type:"identify",ref:"owner",identity:identity(1,"Some User")}); s.engine.event({type:"tick"});
		}
		check(!writes && s.db.data.nick_last_used.$ALICE == START, "usage persisted on every tick");
		maintain(s); check(writes == 1 && s.db.data.nick_last_used.$ALICE == s.now);
		check(s.db.db.query("SELECT * FROM access").length == 1);
	},
	"failed activity transactions roll back and retain dirty timestamps for retry": function() {
		var s = scenario(); s.now += 10000;
		s.db.db.exec("CREATE TRIGGER fail_usage BEFORE UPDATE OF last_used ON channels BEGIN SELECT RAISE(ABORT,'Usage failure'); END;");
		var failed = false; try { s.engine.checkpoint(); } catch (_) { failed = true; }
		check(failed && s.engine.nick_usage.$ALICE == s.now, "failed write lost pending activity");
		check(s.db.data.nick_last_used.$ALICE == START && s.db.db.query("SELECT last_used FROM nicks")[0].last_used == START, "partial transaction escaped rollback");
		s.db.db.exec("DROP TRIGGER fail_usage;"); s.engine.checkpoint();
		check(s.db.data.nick_last_used.$ALICE == s.now && s.db.data.channels["$#TEST"].last_used == s.now);
	},
	"clean backend shutdown persists online activity before closing SQLite": function() {
		var s = scenario(), backend = new IRC_Services_Backend({},null);
		backend.engine = s.engine; backend.store = s.db; s.now += 10000; backend.write({type:"stop"});
		s.db = reopen(s.path);
		check(s.db.data.nick_last_used.$ALICE == s.now && s.db.data.channels["$#TEST"].last_used == s.now);
	},
	"a failed usage checkpoint cannot prevent backend shutdown": function() {
		var s = scenario(), backend = new IRC_Services_Backend({},null);
		backend.engine = s.engine; backend.store = s.db; s.now += 10000;
		s.db.db.exec("CREATE TRIGGER fail_shutdown BEFORE UPDATE OF last_used ON channels BEGIN SELECT RAISE(ABORT,'Shutdown usage failure'); END;");
		backend.write({type:"stop"});
		check(backend.closed && !backend.store && !backend.engine, "usage failure prevented shutdown");
		s.db = reopen(s.path); check(s.db.data.nick_last_used.$ALICE == START, "failed checkpoint partly committed");
	},
	"restart allows account snapshots before evaluating elapsed downtime": function() {
		var s = scenario({nick_expire_days:1,channel_expire_days:1}); leave(s); s.engine.checkpoint(); s.db.close();
		s.db = reopen(s.path); s.now += 2 * DAY;
		s.engine = new IRC_Services_Engine(s.db,{nick_expire_days:1,channel_expire_days:1},function(a) { s.output.push(a); });
		s.engine.now = function() { return s.now; };
		s.engine.event({type:"boot"}); s.engine.event({type:"tick"});
		check(s.db.data.nicks.$ALICE && s.db.data.channels["$#TEST"], "registration expired before startup snapshot");
		maintain(s); absent(s);
	},
	"DROP and re-registration discard pending activity from the previous registration": function() {
		var s = scenario(); s.now += 10000; s.engine.note_user("owner",s.now);
		s.engine.command("NickServ",s.user,"DROP"); s.engine.command("ChanServ",s.user,"DROP #test");
		s.now += 1000; s.engine.command("NickServ",s.user,"REGISTER"); s.engine.command("ChanServ",s.user,"REGISTER #test New");
		s.engine.flush_usage();
		check(s.db.data.nick_last_used.$ALICE == s.now && s.db.data.channels["$#TEST"].last_used == s.now);
	},
	"INFO shows last use without treating an outsider query as activity": function() {
		var s = scenario(); leave(s); s.now += DAY;
		s.engine.flush_usage();
		s.engine.event({type:"channel",channel:{ref:"channel",name:"#test",flags:"+nt",users:[],topic:""}});
		var other = {ref:"other",nick:"Bob",rev:"Bob"}; s.engine.event({type:"user",user:other});
		s.output = []; s.engine.command("NickServ",other,"INFO Alice"); s.engine.command("ChanServ",other,"INFO #test");
		var timestamp = new Date(START).toISOString();
		check(s.output.filter(function(a) { return a.type == "notice" && a.text.indexOf("Last used: " + timestamp) >= 0; }).length == 2);
		check(!Object.keys(s.engine.nick_usage).length && !Object.keys(s.engine.channel_usage).length, "INFO renewed ownership");
	},
	"expiry configuration defaults off, validates days, survives writes and resets on rehash": function() {
		var warnings = [], scope = {ServerName:"hub.test",ServerDesc:"Test",Default_Port:19667,CapabEnforce:false,log:function(level,msg) { warnings.push(msg); }};
		load(scope,"ircd/config.js"); scope.Clear_Config_Globals();
		check(scope.EmbeddedServices_Config.nick_expire_days === 0 && scope.EmbeddedServices_Config.channel_expire_days === 0);
		scope.ini_EmbeddedServices("",{NickExpireDays:"30",ChannelExpireDays:"90"});
		["-1", "1.5", "NaN", "Infinity"].forEach(function(value) { scope.ini_EmbeddedServices("",{NickExpireDays:value,ChannelExpireDays:value}); });
		check(warnings.length == 8 && scope.EmbeddedServices_Config.nick_expire_days == 30 && scope.EmbeddedServices_Config.channel_expire_days == 90);
		var path = "/tmp/ircd-expiry-config-" + IRC_services_random() + ".ini"; paths.push(path); check(scope.Write_Config_File(path));
		var f = new File(path); check(f.open("r")); var saved; try { saved = f.iniGetObject("EmbeddedServices"); } finally { f.close(); }
		check(saved.NickExpireDays == 30 && saved.ChannelExpireDays == 90);
		scope.Clear_Config_Globals(); check(scope.EmbeddedServices_Config.nick_expire_days === 0 && scope.EmbeddedServices_Config.channel_expire_days === 0);
		scope.ini_EmbeddedServices("",saved); check(scope.EmbeddedServices_Config.nick_expire_days == 30 && scope.EmbeddedServices_Config.channel_expire_days == 90);
		scope.ini_EmbeddedServices("",{NickExpireDays:"0"}); check(scope.EmbeddedServices_Config.nick_expire_days === 0 && scope.EmbeddedServices_Config.channel_expire_days == 90);
	}
});
} finally {
	stores.forEach(function(s) { try { s.close(); } catch (_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p + "-journal"); });
}
