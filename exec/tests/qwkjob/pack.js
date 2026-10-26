// system.qwk_pack(): off-line QWK packet creation for a user.
// Requires the QWK job test install (users QWKA/QWKB, sub-boards qwkt-a..d
// mapped by a QWKnet hub, so the Q-restricted users receive them).

function post_new_message(code)
{
	var mb = new MsgBase(code);
	if (!mb.open())
		throw new Error("open " + code + ": " + mb.last_error);
	var ok = mb.save_msg({ to: "All", from: "Seeder", subject: "qwkjob pack test " + Date.now() }
		, "qwk_pack() test message\r\n");
	var err = mb.last_error;
	mb.close();
	if (!ok)
		throw new Error("save_msg " + code + ": " + err);
}

var u = system.matchuser("QWKA");
var qwk = system.data_dir + format("file/%04u.qwk", u);
file_remove(qwk);
post_new_message("qwkt-a");

// A job temp dir orphaned over 6 hours ago is removed by the next job; a recent one is kept,
// as is an old one named for a process still running on this host (its job may just be idle)
var stale = system.temp_dir + "qwk/0000.stalehost.0.0/";
var recent = system.temp_dir + "qwk/0000.recenthost.0.0/";
var dead = system.temp_dir + "qwk/0000." + system.host_name + ".999999999.0/";
var live = system.temp_dir + "qwk/0000." + system.host_name + ".1.0/";   // init
var old = time() - (7 * 60 * 60);
var hours_old = time() - (2 * 60 * 60);
var check_live = system.platform != "Win32";
mkpath(stale);
mkpath(recent);
mkpath(dead);
if (check_live)
	mkpath(live);
var sf = new File(stale + "leftover");
sf.open("w"); sf.writeln("x"); sf.close();
if (!file_utime(stale, old, old) || !file_utime(dead, old, old)
	|| !file_utime(recent, hours_old, hours_old) || (check_live && !file_utime(live, old, old)))
	throw new Error("file_utime");

var r = system.qwk_pack(u);
if (r.result != "ok" || r.msgs < 1 || r.path != qwk || !file_exists(qwk))
	throw new Error("first pack: " + JSON.stringify(r));
if (file_isdir(stale))
	throw new Error("stale job temp dir not removed: " + stale);
if (file_isdir(dead))
	throw new Error("stale temp dir of an ended process not removed: " + dead);
if (!file_isdir(recent))
	throw new Error("recent temp dir removed: " + recent);
rmdir(recent);
if (check_live) {
	var live_kept = file_isdir(live);
	rmdir(live);
	if (!live_kept)
		throw new Error("temp dir of a running process removed: " + live);
}
var r2 = system.qwk_pack(u);	/* pointers advanced: nothing new */
if (r2.result != "empty")
	throw new Error("second pack: " + JSON.stringify(r2));
var lock = new File(system.data_dir + format("pack%04u.lock", u));
lock.open("w"); lock.writeln("otherhost"); lock.close();
var r3 = system.qwk_pack(u);
file_remove(lock.name);
if (r3.result != "busy")
	throw new Error("locked pack: " + JSON.stringify(r3));
// A lock over an hour old is a crashed holder's: taken over, and removed after the job
lock.open("w"); lock.writeln("crashedhost"); lock.close();
file_utime(lock.name, old, old);
var r4 = system.qwk_pack(u);
var lock_left = file_exists(lock.name);
file_remove(lock.name);
if (r4.result != "empty")
	throw new Error("pack under a stale lock: " + JSON.stringify(r4));
if (lock_left)
	throw new Error("stale lock left behind");
if (directory(system.temp_dir + "qwk/*").length)
	throw new Error("job temp dir left behind");
if (system.qwk_pack(99999).result != "bad_user")
	throw new Error("bad user");
if (system.qwk_pack("NOSUCH").result != "bad_hub")
	throw new Error("unknown hub");
var threw = false;
try { system.qwk_pack(true); } catch (e) { threw = true; }
if (!threw)
	throw new Error("boolean argument accepted");
file_remove(qwk);
