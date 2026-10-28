/* Public detailed help, aliases, BBS addressing, and IRC NOTICE framing. */
"use strict";
load(js.exec_dir + "lib.sjs"); load(js.exec_dir + "state-fixture.sjs");
load("sbbsdefs.js"); load("ircd/services-common.js"); load("ircd/services-core.js"); load("ircd/services.js");
var ServerName="edge.test", Users={}, Servers={};
function adapter() {
	var t={wire:[],manager:Object.create(IRC_Services_Manager.prototype)};
	t.user={local:true,nick:"Visitor",servername:ServerName,rawout:function(line) { t.wire.push(line); }};
	t.manager.config={server:ServerName,nickserv:true,memoserv:true,bbs_require_tls:true};
	t.manager.ensure_agent=function() { return true; };
	t.manager.describe=function() { return {ref:"visitor"}; };
	t.manager.central_peer=function() { return null; }; t.manager.peer=function() { return null; };
	t.manager.tls_error=function() { throw new Error("Public help checked private TLS policy"); };
	t.manager.request=t.manager.job=function() { throw new Error("Public help requested account or mailbox data"); };
	return t;
}
run_tests({
	"each supported command and subcommand has detailed syntax, explanation, examples and an ending":function() {
		var topics={
			ChanServ:["REGISTER","INFO","DROP","ACCESS","ACCESS ADD","ACCESS DEL","ACCESS LIST","EXCEPT","EXCEPT ADD","EXCEPT DEL","EXCEPT LIST","SET","SET MLOCK","SET KEEPTOPIC","TOPIC","OP","DEOP","VOICE","DEVOICE","INVITE","UNBAN"],
			NickServ:["IDENTIFY","LOGOUT","REGISTER","GROUP","DROP","INFO","GHOST","ACCOUNTS","LINK","LINK CONFIRM","UNLINK","SET","SET MEMOACCOUNT"],
			MemoServ:["SEND","STATUS","READ"]
		};
		Object.keys(topics).forEach(function(service) { topics[service].forEach(function(topic) {
			var help=IRC_services_help(service,topic.toLowerCase().split(" "),{home:"home.test"});
			check(help.indexOf("Syntax: ")>=0 && help.indexOf("Example: ")>=0 && help.indexOf("End of "+service+" HELP "+topic)>=0,service+" "+topic);
			check(help.split("\n").length>=6 && help.split("\n").every(function(l) { return l.length<=260; }),"incomplete/oversized "+topic);
			check(!/\{NS\}|\{MS\}|undefined|Unknown .* help topic/.test(help),help);
		}); });
	},
	"root help explains detailed requests and optional arguments":function() {
		["ChanServ","NickServ","MemoServ"].forEach(function(service) {
			var help=IRC_services_help(service,[],{home:"home.test"});
			check(help.indexOf("HELP <command> [subcommand]")>=0 && help.indexOf("optional arguments")>=0 && help.indexOf("without identification or TLS")>=0);
			check(help.indexOf("End of "+service+" HELP.")>=0);
		});
	},
	"nested help and short aliases resolve without mistaking topics for channel names":function() {
		check(IRC_services_help("ChanServ",["mlock"]).indexOf("+/-imnpstkl")>=0);
		check(IRC_services_help("ChanServ",["set","keeptopic"]).indexOf("KEEPTOPIC ON|OFF")>=0);
		check(IRC_services_help("NickServ",["group"]).indexOf("alias for REGISTER")>=0);
		check(IRC_services_help("NickServ",["link","confirm"]).indexOf("five minutes")>=0);
	},
	"unknown help topics are bounded and never reflect arbitrary arguments or control codes":function() {
		var help=IRC_services_help("ChanServ",["missing","SECRET\r\nINJECT",new Array(5000).join("X")]);
		check(help.indexOf("Unknown ChanServ help topic")>=0 && help.indexOf("SECRET")<0 && help.length<150 && !/[\r\n]/.test(help));
	},
	"authority HELP is public, does not write policy, and emits complete topics":function() {
		var out=[], data={accounts:{},groups:{},memberships:{},nicks:{},channels:{}}, before=JSON.stringify(data);
		var engine=new IRC_Services_Engine({data:data,save:function() { throw new Error("Help wrote policy"); }},{require_tls:true},function(a) { out.push(a); });
		var user={ref:"visitor",nick:"Visitor",server:"home.test",secure_path:false};
		engine.command("ChanServ",user,"HELP REGISTER");
		engine.command("ChanServ",user,"HELP SET MLOCK");
		engine.command("NickServ",user,"HELP LINK CONFIRM");
		check(out.length==3 && out.every(function(a) { return a.type=="notice" && !a.require_tls && a.text.indexOf("Syntax: ")>=0; }));
		check(out[0].text.indexOf("current nickname")>=0 && out[1].text.indexOf("MLOCK NONE")>=0);
		check(out[2].text.indexOf("ALL linked BBS telegram mailboxes")>=0 && JSON.stringify(data)==before);
	},
	"local and qualified BBS help bypass identification and TLS while retaining the selected address":function() {
		[["NickServ@home.test","HELP IDENTIFY"],["NickServ@home.test","HELP LINK"],["MemoServ@home.test","HELP READ"],["MemoServ@home.test","HELP SEND"]].forEach(function(request) {
			var t=adapter(); check(t.manager.intercept_command(t.user,request[0],request[1]));
			var text=t.wire.join("\n");
			check(text.indexOf("Syntax: ")>=0 && text.indexOf("End of ")>=0 && text.indexOf(request[0])>=0,text);
			check(t.wire.every(function(l) { return l.length<=510 && l.indexOf(" NOTICE Visitor :")>=0 && l.indexOf(" PRIVMSG ")<0; }));
		});
	},
	"NOTICE chunks fit the wire even with long source names and remote bots":function() {
		var saved=ServerName;
		try {
			ServerName=new Array(64).join("a")+"."+new Array(64).join("b")+"."+new Array(64).join("c")+"."+new Array(62).join("d");
			[true,false].forEach(function(local) {
				var t=adapter(); t.user.local=local; t.user.nick=new Array(31).join("N");
				t.manager.chanserv={nuh:"ChanServ!services@"+ServerName};
				t.user.originatorout=function(line,bot) { t.wire.push(":"+bot.nuh+" "+line); };
				t.manager.notice("ChanServ",t.user,IRC_services_help("ChanServ",["EXCEPT"]));
				check(t.wire.length>10 && t.wire.every(function(l) { return l.length<=510; }),"oversized NOTICE");
			});
		} finally { ServerName=saved; }
	},
	"client service aliases retain complete multiword HELP requests":function() {
		var s=new_state(), user=new_user(s,"Visitor"), sent=[];
		user.services_msg=function(service,text) { sent.push(service+":"+text); };
		["cs","chanserv","ns","nickserv","ms","memoserv"].forEach(function(alias) { user.work(alias+" help set mlock"); });
		check(sent.join("|")=="ChanServ:help set mlock|ChanServ:help set mlock|NickServ:help set mlock|NickServ:help set mlock|MemoServ:help set mlock|MemoServ:help set mlock",sent.join("|"));
	}
});
