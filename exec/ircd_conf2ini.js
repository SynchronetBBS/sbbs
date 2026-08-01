/*

 ircd_conf2ini.js

 Converts a legacy Bahamut-style ircd.conf into the ircd.ini format used by
 the Synchronet IRCd and the ircdcfg.js editor.  The original .conf is kept
 as a backup.  Run from the command line:

     jsexec ircd_conf2ini.js [-options] [input-file [output-file]]

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Copyright 2026 Rob Swindell <rob@synchro.net>

*/

"use strict";

load("sbbsdefs.js");

/* Read_Config_File() does runtime work - scheduling outbound connects,
   scanning online clients against new bans - that a converter must not
   trigger.  This is the same flag ircdcfg.js sets to suppress it. */
var IRCDCFG_Editor = true;
var Time_Config_Read = 0;
var Config_Filename = "";

load("ircd/config.js");

var opt_dryrun = false;
var opt_keep = false;
var opt_force = false;

function usage() {
	print("Synchronet IRCd ircd.conf to ircd.ini converter");
	print("");
	print("usage: jsexec ircd_conf2ini.js [-options] [input-file [output-file]]");
	print("");
	print("If no input-file is given, the first of these found in ctrl/ is used:");
	print("    ircd." + system.local_host_name + ".conf");
	print("    ircd." + system.host_name + ".conf");
	print("    ircd.conf");
	print("");
	print("If no output-file is given, the input filename with a .ini extension");
	print("is used.  A filename with no path is relative to the ctrl directory.");
	print("");
	print("options:");
	print("    -n   dry run: report what would be converted, write nothing");
	print("    -k   keep the original .conf in place (default: rename to .bak)");
	print("    -f   overwrite an existing output file or backup file");
	print("    -?   this help");
	exit(1);
}

function ctrl_path(fn) {
	if (fn.indexOf('/') >= 0 || fn.indexOf('\\') >= 0)
		return fn;
	return system.ctrl_dir + fn;
}

function fail(str) {
	print("!ERROR " + str);
	exit(1);
}

/* I:Lines and O:Lines are matched against "user@hostname", so a mask with no
   @ in it cannot match anything - such an entry is already inert in the .conf
   and its absence from the .ini is not a conversion loss. */
function live_mask(mask) {
	return mask.indexOf("@") >= 0;
}

/* Everything the .ini format is able to represent, as a comparable set of
   strings.  Taken before and after the round-trip to prove nothing was lost
   in translation. */
function config_signature() {
	var sig = {};
	var i;

	sig.Info = [ServerName, ServerDesc, Admin1, Admin2, Admin3].join("|");
	sig.Capab = CapabEnforce ? "true" : "false";

	sig.Port = [String(Default_Port)];
	for (i in PLines)
		sig.Port.push(String(PLines[i]));

	sig.Class = [];
	for (i in YLines) {
		if (i == 0) /* hard-coded fallback class, not from the config file */
			continue;
		sig.Class.push(format("%s %s %s %s %s %s", i, YLines[i].pingfreq,
			YLines[i].connfreq, YLines[i].maxlinks, YLines[i].sendq,
			YLines[i].comment ? YLines[i].comment : "-"));
	}

	sig.Allow = [];
	for (i in ILines) {
		if (!live_mask(ILines[i].hostmask))
			continue;
		sig.Allow.push(format("%s %s %s %s", ILines[i].hostmask.toLowerCase(),
			ILines[i].password ? ILines[i].password : "-",
			ILines[i].port ? ILines[i].port : 0, ILines[i].ircclass));
	}

	sig.Operator = [];
	for (i in OLines) {
		if (!live_mask(OLines[i].hostmask))
			continue;
		sig.Operator.push(format("%s %s %s %s %s", OLines[i].nick.toLowerCase(),
			OLines[i].hostmask.toLowerCase(), OLines[i].password,
			OLines[i].flags, OLines[i].ircclass));
	}

	sig.Services = [];
	for (i in ULines)
		sig.Services.push(ULines[i].toLowerCase());

	sig.Ban = [];
	for (i in KLines)
		sig.Ban.push(KLines[i].hostmask.toLowerCase());
	for (i in ZLines)
		sig.Ban.push("*@" + ZLines[i].ipmask.toLowerCase());

	sig.Restrict = [];
	for (i in QLines)
		sig.Restrict.push(QLines[i].nick.toLowerCase());

	sig.Server = [];
	for (i in CLines) {
		sig.Server.push(format("out %s %s %s %s %s",
			CLines[i].servername.toLowerCase(), CLines[i].host,
			CLines[i].password, CLines[i].port ? CLines[i].port : 0,
			CLines[i].ircclass));
	}
	for (i in NLines) {
		sig.Server.push(format("in %s %s %s %s",
			NLines[i].servername.toLowerCase(), NLines[i].password,
			NLines[i].flags, NLines[i].ircclass));
	}

	sig.Hub = [];
	for (i in HLines)
		sig.Hub.push(HLines[i].servername.toLowerCase());

	sig.RBL = [];
	for (i in RBL)
		sig.RBL.push(format("%s %s %s", RBL[i].hostname, RBL[i].good, RBL[i].bad));

	sig.WebIRC = [];
	for (i in WLines)
		sig.WebIRC.push(format("%s %s", WLines[i].hostname, WLines[i].password));

	return sig;
}

/* Duplicate entries in a config file carry no meaning, so compare as sets. */
function unique(list) {
	var seen = {};
	var out = [];
	var i;

	for (i in list) {
		if (seen[list[i]])
			continue;
		seen[list[i]] = true;
		out.push(list[i]);
	}
	return out.sort();
}

function compare_signatures(before, after) {
	var lost = 0;
	var i, j, a, b;

	for (i in before) {
		if (typeof before[i] === "string") {
			if (before[i] != after[i]) {
				print(format("  ! [%s] changed:", i));
				print("      before: " + before[i]);
				print("      after:  " + after[i]);
				lost++;
			}
			continue;
		}
		b = unique(before[i]);
		a = unique(after[i]);
		print(format("  %-10s %3u entr%s", i, b.length,
			b.length == 1 ? "y" : "ies"));
		for (j in b) {
			if (a.indexOf(b[j]) < 0) {
				print("      ! not represented in the .ini: " + b[j]);
				lost++;
			}
		}
	}
	return lost;
}

/* The .conf format has no field for a class description, but the stock file
   documents each Y:Line with a comment on the line above it.  [Class] does
   have a Comment key, so carry those across rather than dropping them. */
function harvest_class_comments(fn) {
	var f = new File(fn);
	var comment = "";
	var line, ircclass;

	if (!f.open("r"))
		return;
	while (!f.eof) {
		line = f.readln();
		if (line === null)
			break;
		line = line.replace(/^\s+|\s+$/g, "");
		if (line.charAt(0) == '#' || line.charAt(0) == ';') {
			comment = line.slice(1).replace(/^\s+|\s+$/g, "");
			continue;
		}
		if (line.charAt(0).toUpperCase() == 'Y' && line.charAt(1) == ':') {
			ircclass = parseInt(line.split(":")[1]);
			/* Drop a leading "Class 30:" - the section name says that */
			comment = comment.replace(/^Class\s+\d+\s*[:-]\s*/i, "");
			if (comment && YLines[ircclass])
				YLines[ircclass].comment = comment;
		}
		comment = "";
	}
	f.close();
}

/* Anything the .conf could express that the .ini deliberately cannot. */
function collect_notes() {
	var notes = [];
	var i;

	function warn(str) {
		notes.push(str);
	}

	if (Die_Password || Restart_Password) {
		warn("X:Line /DIE and /RESTART passwords are not supported by the "
			+ "IRCd and were dropped.  Control who may use those commands "
			+ "with the R and D flags on an [Operator] section instead.");
	}
	if (ZLines.length) {
		warn(format("%u Z:Line(s) became [Ban] sections matching *@<address>. "
			+ "A [Ban] is checked after the client registers, not before, "
			+ "and matches the resolved hostname - so it only stops a "
			+ "connection whose address fails to resolve.", ZLines.length));
	}
	for (i in OLines) {
		if (!live_mask(OLines[i].hostmask)) {
			warn(format("O:Line for oper '%s' has the mask '%s', which is "
				+ "missing the '@' that separates the username from the "
				+ "hostname.  It could never match a client and was dropped; "
				+ "add an [Operator] section with Mask=*@%s if it was meant "
				+ "to work.", OLines[i].nick, OLines[i].hostmask,
				OLines[i].hostmask));
		}
	}
	for (i in ILines) {
		if (!live_mask(ILines[i].hostmask)) {
			warn(format("I:Line mask '%s' is missing the '@' that separates "
				+ "the username from the hostname.  It could never match a "
				+ "client and was dropped.", ILines[i].hostmask));
			continue;
		}
		if (ILines[i].ipmask != ILines[i].hostmask) {
			warn(format("I:Line '%s' used separate IP and hostname masks; an "
				+ "[Allow] section has a single Mask that is matched against "
				+ "both.  The hostname mask '%s' was kept.",
				ILines[i].ipmask, ILines[i].hostmask));
		}
	}
	for (i in HLines) {
		if (HLines[i].allowedmask && HLines[i].allowedmask != "*") {
			warn(format("H:Line for '%s' restricted which servers may use it "
				+ "as a hub ('%s'); [Hub] sections apply to all servers.",
				HLines[i].servername, HLines[i].allowedmask));
		}
	}
	return notes;
}

function print_notes(notes) {
	var i;

	if (!notes.length)
		return;
	print("Notes:");
	for (i in notes)
		print("  * " + notes[i]);
}

var args = [];
for (var argn = 0; argn < argc; argn++) {
	if (argv[argn].charAt(0) != '-') {
		args.push(argv[argn]);
		continue;
	}
	switch (argv[argn].toLowerCase()) {
		case "-n":
			opt_dryrun = true;
			break;
		case "-k":
			opt_keep = true;
			break;
		case "-f":
			opt_force = true;
			break;
		default:
			usage();
	}
}

var infile;
if (args.length > 0) {
	infile = ctrl_path(args[0]);
} else {
	var candidates = [
		"ircd." + system.local_host_name + ".conf",
		"ircd." + system.host_name + ".conf",
		"ircd.conf"
	];
	for (var c in candidates) {
		if (file_exists(system.ctrl_dir + candidates[c])) {
			infile = system.ctrl_dir + candidates[c];
			break;
		}
	}
	if (!infile)
		fail("No ircd.conf found in " + system.ctrl_dir);
}

if (!file_exists(infile))
	fail(infile + " does not exist.");
if (infile.match(/[.][Ii][Nn][Ii]$/))
	fail(infile + " is already in .ini format.");

var outfile;
if (args.length > 1)
	outfile = ctrl_path(args[1]);
else if (infile.match(/[.][Cc][Oo][Nn][Ff]$/))
	outfile = infile.replace(/[.][Cc][Oo][Nn][Ff]$/, ".ini");
else
	outfile = infile + ".ini";

/* The IRCd picks its parser from the filename extension, so an output file
   not named .ini would be read back as legacy .conf format. */
if (!outfile.match(/[.][Ii][Nn][Ii]$/))
	fail("The output filename must end in .ini: " + outfile);

if (file_exists(outfile) && !opt_force && !opt_dryrun)
	fail(outfile + " already exists.  Use -f to overwrite it.");

var backup = infile + ".bak";
if (!opt_keep && !opt_dryrun && file_exists(backup) && !opt_force)
	fail(backup + " already exists.  Use -f to overwrite it, or -k to leave "
		+ infile + " in place.");

print("Reading " + infile);
Config_Filename = infile;
Read_Config_File();
harvest_class_comments(infile);
var before = config_signature();
var notes = collect_notes();

if (opt_dryrun) {
	print("Would write " + outfile);
	print("");
	compare_signatures(before, before);
	print("");
	print_notes(notes);
	print("");
	print("Dry run - nothing written.");
	exit(0);
}

if (!Write_Config_File(outfile))
	fail("Failed to write " + outfile);
print("Wrote " + outfile);

/* Read the file we just wrote back in and prove it says the same thing. */
Config_Filename = outfile;
Read_Config_File();
var after = config_signature();

print("");
print("Verifying " + outfile + ":");
var lost = compare_signatures(before, after);
print("");
print_notes(notes);
print("");

if (lost) {
	print(format("!WARNING %u item(s) did not survive the conversion.", lost));
	print("The original " + infile + " has been left in place.  Review "
		+ outfile + " before removing it.");
	exit(1);
}

print("Conversion verified.");

if (opt_keep) {
	print("Original left in place: " + infile);
} else {
	if (file_exists(backup) && !file_remove(backup))
		fail("Failed to remove " + backup);
	if (!file_rename(infile, backup))
		fail("Failed to rename " + infile + " to " + backup);
	print("Original saved as " + backup);
}

print("");
print("Restart or rehash the IRCd to pick up the new configuration.");
print("Edit it from here on with: jsexec ircdcfg.js");
