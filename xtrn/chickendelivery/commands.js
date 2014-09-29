this.QUERY = function(client, packet) {
	var openOpers = [
		"SUBSCRIBE",
		"UNSUBSCRIBE",
		"READ",
		"KEYS",
		"SLICE"
	];
	var location = packet.location.split(".");
	if(packet.oper == "WRITE" && location.length == 2 && location[1] == "LATEST")
		return false; // It's okay for clients to write to *.LATEST
	if(openOpers.indexOf(packet.oper) >= 0 || admin.verify(client, packet, 90))
		return false;
	log(LOG_ERR,
		format(
			"Invalid command %s from client %s",
			packet.oper, client.remote_ip_address
		)
	);
	return true;
}