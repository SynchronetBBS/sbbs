/* Channel registration binds to an owned registered nick, not just a BBS login. */
"use strict";
load(js.exec_dir + "lib.sjs");
load("sbbsdefs.js"); load("ircd/services-common.js");
load("ircd/services-store.js"); load("ircd/services-core.js");
var paths=[], stores=[];
function check(ok,text) { if (!ok) throw new Error(text || "Assertion failed"); }
function identity(qwk) { return {id:qwk+":1:100",name:qwk+" User",username:qwk+".User+"+qwk,home:qwk.toLowerCase()+".test",qwk:qwk,number:1,created:100}; }
var A=identity("HOME"), B=identity("EDGE");
function scenario() {
	var path="/tmp/ircd-registration-"+IRC_services_random()+".sqlite"; paths.push(path);
	var t={db:new IRC_Services_Store(path),out:[]}; stores.push(t.db);
	t.e=new IRC_Services_Engine(t.db,{},function(a) { t.out.push(a); });
	t.u={ref:"owner",nick:"Owner",rev:"Owner",server:"home.test"};
	t.c={ref:"channel",name:"#test",flags:"+nt",key:"",limit:0,topic:"",exceptions:[],users:[{ref:"owner",op:true,voice:false}]};
	t.e.event({type:"user",user:t.u}); t.e.event({type:"channel",channel:t.c}); return t;
}
function login(t,accounts) { t.e.event({type:"identify",ref:t.u.ref,identity:accounts[accounts.length-1] || null,logins:accounts}); }
function register(t) { t.e.command("ChanServ",t.u,"REGISTER #test Description"); }
function rejected(t) {
	var before=JSON.stringify(t.db.data), error; t.out=[];
	try { register(t); } catch(e) { error=e; }
	check(error && error.services_user,"Registration was not rejected");
	check(JSON.stringify(t.db.data)==before && !t.out.length,"Rejected registration changed state");
	check(!t.db.db.query("SELECT * FROM channels").length,"Rejected registration reached SQLite");
}
try { run_tests({
	"channel ops alone cannot register an unregistered nickname":function() {
		var t=scenario(); rejected(t);
	},
	"a verified PASS-equivalent BBS identity still needs a registered current nickname":function() {
		var t=scenario(); login(t,[A]); rejected(t);
		t.e.command("NickServ",t.u,"REGISTER"); register(t);
		check(t.db.data.channels["$#TEST"].founder==t.db.data.nicks.$OWNER);
	},
	"identification to another identity cannot claim a registered nickname's channel":function() {
		var t=scenario(); login(t,[A]); t.e.command("NickServ",t.u,"REGISTER");
		login(t,[]); rejected(t); login(t,[B]); rejected(t);
	},
	"another registered nickname in the same identity does not replace current-nick registration":function() {
		var t=scenario(); login(t,[A]); t.e.command("NickServ",t.u,"REGISTER");
		t.u.nick=t.u.rev="Unregistered"; rejected(t);
	},
	"the current registered nick still requires channel membership and ops":function() {
		var t=scenario(); login(t,[A]); t.e.command("NickServ",t.u,"REGISTER");
		t.c.users[0].op=false; rejected(t); t.c.users=[]; rejected(t);
	},
	"multi-login registration belongs to the current nickname, not an unrelated active BBS login":function() {
		var t=scenario(); login(t,[A]); t.e.command("NickServ",t.u,"REGISTER");
		login(t,[A,B]); register(t);
		check(t.e.active_group(t.u)!=t.db.data.nicks.$OWNER);
		check(t.db.data.channels["$#TEST"].founder==t.db.data.nicks.$OWNER);
		login(t,[B]); check(t.e.level(t.u,t.c)==0);
		login(t,[A]); check(t.e.level(t.u,t.c)==100);
	},
	"a linked BBS account may register using the shared registered nickname":function() {
		var t=scenario(); login(t,[A,B]); t.e.command("NickServ",t.u,"REGISTER");
		t.e.commit(function(next) { next.memberships[A.id]=next.memberships[B.id]; });
		login(t,[A]); register(t); check(t.e.level(t.u,t.c)==100);
	}
}); } finally {
	stores.forEach(function(s) { try { s.close(); } catch(_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p+"-journal"); });
}
