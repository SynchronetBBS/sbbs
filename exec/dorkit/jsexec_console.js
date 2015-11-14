load("ansi_console.js");

dk.console.remote_io.print = function(string) {
	stdout.write(string);
	stdout.flush();
};
