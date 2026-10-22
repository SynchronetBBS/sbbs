/* Test-only concurrent native BBS caller; the test parent stays in JS. */
"use strict";
load("sbbsdefs.js"); load("sockdefs.js"); load("ircd/services-common.js"); load("ircd/services-backend.js");
var backend = new IRC_Services_Backend(argv[0], null);
try {
	while (!js.terminated) {
		var job = parent_queue.read(250);
		if (!job) continue;
		backend.write(job);
		if (job.type == "stop") break;
		backend.drain();
		while (backend.read_level) parent_queue.write(backend.read());
	}
} finally { backend.write({type:"stop"}); }
