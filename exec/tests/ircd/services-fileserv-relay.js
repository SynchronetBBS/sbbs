/* A real native FileServ crossing the new channel-policy boundary. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork(), owner, viewer;
var hub = {name:"hub1", sname:"hub.files.test", port:19882};
var leaf = {name:"leaf1a", sname:"leaf.files.test", port:19883};
var original_write = net._write_config;
net._write_config = function(srv, peers) {
	var path = original_write.call(this, srv, peers);
	if (srv === leaf) {
		var f = new File(path);
		if (!f.open("a")) throw new Error("Cannot append FileServ test config");
		f.write("\n[BBSServices]\nFileServ=true\n[FileServ]\nChannels=#secure\n"); f.close();
	}
	return path;
};
function check(ok, message) { if (!ok) throw new Error(message); }
try {
	net._start_one(hub, [{sname:leaf.sname, is_hub:false}]);
	owner = net.client(hub, "Owner"); owner.join("#secure");
	owner.send("MODE #secure +SRMc"); owner.expect(" MODE #secure ", 5);
	net._start_one(leaf, [{sname:hub.sname, port:hub.port, is_hub:true}]);
	check(net._wait_links(hub.port, 2, 15), "FileServ network did not link");
	viewer = net.client(leaf, "Viewer");
	var bot = "FileServ_" + String(system.qwk_id).replace(/[^a-z0-9_-]/gi, "_").substr(0, 8);
	owner.wait_for_nick(bot);
	run_tests({
		"native FileServ remains joined on both sides of a protected channel": function() {
			var until = Date.now() + 5000, line;
			do { owner.send("NAMES #secure"); line = owner.expect(" 353 ", 5); if (line.indexOf(bot) >= 0) break; mswait(100); } while (Date.now() < until);
			check(line.indexOf(bot) >= 0, line);
			// Exact WHOIS includes public channel membership for invisible users.
			viewer.send("WHOIS " + bot);
			check(viewer.expect(" 319 ", 5).indexOf("#secure") >= 0, "Home daemon lost its FileServ membership");
		},
		"ordinary clients cannot acquire the daemon role or enter the protected channel": function() {
			viewer.send("JOIN #secure"); viewer.expect(" 477 ", 5);
			var impostor = net.client(hub, false);
			try { impostor.register("FileServ_FAKE", "bbs+ircd+files", "Impostor"); impostor.send("JOIN #secure"); impostor.expect(" 477 ", 5); }
			finally { impostor.quit(); }
		}
	});
} finally {
	if (viewer) viewer.quit(); if (owner) owner.quit(); net.stop();
}
