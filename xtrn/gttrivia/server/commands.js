this.QUERY = function(client, packet)
{
	// Operations that are allowed by clients
	var openOpers = [ "READ", "WRITE", "SUBSCRIBE", "UNSUBSCRIBE" ];
	// The openOpers operations are okay to run; also, allow anything from the current machine
	if (openOpers.indexOf(packet.oper) >= 0 || client.remote_ip_address === '127.0.0.1')
		return false;
	else
		return true;

	// Disallowed operations
	//var invalidOps = [ "PUSH", "POP", "SHIFT", "UNSHIFT", "DELETE", "SLICE" ];
}
