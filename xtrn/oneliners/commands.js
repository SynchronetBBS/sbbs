this.QUERY = function(client, packet) {

	var openOpers = [
		'SUBSCRIBE',
		'UNSUBSCRIBE',
		'READ',
		'SLICE',
		'PUSH'
	];

	if (openOpers.indexOf(packet.oper) >= 0 || client.remote_ip_address === '127.0.0.1') {
		return false;
	} else {
		return true;
	}

}
