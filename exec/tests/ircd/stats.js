/* Public configuration reports must stay useful without exposing secrets. */
"use strict";
load(js.exec_dir + "lib.sjs"); load(js.exec_dir + "state-fixture.sjs");
function scenario() {
	var s=new_state(); s.system=system; s.Clear_Config_Globals(); s.load_config_defaults();
	s.ServerName="leaf.test"; s.ServerDesc="Statistics test"; s.Admin1=s.Admin2=s.Admin3="Public administrator";
	s.TLS_Config.ca_file="/private/ROOT_CA_SECRET.pem"; s.EmbeddedServices_Config.database="/private/DATABASE_SECRET.sqlite";
	s.Die_Password="DIE_SECRET"; s.Restart_Password="RESTART_SECRET";
	s.Default_Port=6667; s.PLines=[6697]; s.TLS_Ports[6697]=true;
	s.CLines=[new s.CLine("out.test","OUTBOUND_SECRET","out.test",6697,1,true)];
	s.NLines=[new s.NLine("in.test","INBOUND_SECRET","in.test",0,2,false),new s.NLine("out.test","MATCHED_SECRET","out.test",0,1,true)];
	s.ILines=[new s.ILine("*@*","CLIENT_SECRET","*@*",0,1)];
	s.OLines=[new s.OLine("*@localhost","OPER_SECRET","Admin",s.OLINE_CAN_REHASH,1)];
	s.WLines=[new s.WLine("gateway.test","WEBIRC_SECRET")];
	s.RBL=[new s.RBL_Config_Object("dnsbl.test","127.0.0.1","127.0.0.2")];
	s.YLines[1]=new s.YLine(120,60,10,1000000); s.YLines[1].comment="Public class comment";
	s.IRC_Services=null; var u=new_user(s,"Reader"); return {s:s,u:u};
}
function report(t,letter) { t.u.received=[]; t.u.work("STATS "+letter); return t.u.received.join("\n"); }
function runtime(t) {
	var s=t.s; s.js.exec_dir=js.exec_dir; s.js.load_path_list=js.load_path_list;
	load(s,"ircd/services-common.js"); load(s,"ircd/services.js");
	new_peer(s,"hub.test",true);
	var authority=new_peer(s,"services.test",false); authority.local=false; authority.parent="hub.test"; authority.linkparent="hub.test"; authority.uline=true;
	var helper=new_user(s,s.IRC_services_agent("hub.test"),"hub.test"); helper.uprefix="bbs+ircd+agent";
	var m=s.IRC_Services=Object.create(s.IRC_Services_Manager.prototype);
	m.config={host:"services.test"}; m.central=false; m.requests={}; m.transport={peers:{}};
	m.transport.peers[s.IRC_services_key(helper.nick)]={key:"TRANSPORT_SECRET",nonce:"NONCE_SECRET"};
	return {m:m,helper:helper,authority:authority};
}
run_tests({
	"STATS U retains trust entries and exposes effective service settings to nonoperators":function() {
		var t=scenario(); t.s.EmbeddedServices_Config.nickserv=true; t.s.EmbeddedServices_Config.host="services.test";
		var text=report(t,"U"); check(text.indexOf(" 246 Reader U services.synchro.net ")>=0);
		["[BBSServices] NickServ=true","[BBSServices] MemoServ=false","[EmbeddedServices] Host=services.test","[EmbeddedServices] MaxNicks=10","[ServicesRuntime] Running=false"].forEach(function(v) { check(text.indexOf(v)>=0,v); });
		check(text.indexOf(" 481 ")<0 && text.indexOf(" 219 Reader U ")>=0);
	},
	"services runtime distinguishes discovered, trusted and authenticated authority state":function() {
		var t=scenario(), r=runtime(t), text=report(t,"U");
		check(text.indexOf("AuthorityTrusted=true")>=0 && text.indexOf("AuthorityTransport=authenticated")>=0);
		delete r.m.transport.peers[t.s.IRC_services_key(r.helper.nick)].key;
		check(report(t,"U").indexOf("AuthorityTransport=pending")>=0);
		r.authority.uline=false; text=report(t,"U");
		check(text.indexOf("AuthorityPresent=true")>=0 && text.indexOf("AuthorityTrusted=false")>=0 && text.indexOf("AuthorityPeer=unavailable")>=0);
		delete t.s.Servers["services.test"]; check(report(t,"U").indexOf("AuthorityPresent=false")>=0);
	},
	"STATS F exposes the welcome identity, clock, capabilities and CA configuration state":function() {
		var t=scenario(), text=report(t,"F");
		["[Info] Servername=leaf.test","[Info] Networkname=Synchronet","[Clock] MaxSkew=30","[Capab] Enforce=false","[TLS] CAConfigured=true"].forEach(function(v) { check(text.indexOf(v)>=0,v); });
	},
	"server reports include inbound-only links and independent TLS configuration":function() {
		var t=scenario(), text=report(t,"C");
		check(text.indexOf(" 214 Reader N in.test * in.test ")>=0,"incoming-only peer omitted");
		check(text.indexOf(" 214 Reader N out.test * out.test ")>=0,"matching peer omitted");
		check(text.indexOf("[Server:in.test] InboundTLS=false")>=0 && text.indexOf("[Server:out.test] OutboundTLS=true")>=0);
	},
	"listener, operator, class, RBL and WebIRC reports cover their public settings":function() {
		var t=scenario(); check(report(t,"P").indexOf("[Port:6697] TLS=true")>=0);
		check(report(t,"O").indexOf("[Operator:Admin] Flags=r")>=0);
		check(report(t,"Y").indexOf("Comment=Public class comment")>=0);
		var text=report(t,"B"); check(text.indexOf("Hostname=dnsbl.test")>=0 && text.indexOf("BadResponses=127.0.0.2")>=0);
		check(report(t,"W").indexOf("Hostname=gateway.test")>=0);
	},
	"every public configuration report omits credentials, transport secrets and private paths":function() {
		var t=scenario(); runtime(t);
		"CcFfHhIiKkPpOoQqUYyBbWw?".split("").forEach(function(letter) {
			var text=report(t,letter); check(text.indexOf("SECRET")<0 && text.indexOf("/private/")<0,"private data in STATS "+letter);
			check(text.indexOf(" 481 ")<0,"public report requires operator status: "+letter);
			check(text.indexOf(" 219 Reader "+letter+" ")>=0,"missing completion: "+letter);
		});
	},
	"new configuration rows sanitize control characters and fit the IRC wire":function() {
		var t=scenario(); t.s.ServerDesc=new Array(1500).join("x")+"\r\nINJECT";
		t.s.IRC_stats_config(t.u,new Array(1500).join("z"),{Value:"\r\nINJECT"});
		check(t.u.received.every(function(line) { return line.length<=510 && !/[\r\n]/.test(line); }),"invalid long section row");
		report(t,"F"); check(t.u.received.every(function(line) { return line.length<=510 && !/[\r\n]/.test(line); }));
	},
	"remote wildcard queries use the destination configuration and complete normally":function() {
		var t=scenario(), p=new_peer(t.s,"hub.test",true), remote=new_user(t.s,"Remote",p.nick);
		p.work(":Remote STATS U :leaf.*");
		check(remote.received.some(function(l) { return l.indexOf("[BBSServices] NickServ=false")>=0; }));
		check(remote.received.some(function(l) { return l.indexOf(" 219 Remote U ")>=0; }));
	},
	"connection counters remain operator-only and help lists the public reports":function() {
		var t=scenario(); check(report(t,"L").indexOf(" 481 ")>=0);
		var text=report(t,"?"); ["STATS F:","U: services","B: RBL","W: WebIRC"].forEach(function(v) { check(text.indexOf(v)>=0); });
	}
});
