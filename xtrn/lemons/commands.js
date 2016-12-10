/*	Lemons JSON DB module commands filter
	Any commands sent by clients to the Lemons JSON DB module are passed
	through the methods defined here before any changes are made to the DB. */

// Restrict what parts of the DB clients can write to
this.QUERY = function(client, packet) {

	// Read-only commands are okay
	var openOpers = [
		'SUBSCRIBE',
		'UNSUBSCRIBE',
		'READ',
		'KEYS',
		'SLICE'
	];

	var location = packet.location.split('.');

	// It's okay for clients to write to *.LATEST (except LEVELS)
	if (packet.oper == 'WRITE' &&
		location.length == 2 &&
		location[0] != 'LEVELS' &&
		location[1] == 'LATEST'
	) {
		return false; // Command not handled (the JSON service can continue processing this command)
	} else if (
		openOpers.indexOf(packet.oper) >= 0 || client.remote_ip_address === '127.0.0.1'
	) {
		return false; // Command not handled (the JSON service can continue processing this command)
	} else {
		return true; // Command has been handled
	}

}
