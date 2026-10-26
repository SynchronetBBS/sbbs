// system.qwk_pack(hub_id) / system.qwk_unpack(hub_id, path): QWKnet hub
// REP creation and QWK import.
// Requires the QWK job test install: enabled hub TESTHUB maps conference 1 to
// sub-board qwkt-c and 2 to qwkt-d; disabled hub QWKPEER. fixture.qwk holds
// 3 messages (with Message-IDs) in conference 1.

var HUB = "TESTHUB";
var rep = system.data_dir + "TESTHUB.REP";
var ptr_file = system.data_dir + "qnet/TESTHUB.ptr";
var hub_lock = system.data_dir + "qnet/TESTHUB.lock";
var temp = system.temp_dir + "qwkjob_hub_test/";
var start = time();

function total_msgs(code)
{
	var mb = new MsgBase(code);
	if (!mb.open())
		throw new Error("open " + code + ": " + mb.last_error);
	var total = mb.total_msgs;
	mb.close();
	return total;
}

function post_message(code, body)
{
	var mb = new MsgBase(code);
	if (!mb.open())
		throw new Error("open " + code + ": " + mb.last_error);
	var ok = mb.save_msg({ to: "All", from: "Seeder", subject: "qwkjob hub test" }, body + "\r\n");
	var err = mb.last_error;
	mb.close();
	if (!ok)
		throw new Error("save_msg " + code + ": " + err);
}

function read_binary(path)
{
	var f = new File(path);
	if (!f.open("rb"))
		throw new Error("open " + path);
	var data = f.read();
	f.close();
	return data;
}

function write_file(path, text)
{
	var f = new File(path);
	if (!f.open("wb"))
		throw new Error("create " + path);
	f.write(text);
	f.close();
}

function remove_dir(dir)
{
	var files = directory(dir + "*");
	for (var i = 0; i < files.length; i++)
		file_remove(files[i]);
	rmdir(dir);
}

function copy_fixture(dest)
{
	if (!file_copy(js.exec_dir + "fixture.qwk", dest))
		throw new Error("copy fixture.qwk to " + dest);
	return dest;
}

// Message-IDs of the fixture's messages, from its HEADERS.DAT
function fixture_msg_ids()
{
	var dir = temp + "fixture/";
	mkpath(dir);
	new Archive(js.exec_dir + "fixture.qwk").extract(dir);
	var lines = read_binary(dir + "HEADERS.DAT").split(/\r?\n/);
	var ids = [];
	for (var i = 0; i < lines.length; i++) {
		var m = lines[i].match(/^Message-ID:\s*(\S+)/);
		if (m)
			ids.push(m[1]);
	}
	remove_dir(dir);
	if (ids.length != 3)
		throw new Error("fixture Message-IDs: " + ids.length);
	return ids;
}

// Fixture messages a previous run already imported (a re-import is all dupes)
function fixture_msgs_present(ids)
{
	var mb = new MsgBase("qwkt-c");
	if (!mb.open())
		throw new Error("open qwkt-c: " + mb.last_error);
	var count = 0;
	for (var i = 0; i < ids.length; i++)
		if (mb.get_msg_header(ids[i]))
			count++;
	mb.close();
	return count;
}

function rep_messages()
{
	var dir = temp + "rep/";
	mkpath(dir);
	new Archive(rep).extract(dir);
	var data = read_binary(dir + "TESTHUB.MSG");
	remove_dir(dir);
	return data;
}

function hub_temp_dirs()
{
	return directory(system.temp_dir + "qwk/" + HUB + ".*");
}

function expect(r, result, what)
{
	if (r.result != result)
		throw new Error(what + ": " + JSON.stringify(r));
}

mkpath(temp);
file_remove(hub_lock);

// Export whatever is pending, so the pack counts below are exact
var r = system.qwk_pack(HUB);
if (r.result != "ok" && r.result != "empty")
	throw new Error("initial pack: " + JSON.stringify(r));
file_remove(rep);

// Import: 3 messages into qwkt-c, the packet file removed
var ids = fixture_msg_ids();
var present = fixture_msgs_present(ids);
var before = total_msgs("qwkt-c");
var qwk = copy_fixture(temp + "in1.qwk");
expect(system.qwk_unpack(HUB, qwk), "ok", "import");
if (total_msgs("qwkt-c") != before + (ids.length - present))
	throw new Error("import: qwkt-c " + before + " -> " + total_msgs("qwkt-c") + " (" + present + " present)");
if (file_exists(qwk))
	throw new Error("imported packet left behind: " + qwk);
if (hub_temp_dirs().length)
	throw new Error("job temp dir left behind: " + hub_temp_dirs());

// Hub ID matching is case-insensitive; the same packet again is all dupes
before = total_msgs("qwkt-c");
qwk = copy_fixture(temp + "in2.qwk");
expect(system.qwk_unpack("testhub", qwk), "ok", "lower-case hub ID import");
if (total_msgs("qwkt-c") != before)
	throw new Error("dupes imported: qwkt-c " + before + " -> " + total_msgs("qwkt-c"));
if (file_exists(qwk))
	throw new Error("imported packet left behind: " + qwk);

// Pack: only the local messages; the imported ones came from the hub
var token = "qwkjob hub " + start + " ";
post_message("qwkt-c", token + "one");
post_message("qwkt-c", token + "two");
r = system.qwk_pack(HUB);
if (r.result != "ok" || r.msgs != 2 || r.path != rep || !file_exists(rep))
	throw new Error("pack: " + JSON.stringify(r));
if (!file_exists(ptr_file))
	throw new Error("pointers not saved: " + ptr_file);

// An unsent REP is merged into, not overwritten
post_message("qwkt-c", token + "three");
r = system.qwk_pack(HUB);
if (r.result != "ok" || r.msgs != 1 || r.path != rep)
	throw new Error("merge pack: " + JSON.stringify(r));
var msgs = rep_messages();
var parts = ["one", "two", "three"];
for (var i = 0; i < parts.length; i++)
	if (msgs.indexOf(token + parts[i]) < 0)
		throw new Error("merged REP lacks message '" + parts[i] + "'");
var ptrs = read_binary(ptr_file);

// Nothing new: no packet, pointers unchanged
file_remove(rep);
expect(system.qwk_pack(HUB), "empty", "pack with nothing new");
if (file_exists(rep))
	throw new Error("empty pack created " + rep);
if (read_binary(ptr_file) != ptrs)
	throw new Error("pointers changed by an empty pack");

// The hub lock held by another job: busy, the packet left alone
write_file(hub_lock, "otherhost");
qwk = copy_fixture(temp + "locked.qwk");
var rp = system.qwk_pack(HUB);
var ru = system.qwk_unpack(HUB, qwk);
file_remove(hub_lock);
expect(rp, "busy", "locked pack");
expect(ru, "busy", "locked import");
if (!file_exists(qwk))
	throw new Error("locked import removed " + qwk);
file_remove(qwk);

// Unknown and disabled hubs
expect(system.qwk_pack("NOSUCH"), "bad_hub", "unknown hub pack");
expect(system.qwk_unpack("NOSUCH", temp + "none.qwk"), "bad_hub", "unknown hub import");
expect(system.qwk_pack("QWKPEER"), "bad_hub", "disabled hub pack");
expect(system.qwk_unpack("QWKPEER", temp + "none.qwk"), "bad_hub", "disabled hub import");

// Bad packets: an empty one is left in place unless it sits in data/ for over a minute (it may
// still be being written), any other is kept as data/<HUBID>.qwk.<hex-time>.bad
qwk = temp + "empty.qwk";
write_file(qwk, "");
file_utime(qwk, time() - 120, time() - 120);
expect(system.qwk_unpack(HUB, qwk), "failed", "empty packet import");
if (!file_exists(qwk))
	throw new Error("empty packet outside data/ removed: " + qwk);
file_remove(qwk);
qwk = system.data_dir + HUB + ".qwk";
write_file(qwk, "");
expect(system.qwk_unpack(HUB, qwk), "failed", "new empty packet in data/");
if (!file_exists(qwk))
	throw new Error("new empty packet in data/ removed: " + qwk);
file_utime(qwk, time() - 120, time() - 120);
expect(system.qwk_unpack(HUB, qwk), "failed", "old empty packet in data/");
if (file_exists(qwk))
	throw new Error("old empty packet in data/ left behind: " + qwk);

// Not a regular file: "failed" at once, the path untouched
function expect_refused(path, what)
{
	var t = time();
	expect(system.qwk_unpack(HUB, path), "failed", what);
	if (time() - t > 5)
		throw new Error(what + ": took " + (time() - t) + " seconds");
}
var dir = temp + "dir.qwk";
mkdir(dir);
expect_refused(dir, "directory");
if (!file_isdir(dir))
	throw new Error("directory moved: " + dir);
rmdir(dir);
var wild = temp + "wild1.qwk";
copy_fixture(wild);
expect_refused(temp + "wild?.qwk", "wildcard ?");
expect_refused(temp + "*.qwk", "wildcard *");
if (!file_exists(wild) || file_size(wild) != file_size(js.exec_dir + "fixture.qwk"))
	throw new Error("wildcard match touched: " + wild);
file_remove(wild);
if (system.platform != "Win32") {
	var fifo = temp + "fifo.qwk";
	if (system.exec("mkfifo " + fifo) != 0)
		throw new Error("mkfifo " + fifo);
	expect_refused(fifo, "FIFO");
	if (!file_exists(fifo))
		throw new Error("FIFO removed: " + fifo);
	file_remove(fifo);
	expect_refused("/dev/null", "device");
}

qwk = temp + "text.qwk";
var junk = "not a QWK packet " + start + "\r\n";
write_file(qwk, junk);
expect(system.qwk_unpack(HUB, qwk), "failed", "text packet import");
if (file_exists(qwk))
	throw new Error("bad packet not moved: " + qwk);
var bad = directory(system.data_dir + HUB + ".qwk.*.bad");
var kept = null;
for (i = 0; i < bad.length; i++)
	if (file_date(bad[i]) >= start && read_binary(bad[i]) == junk)
		kept = bad[i];
if (!kept)
	throw new Error("bad packet not kept: " + JSON.stringify(bad));
file_remove(kept);

if (hub_temp_dirs().length)
	throw new Error("job temp dir left behind: " + hub_temp_dirs());
if (file_exists(hub_lock))
	throw new Error("hub lock left behind");
rmdir(temp);
