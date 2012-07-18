/*	ax25tunnel.js
	echicken -at- bbs.electronicchicken.com (VE3XEC)

	This script is responsible for handling connections from AX.25 clients
	and then tunneling traffic between them and the packet-bbs.js service
	that you'll also need to run.  It should be replaced by something that
	creates an rlogin session for each connecting client and translates
	output from the Synchronet terminal server for transmission to AX.25
	clients.  I don't feel like writing an rlogin client in JS right now,
	so this is what you get.
	
	Prior to running this script, you'll need to edit ctrl/kiss.ini in order
	to configure your TNC(s).  You'll also need to add packet-bbs.js to your
	ctrl/services.ini file (see the comments at the top of packet-bbs.js for
	details.)  Once this is done, run this script via jsexec like so:
	
	jsexec -l ax25tunnel.js tnc1 tnc2 tnc3 etc...
	
	Where tnc1, tnc2, tnc3 are the names of sections from your ctrl/kiss.ini
	file. */
	
load("sbbsdefs.js");
load("kissAX25lib.js");
load("event-timer.js");

var timer = new Timer();
var kissTNCs = [];
for(var k = 0; k < argc; k++) {
	kissTNCs.push(loadKISSInterface(argv[k]));
}
var kissFrame;
var sockRet = "";
var ax25Clients = new Object();

var f = new File(system.ctrl_dir + "services.ini");
f.open("r");
var i = f.iniGetObject("Packet-BBS");
f.close();

function beaconAllTNCs() {
	for(var k = 0; k < kissTNCs.length; k++) {
		kissTNCs[k].beacon();
	}
}
timer.addEvent(600000, true, beaconAllTNCs);
beaconAllTNCs();

while(!js.terminated) {
	// Check for frames waiting on any KISS interface
	for(var k = 0; k < kissTNCs.length; k++) {
		kissFrame = kissTNCs[k].getKISSFrame();
		if(!kissFrame)
			continue;
		var p = new ax25packet();
		p.disassemble(kissFrame);
		if(p.destination != kissTNCs[k].callsign || p.destinationSSID != kissTNCs[k].ssid)
			continue;
		if(!ax25Clients.hasOwnProperty(p.clientID)) {
			ax25Clients[p.clientID] = new ax25Client(p.destination, p.destinationSSID, p.source, p.sourceSSID, kissTNCs[k]);
			ax25Clients[p.clientID].sock = new Socket();
			ax25Clients[p.clientID].sock.connect("localhost", i.Port);
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].callsign + "\r\n");
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].ssid + "\r\n");
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].kissTNC.callsign + "\r\n");
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].kissTNC.ssid + "\r\n");
		}
		sockRet = byteArrayToString(ax25Clients[p.clientID].receive(p));
		if(sockRet)
			ax25Clients[p.clientID].sock.send(sockRet + "\r\n");
		if(!ax25Clients[p.clientID].connected) {
			ax25Clients[p.clientID].sock.close();
			delete(ax25Clients[p.clientID]);
		}
	}
	// Check for data waiting on any sockets
	for(var c in ax25Clients) {
		if(ax25Clients[c].sock.data_waiting && ax25Clients[c].nr == ax25Clients[c].ssv && !ax25Clients[c].reject) {
			var sendMe = ax25Clients[c].sock.recvfrom(false, 256).data;
			sendMe = stringToByteArray(sendMe);
			ax25Clients[c].send(sendMe);
		} else if(!ax25Clients[c].sock.is_connected) {
			ax25Clients[c].disconnect();
			log(LOG_INFO, ax25Clients[c].callsign + "-" + ax25Clients[c].ssid + " has disconnected from " + ax25Clients[c].kissTNC.name + ", " + ax25Clients[c].kissTNC.callsign + "-" + ax25Clients[c].kissTNC.ssid);
			delete(ax25Clients[p.clientID]);
		}
	}
	timer.cycle();
}
