// This was supposed to be the beginning of a packet BBS interface.  It may
// be better to just start again.  Right now it logs the user in and then
// simply echoes back whatever they send.

load("sbbsdefs.js");
load("nodedefs.js");

// Send a string to the client, modifying it as necessary beforehand
function sendString(c) {
/*	var f = new File(system.ctrl_dir + "censorship.txt");
	f.open("r");
	var badWords = f.readAll();
	f.close();
	for(b in badWords) {
		var redact = badWords[b].replace(/./g, "*");
		var re = new RegExp(badWords[b], "ig");
		c = c.replace(re, redact);
	} */
	word_wrap(c);
	client.socket.send(c);
}

// I don't remember why I made this or what good it is. :|
function getInput() {
	while(client.socket.is_connected) {
		mswait(5);
		if(!client.socket.data_waiting) continue;
		var c = client.socket.recvline();
		break;
	}
	return strip_exascii(c.replace(/\r|\n/g, ""));
}

// Uh, some sort of prompt that disconnects the user after 10 bad commands?
function prompt(p, k) {
	var c = "?";
	for(var a = 0; a < 10; a++) {
		if(c == "?") sendString(p + "\r\n[ " + k.join("/") + " ]\r\n");
		c = getInput().toUpperCase();
		for(var i = 0; i < k.length; i++) if(c == k[i].toUpperCase()) return c;
	}
	sendString("Disconnecting.  Please call again later!\r\n");
	client.socket.close();
}

function ax25Login(u) {
	login(u.alias, u.security.password);
	sendString("You are logged in as: " + clientCallsign + "\r\nName: " + user.name + "\r\nAlias: " + user.alias + "\r\n");
	user.note = clientCallsign;
	user.computer = clientCallsign;
	user.connection = "AX.25";
}

var clientCallsign = client.socket.recvline();
var hostCallsign = client.socket.recvline();
log("Connection from " + clientCallsign);
sendString("\r\n[ " + hostCallsign + " : " + system.name + " ]\r\n");
sendString("[ " + system.version_notice + " ]\r\n");
var un = system.matchuserdata(U_COMMENT, clientCallsign.toUpperCase());
if(un > 0) {
	var u = new User(un);
	ax25Login(u);
} else {
	sendString("No user account on file for " + clientCallsign + "\r\n");
	sendString("If you've already created an account on this BBS (eg. via telnet, dial-up or HTTP), the system operator can link your callsign to that account.\r\n");
	var c = prompt("Do you already have an account on " + system.name + "?", ["Y", "N"]);
	if(c == "Y") {
		sendString("What name or alias was your account registered under?\r\n");
		var n = getInput();
		var header = { subject : system.name + " AX.25 Account Linking Request", to : system.operator, to_ext : 1, from : system.operator, from_ext: 1 }
		var body = "Please add the callsign " + clientCallsign + " to the comment field for the user with the name or alias " + n;
		var msgBase = new MsgBase("mail");
		msgBase.open();
		msgBase.save_msg(header, body_text=body);
		msgBase.close();
		sendString("Thanks for calling!\r\n");
		client.socket.close();
	} else {
		for(var a = 0; a < 5; a++) {
			sendString("Enter your full name:\r\n");
			var n = getInput();
			sendString("Enter your city and state/province:\r\n");
			var csp = getInput();
			sendString("Enter your email address, if any:\r\n");
			var ea = getInput();
			if(n.match(/\w+\s\w+/) == null) {
				sendString("Your full name did not match the expected format (eg. 'John Smith'.)  Please try again.\r\n");
			} else if(csp.match(/\w+,\s\w+/) == null) {
				sendString("Your city and state/province did not match the expected format (eg. 'Toronto, Ontario'.)  Please try again.\r\n");
			} else if(ea != "" && ea.match(/\w+\@\w+\.\w+/) == null) {
				sendString("Your email address did not match the expected pattern ('user@domain.suffix', or blank.)  Please try again.\r\n");
			} else {
				var p = "";
				for(var b = 0; b < 8; b++) {
					var r = "";
					while(r.match(/\w/) == null) r = ascii(random(123));
					p += r;
				}
				var u = system.new_user(n);
				u.alias = clientCallsign;
				u.name = n;
				u.location = csp;
				u.netmail = ea;
				u.security.password = p;
				u.comment = clientCallsign;
				ax25Login(u);
				var header = { 
					subject : system.name + " user account information",
					to : u.name,
					from: system.operator,
					from_ext : 1,
					from_ip_addr : client.ip_address,
					replyto : system.operator,
					replyto_ext : 1,
					to_net_addr : u.netmail,
					to_net_type : NET_INTERNET
				}
				var body = "Hello " + u.name + ",\r\n\r\nYou recently registered for an account on " + system.name + " via our packet radio interface.  We're also accessible over the internet via telnet, SSH, 
HTTP and several other protocols at " + system.inet_addr + ".\r\n\r\nIf you connect via the internet, you may log in with your callsign and with the temporary password:\r\n\r\n" + p + "\r\n\r\nThanks for calling.\r\n\r\n" + 
system.operator;
				var msgBase = new MsgBase("mail");
				msgBase.open();
				msgBase.save_msg(header, body_text=body);
				msgBase.close();				
				break;
			}
		}
	}
}

while(client.socket.is_connected) {
	mswait(5);
	if(!client.socket.data_waiting) continue;
	var c = client.socket.recvline();
	sendString("You sent: " + c + "\r\n");
}
