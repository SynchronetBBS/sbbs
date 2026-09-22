// Store the content listings of the archives in one or more file areas, so a
// viewer can serve them without opening the archive.
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

var contents = load({}, "filecontents_lib.js");

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

	for (var i = 0; i < names.length; i++) {
		if (opt.max && totals.extracted >= opt.max)
			break;
		if (!contents.is_archive(names[i])) {
			totals.skipped++;
			continue;
		}
		var path = d.path + names[i];
		if (!file_exists(path)) {
			totals.missing++;
			continue;
		}
		if (!opt.force && contents.get(path) !== null) {
			totals.current++;
			continue;
		}
		var rec = contents.extract(path);
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
		if (!opt.dry && !contents.put(path, rec))
			print("!error storing the listing for " + names[i]);
		if (opt.delay)
			sleep(opt.delay);
	}
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
