// staticservice.js

// Example Synchronet "Static" Service module

// You configure it by adding a line to your ctrl/services.cfg file:

// ;protocol    port    max_clients     options(hex)    command-line
// static	8001	10		802		staticservice.js

while(!server.terminated) {

	if(server.socket.poll(10)<1)
		continue;

	if(server.terminated)
		break;

	log("Incoming...");

	sock = server.socket.accept();

	sock.write("Enter a string: ");

	sock.write("\r\nYou sent: '" + sock.readline() + "'\r\n");

	sock.close();
}