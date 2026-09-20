// Library for the per-file-area content listing store: data/dirs/<code>.contents
//
// Viewers read a stored listing instead of re-opening the archive on every
// view.  See GitLab issue #1247 for the design and the measurements behind it.

require("sbbsdefs.js", "LOG_DEBUG");

var FILECONTENTS_EXT = ".contents";

// Record format version.  Guards the encoding.
var FILECONTENTS_VERSION = 1;

// Extractor-set version.  Guards the *verdict* rather than the encoding: a
// negative record stored because nothing could read the file has to be
// re-examined when the extractor set changes, and nothing else would trigger
// that, since the file itself never changed.
var FILECONTENTS_EXTRACTORS = 1;

// ILLEGAL_FILENAME_CHARS already excludes " and \, so double-quoting a path for
// the shell cannot be broken out of that way.  $ and a backtick are still
// expanded by sh *inside* double quotes, and both are legal in a file-base
// filename, so refuse to hand those to an external tool at all.
var FILECONTENTS_UNSAFE = /[$`]/;

// Extensions worth attempting.  The terminal viewer's catch-all hands every
// unmatched type to archive.js, but a bulk populator should not try to
// enumerate every .txt in the base.
var FILECONTENTS_ARCHIVE_TYPES = ['zip', '7z', 'tgz', 'tar', 'gz', 'bz2',
                                  'rar', 'lha', 'lzh', 'iso', 'cab',
                                  'arc', 'arj', 'zoo'];

var filecontents_dirmap = null;
var filecontents_held = {};
var filecontents_onexit = false;

function filecontents_store(dircode)
{
	return backslash(system.data_dir + "dirs") + dircode + FILECONTENTS_EXT;
}

function filecontents_is_archive(filename)
{
	var m = file_getname(filename).match(/\.([^.]+)$/);

	if (m === null)
		return false;
	return FILECONTENTS_ARCHIVE_TYPES.indexOf(m[1].toLowerCase()) >= 0;
}

// Read file metadata through the File object rather than the global functions
// of the same purpose.  A consumer can shadow those with an incompatible
// signature, and a library must not depend on the caller's scope being clean.
function filecontents_size(path)
{
	return new File(path).length;
}

function filecontents_date(path)
{
	return new File(path).date;
}

function filecontents_exists(path)
{
	return new File(path).exists;
}

// Map an absolute file path back to the file area holding it.  archive.js is
// handed a path and knows nothing about area codes, and cmdstr() has no
// specifier that could pass one, so the mapping has to happen here.
//
// An area the caller cannot see (ARS-filtered in the terminal server) simply
// will not match, and the caller falls back to extracting without storing.
function filecontents_area(path)
{
	var full = fullpath(path);
	var name = file_getname(full);
	var dir = backslash(full.substr(0, full.length - name.length));

	if (filecontents_dirmap === null) {
		filecontents_dirmap = {};
		for (var code in file_area.dir) {
			var d = file_area.dir[code];
			var key = backslash(fullpath(d.path));
			// Index both spellings, so a lookup works whether or not the
			// volume is case-sensitive without having to probe which.
			if (filecontents_dirmap[key] === undefined)
				filecontents_dirmap[key] = d.code;
			var lc = key.toLowerCase();
			if (filecontents_dirmap[lc] === undefined)
				filecontents_dirmap[lc] = d.code;
		}
	}
	if (filecontents_dirmap[dir] !== undefined)
		return filecontents_dirmap[dir];
	return filecontents_dirmap[dir.toLowerCase()];
}

function filecontents_lock(dircode)
{
	var fname = filecontents_store(dircode) + ".lock";
	var start = time();

	while (!file_mutex(fname, "filecontents", 60)) {
		if (time() - start >= 10)
			return false;
		sleep(250);
	}
	filecontents_held[dircode] = fname;
	// An out-of-memory error is not a catchable exception, so release from an
	// exit handler too, else an aborted script strands the lock file.
	if (!filecontents_onexit && typeof js == 'object' && js.on_exit !== undefined) {
		js.on_exit("filecontents_release();");
		filecontents_onexit = true;
	}
	return true;
}

function filecontents_unlock(dircode)
{
	if (filecontents_held[dircode] === undefined)
		return;
	file_remove(filecontents_held[dircode]);
	delete filecontents_held[dircode];
}

function filecontents_release()
{
	var codes = [];
	var code;

	// Collect first: filecontents_unlock() deletes from the object.
	for (code in filecontents_held)
		codes.push(code);
	for (var i = 0; i < codes.length; i++)
		filecontents_unlock(codes[i]);
}

function filecontents_read(dircode)
{
	var fname = filecontents_store(dircode);
	if (!filecontents_exists(fname))
		return { v: FILECONTENTS_VERSION, files: {} };

	var f = new File(fname);
	if (!f.open("r"))
		return { v: FILECONTENTS_VERSION, files: {} };
	var text = f.read();
	f.close();

	var obj;
	try {
		obj = JSON.parse(text);
	} catch (e) {
		log(LOG_WARNING, "filecontents: unparsable store " + fname + ": " + e);
		return { v: FILECONTENTS_VERSION, files: {} };
	}
	if (obj === null || typeof obj != 'object' || obj.v !== FILECONTENTS_VERSION
	    || typeof obj.files != 'object')
		return { v: FILECONTENTS_VERSION, files: {} };
	return obj;
}

function filecontents_write(dircode, obj)
{
	var fname = filecontents_store(dircode);
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
	if (filecontents_exists(fname))
		file_remove(fname);
	return file_rename(tmp, fname);
}

// A record is current when the file it describes has not changed and neither
// the encoding nor (for a stored failure) the extractor set has moved on.
function filecontents_current(rec, path)
{
	if (rec === undefined || rec === null)
		return false;
	if (rec.sz !== filecontents_size(path) || rec.mt !== filecontents_date(path))
		return false;
	if (rec.err !== undefined && rec.xv !== FILECONTENTS_EXTRACTORS)
		return false;
	return true;
}

// Return the stored record for a path, or null when absent or stale.
function filecontents_get(path)
{
	var code = filecontents_area(path);
	if (code === undefined)
		return null;
	var store = filecontents_read(code);
	var rec = store.files[file_getname(path)];
	if (!filecontents_current(rec, path))
		return null;
	return rec;
}

// Store a record for a path.  Returns false when the file lies outside every
// file area, which is not an error: there is nowhere to persist it.
function filecontents_put(path, rec)
{
	var code = filecontents_area(path);
	if (code === undefined)
		return false;
	if (!filecontents_lock(code))
		return false;
	try {
		var store = filecontents_read(code);
		store.files[file_getname(path)] = rec;
		return filecontents_write(code, store);
	} finally {
		filecontents_unlock(code);
	}
}

// Enumerate an archive with libarchive, falling back to an external tool for
// formats it does not support (notably ARC, which it cannot read at all).
function filecontents_extract(path)
{
	var rec = {
		t: "archive",
		sz: filecontents_size(path),
		mt: filecontents_date(path),
		xv: FILECONTENTS_EXTRACTORS
	};
	var list;
	var i;

	try {
		list = Archive(path).list(false);
		rec.x = "libarchive";
	} catch (e) {
		var ext = filecontents_extract_external(path);
		if (ext === null) {
			rec.x = "libarchive";
			rec.err = String(e).replace(/^Error:\s*/, "");
			return rec;
		}
		list = ext;
		rec.x = "lsar";
	}

	rec.l = [];
	for (i = 0; i < list.length; i++)
		if (list[i].type == 'file')
			rec.l.push([list[i].name, list[i].size, list[i].time]);
	return rec;
}

// Expand a stored record into the object shape Archive.list() returns, so a
// consumer's rendering loop does not care which it was handed.  Only name,
// size and time are stored, so a caller needing crc32 or format must extract.
function filecontents_entries(rec)
{
	var list = [];

	if (rec === null || rec === undefined || rec.l === undefined)
		return list;
	for (var i = 0; i < rec.l.length; i++)
		list.push({
			type: 'file',
			name: rec.l[i][0],
			size: rec.l[i][1],
			time: rec.l[i][2]
		});
	return list;
}

// Redirect the tool's output to a file rather than using system.popen(), whose
// _popen() needs a console and so fails inside a Windows service.  system.exec()
// runs through /bin/sh -c or cmd.exe /c, so redirection works on both.
function filecontents_extract_external(path)
{
	if (FILECONTENTS_UNSAFE.test(path))
		return null;

	var tmp = system.temp_dir + "fc"
	          + time().toString(36) + random(0x7fffffff).toString(36) + ".json";
	var list = null;

	if (system.exec("lsar -j \"" + path + "\" > \"" + tmp + "\"") == 0
	    && filecontents_exists(tmp)) {
		var f = new File(tmp);
		if (f.open("r")) {
			var text = f.read();
			f.close();
			try {
				var obj = JSON.parse(text);
				if (obj !== null && obj.lsarContents !== undefined) {
					list = [];
					for (var i = 0; i < obj.lsarContents.length; i++) {
						var e = obj.lsarContents[i];
						list.push({
							type: 'file',
							name: e.XADFileName,
							size: e.XADFileSize,
							time: filecontents_xaddate(e.XADLastModificationDate)
						});
					}
				}
			} catch (e) {
				log(LOG_DEBUG, "filecontents: unparsable lsar output for " + path);
			}
		}
	}
	file_remove(tmp);
	return list;
}

// "1989-12-25 01:02:00 -0800" -> Unix time
function filecontents_xaddate(str)
{
	if (typeof str != 'string')
		return 0;
	var t = Date.parse(str.replace(/^(\d{4})-(\d{2})-(\d{2}) /, "$1/$2/$3 "));
	if (isNaN(t))
		return 0;
	return Math.floor(t / 1000);
}

// The three-tier read path: serve a current record, else extract and store,
// else report the failure and store that so the next view does not retry.
// Set no_extract to serve tier 1 only (leaving process spawning off a path
// that should not do it).
function filecontents_list(path, no_extract)
{
	var rec = filecontents_get(path);

	if (rec === null) {
		if (no_extract)
			return null;
		rec = filecontents_extract(path);
		filecontents_put(path, rec);
	}
	return rec;
}
