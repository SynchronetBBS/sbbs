// Run by fallback.js in a child jsexec against a scratch ctrl directory;
// writes what it observes as JSON to argv[0]. argv[1], when given, is a
// file of patterns to compare system.findstr() on the list with.
// Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");

// Users are created below: refuse to run unless the data directory is
// inside the scratch tree that holds the ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

var result = {
	dirs: { ctrl: system.ctrl_dir, data: system.data_dir },
	inet_addr: system.inet_addr,
	list: system.inet_domain_list,
	listed: {},
	listed_file: {},
	can_send: {}
};

// One ordinary user (made on the first run in this scratch tree)
var number = system.matchuser("Inetdomfb");
var u = number ? new User(number) : system.new_user("Inetdomfb");
if (typeof u != "object")
	throw new Error("new_user returned " + u);
u.security.level = 50;

var hosts = ["sysaddr.example", "example.com", "EXAMPLE.COM", "a.wild.example", "wild.example"
	, "indented.example", "blocked.example.org", "x.example.org", "example.org", "dup.example"
	, "trailing.example", "sect.example", "old.example", "unlisted.example"];
hosts.forEach(function (host) {
	result.listed[host] = system.findstr(system.inet_domain_list, host);
	if (argv.length > 1)
		result.listed_file[host] = system.findstr(argv[1], host);
});
system.inet_domain_list.concat(["sect.example", "old.example"]).forEach(function (domain) {
	result.can_send[domain] = u.can_send_from_domain(domain);
});

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
