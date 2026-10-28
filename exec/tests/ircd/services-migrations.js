/* Upgrade deployed schema-3 databases in private SQLite fixtures only. */
"use strict";
load(js.exec_dir + "lib.sjs"); load("sbbsdefs.js"); load("ircd/services-common.js"); load("ircd/services-store.js");
load("ircd/services-core.js");
var paths=[], handles=[];
function check(ok,text) { if (!ok) throw new Error(text || "Assertion failed"); }
function raw(path) { var db=new SQLite(path); db.busy_timeout=25; handles.push(db); return db; }
function open(path) { var db=new IRC_Services_Store(path); handles.push(db); return db; }
function rejects(fn) { var error; try { fn(); } catch(e) { error=e; } check(error,"Expected migration failure"); }
function tables(db) { return db.query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name").map(function(r) { return r.name; }); }
function snapshot(db) {
	var order={accounts:"id",identities:"id",memberships:"account",nicks:"name",channels:"name",access:"channel,account",channel_exceptions:"channel,mask"}, result={};
	tables(db).forEach(function(name) { result[name]=db.query("SELECT * FROM "+name+" ORDER BY "+order[name]); });
	return result;
}
function version3(exceptions) {
	var path="/tmp/ircd-migration-"+IRC_services_random()+".sqlite"; paths.push(path);
	var db=raw(path);
	/* Frozen deployed schema, independent of the current store constructor. */
	db.exec("PRAGMA foreign_keys=ON;"+
		"CREATE TABLE accounts(id TEXT PRIMARY KEY,name TEXT NOT NULL,home TEXT NOT NULL,qwk TEXT NOT NULL,number INTEGER NOT NULL,created INTEGER NOT NULL);"+
		"CREATE TABLE identities(id TEXT PRIMARY KEY);"+
		"CREATE TABLE memberships(account TEXT PRIMARY KEY REFERENCES accounts(id),identity TEXT NOT NULL REFERENCES identities(id));"+
		"CREATE TABLE nicks(name TEXT PRIMARY KEY,account TEXT NOT NULL REFERENCES identities(id),memo_account TEXT NOT NULL REFERENCES accounts(id),last_used INTEGER NOT NULL);"+
		"CREATE TABLE channels(name TEXT PRIMARY KEY,display TEXT NOT NULL,founder TEXT NOT NULL REFERENCES identities(id),description TEXT NOT NULL,registered INTEGER NOT NULL,last_used INTEGER NOT NULL,mlock TEXT NOT NULL,keeptopic INTEGER NOT NULL,topic TEXT NOT NULL);"+
		"CREATE TABLE access(channel TEXT NOT NULL REFERENCES channels(name) ON DELETE CASCADE,account TEXT NOT NULL REFERENCES identities(id),level INTEGER NOT NULL CHECK(level IN (10,50,80)),PRIMARY KEY(channel,account));"+
		"INSERT INTO accounts VALUES('HOME:1:100','Home User','home.test','HOME',1,100),('EDGE:2:100','Edge User','edge.test','EDGE',2,100),('OTHER:3:100','Other User','other.test','OTHER',3,100);"+
		"INSERT INTO identities VALUES('shared'),('legacy');"+
		"INSERT INTO memberships VALUES('HOME:1:100','shared'),('EDGE:2:100','shared'),('OTHER:3:100','legacy');"+
		"INSERT INTO nicks VALUES('$OWNER','shared','EDGE:2:100',1700000000000),('$ALT','shared','HOME:1:100',1700000001000);"+
		"PRAGMA user_version=3;");
	db.run("INSERT INTO channels VALUES(?,?,?,?,?,?,?,?,?)",["$#TEST","#Test","shared","Saved registration",1600000000000,1700000000000,JSON.stringify({on:"nt",off:"m",key:"secret",limit:12}),1,"Saved topic"]);
	db.run("INSERT INTO channels VALUES(?,?,?,?,?,?,?,?,?)",["$#LEGACY","#Legacy","legacy","Founder has no registered nick",1600000001000,1700000001000,JSON.stringify({on:"",off:""}),0,""]);
	db.exec("INSERT INTO access VALUES('$#TEST','legacy',80),('$#LEGACY','shared',50);");
	if (exceptions) {
		db.exec("CREATE TABLE channel_exceptions(channel TEXT NOT NULL REFERENCES channels(name) ON DELETE CASCADE,mask TEXT NOT NULL,display TEXT NOT NULL,setter TEXT NOT NULL,created INTEGER NOT NULL,PRIMARY KEY(channel,mask));");
		var mask="~msgbypass:external:Bot!*@*";
		db.run("INSERT INTO channel_exceptions VALUES(?,?,?,?,?)",["$#TEST",IRC_services_key(mask),mask,"Home.User+HOME",1700000002000]);
	}
	return {path:path,db:db};
}

function preserved(before,after) {
	Object.keys(before).forEach(function(table) { check(JSON.stringify(before[table])==JSON.stringify(after[table]),"Changed deployed rows in "+table); });
}
try { run_tests({
	"fresh databases initialize the current schema and reopen without changing data":function() {
		var path="/tmp/ircd-migration-"+IRC_services_random()+".sqlite"; paths.push(path);
		var db=open(path); check(db.db.query("PRAGMA user_version")[0].user_version==IRC_SERVICES_SCHEMA);
		var before=snapshot(db.db); db.close(); db=open(path); preserved(before,snapshot(db.db));
	},
	"deployed schema 3 upgrades without altering linked accounts, owners, destinations, access or settings":function() {
		var t=version3(false), before=snapshot(t.db); t.db.close();
		var db=open(t.path); check(db.db.query("PRAGMA user_version")[0].user_version==IRC_SERVICES_SCHEMA);
		preserved(before,snapshot(db.db)); check(!Object.keys(db.data.channels["$#TEST"].exceptions).length);
		check(db.data.channels["$#LEGACY"].founder=="legacy" && !Object.keys(db.data.nicks).some(function(k) { return db.data.nicks[k]=="legacy"; }));
		check(db.data.memo_accounts.$OWNER=="EDGE:2:100" && db.data.memberships["HOME:1:100"]==db.data.memberships["EDGE:2:100"]);
		check(!db.db.query("PRAGMA foreign_key_check").length);
		var after=snapshot(db.db); db.close(); db=open(t.path); preserved(after,snapshot(db.db));
	},
	"schema-3 exception grants survive migration and subsequent saves":function() {
		var t=version3(true), before=snapshot(t.db); t.db.close();
		var db=open(t.path); preserved(before,snapshot(db.db));
		check(db.data.channels["$#TEST"].exceptions["$~MSGBYPASS:EXTERNAL:BOT!*@*"].setter=="Home.User+HOME");
		db.save(IRC_services_copy(db.data)); preserved(before,snapshot(db.db));
	},
	"existing founders retain management rights even without a registered nickname":function() {
		var t=version3(true); t.db.close(); var db=open(t.path);
		var engine=new IRC_Services_Engine(db,{},function() {}), user={ref:"legacy-owner",nick:"Unregistered",rev:"Unregistered",server:"other.test"};
		engine.event({type:"user",user:user});
		engine.event({type:"identify",ref:user.ref,identity:db.data.accounts["OTHER:3:100"]});
		engine.command("ChanServ",user,"SET #Legacy MLOCK +nt");
		check(db.data.channels["$#LEGACY"].founder=="legacy" && db.data.channels["$#LEGACY"].mlock.on=="nt");
		check(!db.data.nicks.$UNREGISTERED);
	},
	"a failed load rolls back schema changes and version before a successful retry":function() {
		var t=version3(false); t.db.exec("UPDATE channels SET mlock='invalid JSON' WHERE name='$#TEST'");
		var before=snapshot(t.db); t.db.close(); rejects(function() { open(t.path); });
		var db=raw(t.path); check(db.query("PRAGMA user_version")[0].user_version==3);
		check(tables(db).indexOf("channel_exceptions")<0); preserved(before,snapshot(db));
		db.run("UPDATE channels SET mlock=? WHERE name=?",[JSON.stringify({on:"nt",off:""}),"$#TEST"]); db.close();
		var fixed=open(t.path); check(fixed.db.query("PRAGMA user_version")[0].user_version==IRC_SERVICES_SCHEMA);
	},
	"a newer database is rejected without changing its deployed rows or version":function() {
		var t=version3(true); t.db.exec("PRAGMA user_version=99"); var before=snapshot(t.db); t.db.close();
		rejects(function() { open(t.path); }); var db=raw(t.path);
		check(db.query("PRAGMA user_version")[0].user_version==99); preserved(before,snapshot(db));
	},
	"unversioned populated databases are rejected without rebuilding their tables":function() {
		var t=version3(true); t.db.exec("PRAGMA user_version=0"); var before=snapshot(t.db); t.db.close();
		rejects(function() { open(t.path); }); var db=raw(t.path);
		check(db.query("PRAGMA user_version")[0].user_version==0); preserved(before,snapshot(db));
	},
	"current-schema corruption does not silently recreate missing tables":function() {
		var t=version3(false); t.db.exec("PRAGMA user_version="+IRC_SERVICES_SCHEMA); var before=snapshot(t.db); t.db.close();
		rejects(function() { open(t.path); }); var db=raw(t.path);
		check(db.query("PRAGMA user_version")[0].user_version==IRC_SERVICES_SCHEMA);
		preserved(before,snapshot(db)); check(tables(db).indexOf("channel_exceptions")<0);
	},
	"a missing future migration cannot silently advance the version or commit partial steps":function() {
		var t=version3(false), before=snapshot(t.db); t.db.close();
		var current=IRC_SERVICES_SCHEMA;
		try { IRC_SERVICES_SCHEMA=current+1; rejects(function() { open(t.path); }); }
		finally { IRC_SERVICES_SCHEMA=current; }
		var db=raw(t.path); check(db.query("PRAGMA user_version")[0].user_version==3);
		preserved(before,snapshot(db)); check(tables(db).indexOf("channel_exceptions")<0);
	}
}); } finally {
	handles.forEach(function(db) { try { db.close(); } catch(_) {} });
	paths.forEach(function(path) { file_remove(path); file_remove(path+"-journal"); });
}
