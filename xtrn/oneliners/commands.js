this.QUERY = function(client, packet) {
	var openOpers = [
		"SUBSCRIBE",
		"UNSUBSCRIBE",
		"READ",
		"SLICE",
		"PUSH"
	];
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