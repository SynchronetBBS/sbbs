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
var r = system.qwk_pack(u);
if (r.result != "ok" || r.msgs < 1 || r.path != qwk || !file_exists(qwk))
	throw new Error("first pack: " + JSON.stringify(r));
var r2 = system.qwk_pack(u);	/* pointers advanced: nothing new */
if (r2.result != "empty")
	throw new Error("second pack: " + JSON.stringify(r2));
var lock = new File(system.data_dir + format("pack%04u.lock", u));
lock.open("w"); lock.writeln("otherhost"); lock.close();
var r3 = system.qwk_pack(u);
file_remove(lock.name);
if (r3.result != "busy")
	throw new Error("locked pack: " + JSON.stringify(r3));
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
