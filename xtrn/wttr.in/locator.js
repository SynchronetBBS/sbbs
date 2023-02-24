// To do: use user.location / zipcode if available?

function getAddress() {
	if (js.global.client === undefined) return; // jsexec

	const addrRe = /(^0\.0\.0\.0$)|(^127\.)|(^192\.168\.)|(^10\.)|(^172\.1[6-9]\.)|(^172\.2[0-9]\.)|(^172\.3[0-1]\.)|(^169\.254\.)|(^::1$)|(^[fF][cCdD])/;
	if (client.ip_address.search(addrRe) < 0) return client.ip_address; // Not a private/local address

	/*
	 * User has a private address and may be coming from websocketservice.js.
	 * Check if their IP address was written to a temp file for us.
	 * 
	 * There's a very slim chance of false positives here, but whatever.
	 */
	const f = new File(system.temp_path + 'sbbs-ws-' + client.socket.remote_port + '.ip');
	if (!f.exists) return;
	if (f.date < client.connect_time) return; // Avoid stale ws ip files
	if (f.date - client.connect_time > 5) return; // Avoid ws ip files probably not belonging to this user (5 seconds arbitrary)
	if (!f.open('r')) return;
	const addr = f.read();
	f.close();

	if (addr.search(addrRe) < 0) return addr;
}

this;
