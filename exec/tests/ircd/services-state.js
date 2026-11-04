/* External identity, reconnect hints and the Bahamut mode boundary. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(), service = new_peer(s, "services.test", false), hub = new_peer(s, "other.test", true);
	s.ULines = [service.nick]; service.uline = true; service.protocol = "bahamut";
	hub.their_capab.push("SVCSTATE");
	var user = new_user(s, "Reader");
	user.created = s.now;
	user.services_identity = {id:"BBS:1:123", home:s.ServerName};
	user.socket = {ssl_session:false};
	return {s:s, service:service, hub:hub, user:user};
}
function grant(t, fields) { t.service.work(":services.test SVSMODE Reader " + fields); }

run_tests({
	"SVSMODE grants external identification and a stamp without BBS or native operator privileges": function() {
		var t = scenario(), proof = t.user.services_identity;
		grant(t, "+dr 12345");
		check(t.s.IRC_services_identified(t.user) && t.user.external_services.stamp == 12345);
		check(t.user.services_identity === proof && !t.user.mode);
		check(t.hub.sent[0] == ":services.test SVSMODE Reader +dr 12345");
		grant(t, "+a"); check(!t.s.IRC_services_admin(t.user));
		t.user.mode |= t.s.USERMODE_OPER; grant(t, "+a");
		check(t.s.IRC_services_admin(t.user) && !(t.user.mode & t.s.USERMODE_ADMIN));
		grant(t, "-r"); check(!t.s.IRC_services_identified(t.user) && !t.s.IRC_services_admin(t.user));
		check(t.user.external_services.stamp == 12345 && t.user.services_identity === proof);
	},
	"SVSMODE validates the complete command, timestamp and stamp before mutation": function() {
		["+rd", "+dr 1 trailing", "+dr -1", "+dr 4294967296", "+dr 1x", "+o", "+R", "+", "1800000001 +r", "4294967296 +r"].forEach(function(fields) {
			var t = scenario(); grant(t, fields);
			check(!t.user.external_services && !t.hub.sent.length, fields);
		});
		var t = scenario(); grant(t, "1800000000 +r"); check(t.s.IRC_services_identified(t.user));
		grant(t, "0 -rd 0"); check(!t.s.IRC_services_identified(t.user) && !t.user.external_services.stamp);
	},
	"ordinary users, untrusted servers and forged service routes cannot grant identification": function() {
		var t = scenario(), leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("SVCSTATE");
		t.service.uline = false; grant(t, "+r");
		t.service.uline = true; t.hub.work(":services.test SVSMODE Reader +r");
		leaf.work(":leaf.test SVSMODE Reader +r");
		leaf.work(":Reader SVSMODE Reader +r");
		check(!t.user.external_services && !t.hub.sent.length);
	},
	"matching intermediate caches still relay grants and old peers receive no invented native modes": function() {
		var t = scenario(), old = new_peer(t.s, "old.test", true);
		grant(t, "+r"); grant(t, "+r");
		check(t.hub.sent.length == 2 && !old.sent.length);
		old.server_nick_info(t.user);
		check(old.sent[0].split(" ")[4] == "+" && old.sent[0].split(" ")[8] == "0");
	},
	"real nickname changes clear grants, preserve opaque stamps and case changes preserve identity": function() {
		var t = scenario(); grant(t, "+dr 987");
		t.s.IRC_local_nick(t.user, "reader", t.s.now); check(t.s.IRC_services_identified(t.user));
		t.s.IRC_local_nick(t.user, "Other", t.s.now + 1);
		check(!t.s.IRC_services_identified(t.user) && t.user.external_services.stamp == 987);
		check(t.s.IRC_external_state(t.user) && t.user.services_identity.id == "BBS:1:123");
	},
	"deoper clears the services administrator marker permanently": function() {
		var t = scenario(); t.user.mode |= t.s.USERMODE_OPER; grant(t, "+ra");
		check(t.s.IRC_services_admin(t.user)); t.user.setusermode("-o");
		t.user.mode |= t.s.USERMODE_OPER; check(!t.s.IRC_services_admin(t.user));
	},
	"WHOIS exposes external identification without claiming BBS authentication or server admin": function() {
		var t = scenario(), viewer = new_user(t.s, "Viewer");
		t.user.mode |= t.s.USERMODE_OPER; grant(t, "+ra"); viewer.do_whois(t.user);
		check(viewer.received.some(function(line) { return line.indexOf(" 307 Viewer Reader ") >= 0; }));
		check(viewer.received.some(function(line) { return line.indexOf(" 320 Viewer Reader :is a Services administrator.") >= 0; }));
	},
	"native snapshots restore observed identity only from a directly configured hub": function() {
		var t = scenario();
		t.hub.work(":other.test SVCSTATE Reader 1800000000 services.test 777 ra");
		check(t.s.IRC_services_identified(t.user) && t.user.services_stamp == 777);
		var leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("SVCSTATE");
		leaf.work(":leaf.test SVCSTATE Reader 1800000000 services.test 888 -");
		check(t.user.services_stamp == 777 && t.s.IRC_services_identified(t.user));
		t.hub.work(":other.test SVCSTATE Reader 1800000001 services.test 999 -");
		t.hub.work(":other.test SVCSTATE Reader 1800000000 unknown.test 999 -");
		t.hub.work(":other.test SVCSTATE Reader 1800000000 services.test 999 malformed");
		check(t.user.services_stamp == 777 && t.s.IRC_services_identified(t.user));
	},
	"ordinary leaf NICK modes and stamp hints cannot grant identification or administrator flags": function() {
		var t = scenario(), leaf = new_peer(t.s, "leaf.test", false); leaf.their_capab.push("SVCSTATE");
		leaf.work("NICK Visitor 1 1700000000 +rdaS user host.test leaf.test 555@services.test 0 :Visitor");
		var user = t.s.Users.VISITOR;
		check(user && !t.s.IRC_services_identified(user) && !t.s.IRC_services_admin(user));
		check(!user.services_stamp && t.s.IRC_client_tls_status(user) == "UNKNOWN");
		check(t.s.IRC_services_stamp(t.service, user) == 0);
	},
	"reconnect hints are authority scoped and never exported to old native peers": function() {
		var t = scenario(), other = new_peer(t.s, "second-services.test", false), old = new_peer(t.s, "old.test", true);
		t.s.ULines.push(other.nick); other.protocol = "bahamut";
		grant(t, "+dr 2147483647");
		check(t.s.IRC_services_stamp(t.service, t.user) == 2147483647);
		check(t.s.IRC_services_stamp(other, t.user) == 0 && t.s.IRC_services_stamp(old, t.user) == 0);
		check(t.s.IRC_services_stamp(t.hub, t.user) == "2147483647@services.test");
		t.hub.server_nick_info(t.user);
		check(t.hub.sent[t.hub.sent.length - 1] == ":hub.test SVCSTATE Reader 1800000000 services.test 2147483647 r");
		t.s.ULines = []; t.s.IRC_services_rehash_state();
		check(!t.s.IRC_services_identified(t.user) && t.s.IRC_services_stamp(t.service, t.user) == 0);
		check(t.user.services_identity.id == "BBS:1:123");
	},
	"Bahamut user modes never alias the native r/d notice modes or first-party TLS": function() {
		var t = scenario(); t.user.mode |= t.s.USERMODE_OPER;
		check(t.s.IRC_services_native_modes(t.service, "+ordSa") == "+o");
		check(t.s.IRC_services_native_modes(t.hub, "+ordSa") == "+ordSa");
		grant(t, "+r"); t.user.socket.ssl_session = true;
		check(t.s.IRC_services_modes(t.service, t.user) == "+orS");
		check(t.s.IRC_services_modes(t.hub, t.user) == "+o");
	},
	"TLS translation uses the canonical first-party result, including conflicting reports": function() {
		var t = scenario(), remote = new_user(t.s, "Remote", t.hub.nick); t.hub.their_capab.push("CLIENTTLS");
		t.hub.work(":other.test CLIENTTLS Remote PLAIN");
		t.hub.work(":other.test CLIENTTLS Remote TLS");
		check(t.s.IRC_client_tls_status(remote) == "PLAIN");
		check(t.service.sent.length == 2 && t.service.sent.every(function(line) { return line == ":other.test MODE Remote -S"; }));
	},
	"trusted hub reconnect hints retain authority without asserting identification": function() {
		var t = scenario();
		t.hub.work("NICK Recovered 1 1800000000 +rdaS user host.test other.test 123@services.test 0 :Recovered user");
		var user = t.s.Users.RECOVERED;
		check(user && user.services_stamp == 123 && !t.s.IRC_services_identified(user));
		check(t.s.IRC_services_stamp(t.service, user) == 123);
		check(t.s.IRC_client_tls_status(user) == "UNKNOWN");
	},
	"remote WHOIS numerics cannot fabricate local identification information": function() {
		var t = scenario();
		t.hub.work(":other.test 307 Reader Reader :identified");
		t.hub.work(":other.test 320 Reader Reader :is a Services administrator.");
		check(!t.user.received.length);
		grant(t, "+r");
		t.hub.work(":other.test 311 Reader Reader user host.test * :Reader");
		check(t.user.received.filter(function(line) { return line.indexOf(" 307 ") >= 0; }).length == 1);
	},
	"structured stamps cannot overflow an otherwise valid NICK introduction": function() {
		var t = scenario(); grant(t, "+dr 123");
		var long_owner = new Array(221).join("s") + ".test";
		t.s.ULines.push(long_owner); t.user.external_services.owner = long_owner;
		t.user.uprefix = "user"; t.user.hostname = new Array(431).join("h"); t.user.realname = "Reader";
		t.hub.sent = []; t.hub.server_nick_info(t.user);
		check(t.hub.sent.every(function(line) { return line.length <= 510; }), t.hub.sent.join(" | "));
		check(t.hub.sent[0].split(" ")[8] == "0" && t.hub.sent[t.hub.sent.length - 1].indexOf(" SVCSTATE ") >= 0);
	}
});
