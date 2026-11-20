// Run by equivalence.js in a child jsexec against a scratch ctrl
// directory: for each host, the answer of the old file (argv[1], read by
// findstr() as the old code did) and of the loaded list, for the hosts
// in argv[2] (a JSON file). Writes JSON to argv[0]. Not a test on its own (the runner only runs *.js files).

var hf = new File(argv[2]);	// JSON array of host names
if (!hf.open("r"))
	throw new Error("Error " + hf.error + " opening " + hf.name);
var hosts = JSON.parse(hf.read());
hf.close();
var result = { ctrl: system.ctrl_dir, list: system.inet_domain_list, old: {}, list_answer: {} };
hosts.forEach(function (host) {
	result.old[host] = system.findstr(argv[1], host);
	result.list_answer[host] = system.findstr(system.inet_domain_list, host);
});
var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
