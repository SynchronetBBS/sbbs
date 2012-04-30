// ax25tunnel.js
// echicken -at- bbs.electronicchicken.com (VE3XEC)
 
load("sbbsdefs.js");
load("kissAX25lib.js");

var kissTNCs = [];
for(var k = 0; k < argc; k++) kissTNCs.push(loadKISSInterface(argv[k]));
var kissFrame;
var sockRet = "";
var ax25Clients = new Object();

while(!js.terminated) {
	// Check for frames waiting on any KISS interface
	for(var k = 0; k < kissTNCs.length; k++) {
		kissFrame = kissTNCs[k].getKISSFrame();
		if(!kissFrame) continue;
		var p = new ax25packet();
		p.disassemble(kissFrame);
		if(p.destination != kissTNCs[k].callsign || p.destinationSSID != kissTNCs[k].ssid) continue;
		if(!ax25Clients.hasOwnProperty(p.clientID)) {
			ax25Clients[p.clientID] = new ax25Client(p.destination, p.destinationSSID, p.source, p.sourceSSID, kissTNCs[k]);
			ax25Clients[p.clientID].sock = new Socket();
			ax25Clients[p.clientID].sock.connect("localhost", 2000);
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].callsign + "\r\n");
			ax25Clients[p.clientID].sock.send(ax25Clients[p.clientID].kissTNC.callsign + "\r\n");
		}
		sockRet = byteArrayToString(ax25Clients[p.clientID].receive(p));
		if(sockRet) ax25Clients[p.clientID].sock.send(sockRet + "\r\n");
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
}
