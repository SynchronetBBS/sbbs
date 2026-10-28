/* Offline DNS wire-format regressions; no resolver sockets or live queries. */
"use strict";
load(js.exec_dir+"../ircd/lib.sjs");
load("dns.js");
function check(ok,message) { if (!ok) throw new Error(message || "Assertion failed"); }
function int16(n) { return ascii((n>>>8)&255)+ascii(n&255); }
function record(words,ttl,length) {
	var dns=Object.create(DNS.prototype);dns.outstanding={42:{}};
	var data=words.map(int16).join(""),size=length===undefined ? 16 : length;
	var packet=int16(42)+int16(0x8180)+int16(0)+int16(1)+int16(0)+int16(0)
		+"\x00"+int16(28)+int16(1)+int16(ttl>>>16)+int16(ttl&65535)+int16(size)+data;
	return dns.handle_response({recv:function() { return packet; }}).ret.answers[0];
}
run_tests({
	"AAAA records preserve leading, trailing, middle and longest zero runs":function() {
		[
			[[0,0,0,0,0,0,0,1],"::1"],
			[[0,0,0,0,0,0,0,0],"::"],
			[[0x2001,0xdb8,0,0,0,0,0,1],"2001:db8::1"],
			[[0x2001,0xdb8,1,2,3,0,0,0],"2001:db8:1:2:3::"],
			[[0,0,1,0,0,0,2,3],"0:0:1::2:3"],
			[[1,2,3,4,5,6,7,8],"1:2:3:4:5:6:7:8"]
		].forEach(function(test) { var got=record(test[0],2).rdata;check(got==test[1],got+" != "+test[1]); });
	},
	"32-bit TTLs read all four bytes and remain unsigned":function() {
		[0,2,0x01020304,0xffffffff].forEach(function(ttl) { check(record([0,0,0,0,0,0,0,1],ttl).ttl==ttl,"Incorrect TTL: "+ttl); });
	},
	"AAAA records require exactly sixteen bytes":function() {
		[[7,14],[9,18],[7,16]].forEach(function(test) {
			var caught=false;
			try { record(new Array(test[0]+1).join("0").split("").map(Number),2,test[1]); }
			catch(e) { caught=true; }
			check(caught,"Malformed AAAA accepted");
		});
	}
});
