/* Async RBL admission: controlled DNS replies, bounded queues, and stale callbacks. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario(ip, lists) {
	var s=new_state(), callbacks=[], dns=[], next=100, sent=[];
	load(s,"ircd/unregistered.js");
	s.js.setInterval=function() { return ++next; };
	s.js.setImmediate=function(fn,obj) { callbacks.push([fn,obj]); };
	s.IRC_tls_port=function() { return false; }; s.IP_Banned=function() { return false; };
	s.dnsbl_exempt=function(address) { return address=="127.0.0.1"; };
	s.MAX_CLIENT_RECVQ=2560; s.MAX_REALNAME=50; s.VERSION="test"; s.Profile={};
	s.EmbeddedServices_Config={}; s.WLines=[{hostname:"127.0.0.1",password:"gateway"}];
	s.RBL=lists || [{hostname:"rbl.test"}];
	s.DNS_Resolver={reverse:function() {},resolveTypeClass:function(name,type,cls,cb,obj) {
		dns.push({name:name,type:type,cls:cls,reply:function(addresses) { cb.call(obj,addresses); }});
	}};
	var sock={remote_ip_address:ip || "203.0.113.8",remote_port:1234,local_port:6667,
		is_connected:true,descriptor:1,outbound:false,send:function(line) { sent.push(line); return line.length; },
		on:function() { return 1; },clearOn:function() {},close:function() { this.is_connected=false; }};
	check(s.accept_new_socket.call({accept:function() { return sock; }}));
	var c=sock.irc, welcomed=0;
	c.dns_pending=false;
	c.welcome=function() { welcomed++; delete s.Unregistered[c.id]; };
	c.quit=function(reason) { c.rbl_pending=false; c.recvq.purge(); c.sendq.purge(); sock.close(); delete s.Unregistered[c.id]; c.reason=reason; };
	var t={s:s,c:c,sock:sock,dns:dns,sent:sent};
	t.welcomed=function() { return welcomed; };
	t.flush=function() { var limit=1000; while(callbacks.length) { check(--limit,"spinning callbacks"); var cb=callbacks.shift(); cb[0].call(cb[1]); } };
	t.feed=function(lines) { sock.recv=function() { return lines.join("\r\n")+"\r\n"; }; c.recvq.recv(sock); t.flush(); };
	return t;
}
run_tests({
	"initial RBL check queues registration and releases it after the DNS reply":function() {
		var t=scenario(); t.feed(["NICK Alice","USER user 0 * :Alice"]);
		check(t.dns.length==1 && t.dns[0].name=="8.113.0.203.rbl.test" && t.dns[0].type=="A" && t.dns[0].cls=="IN");
		check(!t.welcomed() && t.c.recvq.queue.length==2 && t.c.nick=="*");
		t.dns[0].reply([]); t.flush(); check(t.welcomed()==1 && !t.c.recvq.queue.length);
	},
	"a listing denies the connection before buffered registration or authentication":function() {
		var t=scenario(); t.feed(["PASS secret","NICK Alice","USER user 0 * :Alice"]);
		t.dns[0].reply(["127.0.0.2"]); t.flush();
		check(!t.sock.is_connected && !t.welcomed() && !t.c.password && !t.c.recvq.queue.length);
		check(t.sent.some(function(l) { return l.indexOf(" 463 ")>=0; }));
	},
	"pending checks also stop unthrottled server handshake processing":function() {
		var t=scenario(), handled=[]; t.c.server=true; t.c.work=function(l) { handled.push(l); };
		t.feed(["PASS secret","SERVER other.test 1 :Peer","CLOCK ROLE HUB"]);
		check(!handled.length && t.c.recvq.queue.length==3);
		t.dns[0].reply([]); t.flush(); check(handled.length==3 && !t.c.recvq.queue.length);
	},
	"WEBIRC pauses the remaining pipeline until the claimed IP has been checked":function() {
		var t=scenario("127.0.0.1");
		t.feed(["WEBIRC gateway test client.example 198.51.100.9","NICK Alice","USER user 0 * :Alice"]);
		check(t.dns.length==1 && t.dns[0].name=="9.100.51.198.rbl.test" && t.c.webirc_done);
		check(!t.welcomed() && t.c.recvq.queue.length==2);
		t.dns[0].reply(["127.0.0.2"]); t.flush(); check(!t.sock.is_connected && !t.welcomed());
	},
	"WEBIRC completion resumes registration even if NICK and USER preceded it":function() {
		var t=scenario("127.0.0.1"); t.c.dns_pending=true;
		t.feed(["NICK Alice","USER user 0 * :Alice","WEBIRC gateway test client.example 198.51.100.9"]);
		check(!t.welcomed() && t.c.rbl_pending && !t.c.dns_pending);
		t.dns[0].reply([]); t.flush(); check(t.welcomed()==1 && t.c.hostname=="client.example");
	},
	"reverse DNS completion cannot register a connection awaiting an RBL result":function() {
		var t=scenario(); t.c.nick="Alice"; t.c.uprefix="user";
		t.c.Unregistered_Check_User_Registration(); check(!t.welcomed());
		t.dns[0].reply([]); t.flush(); check(t.welcomed()==1);
	},
	"pending receive queues retain the normal flood bound":function() {
		var t=scenario(); t.feed([new Array(2600).join("x")]);
		check(!t.sock.is_connected && t.c.reason.indexOf("Excess Flood")>=0);
		t.dns[0].reply([]); t.flush(); check(!t.welcomed());
	},
	"late and duplicate replies cannot affect closed or replaced connections":function() {
		var t=scenario(); t.c.quit("closed"); t.dns[0].reply([]); t.flush(); check(!t.welcomed());
		var u=scenario(); u.sock.irc={}; u.dns[0].reply(["127.0.0.2"]); u.flush(); check(u.sock.is_connected);
		var v=scenario(); v.feed(["NICK Alice","USER user 0 * :Alice"]);
		v.dns[0].reply([]); v.dns[0].reply(["127.0.0.2"]); v.flush(); check(v.welcomed()==1 && v.sock.is_connected);
	},
	"checks retain their configured policy across rehash and stop at the first listing":function() {
		var t=scenario(null,[{hostname:"first.test",good:["127.0.0.1"]},{hostname:"second.test",bad:["127.0.0.2"]},{hostname:"third.test"}]);
		t.s.RBL=[]; t.dns[0].reply(["127.0.0.1"]); t.flush(); check(t.dns.length==2);
		t.dns[1].reply(["127.0.0.2"]); t.flush(); check(!t.sock.is_connected && t.dns.length==2);
	},
	"DNS failures and timeouts keep the existing allow-through behavior":function() {
		var t=scenario(null,[{hostname:"first.test",good:["127.0.0.1"]},{hostname:"second.test"}]);
		t.feed(["NICK Alice","USER user 0 * :Alice"]); t.dns[0].reply(undefined); t.flush();
		t.dns[1].reply(null); t.flush(); check(t.welcomed()==1);
	},
	"response filtering and resolver submission errors retain the configured admission policy":function() {
		var good=scenario(null,[{hostname:"rbl.test",good:["127.0.0.1"]}]);
		good.dns[0].reply(["127.0.0.99"]); good.flush(); check(!good.sock.is_connected);
		var bad=scenario(null,[{hostname:"rbl.test",bad:["127.0.0.2"]}]);
		bad.feed(["NICK Alice","USER user 0 * :Alice"]);
		bad.dns[0].reply(["127.0.0.99","127.0.0.2"]); bad.flush(); check(bad.welcomed()==1);
		var broken=scenario(null,[{hostname:"first.test"},{hostname:"broken.test"}]);
		broken.s.DNS_Resolver.resolveTypeClass=function() { throw new Error("submission failure"); };
		broken.feed(["NICK Alice","USER user 0 * :Alice"]);
		broken.dns[0].reply([]); broken.flush(); check(broken.welcomed()==1);
	},
	"exempt addresses and an empty RBL list do not delay registration":function() {
		[scenario("127.0.0.1"),scenario(null,[])].forEach(function(t) {
			t.feed(["NICK Alice","USER user 0 * :Alice"]); check(!t.dns.length && t.welcomed()==1);
		});
	},
	"IPv4 and IPv6 DNSBL names are complete and malformed addresses are rejected":function() {
		var p=new_state().IRC_dnsbl_query;
		check(p("::ffff:192.0.2.3","rbl.test")=="3.2.0.192.rbl.test");
		["2001:db8::1","2001:db8::","::1","::","2001:db8:1:2:3:4:5:6"].forEach(function(ip) {
			var name=p(ip,"rbl.test"); check(name && name.split(".").length==34 && name.indexOf("undefined")<0,name);
		});
		check(p("2001:db8::1","rbl.test")=="1."+new Array(24).join("0.")+"8.b.d.0.1.0.0.2.rbl.test");
		["999.1.2.3","2001:::1","2001::1::2","2001:db8:1","2001:db8:12345:0:0:0:0:1","host.test"].forEach(function(ip) { check(p(ip,"rbl.test")===null,ip); });
	}
});
