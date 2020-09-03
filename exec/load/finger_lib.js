// $Id: finger_lib.js,v 1.1 2019/01/12 01:46:42 rswindell Exp $

// A Finger/SYSTAT client library

"use strict";

require('sockdefs.js', 'SOCK_DGRAM');
require('portdefs.js', 'standard_service_port');

// Returns a String on failure, an Array of lines on success
function request(host, query, protocol, udp)
{
	if (protocol === undefined)
		protocol = "finger";
	var sock = new Socket(udp === true ? SOCK_DGRAM : SOCK_STREAM);
	if(!sock.connect(host, standard_service_port[protocol]))
		return "Connection to " + host + " failed with error " + sock.last_error;
	if(query !== undefined)
		sock.send(query + "\r\n");
	var output = [];
	if(udp) {
		output.push(sock.recvfrom().data);
	}
	else {
		while(sock.is_connected && !js.terminated) {
			var line = sock.readline();
			if(line != null)
				output.push(line);
		}
	}
	sock.close();
	return output;
}

this;