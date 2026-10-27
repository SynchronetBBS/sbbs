/* BBS reply addressing, including proxied requests. No live users or sockets. */
"use strict";
load(js.exec_dir + "lib.sjs");
function check(ok, message) { if (!ok) throw new Error(message || "Assertion failed"); }
function scenario() {
	var scope = {ServerName:"edge.test",Users:{},Servers:{},system:{timer:100},log:function() {}};
	load(scope,"ircd/services-common.js"); load(scope,"ircd/services.js");
	var s = {scope:scope,wire:[],requests:[],parts:[],replies:[]};
	s.user = {local:true,nick:"Visitor",servername:"edge.test",rawout:function(line) { s.wire.push(line); }};
	s.manager = Object.create(scope.IRC_Services_Manager.prototype);
	s.manager.config = {server:"edge.test",nickserv:true,memoserv:true};
	s.manager.ensure_agent = function() { return true; };
	s.manager.describe = function(user) { return {ref:"session",nick:user.nick,server:user.servername}; };
	s.manager.find = function(actor) { return s.user; };
	s.manager.central_peer = function() { return "central"; };
	s.manager.peer = function(name) { return {servername:"central.test"}; };
	s.manager.apply_identity = function(user, identity) { user.services_identity = identity; };
	s.manager.request = function(peer, request, callback) { s.requests.push({peer:peer,request:request,callback:callback}); };
	s.manager.job = function(request, callback) { s.requests.push({request:request,callback:callback}); };
	s.manager.send = function(peer, message) { s.parts.push(message); };
	s.manager.reply = function(peer, id, reply) { s.replies.push(reply); };
	return s;
}
function identity() { return {id:"HOME:1:100",qwk:"HOME",home:"home.test",number:1,created:100,name:"Some User",username:"Some.User+HOME"}; }
function text(s) { return s.wire.join("\n"); }
function check_hint(s, service, home) {
	check(text(s).indexOf("[" + service + "@" + home + "]") >= 0, "reply did not identify its BBS: " + text(s));
	check(text(s).indexOf("/msg " + service + "@" + home + " <command>") >= 0, "missing qualified reply instructions: " + text(s));
	check(s.wire.every(function(line) { return line.indexOf(" NOTICE Visitor :") >= 0 && line.indexOf(" PRIVMSG ") < 0; }), "reply was not a NOTICE");
}
run_tests({
	"local BBS notices remain ordinary NOTICE replies": function() {
		["NickServ","MemoServ"].forEach(function(service) {
			var s = scenario(); s.manager.notice(service,s.user,"Local reply","EDGE.TEST");
			check(s.wire.length == 1 && s.wire[0] == ":" + service + "!services@edge.test NOTICE Visitor :Local reply");
		});
	},
	"a proxied BBS reply provides qualified instructions even though the user is local": function() {
		["NickServ","MemoServ"].forEach(function(service) {
			var s = scenario(); s.manager.notice(service,s.user,"Remote reply","HOME.TEST");
			check_hint(s,service,"home.test");
		});
	},
	"central registration notices do not acquire a per-BBS address": function() {
		var s = scenario(); s.manager.notice("NickServ",s.user,"Nickname registered.");
		check(s.wire.length == 1 && /:Nickname registered[.]$/.test(s.wire[0]));
	},
	"remote identification success and failure retain the selected BBS context": function() {
		[true,false].forEach(function(success) {
			var s = scenario(); s.manager.config.memoserv = false;
			check(s.manager.intercept(s.user,"NickServ@home.test","IDENTIFY Some.User EXAMPLE"));
			check(s.requests[0].request.home == "home.test");
			s.requests[0].callback(success ? {identity:identity()} : {error:"Invalid account or password."});
			check_hint(s,"NickServ","home.test"); check(text(s).indexOf("EXAMPLE") < 0, "reply disclosed the password");
		});
	},
	"NickServ syntax errors and HELP show the selected BBS address": function() {
		["IDENTIFY", "HELP"].forEach(function(command) {
			var s = scenario(); check(s.manager.intercept(s.user,"NickServ@home.test",command));
			check_hint(s,"NickServ","home.test"); check(!s.requests.length);
		});
	},
	"unqualified MemoServ replies follow the authenticated BBS rather than the connection server": function() {
		var s = scenario(); s.user.services_identity = identity();
		check(s.manager.intercept(s.user,"MemoServ","STATUS"));
		check(s.requests[0].request.operation == "STATUS");
		s.requests[0].callback({text:"No telegrams waiting."});
		check_hint(s,"MemoServ","home.test");
	},
	"MemoServ HELP and identification guidance provide concrete qualified targets": function() {
		var s = scenario(); s.manager.intercept(s.user,"MemoServ@home.test","HELP");
		check_hint(s,"MemoServ","home.test");
		s.wire = []; s.scope.system.timer += 2;
		s.manager.intercept(s.user,"MemoServ@home.test","READ");
		check(text(s).indexOf("/msg NickServ@home.test IDENTIFY") >= 0, "missing BBS identification instructions");
		check_hint(s,"MemoServ","home.test");
	},
	"reading from the wrong BBS redirects to the authenticated account's server": function() {
		var s = scenario(); s.user.services_identity = identity();
		s.manager.intercept(s.user,"MemoServ@other.test","READ");
		check(!s.requests.length && text(s).indexOf("/msg MemoServ@home.test READ") >= 0);
		check_hint(s,"MemoServ","home.test");
	},
	"telegram chunks and completion acknowledgements retain the destination BBS": function() {
		var s = scenario();
		s.manager.deliver_memo("connection-agent","request",{nick:"Visitor"},identity(),"READ");
		s.requests[0].callback({text:new Array(8101).join("x")});
		check(s.parts.length == 2 && s.parts.every(function(m) { return m.home == "home.test"; }));
		check(s.replies.length == 1 && s.replies[0].home == "home.test");
	},
	"telegram text notices include BBS context with older reply payload fallback": function() {
		[true,false].forEach(function(has_home) {
			var s = scenario(); s.user.services_identity = identity();
			var message = {type:"memo-text",actor:{nick:"Visitor"},text:"Telegram text"};
			if (has_home) message.home = "home.test";
			s.manager.receive("central",message);
			check_hint(s,"MemoServ","home.test");
		});
	},
	"SEND completion uses the destination BBS returned by the central service": function() {
		var s = scenario(); s.user.services_identity = identity();
		s.manager.intercept(s.user,"MemoServ","SEND Other.User+DEST Hello");
		s.requests[0].callback({text:"Telegram delivered.",home:"dest.test"});
		check_hint(s,"MemoServ","dest.test");
	},
	"invalid BBS labels cannot inject reply commands or wire messages": function() {
		var s = scenario(); s.manager.notice("MemoServ",s.user,"Safe reply","home.test\r\nPRIVMSG Visitor :Injected");
		check(s.wire.length == 1 && /:Safe reply$/.test(s.wire[0]));
	}
});
