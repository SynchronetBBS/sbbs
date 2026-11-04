/* Slow native regression: agreed clocks, with pushes queued across stalls. */
"use strict";
load(js.exec_dir + "lib.sjs");
var net = new IrcdTestNetwork(), stopped = {}, ca, cb;
var a = {name:"hub1", sname:"a.defaultclock.test", port:19944};
var b = {name:"hub2", sname:"b.defaultclock.test", port:19945};
function pause(node) {
	system.popen("kill -STOP " + net.pids[node.name]); stopped[node.name] = true;
}
function resume(node) {
	system.popen("kill -CONT " + net.pids[node.name]); delete stopped[node.name];
}
try {
	net._start_one(a, [{sname:b.sname, is_hub:true}]);
	net._start_one(b, [{sname:a.sname, port:a.port, is_hub:true}]);
	if (!net._wait_links(a.port, 2, 20)) throw new Error("Hubs did not link");
	/* B queues A's announcement, then both miss callbacks for over MaxSkew. */
	pause(b); mswait(17000); pause(a); mswait(36000);
	resume(b); resume(a); mswait(1500);
	[a, b].forEach(function(node) {
		var file = new File(net.logpaths[node.name]);
		if (!file.open("r")) throw new Error("Cannot read daemon log");
		var rows = file.readAll(); file.close();
		if (rows.some(function(row) { return row.indexOf("Clock skew") >= 0; }))
			throw new Error("Queued CLOCK caused a false split: " + rows.join("\n"));
	});
	if (!net._wait_links(a.port, 2, 5)) throw new Error("Hubs lost their link after stalls");
	ca = net.client(a, "ClockA"); cb = net.client(b, "ClockB");
	ca.wait_for_nick("ClockB", 5); cb.wait_for_nick("ClockA", 5);
	ca.send("PING :clockalive"); ca.expect("clockalive", 3);
	print("PASS: queued CLOCK across overlapping stalls preserves the hub link at default MaxSkew=30");
} finally {
	[a, b].forEach(function(node) { if (stopped[node.name]) resume(node); });
	if (ca) ca.quit();
	if (cb) cb.quit();
	net.stop();
}
