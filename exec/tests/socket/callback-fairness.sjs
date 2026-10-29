/* Standalone native regression: run with jsexec. No external connections. */
"use strict";
/* A missing completion or callback exception must fail the child process. */
js.global.exit_code = 1;
var listener = new Socket();
if (!listener.bind(0, "127.0.0.1") || !listener.listen(2))
	throw new Error("Cannot listen on loopback");
var senders = [], receivers = [], reads = [0, 0];
for (var i = 0; i < 2; i++) {
	var sender = new Socket();
	if (!sender.connect("127.0.0.1", listener.local_port, 1))
		throw new Error("Cannot connect loopback socket");
	senders.push(sender);
	receivers.push(listener.accept());
}
listener.close();
var began = system.timer, timerRan = false, immediateRan = false;
js.setTimeout(function() { timerRan = true; }, 10);
js.setImmediate(function() { immediateRan = true; });
receivers.forEach(function(receiver, index) {
	receiver.on("read", function() {
		reads[index]++;
		/* Keep both descriptors ready, including the newest event. */
		if (system.timer - began < 0.1 && reads[index] < 100000)
			return;
		receivers.forEach(function(sock) { sock.close(); });
		senders.forEach(function(sock) { sock.close(); });
		if (!timerRan || !immediateRan || !reads[0] || !reads[1])
			throw new Error("Ready sockets starved callbacks: timer=" + timerRan
				+ ", immediate=" + immediateRan + ", reads=" + reads.join(","));
		js.global.exit_code = 0;
		print("PASS: ready sockets, timers and queued callbacks all make progress");
	});
});
senders.forEach(function(sender) { sender.send("x"); });
js.do_callbacks = true;
