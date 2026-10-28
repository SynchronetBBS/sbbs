/* Registered-channel creator ops: policy, queued races and grant provenance. */
"use strict";
load(js.exec_dir + "lib.sjs"); load("sbbsdefs.js");
load("ircd/services-common.js"); load("ircd/services-store.js");
load("ircd/services-core.js"); load("ircd/services.js");
load(js.exec_dir + "state-fixture.sjs");
var paths=[], stores=[], Channels={};
function check(ok,message) { if (!ok) throw new Error(message || "Assertion failed"); }
function account(n) { return {id:"HOME:"+n+":100",name:"User "+n,username:"User."+n+"+HOME",home:"home.test",qwk:"HOME",number:n,created:100}; }
function scenario() {
	var path="/tmp/ircd-creator-"+IRC_services_random()+".sqlite"; paths.push(path);
	var t={db:new IRC_Services_Store(path),out:[],modes:[]}; stores.push(t.db);
	t.e=new IRC_Services_Engine(t.db,{},function(a) { t.out.push(a); });
	t.owner={ref:"owner",nick:"Owner",rev:"Owner",server:"home.test"};
	t.u={ref:"visitor",nick:"Visitor",rev:"Visitor",server:"home.test"};
	t.e.event({type:"user",user:t.owner}); t.e.event({type:"identify",ref:"owner",identity:account(1)});
	t.e.command("NickServ",t.owner,"REGISTER");
	t.c={ref:"original",name:"#test",flags:"+nt",key:"",limit:0,topic:"",exceptions:[],users:[{ref:"owner",op:true,voice:false}]};
	t.e.event({type:"channel",channel:t.c}); t.e.command("ChanServ",t.owner,"REGISTER #test Test");
	t.e.event({type:"channel_gone",ref:t.c.ref}); t.e.event({type:"user",user:t.u});
	t.c.ref="recreated"; t.c.users=[{ref:t.u.ref,op:true,voice:false,creator_op:true}];
	t.live={id:"visitor-id",nick:"Visitor"};
	t.channel=Channels["#TEST"]={nam:"#test",services_ref:t.c.ref,services_creator_op:t.live.id,users:{}};
	t.channel.users[t.live.id]=t.live;
	t.m=Object.create(IRC_Services_Manager.prototype);
	t.m.central=true; t.m.config={}; t.m.queue={engine:t.e}; t.m.find=function() { return t.live; };
	t.m.service_server={set_chanmode:function(c,args) { t.modes.push(args.join(" ")); }};
	t.out=[]; return t;
}
function grants(t) { return t.out.filter(function(a) { return a.type=="creator_op"; }); }
function observe(t) { t.e.event({type:"channel",channel:IRC_services_copy(t.c)}); return grants(t).pop(); }
function access(t,level) {
	t.e.event({type:"identify",ref:t.u.ref,identity:account(2)});
	t.e.commit(function(next) { next.channels["$#TEST"].access[next.memberships[account(2).id]]=level; });
}
try { run_tests({
	"an anonymous creator of a recreated registered channel loses only automatic ops":function() {
		var t=scenario(), a=observe(t); check(a); t.m.action(a);
		check(t.modes.join()=="-o Visitor" && !t.channel.services_creator_op);
		t.m.action(a); check(t.modes.length==1,"duplicate correction");
	},
	"a creator with VOICE access receives voice instead of ops":function() {
		var t=scenario(); access(t,10); t.m.action(observe(t));
		check(t.modes.join()=="-o Visitor,+v Visitor");
	},
	"authenticated OP and SOP creators retain ops and consume the creation check":function() {
		[50,80].forEach(function(level) {
			var t=scenario(); access(t,level); t.m.action(observe(t));
			check(!t.modes.length && !t.channel.services_creator_op);
		});
	},
	"an authenticated founder retains creator ops without a new nickname requirement":function() {
		var t=scenario(); t.e.event({type:"identify",ref:t.u.ref,identity:account(1)});
		t.m.action(observe(t)); check(!t.modes.length && !t.channel.services_creator_op);
	},
	"unregistered channels and existing manual operators are not stripped":function() {
		var t=scenario(); t.c.users[0].creator_op=false; observe(t); check(!grants(t).length);
		t.c.users[0].creator_op=true; t.e.commit(function(next) { delete next.channels["$#TEST"]; });
		observe(t); check(!grants(t).length);
	},
	"a delayed remote founder MODE is checked on the following snapshot":function() {
		var t=scenario(); t.c.users[0].op=false; t.c.users[0].creator_op=false;
		observe(t); check(!grants(t).length);
		t.c.users[0].op=t.c.users[0].creator_op=true;
		t.m.action(observe(t)); check(t.modes.join()=="-o Visitor");
	},
	"queued checks recheck current access and a dropped registration":function() {
		var t=scenario(); access(t,10); var a=observe(t);
		t.e.commit(function(next) { next.channels["$#TEST"].access[next.memberships[account(2).id]]=50; });
		t.m.action(a); check(!t.modes.length && !t.channel.services_creator_op);
		t=scenario(); a=observe(t); t.e.commit(function(next) { delete next.channels["$#TEST"]; });
		t.m.action(a); check(!t.modes.length);
	},
	"authentication while a correction is queued prevents stale deop":function() {
		var t=scenario(), a=observe(t);
		t.e.event({type:"identify",ref:t.u.ref,identity:account(1)});
		t.m.action(a); check(!t.modes.length);
		t.m.action(grants(t).pop()); check(!t.modes.length && !t.channel.services_creator_op);
	},
	"a later manual grant, departure or replacement channel cancels a queued check":function() {
		var t=scenario(), a=observe(t); delete t.channel.services_creator_op;
		t.m.action(a); check(!t.modes.length);
		t=scenario(); a=observe(t); delete t.channel.users[t.live.id];
		t.m.action(a); check(!t.modes.length);
		t=scenario(); a=observe(t); t.channel.services_ref="another-incarnation";
		t.m.action(a); check(!t.modes.length);
	},
	"explicit grants supersede creator provenance, but redundant self-op does not":function() {
		var s=new_state(), creator=new_user(s,"Creator"), other=new_user(s,"Other");
		creator.do_join("#test"); var c=s.Channels["#TEST"];
		check(c.services_creator_op==creator.id);
		creator.work("MODE #test +o Creator"); check(c.services_creator_op==creator.id);
		other.do_join("#test"); creator.work("MODE #test +o Other");
		other.work("MODE #test +o Creator"); check(!c.services_creator_op);
		c.services_creator_op=creator.id;
		creator.work("MODE #test -o Creator"); check(!c.services_creator_op);
	},
	"live remote JOIN plus founder MODE records creation, including a relayed server prefix":function() {
		var s=new_state(), p=new_peer(s,"remote.test",true), u=new_user(s,"Creator",p.nick);
		p.work(":Creator JOIN "+s.now+" #test");
		check(!s.Channels["#TEST"].services_creator_op,"marked before founder MODE");
		p.work(":remote.test MODE #test +o Creator");
		check(s.Channels["#TEST"].services_creator_op==u.id,"direct grant not marked");
		var remote=new s.IRC_Server(); remote.nick="far.test"; remote.parent=p.nick; remote.local=false;
		s.Servers[remote.nick]=remote;
		p.work(":Creator JOIN "+s.now+" #far");
		p.work(":far.test MODE #far +o Creator");
		check(s.Channels["#FAR"].services_creator_op==u.id,"relayed grant not marked");
	},
	"live legacy SJOIN creator ops are distinguished from burst status":function() {
		[false,true].forEach(function(bursting) {
			var s=new_state(), p=new_peer(s,"remote.test",true), u=new_user(s,"Creator",p.nick);
			p.bursting=bursting;
			p.work(":remote.test SJOIN "+s.now+" #test + :@Creator");
			check(!!s.Channels["#TEST"].services_creator_op==!bursting);
		});
	},
	"a hub's matching CSYNC acknowledgement preserves a leaf's creator check":function() {
		var s=new_state(), p=new_peer(s,"remote.test",true), u=new_user(s,"Creator");
		u.do_join("#test");
		channel_sync(p,s.now,"#test","+nt","@Creator","");
		check(s.Channels["#TEST"].services_creator_op==u.id,"snapshot was mistaken for an explicit grant");
	},
	"burst JOIN plus MODE, departures and timestamp replacement do not retain creation checks":function() {
		var s=new_state(), p=new_peer(s,"remote.test",true), u=new_user(s,"Creator",p.nick);
		p.bursting=true; p.work(":Creator JOIN "+s.now+" #burst");
		p.work(":remote.test MODE #burst +o Creator");
		check(!s.Channels["#BURST"].services_creator_op);
		var local=new_user(s,"Local"); local.do_join("#local");
		var c=s.Channels["#LOCAL"]; u.do_join("#local");
		local.work("PART #local"); check(!c.services_creator_op);
		local.do_join("#reset"); c=s.Channels["#RESET"];
		s.Channel_reset_timestamp(c,s.now-1); check(!c.services_creator_op);
	}
}); } finally {
	stores.forEach(function(s) { try { s.close(); } catch(_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p+"-journal"); });
}
