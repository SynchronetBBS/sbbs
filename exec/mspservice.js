// mspservice.js

// Synchronet Service for the Message Send Protocol 2 (RFC 1312/1159)

// $Id$

// Example configuration (in ctrl/services.cfg):

// MSP		18	0-unlimited	0		mspservice.js [options]

load("sockdefs.js");
load("nodedefs.js");

var output_buf = "";
var version=0;
var success=false;

// Write a string to the client socket
function write(str)
{
	output_buf += str;
}

// Write all the output at once
function flush()
{
	if(version==1 || success)
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

function putsmsg(usernumber, message)
{
	var str;
	var file;
	var i;

	if(message==undefined && message=='')
		return(false);

	str=format("%smsgs/%4.4u.msg",system.data_dir,usernumber);
	file=new File(str);
	if(!file.open("a")) {
		log("Cannot open "+str+" for append");
		return(false);
	}
	file.writeln(message);
	file.close();

	for(i in system.node_list) {     /* flag node if user on that msg waiting */
log("Checking node "+i);
log(usernumber + " == "+system.node_list[i].useron);
log(system.node_list[i].status+" == "+NODE_INUSE);
log(system.node_list[i].status+" == "+NODE_QUIET);
log("Bit: "+(system.node_list[i].misc & NODE_MSGW));
		if(system.node_list[i].useron==usernumber 
				&& (system.node_list[i].status == NODE_INUSE || system.node_list[i].status == NODE_QUIET)
				&& !(system.node_list[i].misc & NODE_MSGW)) {
log("Setting but for node "+i);
			system.node_list[i].misc |= NODE_MSGW;
		}
	}
	return(true);
}

function putnmsg(nodenumber, message)
{
	var str;
	var file;

	if(message==undefined && message=='')
		return(false);

	str=format("%smsgs/n%3.3u.msg",system.data_dir,nodenumber);
	file=new File(str);
	if(!file.open("a")) {
		log("Cannot open "+str+" for append");
		return(false);
	}
	file.writeln(message);
	file.close();

	if((system.node_list[i].status == NODE_INUSE || system.node_list[i].status == NODE_QUIET)
			&& !(system.node_list[i].misc & NODE_NMSG)) {
		system.node_list[i].misc |= NODE_NMSG;
	}
	return(true);
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
if(!client.socket.data_waiting && !client.socket.is_connected)
	done();
b=client.socket.recvBin(1);
if(client.socket.type==SOCK_DGRAM)
	write(ascii(b));
switch(b) {
	case 65:
		version=1;
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
message=read_str(true);
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
telegram_buf += "\r\n\1w[\1n";
telegram_buf += client.socket.remote_ip_address;
telegram_buf += "\1h]"
if(client.host_name != undefined && client.host_name != "") {
	telegram_buf += " (\1n";
	telegram_buf += client.host_name;
	telegram_buf += "\1h)";
}
telegram_buf += "\1n:\r\n\1h";
telegram_buf += message;

/* TODO cache cookies and prevent dupes */
if(recipient != "") {
	log("Recipient specified: "+recipient);
	if(to_node) {
		if(system.node_list[to_node].useron==usernum) {
			success=putnmsg(to_node, telegram_buf);
			log("Attempt to send node message: "+(success?"Success":"Failure"));
		}
		else
			log("Cannot send to user "+recipient+" on node "+to_node);
	}
	else {
		success=putsmsg(usernum, telegram_buf);
		log("Attempt to send telegram: "+(success?"Success":"Failure"));
	}
}
else if(to_node) {
	success=putnmsg(to_node, telegram_buf);
	log("Attempt to send node message: "+(success?"Success":"Failure"));
}
else {
	/* Broadcast to all nodes? */
}

done();
