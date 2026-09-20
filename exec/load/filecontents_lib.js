// Library for the per-file-area content listing store: data/dirs/<code>.contents
//
// Viewers read a stored listing instead of re-opening the archive on every
// view.  See GitLab issue #1247 for the design and the measurements behind it.
//
// Load it into its own scope:  var contents = load({}, "filecontents_lib.js");

require("sbbsdefs.js", "LOG_DEBUG");

var EXT = ".contents";

// Record format version.  Guards the encoding.
var VERSION = 1;

// Extractor-set version.  Guards the *verdict* rather than the encoding: a
// negative record stored because nothing could read the file has to be
// re-examined when the extractor set changes, and nothing else would trigger
// that, since the file itself never changed.
var EXTRACTORS = 1;

// ILLEGAL_FILENAME_CHARS already excludes " and \, so double-quoting a path for
// the shell cannot be broken out of that way.  $ and a backtick are still
// expanded by sh *inside* double quotes, and both are legal in a file-base
// filename, so refuse to hand those to an external tool at all.
var UNSAFE = /[$`]/;

// Extensions worth attempting.  The terminal viewer's catch-all hands every
// unmatched type to archive.js, but a bulk populator should not try to
// enumerate every .txt in the base.
var ARCHIVE_TYPES = ['zip', '7z', 'tgz', 'tar', 'gz', 'bz2',
                     'rar', 'lha', 'lzh', 'iso', 'cab',
                     'arc', 'arj', 'zoo'];

var dirmap = null;
var held = {};
var onexit_registered = false;
var external_tool = null;

function store_path(dircode)
{
	return backslash(system.data_dir + "dirs") + dircode + EXT;
}

function is_archive(filename)
{
	var m = file_getname(filename).match(/\.([^.]+)$/);

	if (m === null)
		return false;
	return ARCHIVE_TYPES.indexOf(m[1].toLowerCase()) >= 0;
}

// Read file metadata through the File object rather than the global functions
// of the same purpose.  A calling script's top-level declarations land on the
// global object, so a consumer can replace those globals outright, and loading
// this library into its own scope does not prevent that: the scope object is
// prepended to the chain, not substituted for it.
function fsize(path)
{
	return new File(path).length;
}

function fdate(path)
{
	return new File(path).date;
}

function fexists(path)
{
	return new File(path).exists;
}

function tmpname()
{
	return system.temp_dir + "fc"
	       + time().toString(36) + random(0x7fffffff).toString(36) + ".tmp";
}

// Which external lister this host has, or "" for none.
//
// The store is shared between hosts that do not necessarily have the same
// tools installed, so a "cannot read this" verdict reached on a host with no
// external lister must not be trusted by one that has it.  Recording the tool
// alongside the verdict lets the better-equipped host notice and re-examine.
//
// Probe by redirecting stdout only: a missing command reports on stderr and
// leaves the file empty, on both /bin/sh and cmd.exe.
function external()
{
	if (external_tool !== null)
		return external_tool;

	var tmp = tmpname();
	var text = "";

	external_tool = "";
	system.exec("lsar -v > \"" + tmp + "\"");
	if (fexists(tmp) && fsize(tmp) > 0) {
		var f = new File(tmp);
		if (f.open("r")) {
			text = f.read();
			f.close();
		}
		// Include the version: hosts sharing a store can have builds years
		// apart (1.8.1 and 1.10.8 are both in the wild), and a format the
		// older one rejects may well be one the newer one reads.
		var m = String(text).match(/v?(\d+(?:\.\d+)+)/);
		external_tool = "lsar/" + (m === null ? "?" : m[1]);
	}
	file_remove(tmp);
	return external_tool;
}

// Map an absolute file path back to the file area holding it.  archive.js is
// handed a path and knows nothing about area codes, and cmdstr() has no
// specifier that could pass one, so the mapping has to happen here.
//
// An area the caller cannot see (ARS-filtered in the terminal server) simply
// will not match, and the caller falls back to extracting without storing.
function area(path)
{
	var full = fullpath(path);
	var name = file_getname(full);
	var dir = backslash(full.substr(0, full.length - name.length));

	if (dirmap === null) {
		dirmap = {};
		for (var code in file_area.dir) {
			var d = file_area.dir[code];
			var key = backslash(fullpath(d.path));
			// Index both spellings, so a lookup works whether or not the
			// volume is case-sensitive without having to probe which.
			if (dirmap[key] === undefined)
				dirmap[key] = d.code;
			var lc = key.toLowerCase();
			if (dirmap[lc] === undefined)
				dirmap[lc] = d.code;
		}
	}
	if (dirmap[dir] !== undefined)
		return dirmap[dir];
	return dirmap[dir.toLowerCase()];
}

function lock(dircode)
{
	var fname = store_path(dircode) + ".lock";
	var start = time();

	while (!file_mutex(fname, "filecontents", 60)) {
		if (time() - start >= 10)
			return false;
		sleep(250);
	}
	held[dircode] = fname;
	// An out-of-memory error is not a catchable exception, so release from an
	// exit handler too, else an aborted script strands the lock file.
	if (!onexit_registered && typeof js == 'object' && js.on_exit !== undefined) {
		js.on_exit("release();");
		onexit_registered = true;
	}
	return true;
}

function unlock(dircode)
{
	if (held[dircode] === undefined)
		return;
	file_remove(held[dircode]);
	delete held[dircode];
}

function release()
{
	var codes = [];
	var code;

	// Collect first: unlock() deletes from the object.
	for (code in held)
		codes.push(code);
	for (var i = 0; i < codes.length; i++)
		unlock(codes[i]);
}

function read_store(dircode)
{
	var fname = store_path(dircode);
	if (!fexists(fname))
		return { v: VERSION, files: {} };

	var f = new File(fname);
	if (!f.open("r"))
		return { v: VERSION, files: {} };
	var text = f.read();
	f.close();

	var obj;
	try {
		obj = JSON.parse(text);
	} catch (e) {
		log(LOG_WARNING, "filecontents: unparsable store " + fname + ": " + e);
		return { v: VERSION, files: {} };
	}
	if (obj === null || typeof obj != 'object' || obj.v !== VERSION
	    || typeof obj.files != 'object')
		return { v: VERSION, files: {} };
	return obj;
}

function write_store(dircode, obj)
{
	var fname = store_path(dircode);
	var tmp = fname + ".tmp";
	var f = new File(tmp);

	if (!f.open("w"))
		return false;
	var ok = f.write(JSON.stringify(obj));
	f.close();
	if (!ok) {
		file_remove(tmp);
		return false;
	}
	// Replace rather than rewrite, so a reader never sees a half-written store.
	if (fexists(fname))
		file_remove(fname);
	return file_rename(tmp, fname);
}

// A record is current when the file it describes has not changed.  A stored
// failure additionally has to have been reached under the same conditions this
// host can offer: the same extractor set, and the same external tool.  A
// verdict of "unreadable" from a host without an external lister says nothing
// about a host that has one.
function current(rec, path)
{
	if (rec === undefined || rec === null)
		return false;
	if (rec.sz !== fsize(path) || rec.mt !== fdate(path))
		return false;
	if (rec.err !== undefined && rec.xv !== EXTRACTORS)
		return false;
	if (rec.err !== undefined && rec.ext !== external())
		return false;
	return true;
}

// Return the stored record for a path, or null when absent or stale.
function get(path)
{
	var code = area(path);
	if (code === undefined)
		return null;
	var store = read_store(code);
	var rec = store.files[file_getname(path)];
	if (!current(rec, path))
		return null;
	return rec;
}

// Store a record for a path.  Returns false when the file lies outside every
// file area, which is not an error: there is nowhere to persist it.
function put(path, rec)
{
	var code = area(path);
	if (code === undefined)
		return false;
	if (!lock(code))
		return false;
	try {
		var store = read_store(code);
		store.files[file_getname(path)] = rec;
		return write_store(code, store);
	} finally {
		unlock(code);
	}
}

// Enumerate an archive with libarchive, falling back to an external tool for
// formats it does not support (notably ARC, which it cannot read at all).
function extract(path)
{
	var rec = {
		t: "archive",
		sz: fsize(path),
		mt: fdate(path),
		xv: EXTRACTORS
	};
	var items;
	var i;

	try {
		items = Archive(path).list(false);
		rec.x = "libarchive";
	} catch (e) {
		var ext = extract_external(path);
		if (ext === null) {
			rec.x = "libarchive";
			rec.err = String(e).replace(/^Error:\s*/, "");
			rec.ext = external();
			return rec;
		}
		items = ext;
		rec.x = "lsar";
	}

	rec.l = [];
	for (i = 0; i < items.length; i++)
		if (items[i].type == 'file')
			rec.l.push([items[i].name, items[i].size, items[i].time]);
	return rec;
}

// Redirect the tool's output to a file rather than using system.popen(), whose
// _popen() needs a console and so fails inside a Windows service.  system.exec()
// runs through /bin/sh -c or cmd.exe /c, so redirection works on both.
function extract_external(path)
{
	if (UNSAFE.test(path) || external() === "")
		return null;

	var tmp = tmpname();
	var items = null;

	if (system.exec("lsar -j \"" + path + "\" > \"" + tmp + "\"") == 0
	    && fexists(tmp)) {
		var f = new File(tmp);
		if (f.open("r")) {
			var text = f.read();
			f.close();
			try {
				var obj = JSON.parse(text);
				if (obj !== null && obj.lsarContents !== undefined) {
					items = [];
					for (var i = 0; i < obj.lsarContents.length; i++) {
						var e = obj.lsarContents[i];
						items.push({
							type: 'file',
							name: e.XADFileName,
							size: e.XADFileSize,
							time: xaddate(e.XADLastModificationDate)
						});
					}
				}
			} catch (e) {
				log(LOG_DEBUG, "filecontents: unparsable lsar output for " + path);
			}
		}
	}
	file_remove(tmp);
	return items;
}

// "1989-12-25 01:02:00 -0800" -> Unix time
function xaddate(str)
{
	if (typeof str != 'string')
		return 0;
	var t = Date.parse(str.replace(/^(\d{4})-(\d{2})-(\d{2}) /, "$1/$2/$3 "));
	if (isNaN(t))
		return 0;
	return Math.floor(t / 1000);
}

// Expand a stored record into the object shape Archive.list() returns, so a
// consumer's rendering loop does not care which it was handed.  Only name,
// size and time are stored, so a caller needing crc32 or format must extract.
function entries(rec)
{
	var items = [];

	if (rec === null || rec === undefined || rec.l === undefined)
		return items;
	for (var i = 0; i < rec.l.length; i++)
		items.push({
			type: 'file',
			name: rec.l[i][0],
			size: rec.l[i][1],
			time: rec.l[i][2]
		});
	return items;
}

// The three-tier read path: serve a current record, else extract and store,
// else report the failure and store that so the next view does not retry.
// Set no_extract to serve tier 1 only (leaving process spawning off a path
// that should not do it).
function list(path, no_extract)
{
	var rec = get(path);

	if (rec === null) {
		if (no_extract)
			return null;
		rec = extract(path);
		put(path, rec);
	}
	return rec;
}

this;
