// staticservice.js

// Example Synchronet "Static" Service module

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