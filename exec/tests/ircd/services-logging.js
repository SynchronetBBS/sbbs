/* RPC diagnostics: complete-message summaries, rejection reasons and redaction.
 * No live accounts, telegrams or listeners are used.
 */
"use strict";
load(js.exec_dir + "lib.sjs");
load("ircd/services-common.js");
function check(ok, message) { if (!ok) throw new Error(message || "Assertion failed"); }
var secret = "PRIVATE_SENTINEL_7ed91";
function scenario() {
	var s = {logs:[],sent:[],applied:[],jobs:[]};
	var scope = s.scope = {ServerName:"edge.test",Users:{},Servers:{},system:{timer:100},LOG_DEBUG:7,
		log:function(level, text) { s.logs.push(text); }};
	load(scope,"ircd/services-common.js"); load(scope,"ircd/services.js");
	s.peer = scope.IRC_services_agent("hub.test");
	scope.Users[s.peer.toUpperCase()] = {nick:s.peer,servername:"hub.test",uprefix:"bbs+ircd+agent",local:false};
	scope.Servers["hub.test"] = {};
	scope.Servers["services.test"] = {uline:true,linkparent:"hub.test"};
	s.user = {nick:"Visitor",servername:"edge.test",local:true,created:100,services_ref:new Array(65).join("a"),services_auth_revision:1};
	scope.Users.VISITOR = s.user;
	s.manager = Object.create(scope.IRC_Services_Manager.prototype);
	s.manager.config = {host:"services.test",server:"edge.test",nickserv:true};
	s.manager.agent = {nick:scope.IRC_services_agent("edge.test")};
	s.manager.requests = {}; s.manager.serial = 0;
	s.manager.send = function(peer, message) { s.sent.push(message); };
	s.manager.apply_identity = function(user, identity) { user.uprefix = identity ? identity.username : "~visitor"; s.applied.push(identity); };
	s.manager.job = function(job, callback) { s.jobs.push({job:job,callback:callback}); };
	s.actor = s.manager.describe(s.user);
	return s;
}
function identity() { return {id:"HOME:1:100",qwk:"HOME",home:"home.test",number:1,created:100,name:"Some User",username:"Some.User+HOME"}; }
function logs(s) { return s.logs.join("\n"); }
run_tests({
	"RPC summaries exclude credentials, memos, cookies and raw errors": function() {
		["authenticate","local-auth","identity","rename","memo-text","local-validate","reply","reply-text","session","sync-logins","claim","logout","command","memo","local-memo"].forEach(function(type) {
			var m = {type:type,id:"BBSsvc_home_1234:12",actor:{nick:"Visitor",server:"edge.test",auth_revision:2,ref:secret,account:secret},
				account:secret,password:secret,text:"IDENTIFY " + secret,service:"NickServ",operation:"SEND",identity:{id:secret},logins:[{id:secret}],error:secret};
			var summary = IRC_services_log_rpc(m);
			check(summary.indexOf("operation=" + type) >= 0 && summary.indexOf("nick=Visitor") >= 0);
			check(summary.indexOf(secret) < 0, "Secret exposed by " + type);
		});
	},
	"unrecognized verbs and malformed fields cannot insert log lines": function() {
		var summary = IRC_services_log_rpc({type:"command",id:secret,service:secret,text:secret,actor:{nick:"a\r\nb",server:"x\t y"}});
		check(summary.indexOf(secret) < 0 && !/[\r\n\t]/.test(summary) && summary.indexOf("command=unknown") >= 0);
		check(IRC_services_log_rpc({type:secret}).indexOf(secret) < 0);
		check(IRC_services_log_error({toString:function() { throw new Error("Must not coerce errors"); }}) == "error");
		check(IRC_services_log_error("Destination services are unavailable. " + secret) == "unavailable");
	},
	"fragmented messages have one send summary and one completed receive": function() {
		var frames = [], events = [], received = [], key = "test transport key";
		var a = new IRC_Services_Transport("A",function(peer,line) { frames.push(line); },function() {},undefined,
			function(peer,event,message) { events.push(event + " " + IRC_services_log_rpc(message)); });
		var b = new IRC_Services_Transport("B",function() {},function(peer,message,framing) { received.push({message:message,framing:framing}); },undefined,
			function(peer,event) { events.push(event); });
		a.peers.$B = {key:key,out:0}; b.peers.$A = {key:key,inseq:0};
		a.write("B",{type:"local-auth",password:secret,text:new Array(1001).join("x")});
		check(frames.length > 5 && events.length == 1 && events[0].indexOf("frames=" + frames.length) >= 0);
		frames.forEach(function(frame,index) { b.read("A",frame); if (index < frames.length - 1) check(!received.length); });
		check(received.length == 1 && received[0].framing.frames == frames.length && received[0].message.password == secret);
		check(events.length == 1 && events.join(" ").indexOf(secret) < 0, "Extra fragment diagnostics or exposed password");
		b.read("A",frames[0]);
		check(events.length == 2 && events[1].indexOf("unexpected sequence") >= 0 && received.length == 1);
	},
	"bad message authentication is explained without exposing frame contents": function() {
		var events = [], b = new IRC_Services_Transport("B",function() {},function() { throw new Error("Accepted invalid data"); },undefined,
			function(peer,event) { events.push(event); });
		b.peers.$A = {key:"test transport key",inseq:0};
		b.read("A","SVC1 D " + secret + " 1 1 0 1 " + secret);
		check(events.length == 1 && events[0].indexOf("invalid message authentication") >= 0 && events[0].indexOf(secret) < 0);
	},
	"missing configured authority explains ignored identity and remote authentication": function() {
		var s = scenario(); s.manager.config.host = "absent.test";
		s.manager.receive_message(s.peer,{type:"identity",actor:s.actor,identity:identity()});
		s.manager.receive_message(s.peer,{type:"local-auth",actor:s.actor,password:secret});
		check(!s.applied.length && !s.jobs.length && !s.sent.length);
		check(logs(s).indexOf("authority=absent.test reason=server absent") >= 0 && logs(s).indexOf(secret) < 0);
	},
	"untrusted or different authorities are identified without accepting identity": function() {
		var s = scenario(); s.scope.Servers["services.test"].uline = false;
		s.manager.receive_message(s.peer,{type:"identity",actor:s.actor,identity:identity()});
		check(!s.applied.length && logs(s).indexOf("reason=server not trusted") >= 0);
		s.scope.Servers["services.test"].uline = true;
		s.manager.central_peer = function() { return "another"; };
		s.manager.receive_message(s.peer,{type:"identity",actor:s.actor,identity:identity()});
		check(!s.applied.length && logs(s).indexOf("reason=sender is not the configured authority") >= 0);
	},
	"correct authority accepts identity and dispatches remote authentication": function() {
		var s = scenario();
		s.manager.receive_message(s.peer,{type:"identity",actor:s.actor,identity:identity()},{id:"12",frames:4});
		check(s.applied.length == 1 && logs(s).indexOf("identity applied") >= 0 && logs(s).indexOf("message=12 frames=4") >= 0);
		s.manager.receive_message(s.peer,{type:"local-auth",actor:s.actor,account:secret,password:secret});
		check(s.jobs.length == 1 && s.jobs[0].job.password == secret && logs(s).indexOf(secret) < 0);
	},
	"stale identity and rename session failures have explicit reasons": function() {
		var s = scenario(), old = s.manager.describe(s.user); old.auth_revision = 0;
		s.manager.receive_message(s.peer,{type:"identity",actor:old,identity:identity()});
		check(!s.applied.length && logs(s).indexOf("authentication revision changed") >= 0);
		old.ref = new Array(65).join("b");
		s.manager.receive_message(s.peer,{type:"rename",actor:old});
		check(logs(s).indexOf("rename ignored") >= 0 && logs(s).indexOf("user session not found") >= 0);
	},
	"matched replies correlate their original operation and elapsed time": function() {
		var s = scenario(), result;
		s.manager.request(s.peer,{type:"authenticate",actor:s.actor,home:"home.test",password:secret},function(r) { result = r; });
		var request = s.sent[0]; s.scope.system.timer += 2;
		s.manager.receive_message(s.peer,{type:"reply",id:request.id,error:"Authentication failed. " + secret});
		check(result && !s.manager.requests[request.id]);
		check(logs(s).indexOf("reply matched") >= 0 && logs(s).indexOf("operation=authenticate") >= 0 && logs(s).indexOf("elapsed=2.000s") >= 0);
		check(logs(s).indexOf(secret) < 0);
		s.manager.receive_message(s.peer,{type:"reply",id:request.id});
		check(logs(s).indexOf("unknown or expired request") >= 0);
	},
	"rebinding a nickname timestamp preserves replies already queued for that connection": function() {
		var s = scenario(), confirm;
		s.manager.central = true; s.manager.snapshot = function() {};
		s.user.local = false; s.user.servername = "hub.test"; s.user.services_created = 100; s.user.created = 101;
		var queued = s.manager.describe(s.user), actor = s.manager.describe(s.user); actor.created = 101;
		s.manager.request = function(peer, request, callback) { check(request.type == "check-session"); confirm = callback; };
		s.manager.receive_message(s.peer,{type:"session",actor:actor,multi_login:1});
		check(confirm && s.manager.find(queued) === s.user);
		confirm({valid:true});
		check(s.user.services_created == 101 && s.manager.find(actor) === s.user);
		check(s.manager.find(queued) === s.user, "Rebinding discarded a reply for the same connection");
		s.scope.Users.VISITOR = {nick:"Visitor",servername:"hub.test",created:101,services_ref:new Array(65).join("b")};
		check(!s.manager.find(queued), "A replacement connection inherited a queued reply");
	},
	"timed out authentication logs its destination and original request": function() {
		var s = scenario(), result, m = s.manager;
		m.request(s.peer,{type:"authenticate",actor:s.actor,home:"home.test",password:secret},function(r) { result = r; });
		m.jobs = {}; m.attestations = {}; m.peerobjects = {}; m.transport = {peers:{}};
		m.queue = {drain:function() {},read_level:0}; m.nextsnapshot = Infinity;
		m.config.nickserv = false; s.scope.system.timer += 26; m.tick();
		check(result && result.error == "Destination services are unavailable.");
		check(logs(s).indexOf("request timed out peer=hub.test") >= 0 && logs(s).indexOf("operation=authenticate") >= 0 && logs(s).indexOf(secret) < 0);
	}
});
