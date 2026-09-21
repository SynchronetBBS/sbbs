// getdata.js -- install Test Drive (1987) game data for the syncdrive door
// from a copy the sysop placed in the door directory. Downloads nothing.
//
// Accepts: the loose files in the door dir, files in a subfolder (an
// extracted game folder), or an archive (.zip etc.) in the door dir. Copies
// TDEGA.EXE, CARS.TXT, TDSND.SND, *.PES, *.BIN, *.SS, verifies with
// `syncdrive --check`, and seeds data/testdrive/SCORES from the copy's SCORES
// when there is no high-score table yet.
//
//     jsexec ../xtrn/testdrive/getdata.js
//
// SpiderMonkey 1.8.5. Copyright (C) 2026 Rob Swindell. GPL-2.0.

var NAMED = ["tdega.exe", "cars.txt", "tdsnd.snd", "scores"];
var EXTS  = [".pes", ".bin", ".ss"];
var REQUIRED = ["tdega.exe", "cars.txt", "tdsnd.snd"];
var ARCHIVE_EXT = [".zip", ".7z", ".rar", ".arj", ".lzh", ".tar", ".tgz", ".tar.gz"];

function door_dir() { return backslash(js.exec_dir || js.startup_dir || "./"); }
function base_lc(p) { return String(file_getname(p)).toLowerCase(); }

function wanted(name)
{
	var lc = String(name).toLowerCase(), i;
	for (i = 0; i < NAMED.length; i++)
		if (lc == NAMED[i])
			return true;
	for (i = 0; i < EXTS.length; i++)
		if (lc.length > EXTS[i].length && lc.substr(lc.length - EXTS[i].length) == EXTS[i])
			return true;
	return false;
}

function is_archive(name)
{
	var lc = String(name).toLowerCase(), i;
	for (i = 0; i < ARCHIVE_EXT.length; i++)
		if (lc.length > ARCHIVE_EXT[i].length
		    && lc.substr(lc.length - ARCHIVE_EXT[i].length) == ARCHIVE_EXT[i])
			return true;
	return false;
}

function present(dir)
{
	var have = {}, all = directory(dir + "*"), i;
	for (i = 0; i < all.length; i++)
		if (all[i].charAt(all[i].length - 1) != "/" && wanted(file_getname(all[i])))
			have[base_lc(all[i])] = true;
	return have;
}

function copy_from_tree(dir, sub, depth, have)
{
	var all, i, e, b;
	if (depth < 0)
		return;
	all = directory(sub + "*");
	for (i = 0; i < all.length; i++) {
		e = all[i];
		if (e.charAt(e.length - 1) == "/") {
			copy_from_tree(dir, e, depth - 1, have);
			continue;
		}
		b = file_getname(e);
		if (!wanted(b) || have[b.toLowerCase()])
			continue;
		if (file_copy(e, dir + b.toUpperCase())) {
			have[b.toLowerCase()] = true;
			print("  copied " + b.toUpperCase());
		}
	}
}

function extract_archive(path, dir, have)
{
	var ar, list, i, name, b;
	try { ar = new Archive(path); list = ar.list(false); }
	catch (e) { return; }
	for (i = 0; i < list.length; i++) {
		name = list[i].name;
		if (!name || list[i].type == "directory")
			continue;
		b = file_getname(name);
		if (!wanted(b) || have[b.toLowerCase()])
			continue;
		try {
			ar.extract(dir, false, true, 0, name);
			if (file_getname(name) != b.toUpperCase() && file_exists(dir + b))
				file_rename(dir + b, dir + b.toUpperCase());
			have[b.toLowerCase()] = true;
			print("  extracted " + b.toUpperCase() + " (from " + file_getname(path) + ")");
		} catch (e2) {
			print("  ! failed to extract " + name + ": " + e2);
		}
	}
}

function main()
{
	var dir = door_dir(), have, top, i, missing = [], exe, rc, scores_dir, pes = 0, k;

	print("Test Drive (syncdrive) game-data installer");
	print("  door directory: " + dir);
	have = present(dir);
	top = directory(dir + "*");
	for (i = 0; i < top.length; i++) {
		if (top[i].charAt(top[i].length - 1) == "/")
			copy_from_tree(dir, top[i], 3, have);
		else if (is_archive(file_getname(top[i])))
			extract_archive(top[i], dir, have);
	}
	have = present(dir);
	for (i = 0; i < REQUIRED.length; i++)
		if (!have[REQUIRED[i]])
			missing.push(REQUIRED[i].toUpperCase());
	for (k in have)
		if (/\.pes$/.test(k))
			pes++;
	if (missing.length || pes == 0) {
		print("");
		print("No complete Test Drive (1987, DOS) copy found"
		    + (missing.length ? " (missing: " + missing.join(" ") + ")" : " (no .PES files)") + ".");
		print("Test Drive is commercial content and is NOT shipped with this door.");
		print("Put your copy (the zip, the loose files, or the game folder) in:");
		print("    " + dir);
		print("and re-run:  jsexec ../xtrn/testdrive/getdata.js");
		return 1;
	}

	exe = dir + (system.platform.toLowerCase() == "win32" ? "syncdrive.exe" : "syncdrive");
	if (file_exists(exe)) {
		// system.exec() runs this through the shell (system(3)), so a path
		// containing a space must be quoted or the shell splits it into
		// separate, nonexistent arguments. A double quote in the path can't
		// be safely embedded in a double-quoted argument, so refuse rather
		// than build a command line that runs something other than intended.
		if (exe.indexOf("\"") >= 0 || dir.indexOf("\"") >= 0) {
			print("  ! cannot verify: the door path contains a \" (double-quote)");
			print("  ! character, which cannot be safely passed to the --check command.");
			print("  ! Move the door to a path with no \" in it, or verify by hand:");
			print("      " + exe + " --check --game-dir=" + dir);
			return 1;
		}
		rc = system.exec("\"" + exe + "\" --check \"--game-dir=" + dir + "\"");
		if (rc != 0) {
			print("TDEGA.EXE is not the expected EGA build of Test Drive (1987); the door");
			print("cannot run it. Use the original DOS release's TDEGA.EXE.");
			return 1;
		}
	} else {
		print("  note: the syncdrive binary is not here yet (build, then run");
		print("  jsexec src/doors/syncdrive/deploy.js); skipping the TDEGA.EXE check.");
	}

	scores_dir = backslash(system.data_dir + "testdrive");
	if (!file_exists(scores_dir + "SCORES") && file_exists(dir + "SCORES")) {
		mkpath(scores_dir);
		if (file_copy(dir + "SCORES", scores_dir + "SCORES"))
			print("  seeded the high-score table: " + scores_dir + "SCORES");
	}
	print("Success: Test Drive game data installed.");
	return 0;
}

if (typeof SYNCDRIVE_GETDATA_NO_MAIN == "undefined")
	exit(main());
