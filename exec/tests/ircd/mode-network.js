/* Native callback engine must survive a malformed MODE pipeline. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork(), sender, owner, watcher;
var a = {name:"hub1", sname:"a.modereview.test", port:19922};
var b = {name:"hub2", sname:"b.modereview.test", port:19923};
try {
	net._start_one(a, [{sname:b.sname, is_hub:true}]);
	net._start_one(b, [{sname:a.sname, port:a.port, is_hub:true}]);
	if (!net._wait_links(a.port, 2, 20)) throw new Error("Hubs did not link");
	owner = net.client(a, "Owner"); owner.join("#public");
	sender = net.client(a, "Sender");
	watcher = net.client(b, "Watcher"); watcher.wait_for_nick("Sender");
	sender.sock.send("MODE #public 123\r\nPING :after-malformed-mode\r\n");
	sender.expect(" 461 ", 3); sender.expect("after-malformed-mode", 3);
	owner.send("PING :owner-alive"); owner.expect("owner-alive", 3);
	watcher.send("WHOIS Sender"); watcher.expect(" 311 ", 3);
	var fresh = net.client(a, "Fresh"); fresh.quit();
	sender.send("MODE #public +m"); sender.expect(" 482 ", 3);
	owner.send("MODE #public +m"); owner.expect("MODE #public +m", 3);
	print("PASS: malformed MODE preserves its sender, unrelated clients, new connections and hub state");
} finally {
	if (sender) sender.quit();
	if (owner) owner.quit();
	if (watcher) watcher.quit();
	net.stop();
}
