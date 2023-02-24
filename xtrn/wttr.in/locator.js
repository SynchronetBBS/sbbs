/*
 * Just IP address lookup for now
 * wttr.in does the geoip for us (albeit inaccurately)
 * To do: do something with user.location / zipcode as an alternative?
 */

function getAddress() {
	const addrRe = /(^0\.0\.0\.0$)|(^127\.)|(^192\.168\.)|(^10\.)|(^172\.1[6-9]\.)|(^172\.2[0-9]\.)|(^172\.3[0-1]\.)|(^169\.254\.)|(^::1$)|(^[fF][cCdD])/;
	if (client.ip_address.search(addrRe) < 0) return client.ip_address;

	const f = new File(system.temp_path + 'sbbs-ws-' + client.socket.remote_port + '.ip');
	if (!f.exists) return;
	if (!f.open('r')) return;
	const addr = f.read();
	f.close();

	if (addr.search(addrRe) < 0) return addr;
}

this;