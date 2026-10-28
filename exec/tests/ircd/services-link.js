/* Shared network ownership, independent BBS proofs, and session revocation. */
"use strict";
load(js.exec_dir + "lib.sjs");
load("sbbsdefs.js"); load("irclib.js");
load("ircd/services-common.js"); load("ircd/services-store.js"); load("ircd/services-core.js"); load("ircd/services.js");
function check(ok, text) { if (!ok) throw new Error(text || "Assertion failed"); }
function rejects(fn, text) { var error; try { fn(); } catch (e) { error = e; } check(error && (!text || String(error).indexOf(text) >= 0), "Expected rejection: " + text); }
function identity(qwk, number) { return {id:qwk+":"+(number || 1)+":100", name:qwk+" User", username:qwk+".User+"+qwk, home:qwk.toLowerCase()+".test", qwk:qwk, number:number || 1, created:100}; }
var A = identity("HOME"), B = identity("EDGE"), C = identity("THIRD"), paths = [], stores = [];
function scenario(config) {
	var s = {out:[],now:1800000000000}, path = "/tmp/ircd-link-"+IRC_services_random()+".sqlite";
	paths.push(path); s.path = path; s.db = new IRC_Services_Store(path); stores.push(s.db);
	s.e = new IRC_Services_Engine(s.db, config || {}, function(a) { s.out.push(a); }); s.e.now = function() { return s.now; };
	s.user = {ref:"owner",nick:"Alice",rev:"Alice",server:"home.test",created:100};
	s.e.event({type:"user",user:s.user}); login(s,[A]); s.e.command("NickServ",s.user,"REGISTER");
	return s;
}
function login(s, list, revision, multi) { s.e.event({type:"identify",ref:s.user.ref,identity:list[list.length-1] || null,logins:list,revision:revision || 1,multi:multi !== false}); }
function register(s, name) { s.user.nick = s.user.rev = name; s.e.command("NickServ",s.user,"REGISTER"); }
function channel(s, name) {
	var c = {ref:name,name:name,flags:"+nt",key:"",limit:0,topic:"topic",users:[{ref:s.user.ref,op:true,voice:false}]};
	s.e.event({type:"channel",channel:c}); s.e.command("ChanServ",s.user,"REGISTER "+name+" Description"); return c;
}
function preview(s, first, second) { s.e.command("NickServ",s.user,"LINK "+(first || A).username+" "+(second || B).username); return s.e.links[s.user.ref].code; }
function confirm(s, code) { s.e.command("NickServ",s.user,"LINK CONFIRM "+code); }
function linked() { var s = scenario(); login(s,[B]); register(s,"Bob"); login(s,[A,B]); confirm(s,preview(s)); return s; }
function manager() {
	var m = Object.create(IRC_Services_Manager.prototype); m.config = {server:"home.test",nickserv:true,memoserv:true};
	m.apply_identity = function(u,a) { u.services_identity = a; }; return m;
}
function validation() {
	var m = manager(), u = {services_peer_multi:true,services_auth_revision:0}, s = {m:m,u:u,pending:[],replies:[],claims:[]};
	m.find = function() { return u; }; m.peer = function() { return {}; };
	m.request = function(peer,req,cb) { s.pending.push({req:req,cb:cb}); };
	m.reply = function(peer,id,r) { s.replies.push({id:id,r:r}); };
	m.claim = function(user,a) { s.claims.push(user.services_verified_logins.slice()); }; m.publish_identity = function() {};
	s.verify = function(list,rev,id) { m.verify_logins("connection",u,{actor:{auth_revision:rev},logins:list,id:id || "request"}); };
	return s;
}
function mailbox_reader(s) {
	var m = manager(), r = {m:m,pending:[],sent:[],replies:[]};
	s.user.services_auth_revision = 1; m.queue = {engine:s.e}; m.find = function() { return s.user; }; m.peer = function() { return {}; };
	m.send = function(peer,message) { r.sent.push(message); }; m.reply = function(peer,id,result) { r.replies.push(result); };
	m.request = function(peer,request,callback) { r.pending.push({request:request,callback:callback}); };
	r.actor = {ref:s.user.ref,nick:s.user.nick,rev:s.user.rev,auth_revision:1};
	return r;
}
try { run_tests({
	"two verified BBS logins retain independent groups until explicit confirmation": function() {
		var s = scenario(); login(s,[A,B]); var before = JSON.stringify(s.db.data); preview(s);
		check(JSON.stringify(s.db.data) == before && s.e.owns(s.user,s.db.data.nicks.$ALICE));
		check(s.db.data.memberships[A.id] != s.db.data.memberships[B.id]);
	},
	"LINK requires both exact accounts on this session and upgraded helpers": function() {
		var s = scenario(); rejects(function() { preview(s); },"both exact");
		login(s,[A,B],1,false); rejects(function() { preview(s); },"updated services");
		login(s,[A,B]); s.e.sessions.owner.verified_at = system.timer-31; rejects(function() { preview(s); },"updated services");
	},
	"confirmation is single use and bound to the requesting IRC session": function() {
		var s = scenario(); login(s,[A,B]); var code = preview(s);
		var other = {ref:"other",nick:"Other",rev:"Other"}; s.e.event({type:"user",user:other});
		s.e.event({type:"identify",ref:"other",logins:[A,B],identity:B,multi:true});
		rejects(function() { s.e.command("NickServ",other,"LINK CONFIRM "+code); },"confirmation expired");
		confirm(s,code); rejects(function() { confirm(s,code); },"confirmation expired");
	},
	"confirmation rejects changed authentication, expired codes, and stale group previews": function() {
		var s = scenario(); login(s,[A,B]); var code = preview(s); login(s,[A,B],2);
		rejects(function() { confirm(s,code); },"state changed");
		code = preview(s); s.e.links.owner.until = system.timer-1; rejects(function() { confirm(s,code); },"expired");
		code = preview(s); register(s,"NewNick"); rejects(function() { confirm(s,code); },"state changed");
		code = preview(s); s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT HOME.User+HOME"); confirm(s,code);
	},
	"merge preserves nick usage and telegram destinations without a primary BBS": function() {
		var s = linked(), d = s.db.data; check(d.nicks.$ALICE == d.nicks.$BOB);
		check(d.memo_accounts.$ALICE == A.id && d.memo_accounts.$BOB == B.id);
		check(d.nick_last_used.$ALICE == s.now && d.nick_last_used.$BOB == s.now);
		login(s,[B]); check(s.e.owns(s.user,d.nicks.$ALICE));
		login(s,[A]); check(s.e.owns(s.user,d.nicks.$BOB));
	},
	"merging entire groups preserves founder rights and takes maximum duplicate access": function() {
		var s = scenario(), c = channel(s,"#founder"), ga = s.db.data.memberships[A.id];
		login(s,[B]); var gb = s.db.data.memberships[B.id];
		s.e.commit(function(d) { d.channels["$#FOUNDER"].access[gb] = 50; });
		login(s,[C]); register(s,"Carol"); var gc = s.db.data.memberships[C.id]; channel(s,"#third");
		s.e.commit(function(d) { d.channels["$#THIRD"].access[ga] = 10; d.channels["$#THIRD"].access[gb] = 80; });
		login(s,[A,B]); confirm(s,preview(s));
		check(s.db.data.channels["$#THIRD"].access[ga] == 80 && !s.db.data.channels["$#THIRD"].access[gb]);
		check(s.db.data.channels["$#FOUNDER"].founder == ga && !s.db.data.channels["$#FOUNDER"].access[ga]);
		login(s,[B,C]); confirm(s,preview(s,B,C));
		check(Object.keys(s.db.data.groups).length == 1 && s.db.data.memberships[C.id] == ga && s.e.level(s.user,c) == 100);
	},
	"MaxNicks caps REGISTER and whole-group merges with zero partial changes": function() {
		var s = scenario({max_nicks:1}); rejects(function() { register(s,"Extra"); },"at most 1");
		login(s,[B]); register(s,"Bob"); login(s,[A,B]); var before = JSON.stringify(s.db.data);
		rejects(function() { preview(s); },"MaxNicks=1"); check(JSON.stringify(s.db.data) == before);
		s.e.config.max_nicks = 2; var code = preview(s); s.e.config.max_nicks = 1;
		rejects(function() { confirm(s,code); },"MaxNicks=1"); check(JSON.stringify(s.db.data) == before);
	},
	"failed SQLite merge rolls back ownership and leaves the policy cache intact": function() {
		var s = scenario(); login(s,[B]); register(s,"Bob"); login(s,[A,B]); var code = preview(s), before = JSON.stringify(s.db.data);
		s.db.db.exec("CREATE TRIGGER fail_merge BEFORE UPDATE OF identity ON memberships BEGIN SELECT RAISE(ABORT,'merge failure'); END;");
		rejects(function() { confirm(s,code); },"merge failure"); check(JSON.stringify(s.db.data) == before);
		check(s.db.db.query("SELECT COUNT(*) AS n FROM identities")[0].n == 2);
	},
	"UNLINK rejects the final account and lists destinations that must be retargeted": function() {
		var s = scenario(); rejects(function() { s.e.command("NickServ",s.user,"UNLINK "+A.username); },"final");
		login(s,[A,B]); confirm(s,preview(s));
		rejects(function() { s.e.command("NickServ",s.user,"UNLINK "+A.username); },"ALICE");
		s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username);
		s.e.command("NickServ",s.user,"UNLINK "+A.username);
		login(s,[A]); check(!s.e.owns(s.user,s.db.data.nicks.$ALICE));
		login(s,[B]); check(s.e.owns(s.user,s.db.data.nicks.$ALICE));
	},
	"any linked member may detach another member and channel rights follow remaining group": function() {
		var s = linked(), c = channel(s,"#test");
		s.e.command("NickServ",s.user,"SET Bob MEMOACCOUNT "+A.username); login(s,[A]);
		s.e.command("NickServ",s.user,"UNLINK "+B.username);
		check(s.e.level(s.user,c) == 100); login(s,[B]); check(s.e.level(s.user,c) == 0);
		check(s.e.deadlines.owner && s.db.data.nicks.$BOB == s.db.data.memberships[A.id]);
	},
	"MEMOACCOUNT cannot point outside the nickname's identity": function() {
		var s = scenario(); login(s,[A,B]); rejects(function() { s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username); },"destination must belong");
		confirm(s,preview(s)); s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username);
		check(s.e.resolve_memo("Alice") == B.id && s.e.resolve_memo(A.username) == A.id && s.e.resolve_group("Alice") == s.db.data.memberships[A.id]);
	},
	"server-qualified nick delivery selects its linked BBS account instead of its default": function() {
		var s = linked(); s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username);
		check(s.e.resolve_memo("Alice") == B.id && s.e.resolve_memo("Alice",A.home) == A.id && s.e.resolve_memo("Alice",B.home) == B.id);
		rejects(function() { s.e.resolve_memo("Alice",C.home); },"no linked account");
		rejects(function() { s.e.resolve_memo(A.username,B.home); },"destination account belongs");
		check(s.e.resolve_memo(A.username,A.home) == A.id);
	},
	"server-qualified nick delivery refuses to guess between multiple accounts on one BBS": function() {
		var s = linked(), other = identity("HOME",2), group = s.db.data.memberships[A.id];
		other.name = "Second Home User"; other.username = "Second.Home.User+HOME";
		s.e.commit(function(d) { d.accounts[other.id] = other; d.memberships[other.id] = group; });
		rejects(function() { s.e.resolve_memo("Alice",A.home); },"multiple linked accounts");
		check(s.e.resolve_memo("Alice") == A.id && s.e.resolve_memo(other.username,A.home) == other.id);
	},

	"group reads include all linked BBS accounts with the configured destination first": function() {
		var s = linked(); s.user.nick = s.user.rev = "Alice";
		var actor = {ref:"owner",nick:"Alice",rev:"Alice",auth_revision:1};
		check(s.e.sessions.owner.active == B.id && JSON.stringify(s.e.memo_read(actor)) == JSON.stringify([A.id,B.id]));
		s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username); login(s,[A]);
		check(JSON.stringify(s.e.memo_read(actor)) == JSON.stringify([B.id,A.id]));
		check(s.e.memo_read(actor,B.home)[0] == B.id);
	},
	"one verified group member grants access to all of its BBS mailboxes": function() {
		var s = linked(); s.user.nick = s.user.rev = "Alice"; login(s,[B]);
		var actor = {ref:"owner",nick:"Alice",rev:"Alice",auth_revision:1};
		check(s.e.memo_read(actor).length == 2 && s.e.can_read_memo(actor,A.id) && s.e.can_read_memo(actor,B.id));
		rejects(function() { s.e.memo_read(actor,C.home); },"No account");
		actor.auth_revision = 2; check(!s.e.can_read_memo(actor,A.id)); rejects(function() { s.e.memo_read(actor); },"Authentication changed");
	},
	"unlinked authenticated groups do not become part of a nickname's default mailbox set": function() {
		var s = linked(); s.user.nick = s.user.rev = "Alice"; login(s,[A,C]);
		var actor = {ref:"owner",nick:"Alice",rev:"Alice",auth_revision:1};
		check(s.e.memo_read(actor).length == 2 && s.e.memo_read(actor).indexOf(C.id) < 0);
		check(s.e.memo_read(actor,C.home)[0] == C.id);
	},
	"unlink immediately revokes mailbox access even when the login revision stays unchanged": function() {
		var s = linked(); s.user.nick = s.user.rev = "Alice"; login(s,[A]);
		var actor = {ref:"owner",nick:"Alice",rev:"Alice",auth_revision:1}; check(s.e.can_read_memo(actor,B.id));
		s.e.command("NickServ",s.user,"SET Bob MEMOACCOUNT "+A.username); s.e.command("NickServ",s.user,"UNLINK "+B.username);
		check(!s.e.can_read_memo(actor,B.id) && s.e.memo_read(actor).length == 1);
	},
	"an impostor using a registered nickname cannot read or probe its owner's mailboxes": function() {
		var s = scenario(); login(s,[B]); var actor = {ref:"owner",nick:"Alice",rev:"Alice",auth_revision:1};
		check(!s.e.owns(s.user,s.db.data.nicks.$ALICE)); check(s.e.memo_read(actor)[0] == B.id && !s.e.can_read_memo(actor,A.id));
	},

	"automatic group checks omit empty mailboxes and the completion summary": function() {
		var s = linked(), r = mailbox_reader(s);
		r.m.read_memos("connection","query",r.actor,[A,B],"STATUS",true);
		check(r.sent.length == 1 && r.sent[0].text === "" && r.sent[0].request_id == "query");
		r.pending[0].callback({pending:false,text:"No telegrams waiting."});
		check(r.sent.every(function(m) { return m.text === ""; }) && r.pending.length == 2);
		r.pending[1].callback({text:"No telegrams waiting."});
		check(r.sent.every(function(m) { return m.text === ""; }) && r.replies.length == 1 && !r.replies[0].text && !r.replies[0].error);
	},
	"automatic group checks retain waiting notifications and per-BBS errors": function() {
		var s = linked(), r = mailbox_reader(s);
		r.m.read_memos("connection","query",r.actor,[A,B],"STATUS",true);
		r.pending[0].callback({pending:true,text:"You have telegrams waiting."});
		r.pending[1].callback({error:"Mailbox unavailable."});
		var visible = r.sent.filter(function(m) { return !!m.text; });
		check(visible.length == 4 && visible[0].home == A.home && visible[2].home == B.home);
		check(visible[1].text == "You have telegrams waiting." && visible[3].text == "Mailbox unavailable.");
		check(r.replies.length == 1 && !r.replies[0].text);
	},
	"explicit group STATUS continues to report empty mailboxes and a summary": function() {
		var s = linked(), r = mailbox_reader(s);
		r.m.read_memos("connection","query",r.actor,[A,B],"STATUS");
		r.pending[0].callback({pending:false,text:"No telegrams waiting."});
		r.pending[1].callback({pending:false,text:"No telegrams waiting."});
		check(r.sent.length == 4 && r.sent[1].text == "No telegrams waiting." && r.sent[3].text == "No telegrams waiting.");
		check(r.replies[0].text == "Checked 2 telegram mailboxes.");
	},
	"group mailbox reads preserve useful results when another BBS fails": function() {
		var s = linked(), r = mailbox_reader(s); login(s,[A]);
		r.m.read_memos("connection","query",r.actor,[A,B],"READ"); check(r.pending[0].request.identity.id == A.id);
		r.pending[0].callback({error:"BBS unavailable"}); check(r.pending[1].request.identity.id == B.id);
		r.pending[1].callback({text:"Shared BBS telegram"});
		check(r.sent.some(function(m) { return m.text == "Shared BBS telegram"; }) && r.replies[0].text.indexOf("1 failed") >= 0);
		check(r.sent.every(function(m) { return m.request_id == "query" && m.home; }));
	},
	"unlinking suppresses an in-flight mailbox reply from the detached account": function() {
		var s = linked(); login(s,[A]); s.e.command("NickServ",s.user,"SET Bob MEMOACCOUNT "+A.username);
		var r = mailbox_reader(s); r.m.read_memos("connection","query",r.actor,[A,B],"READ");
		r.pending[0].callback({text:"Authorized telegram"}); check(r.pending.length == 2);
		s.e.command("NickServ",s.user,"UNLINK "+B.username); r.pending[1].callback({text:"Detached private telegram"});
		check(!r.sent.some(function(m) { return m.text.indexOf("Detached private") >= 0; }) && r.replies[0].error.indexOf("account group changed") >= 0);
	},
	"logout suppresses in-flight group mailbox text and stops subsequent reads": function() {
		var s = linked(), r = mailbox_reader(s); r.m.read_memos("connection","query",r.actor,[A,B],"READ");
		s.user.services_auth_revision = 2; r.pending[0].callback({text:"Secret after logout"});
		check(r.pending.length == 1 && !r.sent.some(function(m) { return m.text.indexOf("Secret") >= 0; }) && r.replies[0].error);
	},

	"membership, nickname destinations, and rights survive reopening SQLite": function() {
		var s = linked(); channel(s,"#test"); s.db.close();
		var reopened = new IRC_Services_Store(s.path); stores.push(reopened);
		check(reopened.data.memberships[A.id] == reopened.data.memberships[B.id] && reopened.data.memo_accounts.$BOB == B.id);
		check(reopened.data.accounts[A.id].username == A.username && reopened.data.channels["$#TEST"].founder == reopened.data.memberships[A.id]);
	},
	"any linked member renews usage while unlinked members stop renewing": function() {
		var s = linked(), c = channel(s,"#test"); login(s,[A]); s.now += 1000; s.e.checkpoint();
		check(s.db.data.nick_last_used.$BOB == s.now && s.db.data.channels["$#TEST"].last_used == s.now);
		s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username); s.e.command("NickServ",s.user,"UNLINK "+A.username);
		s.now += 1000; s.e.checkpoint(); check(s.db.data.nick_last_used.$BOB == s.now-1000 && s.db.data.channels["$#TEST"].last_used == s.now-1000);
	},
	"connection retains one login per BBS and logout selects most recent remaining login": function() {
		var m = manager(), u = {}; m.add_login(u,A,true); m.add_login(u,B,true); m.add_login(u,C,true);
		var replacement = identity("EDGE",2); m.add_login(u,replacement,true);
		check(m.logins(u).length == 3 && u.services_identity === replacement && !m.logins(u).some(function(a) { return a.id == B.id; }));
		m.remove_login(u,"edge.test"); check(u.services_identity === C && m.logins(u).length == 2);
		m.remove_login(u,"home.test"); check(u.services_identity === C);
		m.remove_login(u,"third.test"); check(!u.services_identity && !m.logins(u).length);
	},
	"older helpers fall back to one login and LOGOUT ALL clears every retained login": function() {
		var m = manager(), u = {}; m.add_login(u,A,true); m.add_login(u,B,false);
		check(m.logins(u).length == 1 && u.services_identity === B);
		m.add_login(u,A,true); m.remove_login(u,"",true); check(!u.services_identity && !m.logins(u).length);
	},
	"older authorities retain per-BBS verified-login reads": function() {
		var m = manager(), u = {}, jobs = [], requests = []; m.add_login(u,A,true); m.add_login(u,B,true);
		m.describe = function() { return {}; }; m.job = function(job) { jobs.push(job); }; m.notice = function() {};
		m.sync_session = function(user,cb) { cb({}); }; m.central_peer = function() { return "central"; }; m.request = function(peer,req) { requests.push(req); };
		m.memo(u,"READ",[],"home.test"); check(jobs.length == 1 && jobs[0].identity.id == A.id);
		m.memo(u,"STATUS",[]); check(requests.length == 1 && requests[0].home == B.home);
		m.remove_login(u,A.home); m.memo(u,"READ",[],A.home); check(jobs.length == 1 && requests.length == 1,"group membership granted mailbox access");
	},
	"central requires an exact fresh proof from every BBS before accepting a login set": function() {
		var s = validation(); s.verify([A,B],1);
		s.pending[1].cb({identity:B,multi_login:1}); check(!s.claims.length);
		s.pending[0].cb({identity:A,multi_login:1}); check(s.claims.length == 1 && s.claims[0][0].id == A.id && s.claims[0][1].id == B.id && s.u.services_verified_multi);
	},
	"a mismatched BBS proof revokes only that login and advances the authentication revision": function() {
		var s = validation(); s.verify([A,B],1);
		s.pending[0].cb({identity:A,multi_login:1}); s.pending[1].cb({identity:identity("EDGE",2),multi_login:1});
		check(s.claims[0].length == 1 && s.claims[0][0].id == A.id && s.u.services_auth_revision == 2 && s.u.services_identity.id == A.id);
		check(s.replies[0].r.error && s.replies[0].r.logins.length == 1);
	},
	"late proofs cannot restore a login removed by a newer LOGOUT set": function() {
		var s = validation(); s.verify([A],1,"old"); s.verify([],2,"logout");
		s.pending[0].cb({identity:A,multi_login:1});
		check(s.claims.length == 1 && !s.claims[0].length && !s.u.services_identity && s.u.services_auth_revision == 2);
		check(s.replies.some(function(r) { return r.id == "old" && r.r.error; }));
		s.verify([A],1,"replay"); check(s.replies[s.replies.length-1].r.error && s.pending.length == 1);
	},
	"concurrent validation cannot overwrite the latest verification or a replacement session": function() {
		var s = validation(); s.verify([A],1,"old"); s.verify([B],1,"new");
		s.pending[1].cb({identity:B,multi_login:1}); s.pending[0].cb({identity:A,multi_login:1}); check(s.u.services_identity.id == B.id && s.claims.length == 1);
		var t = validation(); t.verify([A],1); t.m.find = function() { return {}; }; t.pending[0].cb({identity:A,multi_login:1}); check(!t.claims.length && t.replies[0].r.error);
	},
	"old helper attestations cannot enable LINK and duplicate BBS logins are rejected": function() {
		var s = validation(); s.verify([A,B],1); s.pending[0].cb({identity:A,multi_login:1}); s.pending[1].cb({identity:B});
		check(!s.u.services_verified_multi && s.replies[0].r.multi_login === 0);
		s.verify([A,identity("HOME",2)],2); check(s.replies[s.replies.length-1].r.error && s.pending.length == 2);
	},
	"connection discards late verification replies after logout or replacement authentication": function() {
		var m = manager(), u = {services_multi:true}, callback, results = [];
		m.add_login(u,A,true); m.describe = function() { return {auth_revision:u.services_auth_revision}; }; m.find = function() { return u; }; m.central_peer = function() { return "central"; };
		m.request = function(peer,req,cb) { callback = cb; }; m.sync_session(u,function(r) { results.push(r); });
		m.remove_login(u,A.home); callback({logins:[A],revision:1,multi_login:1});
		check(!u.services_identity && !m.logins(u).length && results[0].error);
	},

	"queued actions cannot retain ownership after unlinking without an authentication change": function() {
		var s = linked(); login(s,[A]);
		var ghost = {ref:"ghost",nick:"Bob",rev:"Bob"}; s.e.event({type:"user",user:ghost}); s.user.nick = s.user.rev = "Other";
		s.e.command("NickServ",s.user,"GHOST Bob"); var kill = s.out.filter(function(a) { return a.type == "kill"; })[0];
		s.e.command("NickServ",s.user,"SET Alice MEMOACCOUNT "+B.username); s.e.command("NickServ",s.user,"UNLINK "+A.username);
		var m = manager(); m.central = true; m.queue = {engine:s.e}; m.find = function() { throw new Error("Stale action reached socket handling"); };
		m.action(kill); check(kill.actor_groups != s.e.group_signature(s.user));
	},
	"validation on the authority's own connection removes only the invalid BBS login": function() {
		var s = validation(); s.u.local = true; s.u.services_auth_revision = 1;
		s.verify([A,B],1); s.pending[0].cb({error:"Disabled account"}); s.pending[1].cb({identity:B,multi_login:1});
		check(s.u.services_auth_revision == 2 && s.m.logins(s.u).length == 1 && s.u.services_identity.id == B.id);
	},

	"periodic checks and commands share an in-flight validation for the same login revision": function() {
		var m = manager(), u = {services_multi:true}, callbacks = [], requests = [], results = [];
		m.add_login(u,A,true); m.describe = function() { return {nick:"Alice",auth_revision:u.services_auth_revision}; }; m.find = function() { return u; }; m.central_peer = function() { return "central"; };
		m.request = function(peer,req,cb) { requests.push(req); callbacks.push(cb); };
		m.sync_session(u,function(r) { results.push(r); }); m.sync_session(u,function(r) { results.push(r); }); check(requests.length == 1);
		callbacks[0]({logins:[A],revision:1,multi_login:1}); check(results.length == 2 && !results[0].error && !u.services_sync);
		m.sync_session(u,function(r) { results.push(r); }); m.add_login(u,B,true); m.sync_session(u,function(r) { results.push(r); });
		check(requests.length == 3); callbacks[1]({logins:[A],revision:1,multi_login:1}); check(u.services_sync && results[2].error);
		callbacks[2]({logins:[A,B],revision:2,multi_login:1}); check(!u.services_sync && m.logins(u).length == 2);
	},

	"MaxNicks defaults to ten, validates positive integers, and survives config writes": function() {
		var scope = {ServerName:"hub.test",ServerDesc:"Test",Default_Port:19667,CapabEnforce:false,log:function() {}}; load(scope,"ircd/config.js");
		scope.Clear_Config_Globals(); check(scope.EmbeddedServices_Config.max_nicks == 10);
		scope.ini_EmbeddedServices("",{MaxNicks:"25"}); [0,-1,1.5,"NaN","Infinity"].forEach(function(v) { scope.ini_EmbeddedServices("",{MaxNicks:v}); });
		check(scope.EmbeddedServices_Config.max_nicks == 25);
		var path = "/tmp/ircd-maxnicks-"+IRC_services_random()+".ini"; paths.push(path); check(scope.Write_Config_File(path));
		var f = new File(path); check(f.open("r")); var saved = f.iniGetObject("EmbeddedServices"); f.close(); check(saved.MaxNicks == 25);
		scope.Clear_Config_Globals(); check(scope.EmbeddedServices_Config.max_nicks == 10); scope.ini_EmbeddedServices("",saved); check(scope.EmbeddedServices_Config.max_nicks == 25);
	}
}); } finally { stores.forEach(function(s) { try { s.close(); } catch (_) {} }); paths.forEach(function(p) { file_remove(p); file_remove(p+"-journal"); }); }
