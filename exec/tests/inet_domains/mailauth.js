// mailauth.js (a mail processor) on a binary with and without
// system.inet_domain_list: exec/ may be updated before the mail server
// binary is, so without the property it must check the old
// ctrl/domains.cfg as before, and never throw. Runs the script's text
// with stand-ins for the mail processor's globals.

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

var f = new File(js.exec_dir + "../../mailauth.js");
if (!f.open("r"))
	throw new Error("Error " + f.error + " opening " + f.name);
var source = f.read();
f.close();
var script = new Function("load", "exit", "log", "user", "system", "client", "server"
	, "sender_address", "processing_error_filename", "resolve_ip", "File", source);

var EXIT = {};

// Runs mailauth.js for sender_address from client ip; returns what it did
function run(sys, sender, ip)
{
	var outcome = { searched: [], refused: false };
	var fake_system = {
		host_name: sys.host_name,
		ctrl_dir: "/ctrl/",
		findstr: function (list, host) {
			if (list === undefined || list === null)
				throw new Error("findstr: Argument #1 is an unexpected 'null' or 'undefined' value");
			outcome.searched.push(list);
			if (typeof list == "string")
				return sys.file_lists.indexOf(host) >= 0;
			return list.indexOf(host) >= 0;
		}
	};
	if (sys.inet_domain_list !== undefined)
		fake_system.inet_domain_list = sys.inet_domain_list;
	function FakeFile() {
		this.open = function () { return true; };
		this.writeln = function () { outcome.refused = true; };
		this.close = function () {};
	}
	try {
		script(function () {}, function () { throw EXIT; }, function () {}
			, { number: 0, security: { restrictions: 0 } }, fake_system
			, { ip_address: ip }, { interface_ip_addr_list: ["10.9.9.9"] }
			, sender, "/temp/error.txt", function () { return "203.0.113.99"; }, FakeFile);
	} catch (e) {
		if (e !== EXIT)
			outcome.error = String(e);
	}
	return outcome;
}

var outside = "198.51.100.7";

// Older binary: no system.inet_domain_list
var old_sys = { host_name: "bbs.example", file_lists: ["extra.example"] };
var r = run(old_sys, "someone@extra.example", outside);
check("old binary: no error", r.error, undefined);
check("old binary: domains.cfg searched", r.searched, ["/ctrl/domains.cfg"]);
check("old binary: listed domain refused", r.refused, true);
r = run(old_sys, "someone@other.example", outside);
check("old binary, other domain: no error", r.error, undefined);
check("old binary, other domain: not refused", r.refused, false);
r = run(old_sys, "someone@bbs.example", outside);
check("old binary, host name: refused", r.refused, true);

// Current binary: the list (system address first)
var new_sys = { host_name: "bbs.example", file_lists: [], inet_domain_list: ["sysaddr.example", "extra.example"] };
r = run(new_sys, "someone@extra.example", outside);
check("new binary: no error", r.error, undefined);
check("new binary: list searched", r.searched, [new_sys.inet_domain_list]);
check("new binary: listed domain refused", r.refused, true);
r = run(new_sys, "someone@sysaddr.example", outside);
check("new binary: system address refused", r.refused, true);
r = run(new_sys, "someone@other.example", outside);
check("new binary, other domain: not refused", r.refused, false);
r = run(new_sys, "someone@extra.example", "127.0.0.1");
check("new binary, loopback: not refused", r.refused, false);

if (failures.length > 0)
	throw new Error("mailauth.js test failures:\n  " + failures.join("\n  "));
