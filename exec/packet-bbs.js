/*	packet-bbs.js
	echicken -at- bbs.electronicchicken.com (VE3XEC)

	This is a rudimentary service that provides a very simple UI for
	unproto AX.25 clients.  Ideally this and ax25tunnel.js will be
	replaced at some point with something that speaks directly to the
	terminal server, but that's more work than I want to do right now.
	
	Setup of this service is as easy as pasting the following block into
	your ctrl/services.ini file and then recycling services:
	
	[Packet-BBS]
	Port=2000
	Options=NO_HOST_LOOKUP
	Command=packet-bbs.js
	
	In addition, you'll need to edit ctrl/kiss.ini to configure your TNC(s)
	and then run ax25tunnel.js via jsexec:
	
	jsexec -l ax25tunnel.js tnc1 tnc2 tnc3 etc...
	
	ax25tunnel.js is responsible for handling connections from AX.25 clients
	and gating traffic between them and this service. */

load("sbbsdefs.js");
load("nodedefs.js");

var timeout = 600;

/*	We'll assume that this connection is from ax25tunnel.js, and that the
	first four lines of input are:

	- Client callsign
	- Client SSID
	- Host callsign
	- Host SSID

	In the future, we might support different workflows based on this info. */
var clientCallsign = client.socket.recvline();
var clientSSID = client.socket.recvline();
var hostCallsign = client.socket.recvline();
var hostSSID = client.socket.recvline();

// Load an array of banned words
var f = new File(system.ctrl_dir + "censorship.txt");
f.open("r");
var badWords = f.readAll();
f.close();
// Return a string with all banned words redacted
function censor(s) {
	for(var b = 0; b < badWords.length; b++) {
		var redact = badWords[b].replace(/./g, "*");
		var re = new RegExp(badWords[b], "ig");
		s = s.replace(re, redact);
	}
	return s;
}

// If there's data waiting on the socket, fetch and return it
function getInput() {
	var startTime = system.timer;
	while(!client.socket.data_waiting) {
		if(system.timer - startTime > timeout)
			return false;
		mswait(5);
	}
	var s = client.socket.recvline();
	return s.replace(/\r|\n/g, "");
}

/*	If client socket is writeable, censor and send string 's' and return true,
	otherwise return false. */
function sendString(s) {
	if(client.socket.is_writeable) {
		client.socket.send(censor(s));
		return true;
	} else {
		return false;
	}
}

// Display a numbered list of message groups that the user has access to
function listMessageGroups() {
	var m = new MsgBase(user.cursub);
	var l = "";
	for(var i = 0; i < msg_area.grp_list.length; i++) {
		if(i == m.cfg.grp_number)
			l += "* ";
		else
			l += "  ";
		l += msg_area.grp_list[i].number + ") " + msg_area.grp_list[i].name + "\r\n";
	}
	sendString("\r\n" + l);
}

// Change to message group 'n', if it exists and is accessible
function changeMessageGroup(n) {
	if(!user.compare_ars(msg_area.grp_list[n].ars) || n > msg_area.grp_list.length)
		return false;
	for(var i = 0; i < msg_area.grp_list[n].sub_list.length; i++) {
		if(!user.compare_ars(msg_area.grp_list[n].sub_list[i].ars))
			continue;
		user.cursub = msg_area.grp_list[n].sub_list[0].code;
		sendString("Message group changed to: " + msg_area.grp_list[n].name + "\r\n");
		return true;
	}
	return false;
}

// Display a numbered list of message boards that the user has access to
function listMessageBoards() {
	var m = new MsgBase(user.cursub);
	var l = "";
	for(var i = 0; i < msg_area.grp_list[m.cfg.grp_number].sub_list.length; i++) {
		if(msg_area.grp_list[m.cfg.grp_number].sub_list[i].code == user.cursub)
			l += "* ";
		else
			l += "  ";
		l += msg_area.grp_list[m.cfg.grp_number].sub_list[i].index + ") " + msg_area.grp_list[m.cfg.grp_number].sub_list[i].name + "\r\n";
	}
	sendString("\r\n" + l);
}

// Change to message group sub-board 'n' if it exists and is accessible
function changeMessageBoard(n) {
	var m = new MsgBase(user.cursub);
	if(n > msg_area.grp_list[m.cfg.grp_number].sub_list.length || !user.compare_ars(msg_area.grp_list[m.cfg.grp_number].sub_list[n].ars))
		return false;
	user.cursub = msg_area.grp_list[m.cfg.grp_number].sub_list[n].code;
	sendString("Message board changed to: " + msg_area.grp_list[m.cfg.grp_number].sub_list[n].name + "\r\n");
	return true;
}

// List all valid messages in the current sub-board
function listMessages() {
	var h;
	var l = format("%-5s", "####") + format("%-11s", "From") + format("%-11s", "To") + format("%-26s", "Date") + "Subject\r\n";
	var m = new MsgBase(user.cursub);
	m.open();
	for(var i = m.first_msg; i < m.last_msg; i++) {
		h = m.get_msg_header(i);
		if(h == null || h.attr&MSG_DELETE)
			continue;
		l += format("%-5s", i) + format("%-11s", h.from.substring(0, 10)) + format("%-11s", h.to.substring(0, 10)) + format("%-26s", h.date.substring(0, 25)) + h.subject.substring(0, 25) + "\r\n";
	}
	m.close();
	sendString(l);
}

// Read message 'n' of the current sub-board if it is valid
function readMessage(n) {
	var m = new MsgBase(user.cursub);
	m.open();
	var h = m.get_msg_header(n);
	var b = m.get_msg_body(n);
	m.close();
	if(h == null || h.attr&MSG_DELETE)
		return;
	var l = "\r\nSubject: " + h.subject + "\r\n" + format("%-9s", "From:") + h.from + "\r\n" + format("%-9s", "To:") + h.to + "\r\n" + format("%-9s", "Date:") + h.date + "\r\n\r\n";
	l += b;
	sendString(l + "\r\n");
}

function postMessage() {
	var m = new MsgBase(user.cursub);
	sendString("Posting a new message on: " + m.cfg.name + "\r\n");
	sendString("To: (Leave blank for ALL)\r\n");
	var to = getInput();
	if(to == "")
		to = "All";
	sendString("Subject:\r\n");
	var subject = getInput();
	if(subject == "") {
		sendString("\r\nInvalid subject line.  Returning to menu.\r\n");
		return false;
	}
	sendString("\r\nEnter your message below.  Send a line containing only /S to save.\r\n");
	var b = "";
	var c = "";
	while(c.toUpperCase() != "/S") {
		b += c;
		c = getInput(); // Yeah, I know.  It's late.  Suck it.  :|
		b += "\r\n";
	}
	if(b == "") {
		sendString("\r\nInvalid message text.  Returning to menu.\r\n");
		return false;
	}
	var header = {
		"subject" : subject,
		"to" : to,
		"from" : user.alias		
	}
	m.open();
	m.save_msg(header, body_text=b);
	m.close();
}

// List all valid messages in the mail sub-board for this user
function listEmail() {
	var h;
	var l = format("%-5s", "####") + format("%-11s", "From") + format("%-26s", "Date") + "Subject\r\n";
	var m = new MsgBase('mail');
	m.open();
	for(var i = m.first_msg; i < m.last_msg; i++) {
		h = m.get_msg_header(i);
		if(h == null || h.attr&MSG_DELETE  || h.attr&MSG_READ || (h.to != user.alias && h.to != user.name))
			continue;
		l += format("%-5s", i) + format("%-11s", h.from.substring(0, 10)) + format("%-26s", h.date.substring(0, 25)) + h.subject.substring(0, 36) + "\r\n";
	}
	m.close();
	sendString(l);
}

// Read message 'n' of the mail sub-board if it is valid
function readEmail(n) {
	var m = new MsgBase('mail');
	m.open();
	var h = m.get_msg_header(n);
	var b = m.get_msg_body(n);
	m.close();
	if(h == null || h.attr&MSG_DELETE || h.attr&MSG_READ || (h.to != user.alias && h.to != user.name))
		return;
	var l = "\r\nSubject: " + h.subject + "\r\n" + format("%-9s", "From:") + h.from + "\r\n" + format("%-9s", "Date:") + h.date + "\r\n\r\n";
	l += b;
	sendString(l + "\r\n");
}

function postEmail() {
	var m = new MsgBase("mail");
	sendString("Sending Email\r\n");
	sendString("To: \r\n");
	var to = getInput();
	if(to == "") {
		sendString("\r\nInvalid addressee.  Returning to menu.\r\n");
		return false;
	}
	sendString("Subject:\r\n");
	var subject = getInput();
	if(subject == "") {
		sendString("\r\nInvalid subject line.  Returning to menu.\r\n");
		return false;
	}
	sendString("\r\nEnter your message below.  Send a line containing only /S to save.\r\n");
	var b = "";
	var c = "";
	while(c.toUpperCase() != "/S") {
		b += c;
		c = getInput();
		b += "\r\n"; // Yeah, I know.  It's late.  Suck it.  :|
	}
	if(b == "") {
		sendString("\r\nInvalid message text.  Returning to menu.\r\n");
		return false;
	}
	var header = {
		"subject" : subject,
		"to" : to,
		"to_net_type" : netaddr_type(to),
		"from_net_type" : netaddr_type(to),
		"from" : user.alias		
	}
	m.open();
	m.save_msg(header, body_text=b);
	m.close();
}

// Draw the main menu and handle commands
var f = new File(system.text_dir + "packet/menu.asc");
f.open("r");
var menuBanner = f.readAll();
f.close();
var prompt = menuBanner[menuBanner.length - 1];
menuBanner = menuBanner.slice(0, menuBanner.length - 2).join("\r\n");
function menu() {
	var s = "?";
	while(client.socket.is_connected) {
		switch(s.substring(0, 2)) {
			case "?":
				// Redraw menu
				sendString("\r\n" + menuBanner + "\r\n");
				break;
			case "D":
				// Disconnect
				client.socket.close();
				break;
			case "MG":
				// Message groups
				if(s.length < 3 || isNaN(parseInt(s.substring(2))))
					listMessageGroups();
				else
					changeMessageGroup(s.substring(2));
				break;
			case "MB":
				// Message boards;
				if(s.length < 3 || isNaN(parseInt(s.substring(2))))
					listMessageBoards();
				else
					changeMessageBoard(s.substring(2));
				break;
			case "ML":
				// List messages
				listMessages();
				break;
			case "MR":
				// Read message
				if(s.length < 3 || isNaN(parseInt(s.substring(2))))
					readMessage(1);
				else
					readMessage(parseInt(s.substring(2)));
				break;
			case "MP":
				// Post a message
				postMessage();
				break;
			case "EL":
				// List email
				listEmail();
				break;
			case "ER":
				// Read email
				if(s.length > 3 && !isNaN(parseInt(s.substring(2))))
					readEmail(parseInt(s.substring(2)));
				break;
			case "ES":
				// Send email
				postEmail();
				break;
			case false:
				sendString("Idle timeout.  Disconnecting.");
				client.socket.close();
				break;
			default:
				break;
		}
		sendString("\r\n" + prompt + "\r\n");
		var s = getInput().toUpperCase();
	}
}

/*	Search user comment fields for this user's callsign.  If there's a match,
	log them in as that user.  Otherwise create a new account (and present
	info on account linking options.) */
var un = system.matchuserdata(U_COMMENT, clientCallsign.toUpperCase());
if(un > 0) {
	var u = new User(un);
	login(u.alias, u.security.password);
	sendString("You are logged in as: " + clientCallsign + "\r\nName: " + user.name + "\r\nAlias: " + user.alias + "\r\n");
	user.note = clientCallsign;
	user.computer = clientCallsign;
	user.connection = "AX.25";
} else {
	// To do: interactive sign-up, callsign lookup
	var u = system.new_user(clientCallsign);
	u.alias = clientCallsign;
	u.name = clientCallsign;
	u.handle = clientCallsign;
	var p = "";
	for(var b = 0; b < 8; b++) {
		var r = "";
		while(r.match(/\w/) == null) {
			r = ascii(random(123));
		}
		p += r;
	}
	u.security.password = p;
	u.comment = clientCallsign;
	u.note = clientCallsign;
	u.computer = clientCallsign;
	u.connection = "AX.25";
	login(u.alias, u.security.password);
	user.cursub = msg_area.grp_list[0].sub_list[0].code;
	sendString("You are logged in as: " + clientCallsign + "\r\nName: " + user.name + "\r\nAlias: " + user.alias + "\r\n");
}

//	Loop on the menu until the user logs off, times out, or disconnects.
menu();
