// domains_cfg_upgrade.js: conversion of ctrl/domains.cfg into [domain:]
// sections of msgs.ini (as update.js does), in a scratch directory below
// system.temp_dir standing in for the ctrl directory

load(js.exec_dir + "../../load/domains_cfg_upgrade.js");

var failures = [];
var dir = system.temp_dir + format("inet_domains_test_%u_%u/", time(), random(1000000));

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function write_file(name, lines)
{
	var f = new File(dir + name);
	if (!f.open("w"))
		throw new Error("Error " + f.error + " creating " + f.name);
	f.write(lines.join("\r\n") + "\r\n");
	f.close();
}

function write_text(name, text)
{
	var f = new File(dir + name);
	if (!f.open("w"))
		throw new Error("Error " + f.error + " creating " + f.name);
	f.write(text);
	f.close();
}

function read_text(name)
{
	var f = new File(dir + name);
	if (!f.open("r"))
		return null;
	var text = f.read();
	f.close();
	return text;
}

// [ name, send_from, send_ars ] of each [domain:] section, in file order
function domain_sections()
{
	var f = new File(dir + "msgs.ini");
	if (!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var list = f.iniGetSections("domain:").map(function (section) {
		return [section.substr(7), f.iniGetValue(section, "send_from"), f.iniGetValue(section, "send_ars", "")];
	});
	f.close();
	return list;
}

// The [Internet] domains_converted value (undefined when absent)
function converted_key()
{
	var f = new File(dir + "msgs.ini");
	if (!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var value = f.iniGetValue("Internet", "domains_converted");
	f.close();
	return value;
}

// msgs.ini without its domains_converted line, with LF line endings (the
// ini library that sets the key writes them as SCFG does: LF on Unix)
function text_without_key()
{
	return lf(read_text("msgs.ini")).replace(/[^\n]*domains_converted[^\n]*\n/, "");
}

function lf(text)
{
	return text.replace(/\r\n/g, "\n");
}

function cleanup()
{
	directory(dir + "*").forEach(function (path) { file_remove(path); });
	rmdir(dir);
}

try {
	if (!mkpath(dir))
		throw new Error("Error creating " + dir);

	// No domains.cfg: nothing to do
	write_file("msgs.ini", ["settings=0", "", "[Internet]", "addr=bbs.example"]);
	var before = read_text("msgs.ini");
	check("no file: status", upgrade_domains_cfg(dir).status, "none");
	check("no file: msgs.ini unchanged", read_text("msgs.ini") === before, true);

	// A domains.cfg with no entries (as once shipped) is left alone
	write_file("domains.cfg", ["; List additional domain names", "", "   ", "; another comment"]);
	var result = upgrade_domains_cfg(dir);
	check("no entries: status", result.status, "none");
	check("no entries: no warnings", result.warnings, []);
	check("no entries: msgs.ini unchanged", read_text("msgs.ini") === before, true);
	check("no entries: domains.cfg kept", file_exists(dir + "domains.cfg"), true);
	check("no entries: not marked", converted_key(), undefined);

	// A missing msgs.ini is an error and nothing is created
	write_file("domains.cfg", ["example.com"]);
	file_remove(dir + "msgs.ini");
	result = upgrade_domains_cfg(dir);
	check("no msgs.ini: status", result.status, "error");
	check("no msgs.ini: not created", file_exists(dir + "msgs.ini"), false);
	check("no msgs.ini: domains.cfg kept", file_exists(dir + "domains.cfg"), true);
	write_file("msgs.ini", ["settings=0", "", "[Internet]", "addr=bbs.example"]);

	// A list ending with a negated entry: converted, with a warning that
	// differs when a comment line after it used to prevent its effect
	write_file("domains.cfg", ["*.example.com", "!test.example.com"]);
	result = upgrade_domains_cfg(dir, /* dry_run: */ true);
	check("trailing negation: status", result.status, "converted");
	check("trailing negation: warned", result.warnings.length == 1
		&& result.warnings[0].indexOf("ends with the negated entry !test.example.com") >= 0, true);
	write_file("domains.cfg", ["*.example.com", "!test.example.com", "", "; end of list"]);
	result = upgrade_domains_cfg(dir, /* dry_run: */ true);
	// The comment after it kept unlisted hosts unlisted: it is kept, so
	// the converted list means the same (and does not end with '!')
	check("negation then comment: not warned", result.warnings, []);
	check("negation then comment: comment kept", result.domains
		, ["*.example.com", "!test.example.com", "; end of list"]);
	write_file("domains.cfg", ["!test.example.com", "*.example.com", "; end of list"]);
	result = upgrade_domains_cfg(dir, /* dry_run: */ true);
	check("negation first: no warning", result.warnings, []);
	check("negation tests: msgs.ini unchanged", read_text("msgs.ini") === before, true);

	write_file("domains.cfg", [
		"; Additional domains",
		"",
		"example.com",
		"  indented.example\tmetadata after a tab",
		"*.wild.example",
		"!bad.example.org",
		"*.example.org",
		"trailing.example   ",
		"sub~",
		"EXAMPLE.COM",
		";commented.example",
		"odd]name",
	]);

	// Dry run: reports, writes nothing
	result = upgrade_domains_cfg(dir, /* dry_run: */ true);
	check("dry run: status", result.status, "converted");
	check("dry run: msgs.ini unchanged", read_text("msgs.ini") === before, true);
	check("dry run: domains.cfg kept", file_exists(dir + "domains.cfg"), true);
	check("dry run: not marked", converted_key(), undefined);

	var expected_names = ["example.com", "indented.example", "*.wild.example", "!bad.example.org"
		, "*.example.org", "trailing.example", "sub~", "odd]name"];
	result = upgrade_domains_cfg(dir);
	check("convert: status", result.status, "converted");
	check("convert: domains", result.domains, expected_names);
	check("convert: warnings", result.warnings, []);	// the duplicate is dropped: it cannot change any answer
	check("convert: sections", domain_sections(), expected_names.map(function (name) {
		return [name, false, ""];
	}));
	check("convert: existing text kept", text_without_key().substr(0, lf(before).length) === lf(before), true);
	check("convert: marked", converted_key(), true);
	check("convert: domains.cfg left in place", file_exists(dir + "domains.cfg"), true);
	check("convert: no domains.cfg.old", file_exists(dir + "domains.cfg.old"), false);
	var f = new File(dir + "msgs.ini");
	f.open("r");
	check("convert: other settings kept", f.iniGetValue("Internet", "addr"), "bbs.example");
	f.close();

	// The converted list matches each host as the old file did
	["example.com", "Example.COM", "indented.example", "a.wild.example", "wild.example"
		, "bad.example.org", "x.example.org", "trailing.example", "xsuby", "odd]name"
		, "commented.example", "other.example"].forEach(function (host) {
		check("match " + host, system.findstr(expected_names, host)
			, system.findstr(dir + "domains.cfg", host));
	});

	// Running again changes nothing
	var converted = read_text("msgs.ini");
	check("second run: status", upgrade_domains_cfg(dir).status, "none");
	check("second run: msgs.ini unchanged", read_text("msgs.ini") === converted, true);

	// Once marked, a changed domains.cfg is not used
	write_file("domains.cfg", ["new.example"]);
	check("marked, new file: status", upgrade_domains_cfg(dir).status, "none");
	check("marked, new file: msgs.ini unchanged", read_text("msgs.ini") === converted, true);

	// Without the key (msgs.ini converted by an earlier version): a new
	// domains.cfg is not merged into existing [domain:] sections
	var unmarked = text_without_key();
	write_text("msgs.ini", unmarked);
	result = upgrade_domains_cfg(dir);
	check("sections present: status", result.status, "skipped");
	check("sections present: warned", result.warnings.length, 1);
	check("sections present: msgs.ini unchanged", read_text("msgs.ini") === unmarked, true);
	check("sections present: domains.cfg kept", file_exists(dir + "domains.cfg"), true);

	// A domains.cfg whose entries are all already sections (as after an
	// older SCFG saved the list it read from the file): msgs.ini is marked,
	// the file is left in place; a dry run only reports it
	write_file("domains.cfg", ["; saved by SCFG", "EXAMPLE.com", "*.wild.example"]);
	result = upgrade_domains_cfg(dir, /* dry_run: */ true);
	check("all present, dry run: status", result.status, "marked");
	check("all present, dry run: msgs.ini unchanged", read_text("msgs.ini") === unmarked, true);
	result = upgrade_domains_cfg(dir);
	check("all present: status", result.status, "marked");
	check("all present: no warnings", result.warnings, []);
	check("all present: marked", converted_key(), true);
	check("all present: sections unchanged", text_without_key() === unmarked, true);
	check("all present: domains.cfg left in place", file_exists(dir + "domains.cfg"), true);
	check("all present: no domains.cfg.old", file_exists(dir + "domains.cfg.old"), false);

	// Marked with no [domain:] section (every domain deleted in SCFG): the
	// file is still not converted again
	write_text("msgs.ini", "settings=0\r\n\r\n[Internet]\r\naddr=bbs.example\r\ndomains_converted=true\r\n");
	before = read_text("msgs.ini");
	check("marked, empty list: status", upgrade_domains_cfg(dir).status, "none");
	check("marked, empty list: msgs.ini unchanged", read_text("msgs.ini") === before, true);
}
finally {
	cleanup();
}

check("scratch directory removed", file_isdir(dir), false);

if (failures.length > 0)
	throw new Error("domains.cfg upgrade test failures:\n  " + failures.join("\n  "));
