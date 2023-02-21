/*
 * Just IP address lookup for now
 * wttr.in does the geoip for us (albeit inaccurately)
 * To do: do something with user.location / zipcode as an alternative?
 *
 * IP address lookup stuff adapted from syncWXremix
 * (Contributed by echicken)
 * https://github.com/KenDB3/syncWXremix
 * Copyright (c) 2015,  Kenny DiBattista <kendb3@bbs.kd3.us>
 * 
 * ISC License
 *
 * Copyright (c) 2022 Zaidhaan Hussain
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

// To do: I think this is only good for IPv4
function wstsGetIPAddress() {
	const ip = [];
	const data = [];

	try {
		console.lock_input(true);
		console.telnet_cmd(253, 28); // DO TTYLOC

		const stime = system.timer;
		while (system.timer - stime < 1) {
			if (!client.socket.data_waiting) continue;
			data.push(client.socket.recvBin(1));
			if (data.length >= 14 && data[data.length - 3] !== 255 && data[data.length - 2] === 255 && data[data.length - 1] === 240) break;
		}

		// Check for a valid reply
		if (data.length < 14 || // Minimum response length
			// Should start like this
			data[0] !== 255 || // IAC
			data[1] !== 250 || // SB
			data[2] !== 28 || // TTYLOC
			data[3] !== 0 || // FORMAT
			// Should end like this
			data[data.length - 2] !== 255 || // IAC
			data[data.length - 1] !== 240 // SE
		) {
			throw 'Invalid reply to TTYLOC command.';
		}

		for (var d = 4; d < data.length - 2; d++) {
			ip.push(data[d]);
			if (data[d] === 255) d++;
		}
		if (ip.length !== 8) throw 'Invalid reply to TTYLOC command.';
	} catch (err) {
		log(LOG_DEBUG, err);
	} finally {
		console.lock_input(false);
		if (ip.length !== 8) return;
		return ip.slice(0, 4).join('.');
	}
}

// for webv4 ... I think
function wsrsGetIPAddress() {
	var fn = format('%suser/%04d.web', system.data_dir, user.number);
	if (!file_exists(fn)) return;
	var f = new File(fn);
	if (!f.open('r')) return;
	var session = f.iniGetObject();
	f.close();
	if (typeof session.ip_address === 'undefined') return;
	return session.ip_address;
}

function getAddress() {
	const addrRe = /^(127\.)|(192\.168\.)|(10\.)|(172\.1[6-9]\.)|(172\.2[0-9]\.)|(172\.3[0-1]\.)|(169\.254\.)|(::1$)|([fF][cCdD])/;
	if (user.ip_address.search(addrRe) > -1) {
		var addr;
		if (client.protocol === 'Telnet') {
			addr = wstsGetIPAddress();
		} else if (bbs.sys_status&SS_RLOGIN) {
			addr = wsrsGetIPAddress();
		}
		if (addr === undefined || addr.search(addrRe) > -1) return;
		return addr;
	}
	return user.ip_address;
}

this;