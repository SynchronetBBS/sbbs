// mspservice.js

// Synchronet Service for the Message Send Protocol 2 (RFC 1312/1159)

// $Id: mspservice.js,v 1.10 2018/10/22 06:42:00 rswindell Exp $

// Example configuration (in ctrl/services.ini):

// [MSP]
// Port=18
// MaxClients=10
// Command=mspservice.js

// [MSP-UDP]
// Port=18
// MaxClients=10
// Options=UDP
// Command=mspservice.js

load("sockdefs.js");
load("nodedefs.js");
load("sbbsdefs.js");
var userprops = load({}, "userprops.js");
var section = "imsg received";
var output_buf = "";
var version=0;
var send_response=false;

// Write a string to the client socket
function write(str)
{
	output_buf += str;
}

// Write all the output at once
function flush()
{
	if(version==1 || send_response)
		client.socket.send(output_buf);
}

function done()
{
	flush();
	exit();
}

function read_str(msg)
{
	var b;
	var str='';

	if(msg==undefined)
		msg=false;

	while(1) {
		if(!client.socket.data_waiting && !client.socket.is_connected)
			exit();
		b=client.socket.recvBin(1);
		if(client.socket.type==SOCK_DGRAM)
			write(ascii(b));
		if(b==0)
			return(str);
		if(msg) {
			if(b<32 && b != 9 && b != 10 && b != 13)
				continue;
		}
		str+=ascii(b);
	}
}

function update_user_imsg(usernum, sender, bbs, client, count)
{
	var last = {
		"name": sender,
		"BBS": bbs,
		"ip_address": client.ip_address,
		"host_name": client.host_name,
		"protocol": client.protocol,
		"localtime": new Date(),
		"count": count
	};
	return userprops.set(section, /* key: */null, last, usernum);
}

var b;
var recipient="";
var recip_node="";
var to_node=0;
var message="";
var sender="";
var sender_term="";
var cookie="";
var signature="";
var usernum=0;

/* Read version */
if(!client.socket.data_waiting || !client.socket.is_connected)
	done();
b=client.socket.recvBin(1);
if(client.socket.type==SOCK_DGRAM)
	write(ascii(b));
switch(b) {
	case 65:
		version=1;
		send_response=1;
		break;
	case 66:
		version=2;
		break;
	default:
		done();
}
recipient=read_str();
usernum=system.matchuser(recipient,true);
recip_node=read_str();
if(recip_node.substr(0,6)=="Node: ")
	to_node=parseInt(recip_node.substr(6));
message = '  ' + read_str(true).replace(/\n/g, '\n  ').trimRight() + '\r\n';
if(version==2) {
	sender=read_str();
	sender_term=read_str();
	cookie=read_str();
	signature=read_str();
}
else {
	sender="Unknown User";
}

/* Check values */
if(recipient != "" && usernum == 0)
	done();
if(recip_node != "" && to_node==0)
	done();
if(message == "")
	done();

var telegram_buf="\1n\1h\1cInstant Message\1n from \1h\1y";
telegram_buf += sender;
if(sender_term != "")
	telegram_buf += " \1n"+sender_term+"\1h";
telegram_buf += " \1w[\1n";
if(signature != "") {
	telegram_buf += signature;
	telegram_buf += "\1h]"
}
else {
	telegram_buf += client.socket.remote_ip_address;
	telegram_buf += "\1h]"
	if(client.host_name != undefined && client.host_name != "") {
		telegram_buf += " (\1n";
		telegram_buf += client.host_name;
		telegram_buf += "\1h)";
	}
}
var last = userprops.get(section, /* key: */null, /* deflt: */null, usernum);
telegram_buf += "\1n:";
if(signature.length && (!last || signature != last.BBS || sender != last.name)) {
	var avatar_lib = load({}, "avatar_lib.js");
	var avatar = avatar_lib.read(null, sender, signature);
	if(avatar_lib.is_enabled(avatar)) {
		load('graphic.js');
		var graphic = new Graphic(avatar_lib.defs.width, avatar_lib.defs.height);
		try {
			graphic.BIN = base64_decode(avatar.data);
			telegram_buf += "\r\n" + graphic.MSG;
		} catch(e) {
			log(LOG_ERR, e);
		};
	}
}
telegram_buf += "\1n\r\n\1h" + message;

/* TODO cache cookies and prevent dupes */
if(recipient != "") {
	if(to_node) {
		if(system.node_list[to_node-1].useron==usernum && !(system.node_list[to_node-1].misc&NODE_POFF)) {
			send_response=system.put_node_message(to_node, telegram_buf);
			log("Attempt to send node message: "+(send_response?"Success":"Failure"));
		}
		else
			log("Cannot send to user ("+recipient+") on node "+to_node);
	}
	else {
		var u = User(usernum);
		if(u.chat_settings & CHAT_NOPAGE) {
			log(LOG_NOTICE, "Attempt to send telegram to user (" + recipient + ") with paging disabled");
			done();
		}
		send_response=system.put_telegram(usernum, telegram_buf);
		log("Attempt to send telegram: "+(send_response?"Success":"Failure"));
		if(send_response)
			update_user_imsg(usernum, sender, signature, client, last ? last.count + 1 : 1);
	}
}
else if(to_node) {
	success=system.put_node_message(to_node, telegram_buf);
	log("Attempt to send node message: "+(success?"Success":"Failure"));
}
else {
	/* Broadcast to all nodes? */
}

done();
