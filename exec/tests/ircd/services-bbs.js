/* Native BBS/telegram worker integration. Run ONLY with the isolated ctrl
 * fixture described in services.md and --bbs-fixture. Never uses live data.
 */
"use strict";
load(js.exec_dir + "lib.sjs"); load("sbbsdefs.js"); load("ircd/services-common.js");
if (argv.indexOf("--bbs-fixture") < 0) {
	print("SKIP: native BBS services tests require an isolated --bbs-fixture");
} else {
	if (system.data_dir.indexOf("/tmp/ircd-services-bbs-fixture/data/") !== 0 || system.ctrl_dir.indexOf("/tmp/ircd-services-bbs-fixture/ctrl/") !== 0)
		throw new Error("Refusing to modify non-fixture BBS accounts or telegrams");
	var account = system.matchuser("Some User", false) || system.new_user("Some User").number;
	var bbsuser = new User(account); bbsuser.settings &= ~(USER_DELETED | USER_INACTIVE);
	bbsuser.security.restrictions = 0; bbsuser.security.password = "TESTPASS";
	var home = String(system.qwk_id).toLowerCase() + ".test";
	var identity = IRC_services_identity(bbsuser, system.qwk_id, home);
	var queue = load(true, js.exec_dir + "services-bbs-worker.sjs", {
		directory:js.exec_dir + "../../load/ircd/", server:home, qwk:system.qwk_id,
		host:"127.0.0.1", binds:["127.0.0.1"], database:system.data_dir + "services-test.sqlite"
	});
	var serial = 0;
	var job = function(data) {
		data.id = String(++serial); queue.write(data);
		var end = system.timer + 10;
		while (system.timer < end) { var r = queue.read(100); if (r && r.id == data.id) return r; }
		throw new Error("Worker request timed out: " + data.type);
	};
	var assert = function(ok, msg) { if (!ok) throw new Error(msg || "Assertion failed"); };
	var spool = system.data_dir + "msgs/" + format("%04u", account) + ".msg";
	try {
		run_tests({
			"native alias matching accepts dots for spaces": function() { assert(system.matchuser("Some.User", false) == account); },
			"worker authenticates a dotted BBS alias": function() {
				var r = job({type:"authenticate",account:"Some.User",password:"testpass"});
				assert(r.identity && r.identity.id == identity.id && r.identity.username == "Some.User+" + system.qwk_id);
			},
			"wrong passwords do not authenticate an existing BBS account": function() { assert(!job({type:"authenticate",account:"Some.User",password:"wrong"}).identity); },
			"worker identifies a local DNS services host": function() {
				queue.write({type:"dns"}); var r = queue.read(5000); assert(r && r.type == "dns" && r.local);
			},
			"STATUS and implicit checks preserve native BBS telegrams": function() {
				system.get_telegram(account); assert(system.put_telegram(account,"First telegram\r\n"));
				assert(job({type:"memo",operation:"STATUS",identity:identity}).text.indexOf("You have telegrams") >= 0);
				assert(job({type:"memo",operation:"STATUS",identity:identity}).text.indexOf("You have telegrams") >= 0);
				var f = new File(spool); assert(f.open("rb",false)); try { assert(f.read() == "First telegram\r\n"); } finally { f.close(); }
			},
			"READ consumes through native get_telegram": function() {
				assert(job({type:"memo",operation:"READ",identity:identity}).text == "First telegram\r\n");
				assert(job({type:"memo",operation:"STATUS",identity:identity}).text == "No telegrams waiting.");
			},
			"native telegram writer respects snapshot read locking": function() {
				assert(system.put_telegram(account,"Before lock\r\n"));
				var f = new File(spool); assert(f.open("rb",false));
				var id = String(++serial);
				try {
					queue.write({type:"memo",operation:"SEND",identity:identity,text:"After lock\r\n",id:id});
					assert(!queue.read(100),"Native writer ran while snapshot lock was held");
					assert(f.read() == "Before lock\r\n");
				} finally { f.close(); }
				var r = queue.read(5000); assert(r && r.id == id && !r.error);
				assert(job({type:"memo",operation:"READ",identity:identity}).text == "Before lock\r\nAfter lock\r\n");
			},
			"reused or wrong-home accounts cannot read telegrams": function() {
				var stale = IRC_services_copy(identity); stale.created--;
				assert(job({type:"memo",operation:"READ",identity:stale}).type == "failure");
				stale = IRC_services_copy(identity); stale.home = "other.test";
				assert(job({type:"memo",operation:"READ",identity:stale}).type == "failure");
			}
		});
	} finally { queue.write({type:"stop"}); }
}
