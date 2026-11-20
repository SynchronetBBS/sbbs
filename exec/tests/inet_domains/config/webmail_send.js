// The web mail client's Send against a real configuration and real users:
// a child jsexec loads a scratch ctrl directory (below system.temp_dir)
// with a [domain:] section and a new, empty user base, and sends with a
// real User object, the real Address.domains_for() and system resolvers
// and the real system values, so the production paths that the webmail
// tests replace with fakes are run once.

var failures = [];
var sc = load({}, js.exec_dir + "scratch.inc").scratch_ctrl("webmail");

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

try {
	sc.setup();
	sc.add_sections([["plain-on.example", true, ""], ["plain-off.example", false, ""]]);
	var r = sc.run(js.exec_dir + "webmail_send_child.inc");
	if (argv.indexOf("-v") >= 0)
		print(JSON.stringify(r));
	check("child exit code", r.exit_code, 0);
	check("child data dir", r.dirs.data, sc.data);
	check("no exception", r.exception, undefined);
	check("user e-mail", r.email, "Webmail.Sender@sysaddr.example");
	check("domains", r.domains, ["sysaddr.example", "plain-on.example"]);
	check("a From domain the user may not use", r.bad_domain, { error: "invalid", reason: "from_domain" });
	check("sent", r.sent && r.sent.ok, true);
	check("two copies", r.copies.length, 2);
	var inet = null;
	var local = null;
	(r.copies || []).forEach(function (c) {
		if (c.to_net_addr === "ext@example.org")
			inet = c;
		else if (c.to_ext == String(r.users.rcpt))
			local = c;
	});
	check("Internet copy From address", inet && [inet.from_net_addr, inet.reverse_path, inet.from_ext],
		["Webmail.Sender@plain-on.example", "Webmail.Sender@plain-on.example", String(r.users.sender)]);
	check("local copy", local && [local.to, local.from_ext], ["Webmailrcpt", String(r.users.sender)]);
	check("CC list", inet && inet.cc_list, "Webmailrcpt <Webmailrcpt@sysaddr.example>");
	check("same Message-ID", inet && local && inet.id === local.id && /@plain-on\.example>$/.test(inet.id), true);
	check("e-mail counted", r.emails_today, 2);
	check("no Internet mail for a restricted user", r.nonet, { error: "forbidden", reason: "recipient" });
	check("local mail for that user", r.nonet_local && r.nonet_local.ok, true);
} finally {
	check("scratch directory removed", sc.remove(), true);
}

if (failures.length > 0)
	throw new Error("webmail send test failures:\n  " + failures.join("\n  "));
