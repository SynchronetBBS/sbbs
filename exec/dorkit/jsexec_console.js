load("ansi_console.js");

dk.console.remote_io.print = function(string) {
	'use strict';
	stdout.raw_write(string);
};

var jsexec_input_queue = load(true, "jsexec_input.js");
js.on_exit("jsexec_input_queue.write(''); jsexec_input_queue.poll(0x7fffffff);");
