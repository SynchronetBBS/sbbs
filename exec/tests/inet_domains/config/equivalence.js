// For a table of domains.cfg shapes, three answers must be equal for every
// host: findstr() on the old file (what the mail server did before), the
// list the configuration loader reads from the file as a fallback, and
// the list after update.js's converter turned the file into [domain:]
// sections. (The JS list also starts with the system address, which is
// not among the hosts.)

load(js.exec_dir + "../../../load/domains_cfg_upgrade.js");
var Scratch = load({}, js.exec_dir + "scratch.inc");
var child = js.exec_dir + "equivalence_child.inc";
var failures = [];

var hosts = ["example.com", "EXAMPLE.COM", "www.example.com", "example.org", "x.example.org"
	, "blocked.example.org", "a.wild.example", "dup.example", "x.example", "mysub.example"
	, "10.1.2.3", "other.test", ""];

var shapes = [
	["plain names", "example.com\nexample.org\n"],
	["wildcards", "*.example.com\n*.wild.example\n"],
	["negation first", "!blocked.example.org\n*.example.org\n"],
	["negation in the middle", "example.com\n!blocked.example.org\n*.example.org\n"],
	["negation last", "*.example.org\n!blocked.example.org\n"],
	["comment after a trailing negation", "*.example.org\n!blocked.example.org\n; end of list\n"],
	["comment in the middle", "example.com\n; middle\nexample.org\n"],
	["duplicate negation last", "!dup.example\nexample.com\n!DUP.example\n"],
	["duplicate plain last after negation", "example.com\n!x.example\nEXAMPLE.COM\n"],
	["duplicate in the middle", "example.com\nEXAMPLE.com\nexample.org\n"],
	["mixed case", "Example.COM\n*.WILD.example\n"],
	["escapes", "exa\\x6dple.com\nexample\\x2eorg\n\\x2a.wild.example\n"],
	["escaped comment and empty line", "\\x3bnot a comment\nexample.com\n!x.example\n\\x00\n"],
	["tab metadata", "example.com\tsome metadata\n!blocked.example.org\tmeta\n*.example.org\n"],
	["CRLF", "example.com\r\n*.example.org\r\n\r\n"],
	["0xFF white space", "\xFF example.com \xFF\n\xFF\n*.wild.example\xFF\n"],
	["no final newline", "*.example.org\n!blocked.example.org"],
	["blank lines", "\n\n   \nexample.com\n\n\t\n"],
	["CIDR", "10.0.0.0/8\n"],
	["sub-string and prefix", "sub~\nexam^\n"],
	["only comments", "; nothing here\n\n; at all\n"],
	["trailing backslash, no final newline", "example.com\nexa\\"],
	["escaped line break", "examp\\\nle.com\nexample.org\n"],
	// Cannot be a section name: an inert entry in both lists (the converter
	// warns); the answers agree for host names that do not begin with white
	// space
	["leading space after unescaping", "\\x20leading.example\nexample.com\n", "inexact"],
];

// Over the 10000-line cap: the loader and the converter read the same
// first 10000 lines (the old file's line 10001 matched; neither list has it)
(function () {
	var big = "";
	for (var n = 0; n < 9999; n++)
		big += "host" + n + ".example\n";
	big += "*.example.org\n!blocked.example.org\n";	// lines 10000 and 10001
	shapes.push(["over the line cap", big, "capped"]);
})();

shapes.forEach(function (shape) {
	var name = shape[0];
	var sc = Scratch.scratch_ctrl("eq");
	try {
		sc.setup();
		var cfg = sc.ctrl + "domains.cfg";
		Scratch.write_text(cfg, shape[1], "wb");
		var hosts_file = sc.base + "hosts.json";
		Scratch.write_text(hosts_file, JSON.stringify(hosts));
		var fallback = sc.run(child, [cfg, hosts_file]);
		if (fallback.exit_code !== 0)
			throw new Error("child exit code " + fallback.exit_code);
		if (fallback.ctrl !== sc.ctrl)
			throw new Error("child ctrl dir " + fallback.ctrl);
		var conv = upgrade_domains_cfg(sc.ctrl);
		// domains.cfg stays in place after the conversion
		if (!file_exists(cfg))
			throw new Error("domains.cfg was removed by the conversion");
		var converted = sc.run(child, [cfg, hosts_file]);
		if (converted.exit_code !== 0)
			throw new Error("child exit code " + converted.exit_code);
		if (shape[2] == "inexact"
		    && (conv.warnings.length != 1 || conv.warnings[0].indexOf("Not convertible exactly") != 0))
			failures.push(name + ": expected a warning, got " + JSON.stringify(conv.warnings));
		if (conv.status == "converted" && JSON.stringify(converted.list) !== JSON.stringify(fallback.list))
			failures.push(name + ": converted list " + JSON.stringify(converted.list)
				+ " differs from the fallback list " + JSON.stringify(fallback.list));
		if (shape[2] == "capped") {
			if (conv.warnings.join().indexOf("Only the first 10000 lines") < 0)
				failures.push(name + ": no warning about the cap: " + JSON.stringify(conv.warnings));
			// the last line read is *.example.org: blocked.example.org is
			// listed by both lists, but was not with the whole old file
			if (fallback.list_answer["blocked.example.org"] !== true
			    || converted.list_answer["blocked.example.org"] !== true)
				failures.push(name + ": both lists should list blocked.example.org");
			return;
		}
		hosts.forEach(function (host) {
			var old = fallback.old[host];
			if (fallback.list_answer[host] !== old)
				failures.push(name + ": fallback answers " + fallback.list_answer[host] + " for "
					+ JSON.stringify(host) + ", the old file " + old + " (list " + JSON.stringify(fallback.list) + ")");
			if (converted.list_answer[host] !== old)
				failures.push(name + ": converted list answers " + converted.list_answer[host] + " for "
					+ JSON.stringify(host) + ", the old file " + old + " (list " + JSON.stringify(converted.list) + ")");
		});
	} catch (e) {
		failures.push(name + ": " + e);
	} finally {
		if (!sc.remove())
			failures.push(name + ": scratch directory not removed");
	}
});

if (failures.length > 0)
	throw new Error("domains.cfg equivalence failures:\n  " + failures.join("\n  "));
