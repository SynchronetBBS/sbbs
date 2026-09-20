// Populate the per-file-area content store (data/dirs/<code>.contents) by
// enumerating the archives in one or more file areas.
//
// Nothing has to be pre-populated: viewers extract and store on a miss.  This
// exists to warm cold areas ahead of demand, and to re-examine stored failures
// after the extractor set changes.
//
// Usage: jsexec inventory_archives.js [options] [area-code ...]
//
//   (no area codes)  every file area
//   -force           re-extract even when the stored record is current
//   -delay <ms>      pause between extractions, to spread the load
//   -max <n>         stop after this many extractions
//   -dry             report what would be done, write nothing
//   -quiet           totals only

"use strict";

load("filecontents_lib.js");

// Merge accumulated records into the store this often.  Extraction runs
// unlocked and only the merge takes the area lock, so a long run does not hold
// viewers off an area while it works.
var FLUSH_EVERY = 50;

var opt = {
	force: false,
	dry: false,
	quiet: false,
	delay: 0,
	max: 0
};
var codes = [];
var totals = {
	areas: 0, skipped: 0, missing: 0, current: 0,
	extracted: 0, stored: 0, failed: 0, entries: 0
};

function usage()
{
	print("usage: jsexec inventory_archives.js [options] [area-code ...]");
	print("  -force      re-extract even when the stored record is current");
	print("  -delay <ms> pause between extractions");
	print("  -max <n>    stop after this many extractions");
	print("  -dry        report what would be done, write nothing");
	print("  -quiet      totals only");
	exit(1);
}

for (var a = 0; a < argv.length; a++) {
	switch (argv[a]) {
		case '-force':
			opt.force = true;
			break;
		case '-dry':
			opt.dry = true;
			break;
		case '-quiet':
			opt.quiet = true;
			break;
		case '-delay':
			opt.delay = parseInt(argv[++a], 10) || 0;
			break;
		case '-max':
			opt.max = parseInt(argv[++a], 10) || 0;
			break;
		case '-?':
		case '-h':
		case '-help':
			usage();
			break;
		default:
			if (argv[a].charAt(0) == '-')
				usage();
			codes.push(argv[a]);
			break;
	}
}

// Merge what has been extracted so far into the area's store.
function flush(code, pending, count)
{
	if (opt.dry || !count)
		return true;
	if (!filecontents_lock(code)) {
		print("!timeout locking the store for " + code);
		return false;
	}
	var store = filecontents_read(code);
	for (var name in pending)
		store.files[name] = pending[name];
	var ok = filecontents_write(code, store);
	filecontents_unlock(code);
	if (!ok)
		print("!error writing the store for " + code);
	return ok;
}

function inventory_area(code)
{
	var d = file_area.dir[code];

	if (d === undefined) {
		print("!no such file area: " + code);
		return;
	}
	var fb = new FileBase(code);
	if (!fb.open()) {
		print("!" + code + ": " + fb.error);
		return;
	}
	var names = fb.get_names();
	fb.close();

	totals.areas++;
	var store = filecontents_read(code);
	var pending = {};
	var npending = 0;

	for (var i = 0; i < names.length; i++) {
		if (opt.max && totals.extracted >= opt.max)
			break;
		if (!filecontents_is_archive(names[i])) {
			totals.skipped++;
			continue;
		}
		var path = d.path + names[i];
		if (!file_exists(path)) {
			totals.missing++;
			continue;
		}
		if (!opt.force && filecontents_current(store.files[names[i]], path)) {
			totals.current++;
			continue;
		}
		var rec = filecontents_extract(path);
		totals.extracted++;
		if (rec.err !== undefined) {
			totals.failed++;
			if (!opt.quiet)
				print(format("%-24s %-12s %s", names[i], rec.x, rec.err));
		} else {
			totals.stored++;
			totals.entries += rec.l.length;
			if (!opt.quiet)
				print(format("%-24s %-12s %u entries", names[i], rec.x, rec.l.length));
		}
		pending[names[i]] = rec;
		npending++;
		if (npending >= FLUSH_EVERY) {
			flush(code, pending, npending);
			pending = {};
			npending = 0;
		}
		if (opt.delay)
			sleep(opt.delay);
	}
	flush(code, pending, npending);
}

if (!codes.length)
	for (var code in file_area.dir)
		codes.push(code);

for (var c = 0; c < codes.length && !js.terminated; c++) {
	if (opt.max && totals.extracted >= opt.max)
		break;
	inventory_area(codes[c]);
}

print("");
print(format("%u area(s): %u stored (%u entries), %u failed, %u already current"
	, totals.areas, totals.stored, totals.entries, totals.failed, totals.current));
print(format("%u non-archive file(s) skipped, %u missing from disk"
	, totals.skipped, totals.missing));
if (opt.dry)
	print("(dry run: nothing was written)");
