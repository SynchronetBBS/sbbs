this.QUERY = function(client, packet) {

	var openOpers = [
		'SUBSCRIBE',
		'UNSUBSCRIBE',
		'READ',
		'KEYS',
		'SLICE'
	];

	var location = packet.location.split(".");

	if (packet.oper == "WRITE" && location.length == 2 && location[1] == "LATEST") {
		return false; // It's okay for clients to write to *.LATEST
	} else if (openOpers.indexOf(packet.oper) >= 0 || client.remote_ip_address === '127.0.0.1') {
		return false;
	} else {
		return true;
	}

}
