// "Keep a local copy of e-mail forwarded to netmail" (MM_FWDKEEP) for a
// notice sent with system.notify(): a child jsexec loads a scratch copy
// of the configuration whose data directory is a new, empty tree, with
// forwarding allowed (SYS_FWDTONET), and notifies a user who forwards
// e-mail to an Internet address. Run once with MM_FWDKEEP clear and once
// with it set.
//
// Clear: one message, addressed to the forwarding address, not in the
// user's mailbox (as before the option existed).
// Set: the same forwarded header plus a local copy addressed to the user,
// with the same body; only the local copy is indexed to the user. The two
// headers share one copy of the data: chksmb checks the data blocks'
// reference counts against the headers that use them.

load("sbbsdefs.js");

var failures = [];
var scratch = load({}, js.exec_dir + "scratch.inc");

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function hex_setting(f, section, fn)
{
	var v = parseInt(f.iniGetValue(section, "settings", "0"));
	f.iniSetValue(section, "settings", "0x" + (fn(v) >>> 0).toString(16));
}

// The chksmb next to the jsexec running this test, run on the scratch base
function chksmb(sc, base)
{
	var exe = scratch.running_jsexec();
	var dir = exe.substr(0, exe.length - file_getname(exe).length);
	var path = dir + "chksmb" + (system.platform.toLowerCase() == "win32" ? ".exe" : "");
	if (!file_exists(path))
		return "no " + path;
	var saved = (env.SBBSCTRL !== undefined) ? env.SBBSCTRL : system.ctrl_dir;
	putenv("SBBSCTRL=" + sc.ctrl);
	try {
		return system.exec(format('"%s" -q "%s"', path, base));
	} finally {
		putenv("SBBSCTRL=" + saved);
	}
}

function run_case(keep)
{
	var sc = scratch.scratch_ctrl(keep ? "fwdkeep_set" : "fwdkeep_clear");
	var r;
	try {
		sc.setup();
		if (!mkpath(sc.data + "msgs/"))
			throw new Error("Error creating " + sc.data + "msgs/");
		scratch.ini_edit(sc.ctrl + "main.ini", function (f) {
			hex_setting(f, null, function (v) { return v | SYS_FWDTONET; });
		});
		scratch.ini_edit(sc.ctrl + "msgs.ini", function (f) {
			hex_setting(f, null, function (v) { return keep ? (v | MM_FWDKEEP) : (v & ~MM_FWDKEEP); });
		});
		r = sc.run(js.exec_dir + "fwd_keep_child.inc");
		if (argv.indexOf("-v") >= 0)
			print(JSON.stringify(r, null, 1));
		r.chksmb = chksmb(sc, sc.data + "mail");
	} finally {
		check((keep ? "set" : "clear") + ": scratch directory removed", sc.remove(), true);
	}
	return r;
}

var name;
var r;
var fwd = { to: "Fwdkeeper", to_ext: null, to_net_type: NET_INTERNET, to_net_addr: "fwdkeep@example.com",
	subject: "Forward keep subject" };

// MM_FWDKEEP clear: forwarded only
name = "clear";
r = run_case(false);
check(name + ": child exit code", r.exit_code, 0);
check(name + ": exception", r.exception, undefined);
check(name + ": MM_FWDKEEP", Boolean(r.msg_settings & MM_FWDKEEP), false);
check(name + ": SYS_FWDTONET", Boolean(r.sys_settings & SYS_FWDTONET), true);
check(name + ": notified", r.notified, true);
check(name + ": messages", r.total_msgs, 1);
check(name + ": forwarded header", r.headers && r.headers.length == 1 && {
	to: r.headers[0].to, to_ext: r.headers[0].to_ext, to_net_type: r.headers[0].to_net_type,
	to_net_addr: r.headers[0].to_net_addr, subject: r.headers[0].subject }, fwd);
check(name + ": index", r.index && r.index.map(function (i) { return i.to; }), [0]);
check(name + ": mail waiting", r.mail_waiting, 0);
check(name + ": one notice", typeof r.telegram == "string" && r.telegram.indexOf("Forward keep subject") >= 0, true);
check(name + ": chksmb exit code", r.chksmb, 0);

// MM_FWDKEEP set: forwarded, and a local copy for the user
name = "set";
r = run_case(true);
check(name + ": child exit code", r.exit_code, 0);
check(name + ": exception", r.exception, undefined);
check(name + ": MM_FWDKEEP", Boolean(r.msg_settings & MM_FWDKEEP), true);
check(name + ": notified", r.notified, true);
check(name + ": messages", r.total_msgs, 2);
var h = r.headers || [];
check(name + ": headers", h.length, 2);
if (h.length == 2) {
	check(name + ": forwarded header", {
		to: h[0].to, to_ext: h[0].to_ext, to_net_type: h[0].to_net_type,
		to_net_addr: h[0].to_net_addr, subject: h[0].subject }, fwd);
	check(name + ": local copy", {
		to: h[1].to, to_ext: h[1].to_ext, to_net_type: h[1].to_net_type,
		to_net_addr: h[1].to_net_addr, subject: h[1].subject },
		{ to: "Fwdkeeper", to_ext: String(r.user), to_net_type: null, to_net_addr: null,
			subject: "Forward keep subject" });
	check(name + ": same body", h[1].body, h[0].body);
	check(name + ": body", typeof h[0].body == "string" && h[0].body.indexOf("Forward keep body") >= 0, true);
	check(name + ": index", r.index.map(function (i) { return [i.number, i.to]; }),
		[[h[0].number, 0], [h[1].number, r.user]]);
}
check(name + ": mail waiting", r.mail_waiting, 1);
check(name + ": one notice", typeof r.telegram == "string"
	&& r.telegram.split("Forward keep subject").length == 2, true);
check(name + ": chksmb exit code (data reference counts)", r.chksmb, 0);

if (failures.length > 0)
	throw new Error("forward and keep failures:\n  " + failures.join("\n  "));
