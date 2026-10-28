/* Persistent ChanServ exception grants; private SQLite files only. */
"use strict";
load(js.exec_dir + "lib.sjs"); load("sbbsdefs.js");
load("ircd/services-common.js"); load("ircd/services-store.js"); load("ircd/services-core.js");
load("ircd/services.js");
var Channels={}, CHANMODE_EXCEPT=4096;
var paths=[], stores=[];
function check(ok,message) { if(!ok) throw new Error(message || "Assertion failed"); }
function rejects(fn,text) { var e; try { fn(); } catch(x) { e=x; } check(e && (!text || String(e).indexOf(text)>=0),"Expected rejection: "+text); }
function open(path) { var db=new IRC_Services_Store(path); stores.push(db); return db; }
function account(n) { return {id:"TEST:"+n+":100",name:"User "+n,home:"test.example",qwk:"TEST",number:n,created:100,username:"User."+n+"+TEST"}; }
function scenario() {
	var path="/tmp/ircd-service-except-"+IRC_services_random()+".sqlite"; paths.push(path);
	var t={path:path,out:[],db:open(path)};
	t.e=new IRC_Services_Engine(t.db,{},function(a) { t.out.push(a); });
	t.u={ref:"owner",nick:"Owner",rev:"Owner",server:"test.example",username:"User.1+TEST",created:100};
	t.e.event({type:"user",user:t.u}); t.e.event({type:"identify",ref:"owner",identity:account(1)});
	t.c={ref:"channel",name:"#test",flags:"+nt",key:"",limit:0,topic:"",exceptions:[],users:[{ref:"owner",op:true,voice:false}]};
	t.e.event({type:"channel",channel:t.c}); t.e.command("ChanServ",t.u,"REGISTER #test Test"); t.out=[];
	return t;
}
function command(t,text,u) { t.e.command("ChanServ",u || t.u,"EXCEPT #test "+text); }
function record(t) { return t.db.data.channels["$#TEST"]; }
function actions(t) { return t.out.filter(function(a) { return a.type=="exception"; }); }
try { run_tests({
	"ADD persists canonical supported grants and restores missing live entries":function() {
		var t=scenario(); command(t,"ADD ~m:external:Bot");
		var e=record(t).exceptions["$~MSGBYPASS:EXTERNAL:BOT!*@*"];
		check(e && e.mask=="~msgbypass:external:Bot!*@*" && e.setter==t.u.username && e.created>0);
		check(actions(t).length==1 && actions(t)[0].add);
		t.db.close(); t.db=open(t.path); check(record(t).exceptions["$~MSGBYPASS:EXTERNAL:BOT!*@*"].mask==e.mask);
		t.e.store=t.db; t.out=[]; t.e.enforce(t.c); check(actions(t).length==1);
	},
	"a matching live entry is adopted without MODE churn and manual grants are independent":function() {
		var t=scenario(); t.c.exceptions=["Bot!*@*","Manual!*@*"]; command(t,"ADD bot");
		check(!actions(t).length && Object.keys(record(t).exceptions).length==1);
		t.out=[]; command(t,"ADD BOT!*@*"); check(!actions(t).length);
		command(t,"LIST"); check(t.out.some(function(a) { return a.type=="notice" && a.text.indexOf("bot!*@*")>=0; }));
		check(!record(t).exceptions["$MANUAL!*@*"]);
	},
	"DEL removes saved and live grants and they are not restored":function() {
		var t=scenario(); command(t,"ADD Bot"); t.c.exceptions=["Bot!*@*"]; t.out=[];
		command(t,"DEL bot!*@*"); check(!Object.keys(record(t).exceptions).length);
		check(actions(t).length==1 && !actions(t)[0].add);
		t.c.exceptions=[]; t.out=[]; t.e.enforce(t.c); check(!actions(t).length);
		check(t.db.db.query("SELECT * FROM channel_exceptions").length==0);
	},
	"unsupported bypasses and account selectors never enter ChanServ storage":function() {
		var t=scenario();
		["~m:censor:Bot","~m:notice:Bot","~m:color:Bot","~account:Owner","~m:external:~account:Owner","~unknown:Bot"].forEach(function(m) { rejects(function() { command(t,"ADD "+m); },"Unsupported"); });
		check(!Object.keys(record(t).exceptions).length && !t.db.db.query("SELECT * FROM channel_exceptions").length);
		command(t,"ADD ~m:moderated:Bot"); check(Object.keys(record(t).exceptions).length==1);
	},
	"SOP edits and OP listing follow authenticated channel access":function() {
		var t=scenario(), op={ref:"op",nick:"Operator",rev:"Operator",server:"test.example"};
		t.e.event({type:"user",user:op}); t.e.event({type:"identify",ref:op.ref,identity:account(2)});
		t.e.command("NickServ",op,"REGISTER"); t.e.command("ChanServ",t.u,"ACCESS #test ADD Operator OP");
		command(t,"LIST",op); rejects(function() { command(t,"ADD Bot",op); },"SOP");
		t.e.command("ChanServ",t.u,"ACCESS #test ADD Operator SOP"); command(t,"ADD Bot",op);
		t.e.command("ChanServ",t.u,"ACCESS #test ADD Operator VOICE"); rejects(function() { command(t,"LIST",op); },"operator");
	},
	"the services TLS policy protects EXCEPT reads and writes":function() {
		var t=scenario(); t.e.config.require_tls=true;
		rejects(function() { command(t,"LIST"); },"TLS"); rejects(function() { command(t,"ADD Bot"); },"TLS");
		t.u.secure_path=true; command(t,"ADD Bot");
		check(t.out.some(function(a) { return a.type=="notice" && a.require_tls; }));
	},
	"capacity blocks new grants and restoration never evicts manual entries":function() {
		var t=scenario(); command(t,"ADD Bot"); t.c.exceptions=[];
		for(var i=0;i<25;i++) t.c.exceptions.push("Manual"+i+"!*@*");
		t.out=[]; t.e.enforce(t.c); check(!actions(t).length && record(t).exceptions["$BOT!*@*"]);
		check(t.e.exception_warnings["$#TEST"]); t.e.enforce(t.c); check(!actions(t).length);
		rejects(function() { command(t,"ADD New"); },"full");
		t.c.exceptions.pop(); t.e.enforce(t.c); check(actions(t).length==1 && !t.e.exception_warnings["$#TEST"]);
		check(t.c.exceptions.length==24);
	},
	"managed limits and full-list adoption do not silently evict grants":function() {
		var t=scenario(); for(var i=0;i<25;i++) command(t,"ADD User"+i);
		rejects(function() { command(t,"ADD Full"); },"full"); check(Object.keys(record(t).exceptions).length==25);
		var other=scenario(); for(i=0;i<25;i++) other.c.exceptions.push("User"+i+"!*@*");
		command(other,"ADD User0"); check(Object.keys(record(other).exceptions).length==1);
	},
	"queued changes recheck policy, capacity, channel identity and authentication":function() {
		var t=scenario(), applied=[], c={nam:"#test",services_ref:t.c.ref};
		Channels["#TEST"]=c;
		c.is_str_member_of_chanmode_list=function(_,mask) { return t.c.exceptions.some(function(m) { return m.toUpperCase()==mask.toUpperCase(); }); };
		c.count_modelist=function() { return t.c.exceptions.length; };
		var m=Object.create(IRC_Services_Manager.prototype);
		m.central=true; m.config={}; m.queue={engine:t.e}; m.find=function() { return live; };
		var live={services_identity:{id:"proof"},services_auth_revision:2};
		m.service_server={set_chanmode:function(_,args) { applied.push(args.join(" ")); }};
		command(t,"ADD Bot"); var add=actions(t)[0];
		command(t,"DEL Bot"); m.action(add); check(!applied.length,"revived deleted policy");
		var del=actions(t)[1]; del.actor=null; command(t,"ADD Bot"); m.action(del);
		check(!applied.length,"deleted re-added policy");
		m.action(add); check(applied.pop()=="+e Bot!*@*");
		for(var i=0;i<25;i++) t.c.exceptions.push("Manual"+i+"!*@*");
		m.action(add); check(!applied.length && c.services_exception_full);
		t.c.exceptions.pop(); m.action(add); check(applied.pop()=="+e Bot!*@*" && !c.services_exception_full);
		var a=IRC_services_copy(add); a.mask="~m:censor:Bot"; m.action(a); check(!applied.length);
		a=IRC_services_copy(add); a.channel.ref="old-channel"; m.action(a); check(!applied.length);
		a=IRC_services_copy(add); a.actor={ref:"owner",account:"proof",auth_revision:1}; m.action(a);
		check(!applied.length,"used stale authentication");
		a.actor.auth_revision=2; m.action(a); check(applied.pop()=="+e Bot!*@*");
	},
	"failed SQLite grant changes roll back policy and emit no mutation":function() {
		var t=scenario(); t.db.db.exec("CREATE TRIGGER fail_exception BEFORE INSERT ON channel_exceptions BEGIN SELECT RAISE(ABORT,'exception failure'); END;");
		rejects(function() { command(t,"ADD Bot"); }); check(!Object.keys(record(t).exceptions).length && !actions(t).length);
		check(!t.db.db.query("SELECT * FROM channel_exceptions").length);
	},
	"opening an existing schema-3 database adds an empty grant table without losing registrations":function() {
		var t=scenario(); t.db.db.exec("DROP TABLE channel_exceptions;"); t.db.close(); t.db=open(t.path);
		check(record(t) && !Object.keys(record(t).exceptions).length && t.db.data.accounts[account(1).id]);
		check(t.db.db.query("PRAGMA user_version")[0].user_version==3);
	},
	"registration deletion and expiry cascade to managed exception rows":function() {
		var t=scenario(); command(t,"ADD Bot"); t.e.exception_warnings["$#TEST"]=true;
		t.e.command("ChanServ",t.u,"DROP #test");
		check(!t.db.db.query("SELECT * FROM channel_exceptions").length && !t.e.exception_warnings["$#TEST"]);
		var e=scenario(); command(e,"ADD Bot"); e.e.config.channel_expire_days=1;
		e.e.event({type:"quit",ref:e.u.ref}); e.e.event({type:"channel_gone",ref:e.c.ref});
		e.e.expire(Date.now()+2*86400000); check(!e.db.data.channels["$#TEST"] && !e.db.db.query("SELECT * FROM channel_exceptions").length);
	}
}); } finally {
	stores.forEach(function(s) { try { s.close(); } catch(_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p+"-journal"); });
}
