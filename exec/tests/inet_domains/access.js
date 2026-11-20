// Script access to the additional Internet domains of the loaded
// configuration: system.inet_domain_list (names, matched with
// system.findstr()) and User.can_send_from_domain(). Holds for any
// configuration, including one with no [domain:] sections.

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function throws(fn)
{
	try {
		fn();
	} catch (e) {
		return true;
	}
	return false;
}

var list = system.inet_domain_list;
check("list is an array", Array.isArray(list), true);
check("list is frozen", Object.isFrozen(list), true);
check("enumerable", Object.keys(system).indexOf("inet_domain_list") >= 0, true);
for (var i = 0; i < list.length; i++)
	check("entry " + i + " type", typeof list[i], "string");
// The system's primary Internet address comes first, then every
// configured additional domain, unchanged and in order
if (system.inet_addr)
	check("first element is the system address", list[0], system.inet_addr);
var names = list.slice(system.inet_addr ? 1 : 0);

// Read-only: assignments are ignored
system.inet_domain_list = [];
check("list not replaced", system.inet_domain_list.length, list.length);
check("push refused", throws(function () { list.push("x.example"); }), true);
if (list.length > 0) {
	var first = list[0];
	list[0] = "changed.example";
	check("entry not changed", list[0], first);
}

// No other homes for it (unreleased interfaces that were removed)
check("no system.inet_domain_listed", typeof system.inet_domain_listed, "undefined");
check("no msg_area.inet_domain_list", typeof msg_area.inet_domain_list, "undefined");
check("no msg_area.inet_domain_listed", typeof msg_area.inet_domain_listed, "undefined");

// Matching: system.findstr() with the array. An empty list (no
// [domain:] sections) matches nothing, as a missing domains.cfg did
check("empty array matches nothing", system.findstr([], "example.com"), false);
check("empty array, empty host", system.findstr([], ""), false);
if (system.inet_addr)
	check("system address matches", system.findstr(list, system.inet_addr), true);
if (names.length == 0) {
	// No [domain:] sections: only the system address is one of ours
	["example.com", "mail." + system.inet_addr, ""].forEach(function (host) {
		check("no additional domains: " + host, system.findstr(list, host), false);
	});
}

// User.can_send_from_domain(): the per-user check is a User method;
// msg_area has none
check("no msg_area method", typeof msg_area.can_send_from_inet_domain, "undefined");
check("User method", typeof user.can_send_from_domain, "function");
var nobody = new User(0);
check("new User(0) number", nobody.number, 0);
check("send user 0", nobody.can_send_from_domain("example.com"), false);
check("send user 0 from the system address", nobody.can_send_from_domain(system.inet_addr), false);
if (user.number == 0)	// jsexec: no logged-in user
	check("current user 0 from the system address", user.can_send_from_domain(system.inet_addr), false);
check("send unknown user: new User() throws"
	, throws(function () { new User(system.lastuser + 1); }), true);
check("send null domain throws", throws(function () { nobody.can_send_from_domain(null); }), true);
check("send no argument throws", throws(function () { nobody.can_send_from_domain(); }), true);
check("send on other object throws"
	, throws(function () { nobody.can_send_from_domain.call({}, "example.com"); }), true);
// User 1 when the user base has one (a read-only check), otherwise user 0
var subject = system.lastuser >= 1 ? new User(1) : nobody;
names.forEach(function (name) {
	if (name.toLowerCase() == system.inet_addr.toLowerCase())
		return;	// the system's own address is always usable
	if (!system.findstr(list, name))
		check("send unlisted " + name, subject.can_send_from_domain(name), false);
	// Only letters, digits, dots and hyphens, with a dot not at either end
	if (!/^[A-Za-z0-9-][A-Za-z0-9.-]*\.[A-Za-z0-9.-]*[A-Za-z0-9-]$/.test(name))
		check("send pattern " + name, subject.can_send_from_domain(name), false);
});

if (failures.length > 0)
	throw new Error("system.inet_domain_list test failures:\n  " + failures.join("\n  "));
