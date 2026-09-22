// Library for stored archive content listings.
//
// A viewer reads the stored listing instead of re-opening the archive on every
// view.  The listing is kept in the file record's own auxdata, so reading one
// costs that record's data blocks rather than a file area's worth of listings,
// and it is removed along with the record it describes.  See #1247 for the
// design and the measurements behind it.
//
// Load it into its own scope:  var contents = load({}, "filecontents_lib.js");

require("sbbsdefs.js", "LOG_DEBUG");

// auxdata is a general-purpose per-file slot rather than ours, so the listing
// goes under a key of its own and anything else found there is preserved.
var AUXKEY = "archive_contents";

// Record format version.  Guards the encoding: a record written in a form this
// code does not know is treated as absent and re-extracted.
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
//
// 'exe' is here for the self-extracting archives a file base is full of:
// libarchive finds the Zip or 7-Zip data inside the stub rather than
// insisting it start at offset 0.  A plain program simply fails to extract,
// which is stored like any other negative verdict and not retried.
var ARCHIVE_TYPES = ['zip', '7z', 'tgz', 'tar', 'gz', 'bz2',
                     'rar', 'lha', 'lzh', 'iso', 'cab',
                     'arc', 'arj', 'zoo', 'exe'];

var dirmap = null;
var external_tool = null;

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

// Which external lister this host has, or "" for none.
//
// The store is shared between hosts that do not necessarily have the same
// tools installed, so a "cannot read this" verdict reached on a host with no
// external lister must not be trusted by one that has it.  Recording the tool
// alongside the verdict lets the better-equipped host notice and re-examine.
//
// Probe by capturing stdout only: a missing command reports on stderr, which
// system.popen() doesn't capture, so it simply yields nothing.
function external()
{
	if (external_tool !== null)
		return external_tool;

	var text = system.popen("lsar -v").join("");

	external_tool = "";
	if (text.length > 0) {
		// Include the version: hosts sharing a store can have builds years
		// apart (1.8.1 and 1.10.8 are both in the wild), and a format the
		// older one rejects may well be one the newer one reads.
		var m = text.match(/v?(\d+(?:\.\d+)+)/);
		external_tool = "lsar/" + (m === null ? "?" : m[1]);
	}
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

// The file record, at the detail level that carries auxdata.  Opening the base
// per operation keeps this re-entrant: the terminal viewer, the web server and
// the populator all reach the same record from different processes.
function open_base(dircode)
{
	var fb = new FileBase(dircode);

	if (!fb.open())
		return null;
	return fb;
}

// The auxdata object for a file record, {} when it has none, or null when it
// holds something this library did not write and cannot parse.
function parse_aux(file)
{
	var obj;

	if (file === null || file === undefined || file.auxdata === undefined)
		return {};
	if (file.auxdata.replace(/\s*$/, '') === '')
		return {};
	try {
		obj = JSON.parse(file.auxdata);
	} catch (e) {
		return null;
	}
	if (obj === null || typeof obj != 'object')
		return null;
	return obj;
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
	if (rec.v !== VERSION)
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
	var fb = open_base(code);
	if (fb === null)
		return null;
	try {
		var aux = parse_aux(fb.get(file_getname(path), FileBase.DETAIL.AUXDATA));
		if (aux === null || !current(aux[AUXKEY], path))
			return null;
		return aux[AUXKEY];
	} finally {
		fb.close();
	}
}

// Store a record for a path.  Returns false when the file lies outside every
// file area, which is not an error: there is nowhere to persist it.
function put(path, rec)
{
	var code = area(path);
	if (code === undefined)
		return false;
	var fb = open_base(code);
	if (fb === null)
		return false;
	try {
		var name = file_getname(path);
		var file = fb.get(name, FileBase.DETAIL.AUXDATA);
		if (file === null)
			return false;
		var aux = parse_aux(file);
		if (aux === null) {
			// Auxdata this library did not write: leave someone else's
			// metadata alone rather than replacing it with a listing.
			log(LOG_DEBUG, "filecontents: unrecognized auxdata left alone: " + name);
			return false;
		}
		aux[AUXKEY] = rec;
		var props = { auxdata: JSON.stringify(aux) };
		// Hand the description and the extended description back rather than
		// trusting the running build with either.  An older FileBase.update()
		// wrote NULL over any text the file object didn't carry, and failed
		// outright (SMB_ERR_NOT_FOUND) on an object holding nothing but
		// auxdata.  Naming both costs nothing on a build that needs neither.
		if (file.desc !== undefined)
			props.desc = file.desc;
		if (file.extdesc !== undefined)
			props.extdesc = file.extdesc;
		return fb.update(name, props) === true;
	} finally {
		fb.close();
	}
}

// Enumerate an archive with libarchive, falling back to an external tool for
// formats it does not support (notably ARC, which it cannot read at all).
function extract(path)
{
	var rec = {
		v: VERSION,
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
		// Same identity string the failure path records, so a listing can be
		// traced to the build that produced it: lsar returns a partial listing
		// for a truncated archive rather than failing, and builds differ on
		// where they draw that line.  libarchive exposes no version to JS, so
		// the native path stays unqualified.
		rec.x = external();
	}

	rec.l = [];
	for (i = 0; i < items.length; i++)
		if (items[i].type == 'file')
			rec.l.push([items[i].name, items[i].size, items[i].time]);
	return rec;
}

// system.popen() captures the tool's stdout and nothing else: its complaints
// about an archive it can't read go to stderr and stay out of the JSON, and
// the command never gets a console window of its own on Windows.
function extract_external(path)
{
	if (UNSAFE.test(path) || external() === "")
		return null;

	var text = system.popen("lsar -j \"" + path + "\"").join("");
	var items = null;

	if (text.length > 0) {
		try {
			var obj = JSON.parse(text);
			if (obj !== null && obj.lsarContents !== undefined) {
				// An archive it recognizes but cannot read yields an empty
				// listing and a non-zero lsarError -- a self-extracting .exe
				// whose stub offsets it won't follow, say.  Storing that as an
				// empty archive would misreport it; let the caller record the
				// failure instead.
				if (obj.lsarContents.length === 0 && obj.lsarError)
					return null;
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
