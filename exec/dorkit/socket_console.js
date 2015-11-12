load("ansi_console.js");

dk.console.remote_io.sock = new Socket(true, dk.connection.socket);
if (!dk.console.remote_io.sock.is_connected)
	throw("Unable to create socket object");

dk.console.remote_io.print = function(string) {
	this.sock.write(string.replace(/\xff/g, "\xff\xff"));
};
