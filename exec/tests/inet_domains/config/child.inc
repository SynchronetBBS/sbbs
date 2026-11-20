// Run by configured.js in a child jsexec whose ctrl directory is a scratch
// copy with [domain:] sections; writes what it observes as JSON to argv[0].
// Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");

var result = {
	dirs: { ctrl: system.ctrl_dir, data: system.data_dir, temp: system.temp_dir },
	inet_addr: system.inet_addr,
	list: system.inet_domain_list,
	list_is_frozen: Object.isFrozen(system.inet_domain_list),
	listed: {},
	listed_file: {},
	can_send: {},
	users: {}
};

// Users are created below: refuse to run unless the data directory is
// inside the scratch tree that holds the ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

function make_user(name, level, settings, restrictions)
{
	var u = system.new_user(name);
	if (typeof u != "object")
		throw new Error("new_user(" + name + ") returned " + u);
	u.security.level = level;
	if (restrictions)
		u.security.restrictions |= restrictions;
	if (settings)
		u.settings |= settings;
	return u.number;
}

result.users.normal = make_user("Inetdoma", 50);
result.users.high = make_user("Inetdomb", 60);
result.users.sysop = make_user("Inetdomc", 90);
result.users.guest = make_user("Inetdomd", 60, 0, UREST_EDIT_DEFAULTS);
result.users.inactive = make_user("Inetdome", 60, USER_INACTIVE);
result.users.nonet = make_user("Inetdomg", 60, 0, UREST_SEND_NETMAIL);
result.users.deleted = make_user("Inetdomf", 60);
system.del_user(result.users.deleted);
result.users.none = 0;
result.users.unknown = system.lastuser + 1;

["plain-on.example", "PLAIN-ON.Example", "ars-on.example", "plain-off.example"
	, "a.wild.example", "x.y.wild.example", "wild.example", "blocked.wild.example"
	, "dup.example", "bad_name.example", "nodot", "unlisted.example", "", "sysaddr.example"].forEach(function (host) {
	result.listed[host] = system.findstr(system.inet_domain_list, host);
	result.listed_file[host] = system.findstr(argv[1], host);
});

var domains = ["plain-on.example", "PLAIN-ON.EXAMPLE", "ars-on.example", "plain-off.example"
	, "a.wild.example", "*.wild.example", "blocked.wild.example", "!blocked.wild.example"
	, "dup.example", "bad_name.example", "nodot", "unlisted.example"
	, "sysaddr.example", "SysAddr.Example"];
// User.can_send_from_domain() for each user; new User() of a number
// with no record (beyond lastuser) throws, which is recorded as a string
result.user_number = {};
for (var key in result.users) {
	var u;
	try {
		u = new User(result.users[key]);
	} catch (e) {
		result.can_send[key] = "new User() threw";
		continue;
	}
	result.user_number[key] = u.number;
	var row = {};
	domains.forEach(function (domain) {
		row[domain] = u.can_send_from_domain(domain);
	});
	result.can_send[key] = row;
}

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
