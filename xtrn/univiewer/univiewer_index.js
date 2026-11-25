/* univiewer_index.js - build the file name search index for the Universal Viewer library
 *
 * Run it from jsexec when the files in the libraries have changed, for example from a Synchronet timed event
 * (once a night is plenty; install-xtrn.js sets up such an event):
 *
 *	jsexec /sbbs/xtrn/univiewer/univiewer_index.js [settings.ini ...] [-q] [-full]
 *
 * With no file named it indexes the libraries in settings.ini.  A settings file that menu entries use through
 * ?univiewerlibrary.js music.ini needs its own run (music.ini), and gets its own index.  -q stops it printing what
 * it did.  -full makes it read every folder again (see below).
 *
 * What goes in the index is what a user would see in the library's lists: the same include, exclude, subdirs and
 * file types, the .desc descriptions, and the files of the sub-directories if subdirs is true.  Libraries that are
 * online (module= or url=) are not indexed, and neither are libraries with noindex = true in their section.
 * noindex = true at the top of the settings file (before any section) stops the indexer indexing anything for that
 * file, and removes the index files it made before.  Who may search a library is decided by its ars= when somebody
 * searches.
 *
 * The index is a plain text file for each library in the .index directory of the univiewer directory (see
 * univiewer_search_lib.js).  A new one is written under another name and renamed over the old one when it is
 * complete, so a library can go on searching the old one while a new one is being made, and whatever a run that was
 * stopped part way left behind is deleted at the start of the next run and at the end of every one.
 *
 * Reading the folders is nearly all the time it takes (minutes for a big library on a network disk), so after the
 * first run only the folders that have changed are read again.  A folder has changed if its modification time or
 * that of its .desc file is not what it was last time; it is also read again if it was changed within ten minutes
 * before the last run began, because a change made while that run was reading might not show in its time (the clock
 * of a file server can be some seconds out, and a time may be exact only to two seconds).  A folder's time
 * changes when a file or folder is added, removed or renamed in it, but not when a file's contents are changed, so
 * the sizes and dates of files that were only edited stay as they were until -full is used (see -full)
 * keeps them right).  Everything is read again, too, when the library's settings that decide what is listed
 * (dir, include, exclude, subdirs) have changed.
 */
"use strict";

var UNIVIEWER_INDEXING = true;		/* tells univiewerlibrary.js not to start the menu */
var index_args = argv.slice();
var index_quiet = false;
var index_full = false;
var index_settings = [];
for (var ai = 0; ai < index_args.length; ai++) {
	if (index_args[ai] == "-q")
		index_quiet = true;
	else if (index_args[ai] == "-full")
		index_full = true;
	else
		index_settings.push(index_args[ai]);
}
if (!index_settings.length)
	index_settings.push("settings.ini");

load("univiewerlibrary.js");
var IndexLib = load({}, BASE_DIR + "univiewer_search_lib.js");

var INDEX_MAX_DEPTH = 32;
var INDEX_MARGIN = 600;		/* seconds; see the comment at the top */
var INDEX_FORMAT = "2";		/* changes when what is written for a folder changes, so every sidecar made before is ignored */

function index_say(text)
{
	if (!index_quiet)
		print(text);
}

/* What decides which files a library lists: if it changes, every folder has to be read again */
function index_signature(lib)
{
	var o = lib.options || {};
	return md5_calc([INDEX_FORMAT, lib.dir, o.include || "", o.exclude || "", o.hide || "", lib.subdirs ? "subdirs" : ""].join("|"), true);
}

/* Index one library.  Returns { files, folders, scanned } */
function index_library(ini, lib)
{
	var path = IndexLib.idx_path(ini, lib.name);
	var signature = index_signature(lib);
	var scan_started = time();
	/* what the last run recorded, if it can be trusted: the index file is the one it made, and the library's
	 * settings haven't changed */
	var old = null;
	var oldfile = null;
	if (!index_full) {
		old = IndexLib.read_dirs(ini, lib.name);
		var oh = IndexLib.header(ini, lib.name);
		if (old && (!oh || String(oh.built) != old.stamp || file_size(path) != old.size || old.signature != signature))
			old = null;
		if (old) {
			oldfile = new File(path);
			if (!oldfile.open("rb")) {
				old = null;
				oldfile = null;
			}
		}
	}
	var writer = IndexLib.open_writer(ini, lib.name);
	var recorded = [];		/* the folders as they are now, for the next run */
	var folders = 0;
	var scanned = 0;
	var files = 0;
	/* The text a folder had in the old index file, or null if it isn't what the record says */
	var old_block = function (rec) {
		if (rec.len < 1 || rec.count < 1)
			return "";
		oldfile.position = rec.off;
		var text = oldfile.read(rec.len);
		if (!text || text.length != rec.len || text.charAt(text.length - 1) != "\n" || text.split("\n").length - 1 != rec.count)
			return null;
		return text;
	};
	var walk = function (subpath) {
		var folder = subpath.join("/");
		var dir = lib.dir + (folder.length ? folder + "/" : "");
		var m = file_date(dir);
		var rec = old && old.dirs[folder];
		/* A folder that had no .desc file and has the time it had can't have one now (making one changes the folder's
		 * time), so its .desc isn't looked for: over a network that is one request saved for every folder. */
		var d = rec && rec.m == m && rec.d <= 0 && m < old.scan - INDEX_MARGIN ? rec.d : file_date(dir + ".desc");
		/* unchanged: the same times as last time, and not changed so close to when that run began that a change made
		 * while it was reading might have the same time */
		var same = !!rec && rec.m == m && rec.d == d && m < old.scan - INDEX_MARGIN;
		var entries = null;
		var children;
		folders++;
		if (same)
			children = rec.children;
		else {
			scanned++;
			entries = list_entries(lib, subpath);
			children = [];
			for (var k = 0; k < entries.length; k++) {
				if (entries[k].isdir && !entries[k].up)
					children.push(entries[k].name);
			}
		}
		if (lib.subdirs && subpath.length < INDEX_MAX_DEPTH) {
			for (var c = 0; c < children.length; c++)
				walk(subpath.concat([children[c]]));
		}
		var off = writer.offset();
		var count = 0;
		var block = same ? old_block(rec) : null;
		if (block !== null) {
			writer.add_block(block, rec.count);
			count = rec.count;
		}
		else {
			if (entries === null) {		/* the old text for this folder wasn't usable: read the folder after all */
				scanned++;
				entries = list_entries(lib, subpath);
			}
			for (var e = 0; e < entries.length; e++) {
				if (entries[e].up || entries[e].isdir)
					continue;
				writer.add(IndexLib.file_line(entries[e].name, entries[e].desc || "", folder, entries[e].size, entries[e].date));
				count++;
			}
		}
		files += count;
		/* a folder whose name has a control character in it can't be put in the record, so it is always read again */
		var plain = !/[\x00-\x1f]/.test(folder) && children.every(function (n) { return !/[\x00-\x1f]/.test(n); });
		if (plain)
			recorded.push({ folder: folder, m: m, d: d, off: off, len: writer.offset() - off, count: count, children: children });
	};
	try {
		walk([]);
		var size = writer.close();
		IndexLib.write_dirs(ini, lib.name, writer.stamp, size, scan_started, signature, recorded);
		return { files: files, folders: folders, scanned: scanned, bytes: size };
	}
	catch (e) {
		writer.abort();
		throw e;
	}
	finally {
		if (oldfile)
			oldfile.close();
	}
}

/* Index one settings file.  Returns the number of files, or -1 if it could not be done. */
function index_build(ini)
{
	argv.length = 0;		/* the library finds its settings file in argv[0] */
	argv.push(ini);
	var settings = read_settings();
	if (!settings) {
		log(LOG_ERR, "univiewer_index: unable to read " + ini);
		print("univiewer_index: unable to read " + settings_path());
		return -1;
	}
	mkpath(BASE_DIR + ".index/");
	if (NOINDEX_ALL) {		/* noindex=true in the root of the settings file: no index at all */
		var removed = IndexLib.remove_except(ini, []);
		index_say(ini + ": noindex is true; nothing indexed" + (removed ? " (" + removed + " old index file" + (removed == 1 ? "" : "s") + " removed)" : ""));
		return 0;
	}
	var lock = BASE_DIR + ".index/" + file_getname(settings_path()).replace(/[^A-Za-z0-9_.-]/g, "_") + ".lock";
	if (file_exists(lock) && time() - file_date(lock) < 6 * 3600) {
		print("univiewer_index: another run for " + ini + " started less than six hours ago (" + file_getname(lock) + "); not starting");
		return -1;
	}
	var lf = new File(lock);
	if (lf.open("w")) {
		lf.writeln(String(time()));
		lf.close();
	}
	var total = 0;
	var names = [];
	try {
		IndexLib.clean_tmp(ini);		/* what an earlier run that was stopped left */
		for (var i = 0; i < settings.libraries.length; i++) {
			var lib = settings.libraries[i];
			if (!lib.dir || lib.module || lib.web)
				continue;
			if (lib.noindex) {
				index_say(ini + ": " + lib.name + ": noindex is true; not indexed");
				continue;
			}
			if (!file_isdir(lib.dir)) {
				print("univiewer_index: " + lib.name + ": " + lib.dir + " is not a directory; skipped");
				continue;
			}
			var t0 = system.timer;
			var r = index_library(ini, lib);
			names.push(lib.name);
			total += r.files;
			index_say(ini + ": " + lib.name + ": " + r.files + " files, " + Math.round(r.bytes / 1024) + " KB; " + r.scanned + " of " +
				r.folders + " folders read, in " + (system.timer - t0).toFixed(1) + " s");
		}
		var gone = IndexLib.remove_except(ini, names);
		if (gone)
			index_say(ini + ": removed " + gone + " index file" + (gone == 1 ? "" : "s") + " of libraries no longer indexed");
		index_say(ini + ": " + total + " files indexed");
		return total;
	}
	catch (e) {
		log(LOG_ERR, "univiewer_index: " + ini + ": " + e);
		print("univiewer_index: " + ini + ": " + e);
		return -1;
	}
	finally {
		IndexLib.clean_tmp(ini);		/* nothing is left behind, whatever happened */
		file_remove(lock);
	}
}

var index_failed = false;
for (var si = 0; si < index_settings.length; si++) {
	if (index_build(index_settings[si]) < 0)
		index_failed = true;
}
if (index_failed)
	exit(1);
