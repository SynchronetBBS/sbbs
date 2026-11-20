// The transitional fallback: while msgs.ini has no [domain:] section the
// configuration loader reads the additional domains from the old
// ctrl/domains.cfg, exactly as update.js would convert them. Each case
// runs a child jsexec against its own scratch ctrl directory (below
// system.temp_dir; new, empty user base).

load(js.exec_dir + "../../../load/domains_cfg_upgrade.js");
var Scratch = load({}, js.exec_dir + "scratch.inc");
var child = js.exec_dir + "fallback_child.inc";
var sysaddr = "sysaddr.example";
var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function run_case(name, fn)
{
	var sc = Scratch.scratch_ctrl("fb");
	try {
		sc.setup();
		fn(sc);
	} catch (e) {
		failures.push(name + ": " + e);
	} finally {
		check(name + ": scratch directory removed", sc.remove(), true);
	}
}

function check_child(name, sc, r)
{
	check(name + ": child exit code", r.exit_code, 0);
	check(name + ": child ctrl dir", r.dirs.ctrl, sc.ctrl);
	check(name + ": child system address", r.inet_addr, sysaddr);
}

var old_lines = ["; additional domains", "", "Example.COM   ", "*.wild.example"
	, "  indented.example\tmetadata after a tab", "!blocked.example.org", "*.example.org"
	, "dup.example", "DUP.EXAMPLE", "; a comment", "trailing.example \t "];
var case1_list;

// (1) Only domains.cfg, with entries: the list is what the converter
// would produce, matching agrees with the old file, nobody may send from
// these entries (send_from off) but the system address stays usable.
// (5) Then convert it: the list is the same as the fallback's.
run_case("fallback", function (sc) {
	Scratch.write_text(sc.ctrl + "domains.cfg", old_lines.join("\r\n") + "\r\n");
	var converted = upgrade_domains_cfg(sc.ctrl, /* dry_run: */ true);
	check("fallback: converter dry run", converted.status, "converted");
	var r = sc.run(child, [sc.ctrl + "domains.cfg"]);
	check_child("fallback", sc, r);
	check("fallback: list", r.list, [sysaddr].concat(converted.domains));
	check("fallback: entries", converted.domains, ["Example.COM", "*.wild.example", "indented.example"
		, "!blocked.example.org", "*.example.org", "dup.example", "trailing.example"]);
	for (var host in r.listed) {
		if (host == sysaddr)
			check("fallback: system address matches", r.listed[host], true);
		else
			check("fallback: " + host + " as with domains.cfg", r.listed[host], r.listed_file[host]);
	}
	check("fallback: wildcard host", r.listed["a.wild.example"], true);
	check("fallback: excluded host", r.listed["blocked.example.org"], false);
	check("fallback: unlisted host", r.listed["unlisted.example"], false);
	for (var domain in r.can_send) {
		check("fallback: can_send " + domain, r.can_send[domain], domain == sysaddr);
	}
	case1_list = r.list;

	// (5) converted by update.js's converter: the same list
	check("convert: status", upgrade_domains_cfg(sc.ctrl).status, "converted");
	check("convert: domains.cfg left in place", file_exists(sc.ctrl + "domains.cfg"), true);
	r = sc.run(child);
	check_child("converted", sc, r);
	check("converted: list same as the fallback's", r.list, case1_list);
});

// (6) domains.cfg still present, msgs.ini says the list was converted and
// has no [domain:] section (every domain deleted in SCFG): the list is
// empty, domains.cfg is not read; without the key it would be
run_case("converted, empty", function (sc) {
	Scratch.write_text(sc.ctrl + "domains.cfg", "old.example\r\nexample.com\r\n");
	sc.set_converted(true);
	var r = sc.run(child);
	check_child("converted, empty", sc, r);
	check("converted, empty: list", r.list, [sysaddr]);
	check("converted, empty: old.example not listed", r.listed["old.example"], false);
	check("converted, empty: example.com not listed", r.listed["example.com"], false);
	sc.set_converted(false);
	r = sc.run(child);
	check_child("key false", sc, r);
	check("key false: domains.cfg read", r.list, [sysaddr, "old.example", "example.com"]);
	sc.set_converted(undefined);
	r = sc.run(child);
	check_child("no key", sc, r);
	check("no key: domains.cfg read", r.list, [sysaddr, "old.example", "example.com"]);
});

// (2) Only [domain:] sections: as before
run_case("sections", function (sc) {
	sc.add_sections([["sect.example", true, ""], ["*.wild.example", false, ""]]);
	var r = sc.run(child);
	check_child("sections", sc, r);
	check("sections: list", r.list, [sysaddr, "sect.example", "*.wild.example"]);
	check("sections: sect.example listed", r.listed["sect.example"], true);
	check("sections: can_send sect.example", r.can_send["sect.example"], true);
	check("sections: can_send system address", r.can_send[sysaddr], true);
});

// (3) Both: the sections win, domains.cfg is ignored
run_case("both", function (sc) {
	sc.add_sections([["sect.example", true, ""]]);
	Scratch.write_text(sc.ctrl + "domains.cfg", "old.example\r\nexample.com\r\n");
	var r = sc.run(child);
	check_child("both", sc, r);
	check("both: list", r.list, [sysaddr, "sect.example"]);
	check("both: old.example not listed", r.listed["old.example"], false);
	check("both: example.com not listed", r.listed["example.com"], false);
	check("both: can_send old.example", r.can_send["old.example"], false);
});

// (4) domains.cfg with no entries, no sections: only the system address
run_case("no entries", function (sc) {
	Scratch.write_text(sc.ctrl + "domains.cfg", "; List additional domain names\r\n\r\n   \r\n");
	var r = sc.run(child);
	check_child("no entries", sc, r);
	check("no entries: list", r.list, [sysaddr]);
	check("no entries: unlisted host", r.listed["unlisted.example"], false);
	check("no entries: example.com", r.listed["example.com"], false);
	check("no entries: system address", r.listed[sysaddr], true);
});

if (failures.length > 0)
	throw new Error("domains.cfg fallback test failures:\n  " + failures.join("\n  "));
