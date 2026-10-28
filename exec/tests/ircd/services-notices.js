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
	s.manager.requests = {};
	s.manager.config = {server:"edge.test",nickserv:true,memoserv:true};
	s.manager.ensure_agent = function() { return true; };
	s.manager.describe = function(user) { return {ref:"session",nick:user.nick,server:user.servername,auth_revision:user.services_auth_revision || 0}; };
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
function secure(s) {
	load(s.scope,"ircd/link-security.js"); load(s.scope,"ircd/client-security.js");
	s.scope.Local_Servers = {};
	s.user.socket = {ssl_session:true}; s.scope.Users.VISITOR = s.user;
	["central.test","home.test"].forEach(function(name) {
		var p = s.scope.Servers[name] = {nick:name,server:true,local:true,admitted:true,linkparent:"edge.test",socket:{ssl_session:true}};
		s.scope.IRC_linksec_admit(p);
	});
	s.manager.config.bbs_require_tls = true;
}
function check_hint(s, service, home) {
	check(text(s).indexOf("[" + service + "@" + home + "]") >= 0, "reply did not identify its BBS: " + text(s));
	check(text(s).indexOf("/msg " + service + "@" + home + " <command>") >= 0, "missing qualified reply instructions: " + text(s));
	check(s.wire.every(function(line) { return line.indexOf(" NOTICE Visitor :") >= 0 && line.indexOf(" PRIVMSG ") < 0; }), "reply was not a NOTICE");
}
run_tests({
	"queued private replies check the live path while public replies remain available": function() {
		var s = scenario(); secure(s); s.manager.central = true; s.manager.config.require_tls = true; s.manager.queue = {};
		var actor = s.manager.describe(s.user);
		s.user.socket.ssl_session = false;
		s.manager.action({type:"notice",actor:actor,service:"NickServ",text:"PRIVATE",require_tls:true});
		check(!s.wire.length);
		s.manager.action({type:"notice",actor:actor,service:"NickServ",text:"Public HELP"});
		check(text(s).indexOf("Public HELP") >= 0);
		s.user.socket.ssl_session = true;
		s.manager.action({type:"notice",actor:actor,service:"NickServ",text:"PRIVATE",require_tls:true});
		check(text(s).indexOf("PRIVATE") >= 0);
	},
	"local NickServ refuses plaintext passwords before starting any authentication": function() {
		var s = scenario(); secure(s); s.user.socket.ssl_session = false;
		s.manager.intercept_command(s.user,"NickServ","IDENTIFY Some.User PRIVATE");
		check(!s.requests.length && text(s).indexOf("requires TLS") >= 0 && text(s).indexOf("PRIVATE") < 0);
	},
	"remote identification checks both the authority and destination BBS paths": function() {
		["central.test","home.test"].forEach(function(name) {
			var s = scenario(); secure(s); delete s.scope.Servers[name].link_security;
			s.manager.intercept_command(s.user,"NickServ@home.test","IDENTIFY Some.User PRIVATE");
			check(!s.requests.length && text(s).indexOf("unknown") >= 0);
		});
		var s = scenario(); secure(s);
		s.manager.intercept_command(s.user,"NickServ@home.test","IDENTIFY Some.User PRIVATE");
		check(s.requests.length == 1 && s.requests[0].request.type == "authenticate");
	},
	"public INFO and HELP bypass login verification on a plaintext connection": function() {
		var s = scenario(); secure(s); s.user.socket.ssl_session = false; s.user.services_multi = true;
		s.manager.sync_session = function() { throw new Error("Public command tried to authenticate"); };
		s.manager.intercept_command(s.user,"NickServ","INFO Visitor");
		check(s.requests.length == 1 && s.requests[0].request.type == "command");
		s.scope.system.timer += 2; s.manager.intercept_command(s.user,"NickServ","HELP");
		check(text(s).indexOf("IDENTIFY") >= 0);
		check(!s.scope.IRC_services_requires_tls("NickServ","LOGOUT ALL"));
	},
	"private mailbox operations are blocked before reads and rechecked before replies": function() {
		var s = scenario(); secure(s); s.user.services_identity = identity();
		s.user.socket.ssl_session = false;
		s.manager.intercept_command(s.user,"MemoServ@home.test","READ");
		check(!s.requests.length && text(s).indexOf("requires TLS") >= 0);
		s.user.socket.ssl_session = true; s.scope.system.timer += 2; s.wire = [];
		s.manager.intercept_command(s.user,"MemoServ@home.test","READ"); check(s.requests.length == 1);
		s.user.socket.ssl_session = false; s.requests[0].callback({text:"PRIVATE MAILBOX"});
		check(text(s).indexOf("requires TLS") >= 0 && text(s).indexOf("PRIVATE MAILBOX") < 0);
	},
	"a destination BBS independently refuses unprotected authenticated RPCs": function() {
		["local-auth","local-memo","local-resolve","local-validate"].forEach(function(type) {
			var s = scenario(); secure(s); s.user.socket.ssl_session = false;
			s.manager.receive_message("central",{type:type,id:"test",actor:{nick:"Visitor",server:"edge.test",secure_path:true},operation:"READ",password:"PRIVATE"});
			check(!s.requests.length && s.replies.length == 1 && s.replies[0].error.indexOf("requires TLS") >= 0);
		});
	},
	"BBS replies do not release private results after the path is lost": function() {
		var s = scenario(); secure(s); s.manager.attestations = {};
		s.manager.receive_message("central",{type:"local-auth",id:"test",actor:{nick:"Visitor",server:"edge.test",ref:"test"},account:"Some User",password:"PRIVATE"});
		check(s.requests.length == 1); s.user.socket.ssl_session = false;
		s.requests[0].callback({identity:identity(),text:"PRIVATE"});
		check(s.replies.length == 1 && s.replies[0].error.indexOf("requires TLS") >= 0 && !s.replies[0].identity && !s.replies[0].text);
	},
	"central policy ignores a caller-supplied secure flag before authentication or memo RPCs": function() {
		["authenticate","memo","sync-logins","claim"].forEach(function(type) {
			var s = scenario(); secure(s); s.manager.central = true; s.manager.config.require_tls = true;
			s.user.socket.ssl_session = false; s.manager.bind_actor = function() { return s.user; };
			s.manager.snapshot = function() { throw new Error("Unprotected request reached services"); };
			s.manager.receive_message("central",{type:type,id:"test",actor:{nick:"Visitor",secure_path:true},logins:[identity()],identity:identity()});
			check(s.replies.length == 1 && s.replies[0].error.indexOf("requires TLS") >= 0);
		});
	},
	"central outbound requests check the destination before forwarding credentials": function() {
		var s = scenario(); secure(s); s.manager.central = true; s.manager.config.require_tls = true;
		delete s.scope.Servers["central.test"].link_security;
		var result;
		s.scope.IRC_Services_Manager.prototype.request.call(s.manager,"central",{type:"local-auth",actor:{nick:"Visitor"},password:"PRIVATE"},function(r) { result = r; });
		check(result && result.error.indexOf("requires TLS") >= 0 && !s.parts.length);
	},
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
	"identification requests automatic memo notifications instead of an explicit STATUS report": function() {
		var s = scenario(), automatic;
		s.manager.memo = function(user, verb, args, home, notify_only) { automatic = verb == "STATUS" && notify_only === true; };
		s.manager.intercept(s.user,"NickServ@home.test","IDENTIFY Some.User EXAMPLE");
		s.requests[0].callback({identity:identity()});
		check(automatic && text(s).indexOf("Identified as") >= 0);
	},
	"automatic local STATUS is silent only for empty mailboxes and explicit STATUS still replies": function() {
		[{pending:false,text:"No telegrams waiting."},{text:"No telegrams waiting."},
			{pending:true,text:"You have telegrams waiting. Use READ to receive them."},{error:"Mailbox unavailable."}].forEach(function(result) {
			var s = scenario(), a = identity(); a.home = "edge.test"; s.user.services_identity = a;
			s.manager.memo(s.user,"STATUS",[],undefined,true);
			check(s.requests[0].request.type == "memo"); s.requests[0].callback(result);
			if (result.error || result.pending) check(text(s).indexOf(result.error || result.text) >= 0);
			else check(!s.wire.length, "Empty automatic status produced a NOTICE");
			s.wire = []; s.manager.memo(s.user,"STATUS",[]); s.requests[1].callback(result);
			check(s.wire.length && text(s).indexOf(result.error || result.text) >= 0, "Explicit status was suppressed");
		});
	},
	"automatic remote checks request notification-only results and suppress completion acknowledgements": function() {
		var s = scenario(); s.user.services_identity = identity(); s.user.services_memo_routing = true;
		s.manager.memo(s.user,"STATUS",[],undefined,true);
		check(s.requests[0].request.notify_only === true && s.requests[0].request.group_mailboxes === true);
		s.requests[0].callback({group_mailboxes:true}); check(!s.wire.length);
		s.manager.memo(s.user,"STATUS",[],undefined,true);
		s.requests[1].callback({error:"Destination services are unavailable."});
		check(text(s).indexOf("Destination services are unavailable") >= 0);
	},
	"automatic single-mailbox proxy checks suppress empty responses from old and new helpers": function() {
		[{pending:false,text:"No telegrams waiting."},{text:"No telegrams waiting."},
			{pending:true,text:"You have telegrams waiting."},{error:"Mailbox unavailable."}].forEach(function(result) {
			var s = scenario();
			s.manager.deliver_memo("connection-agent","request",{nick:"Visitor"},identity(),"STATUS",undefined,true);
			s.requests[0].callback(result);
			check(s.parts.length == (result.pending ? 1 : 0));
			check(s.replies.length == 1 && !s.replies[0].text && s.replies[0].error == result.error);
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
	"SEND uses a configured destination unless the service address explicitly selects a BBS": function() {
		var s = scenario(); s.user.services_identity = identity(); s.user.services_memo_routing = true;
		s.manager.intercept(s.user,"MemoServ","SEND Alice Default delivery"); check(!s.requests[0].request.destination_home);
		s.scope.system.timer += 2; s.manager.intercept(s.user,"MemoServ@other.test","SEND Alice Direct delivery");
		check(s.requests[1].request.destination_home == "other.test");
	},
	"older authorities cannot silently ignore an explicitly targeted delivery server": function() {
		var s = scenario(); s.user.services_identity = identity();
		s.manager.intercept(s.user,"MemoServ@other.test","SEND Alice Direct delivery");
		check(!s.requests.length && text(s).indexOf("Server-specific delivery requires updated central services") >= 0);
	},

	"READ and STATUS ask the authority for group mailboxes and preserve explicit server selection": function() {
		["READ","STATUS"].forEach(function(verb) {
			var s = scenario(); s.user.services_identity = identity(); s.user.services_memo_routing = true;
			s.manager.intercept(s.user,"MemoServ",verb); check(s.requests[0].request.group_mailboxes === true && !s.requests[0].request.read_home);
			s.scope.system.timer += 2; s.manager.intercept(s.user,"MemoServ@home.test",verb); check(s.requests[1].request.group_mailboxes === true && s.requests[1].request.read_home == "home.test");
		});
	},
	"older authorities keep their original default mailbox selection": function() {
		var s = scenario(); s.user.services_identity = identity(); s.manager.intercept(s.user,"MemoServ","STATUS");
		check(s.requests[0].request.group_mailboxes === false && s.requests[0].request.home == "home.test");
	},

	"group mailbox progress refreshes only the matching authenticated RPC": function() {
		var s = scenario(); s.user.services_identity = identity();
		s.manager.requests.query = {peer:"CENTRAL",until:0}; s.manager.requests.other = {peer:"OTHER",until:0};
		s.manager.receive("central",{type:"memo-text",request_id:"query",actor:{auth_revision:0},home:"home.test",text:"Mailbox progress"});
		check(s.manager.requests.query.until == 130);
		s.manager.receive("central",{type:"memo-text",request_id:"other",actor:{auth_revision:0},home:"home.test",text:"Mailbox progress"});
		check(s.manager.requests.other.until === 0);
	},
	"silent mailbox progress refreshes the request without producing a blank notice": function() {
		var s = scenario(); s.user.services_identity = identity();
		s.manager.requests.query = {peer:"CENTRAL",until:0};
		s.manager.receive("central",{type:"memo-text",request_id:"query",actor:{auth_revision:0},home:"home.test",text:""});
		check(s.manager.requests.query.until == 130 && !s.wire.length);
	},

	"invalid BBS labels cannot inject reply commands or wire messages": function() {
		var s = scenario(); s.manager.notice("MemoServ",s.user,"Safe reply","home.test\r\nPRIVMSG Visitor :Injected");
		check(s.wire.length == 1 && /:Safe reply$/.test(s.wire[0]));
	}
});
