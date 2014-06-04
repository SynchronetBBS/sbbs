// ax25tunnel.js
// VE3XEC (echicken -at- bbs.electronicchicken.com)
// Work-in-progress AX.25 unproto <-> Synchronet RLogin server tunnel

load("sbbsdefs.js");
load("sockdefs.js");
load("callsign.js");
load("kissAX25lib.js");

/*	This could be made into more of a proper RLogin client, but for now it's
	just the bare minimum required for the purposes of this script. */
var RLogin = function(host, port, username, password, terminal) {
	
	var settings = {
		'timeout'		: 30,
		'receiveBuffer'	: 255
	};
	
	var socket = new Socket();
	
	this.__defineGetter__(
		"connected",
		function() {
			return socket.is_connected;
		}
	);
	
	this.__defineGetter__(
		"dataWaiting",
		function() {
			return socket.data_waiting;
		}
	);
	
	this.connect = function() {
		socket.connect(host, port);
		socket.sendBin(0, 1);
		socket.send(password);
		socket.sendBin(0, 1);
		socket.send(username);
		socket.sendBin(0, 1);
		socket.send(terminal);
		socket.sendBin(0, 1);
		var sTime = time();
		while(time() - sTime < settings.timeout) {
			if(socket.poll(.1))
				break;
		}
		if(socket.recvBin(1) != 0)
			return false;
		return true;
	}
	
	this.disconnect = function() {
		socket.close();
	}
	
	this.receive = function() {
		if(!socket.is_connected || !socket.data_waiting)
			return false;
		var r = socket.recv(settings.receiveBuffer);
		return r;
	}
	
	this.send = function(str) {
		if(!socket.is_connected || !socket.is_writeable)
			return false;
		socket.write(str);
	}

}

var f = new File(system.text_dir + "badwords.can");
if(f.exists) {
	f.open("r");
	var badWords = f.readAll();
	f.close();
} else {
	var badWords = [];
}
var censor = function(s) {
	for(var b = 0; b < badWords.length; b++) {
		var redact = badWords[b].replace(/./g, "*");
		var re = new RegExp(badWords[b], "ig");
		s = s.replace(re, redact);
	}
	return s;
}

AX25.logging = true;
var tnc = AX25.loadAllTNCs();
var tunnels = {};

while(!js.terminated) {
	
	for(var tnc in AX25.tncs) {
		AX25.tncs[tnc].cycle();
		while(AX25.tncs[tnc].dataWaiting) {
			var packet = AX25.tncs[tnc].receive();
			if(
				packet.destinationCallsign != AX25.tncs[tnc].callsign
				||
				packet.destinationSSID != AX25.tncs[tnc].ssid
			) {
				continue;
			}
			if(!AX25.clients.hasOwnProperty(packet.clientID)) {
				var a = new AX25.Client(AX25.tncs[tnc], packet);
				var usernumber = system.matchuserdata(U_HANDLE, packet.sourceCallsign + packet.sourceSSID);
				if(usernumber < 1) {
					var u = system.new_user(packet.sourceCallsign + packet.sourceSSID);
					u.alias = packet.sourceCallsign;
					u.handle = packet.sourceCallsign + packet.sourceSSID;
					u.security.password = time(); // Do something better here
					try {
						var callsign=CallSign.Lookup.Any(u.alias);
					} catch (e) {
						log("AX.25 Tunnel: " + e);
					}
					if(typeof callsign != "undefined") {
						u.name = callsign.name;
						u.address = callsign.address;
						u.location = callsign.city + ", " + callsign.provstate;
						u.zipcode = callsign.postalzip;
					}
				} else {
					var u = new User(usernumber);
				}
				var username = u.alias;
				var password = u.security.password;
				tunnels[packet.clientID] = new RLogin(
					system.inet_addr,
					513,
					username,
					password,
					"AX25/9600"
				);
				tunnels[packet.clientID].connect();
			} else {
				AX25.clients[packet.clientID].receivePacket(packet);
			}
		}
	}
	
	for(var c in AX25.clients) {
		AX25.clients[c].cycle();
		if(!AX25.clients[c].connected) {
			if(tunnels.hasOwnProperty(AX25.clients[c].id)) {
				tunnels[AX25.clients[c].id].disconnect();
				delete tunnels[AX25.clients[c].id];
			}
			delete AX25.clients[c];
			continue;
		}
		if(
			!tunnels.hasOwnProperty(AX25.clients[c].id)
			||
			!tunnels[AX25.clients[c].id].connected
		) {
			AX25.clients[c].disconnect();
			continue;
		}
		if(tunnels[AX25.clients[c].id].dataWaiting) {
			var fromTunnel = tunnels[AX25.clients[c].id].receive();
			if(fromTunnel.length > 0)
				AX25.clients[c].sendString(censor(fromTunnel));
		}
		if(AX25.clients[c].dataWaiting) {
			var fromClient = AX25.clients[c].receiveString();
			if(fromClient.length > 0)
				tunnels[AX25.clients[c].id].send(fromClient);
		}
	}
	
	mswait(5);
}