// staticservice.js

// Example Synchronet "Static" Service module

while(!js.terminated) {

	if(server.socket.poll(10)<1)
		continue;

	if(js.terminated)
		break;

	log("Incoming...");

	sock = server.socket.accept();

	sock.write("Enter a string: ");

	sock.write("\r\nYou sent: '" + sock.readline() + "'\r\n");

	sock.close();
}