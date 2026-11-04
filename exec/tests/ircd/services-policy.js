/* Embedded identity markers, registrations and extended mode locks. */
"use strict";
load(js.exec_dir + "lib.sjs");
load("sbbsdefs.js"); load("ircd/services-common.js");
load("ircd/services-store.js"); load("ircd/services-core.js");
var paths = [], stores = [];
function check(ok, text) { if (!ok) throw new Error(text || "Assertion failed"); }
function scenario() {
	var path = IRCD_TEST_TEMP_DIR + "ircd-policy-" + IRC_services_random() + ".sqlite"; paths.push(path);
	var t = {db:new IRC_Services_Store(path), out:[]}; stores.push(t.db);
	t.e = new IRC_Services_Engine(t.db, {}, function(a) { t.out.push(a); });
	t.u = {ref:"owner", nick:"Owner", rev:"Owner", server:"home.test", created:100, identified:false};
	t.c = {ref:"channel", name:"#test", flags:"+nt", key:"", limit:0, joinrate:"", topic:"", users:[{ref:"owner", op:true}]};
	t.a = {id:"HOME:1:100", name:"Some User", home:"home.test", qwk:"HOME", number:1, created:100, username:"Some.User+HOME"};
	t.e.event({type:"user", user:t.u}); t.e.event({type:"channel", channel:t.c}); return t;
}
function login(t, identity) { t.e.event({type:"identify", ref:t.u.ref, identity:identity}); }
function registered(t) { login(t, t.a); t.e.command("NickServ", t.u, "REGISTER"); t.e.command("ChanServ", t.u, "REGISTER #test Description"); t.out = []; }
try { run_tests({
	"a BBS login alone does not assert registered-nickname identification": function() {
		var t = scenario(); login(t, t.a);
		check(!t.out.some(function(a) { return a.type == "identified" && a.identified; }));
		t.e.command("NickServ", t.u, "REGISTER");
		check(t.out.some(function(a) { return a.type == "identified" && a.identified && a.target === t.u; }));
	},
	"logout and dropping a registration revoke the observed marker": function() {
		var t = scenario(); registered(t); t.u.identified = true; login(t, null);
		check(t.out.some(function(a) { return a.type == "identified" && !a.identified; }));
		login(t, t.a); t.out = []; t.e.command("NickServ", t.u, "DROP");
		check(t.out.some(function(a) { return a.type == "identified" && !a.identified; }));
	},
	"a new unregistered nickname cannot retain the previous nickname's grant": function() {
		var t = scenario(); registered(t); t.u.identified = true;
		var next = IRC_services_copy(t.u); next.nick = next.rev = "Other";
		t.e.event({type:"user", user:next}); check(t.out.some(function(a) { return a.type == "identified" && !a.identified; }));
	},
	"ChanServ sets and clears the registered-channel marker independently of mode locks": function() {
		var t = scenario(); login(t, t.a); t.e.command("NickServ", t.u, "REGISTER"); t.out = [];
		t.e.command("ChanServ", t.u, "REGISTER #test Description");
		check(t.out.some(function(a) { return a.type == "mode" && a.mode == "+r"; }));
		t.c.flags += "r"; t.out = []; t.e.command("ChanServ", t.u, "DROP #test");
		check(t.out.some(function(a) { return a.type == "mode" && a.mode == "-r"; }));
	},
	"extended mode locks preserve parameter order and persist through SQLite": function() {
		var t = scenario(); registered(t);
		t.e.command("ChanServ", t.u, "SET #test MLOCK +RMOcSjkl 2:10 secret 25");
		var lock = t.db.data.channels["$#TEST"].mlock;
		check(lock.on == "RMOcS" && lock.joinrate == "2:10" && lock.key == "secret" && lock.limit == 25);
		check(t.out.some(function(a) { return a.mode == "+j" && a.arg == "2:10"; }));
		t.db.close(); t.db = new IRC_Services_Store(paths[paths.length - 1]); stores.push(t.db);
		check(t.db.data.channels["$#TEST"].mlock.joinrate == "2:10");
	},
	"mode-lock removals take no join-rate argument and invalid locks do not modify policy": function() {
		var t = scenario(); registered(t); t.e.command("ChanServ", t.u, "SET #test MLOCK -j+kl secret 10");
		check(t.db.data.channels["$#TEST"].mlock.joinrate === "");
		var before = JSON.stringify(t.db.data);
		["+j 0", "+j 1:bad", "+r", "+I Visitor", "+jkl 2:10 secret"].forEach(function(text) {
			var rejected = false; try { t.e.command("ChanServ", t.u, "SET #test MLOCK " + text); } catch(e) { rejected = !!e.services_user; }
			check(rejected && JSON.stringify(t.db.data) == before, text);
		});
	},
	"matching observed policy does not repeatedly emit new grants or join-rate resets": function() {
		var t = scenario(); registered(t); t.e.command("ChanServ", t.u, "SET #test MLOCK +RMSj 2:10");
		t.u.identified = true; t.c.flags = "+ntrRMSj"; t.c.joinrate = "2:10"; t.out = [];
		t.e.event({type:"user", user:t.u}); t.e.event({type:"channel", channel:t.c}); t.e.authorization_changed();
		check(!t.out.some(function(a) { return a.type == "mode" || a.type == "identified"; }));
	}
}); } finally {
	stores.forEach(function(s) { try { s.close(); } catch(_) {} });
	paths.forEach(function(p) { file_remove(p); file_remove(p + "-journal"); });
}
