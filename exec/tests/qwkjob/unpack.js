// system.qwk_unpack(): off-line QWK reply (REP) packet import for a user.
// Requires the QWK job test install (user QWKB; sub-board qwkt-b is QWK
// conference 2, which both fixtures address).

var SUB_CFG_NSCAN = 0x0005;
var SUBS = ["qwkt-a", "qwkt-b", "qwkt-c", "qwkt-d"];

var u = system.matchuser("QWKB");
var fixture_dir = js.exec_dir;
var subs_file = system.data_dir + format("user/%04u.subs", u);

function total_msgs(code)
{
	var mb = new MsgBase(code);
	if (!mb.open())
		throw new Error("open " + code + ": " + mb.last_error);
	var total = mb.total_msgs;
	mb.close();
	return total;
}

function copy_fixture(name, dest)
{
	if (!file_copy(fixture_dir + name, dest))
		throw new Error("copy " + name + " to " + dest);
	return dest;
}

function bad_files()
{
	return directory(system.data_dir + format("file/%04u.rep.*.bad", u));
}

function read_subs()
{
	var f = new File(subs_file);
	if (!f.open("r"))
		throw new Error("open " + subs_file);
	var obj = {};
	for (var i = 0; i < SUBS.length; i++)
		obj[SUBS[i]] = { ptr: f.iniGetValue(SUBS[i], "ptr", 0), cfg: f.iniGetValue(SUBS[i], "cfg", 0) };
	f.close();
	return obj;
}

function set_scan_cfg(code, cfg)
{
	var f = new File(subs_file);
	if (!f.open(file_exists(subs_file) ? "r+" : "w+"))
		throw new Error("open " + subs_file);
	f.iniSetValue(code, "cfg", format("0x%x", cfg));
	f.close();
}

var temp = system.temp_dir + "qwkjob_unpack_test/";
mkpath(temp);

// A good REP: one message posted to qwkt-b, the packet file removed
var before = total_msgs("qwkt-b");
var rep = copy_fixture("fixture.rep", temp + "good.rep");
var r = system.qwk_unpack(u, rep);
if (r.result != "ok")
	throw new Error("good REP: " + JSON.stringify(r));
if (total_msgs("qwkt-b") != before + 1)
	throw new Error("good REP: qwkt-b count " + total_msgs("qwkt-b") + ", expected " + (before + 1));
if (file_exists(rep))
	throw new Error("good REP left behind: " + rep);

// Garbage REPs: "failed"; an empty one is left in place unless it sits in data/file/ for over
// a minute (it may still be being written), any other is kept as a .bad file
var bad_start = time();
var f = new File(temp + "empty.rep");
f.open("w"); f.close();
r = system.qwk_unpack(u, f.name);
if (r.result != "failed")
	throw new Error("0-byte REP: " + JSON.stringify(r));
if (!file_exists(f.name))
	throw new Error("0-byte REP outside data/file/ removed: " + f.name);
file_utime(f.name, time() - 120, time() - 120);
r = system.qwk_unpack(u, f.name);
if (r.result != "failed" || !file_exists(f.name))
	throw new Error("old 0-byte REP outside data/file/: " + JSON.stringify(r) + " exists=" + file_exists(f.name));
file_remove(f.name);
var swept = system.data_dir + format("file/%04u.rep", u);
f = new File(swept);
f.open("w"); f.close();
r = system.qwk_unpack(u, swept);
if (r.result != "failed" || !file_exists(swept))
	throw new Error("new 0-byte REP in data/file/: " + JSON.stringify(r) + " exists=" + file_exists(swept));
file_utime(swept, time() - 120, time() - 120);
r = system.qwk_unpack(u, swept);
if (r.result != "failed" || file_exists(swept))
	throw new Error("old 0-byte REP in data/file/: " + JSON.stringify(r) + " exists=" + file_exists(swept));
// An old empty REP in data/file/ is left alone while the user's lock is held (its writer may hold it)
var user_lock = system.data_dir + format("pack%04u.lock", u);
f = new File(swept);
f.open("w"); f.close();
file_utime(swept, time() - 120, time() - 120);
f = new File(user_lock);
f.open("w"); f.write("otherhost"); f.close();
r = system.qwk_unpack(u, swept);
file_remove(user_lock);
var swept_kept = file_exists(swept);
file_remove(swept);
if (r.result != "busy" || !swept_kept)
	throw new Error("old 0-byte REP under a held lock: " + JSON.stringify(r) + " exists=" + swept_kept);
f = new File(temp + "text.rep");
f.open("w"); f.writeln("This is not a QWK reply packet."); f.close();
r = system.qwk_unpack(u, f.name);
if (r.result != "failed")
	throw new Error("text REP: " + JSON.stringify(r));
if (file_exists(f.name))
	throw new Error("text REP not moved: " + f.name);
var kept = bad_files().filter(function (path) { return file_date(path) >= bad_start; });
if (!kept.length)
	throw new Error("no .bad file kept: " + JSON.stringify(bad_files()));

// A QWK control message (DROP) takes effect in the user's saved scan config
set_scan_cfg("qwkt-b", SUB_CFG_NSCAN);
var subs_before = read_subs();
rep = copy_fixture("fixture-ctl.rep", temp + "ctl.rep");
r = system.qwk_unpack(u, rep);
if (r.result != "ok")
	throw new Error("control REP: " + JSON.stringify(r));
var subs_after = read_subs();
if (subs_before["qwkt-b"].cfg != SUB_CFG_NSCAN)
	throw new Error("qwkt-b cfg not set up: " + subs_before["qwkt-b"].cfg);
if (subs_after["qwkt-b"].cfg & SUB_CFG_NSCAN)
	throw new Error("DROP not saved: qwkt-b cfg=" + subs_after["qwkt-b"].cfg);
for (var i = 0; i < SUBS.length; i++) {
	var code = SUBS[i];
	if (subs_after[code].ptr != subs_before[code].ptr)
		throw new Error(code + " pointer changed: " + subs_before[code].ptr + " -> " + subs_after[code].ptr);
}
set_scan_cfg("qwkt-b", SUB_CFG_NSCAN);

// Not a regular file: "failed" at once, the path untouched
function expect_refused(path, what)
{
	var t = time();
	var r = system.qwk_unpack(u, path);
	if (r.result != "failed")
		throw new Error(what + ": " + JSON.stringify(r));
	if (time() - t > 5)
		throw new Error(what + ": took " + (time() - t) + " seconds");
}
var dir = temp + "dir.rep";
mkdir(dir);
expect_refused(dir, "directory");
if (!file_isdir(dir))
	throw new Error("directory moved: " + dir);
rmdir(dir);
var wild = temp + "wild1.rep";
copy_fixture("fixture.rep", wild);
expect_refused(temp + "wild?.rep", "wildcard ?");
expect_refused(temp + "*.rep", "wildcard *");
if (!file_exists(wild) || file_size(wild) != file_size(fixture_dir + "fixture.rep"))
	throw new Error("wildcard match touched: " + wild);
file_remove(wild);
if (system.platform != "Win32") {
	var fifo = temp + "fifo.rep";
	if (system.exec("mkfifo " + fifo) != 0)
		throw new Error("mkfifo " + fifo);
	expect_refused(fifo, "FIFO");
	if (!file_exists(fifo))
		throw new Error("FIFO removed: " + fifo);
	file_remove(fifo);
}

rmdir(temp);
if (directory(system.temp_dir + "qwk/*").length)
	throw new Error("job temp dir left behind");
if (system.qwk_unpack(99999, fixture_dir + "fixture.rep").result != "bad_user")
	throw new Error("bad user");
