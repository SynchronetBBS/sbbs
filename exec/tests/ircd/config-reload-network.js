/* Failed live REHASH must retain identity and services trust across hubs. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork(), owner, watcher, peer, moved = false, path;
var a = {name:"hub1", sname:"a.cfgreview.test", port:19928};
var b = {name:"hub2", sname:"b.cfgreview.test", port:19929};
var write_config = net._write_config;
net._write_config = function(node, peers) {
	var path = write_config.call(this, node, peers), file = new File(path);
	if (!file.open("a")) throw new Error("Cannot append configuration");
	file.write("\n[Services]\nDisabled=true\n"); file.close();
	return path;
};
function barrier() {
	peer.send("PING :cfgbarrier");
	var until = Date.now() + 5000;
	while (Date.now() < until) {
		var line = peer.sock.recvline(512, 0.2);
		if (line && line.indexOf("cfgbarrier") >= 0) return;
	}
	throw new Error("Peer did not answer barrier");
}
function service_whois() {
	owner.send("WHOIS Agent");
	var output = "", line;
	do { line = owner.readln(3); output += line + "\n"; } while (line.indexOf(" 318 ") < 0);
	return output.indexOf("is a Network Service.") >= 0;
}
try {
	net._start_one(a, [{sname:b.sname, is_hub:true}, {sname:"services.synchro.net", is_hub:false}]);
	net._start_one(b, [{sname:a.sname, port:a.port, is_hub:true}]);
	if (!net._wait_links(a.port, 2, 20)) throw new Error("Hubs did not link");
	owner = net.client(a, "Owner"); owner.send("OPER testop testpass"); owner.expect(" 381 ", 5);
	owner.join("#guard"); watcher = net.client(b, "Watcher"); watcher.join("#guard");
	/* This is a private, untrusted peer using the hostname in startup defaults. */
	peer = new FakeLeafServer(a.port, "services.synchro.net");
	peer.legacy = true; peer.capabilities = "TS3 SSJOIN NICKIP TSMODE"; peer.connect();
	peer.send("NICK Agent 1 1700000000 + user host services.synchro.net 0 0 :Agent");
	barrier(); owner.wait_for_nick("Agent");
	var before = service_whois();
	peer.send(":services.synchro.net MODE #guard +m"); barrier();
	owner.send("MODE #guard"); var oldmode = owner.expect(" 324 ", 5);
	path = net.cfgpaths[a.name];
	if (!file_rename(path, path + ".hidden")) throw new Error("Cannot hide configuration");
	moved = true;
	owner.send("REHASH"); owner.expect("Configuration reload failed", 5);
	owner.send("PING :rehashdone"); owner.expect("rehashdone", 5);
	var after = service_whois();
	peer.send(":services.synchro.net MODE #guard +m"); barrier();
	owner.send("MODE #guard"); var newmode = owner.expect(" 324 ", 5);
	watcher.send("MODE #guard"); var remote = watcher.expect(" 324 ", 5);
	if (before || after || newmode != oldmode || / \+[^ ]*m/.test(remote))
		throw new Error("Failed reload changed services trust or channel state");
	owner.send("STATS F"); owner.expect("[Info] Servername=" + a.sname, 5);
	if (!file_rename(path + ".hidden", path)) throw new Error("Cannot restore configuration");
	moved = false;
	owner.send("REHASH"); owner.expect(" 382 ", 5);
	print("PASS: failed REHASH retains identity and trust on both hubs; restored configuration reloads");
} finally {
	if (moved) file_rename(path + ".hidden", path);
	if (peer) peer.close();
	if (owner) owner.quit();
	if (watcher) watcher.quit();
	net.stop();
}
