// gopherservice.js

// Synchronet Service for the Gopher Protocol (RFC 1436)

// Example configuration (in ctrl/services.cfg):

// Gopher		70	0-unlimited	0		gopherservice.js

load("sbbsdefs.js");
load("nodedefs.js");

const VERSION = "1.00 Alpha";
const GOPHER_PORT = 70;

var debug = false;
var no_anonymous = false;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-d")
		debug = true;

// Write a string to the client socket
function write(str)
{
	if(0 && debug)
		log(format("rsp: %s",str));
	client.socket.send(str);
}

function writeln(str)
{
	write(str + "\r\n");
}

// Send the contents of a text file to the client socket
function send_file(fname)
{
	f = new File(fname);
	if(!f.open("r")) 
		return;
	txt = f.readAll();
	f.close();
	for(l in txt)
		writeln(strip_ctrl(txt[l]));
}

var msgbase=null;

if(!login("guest"))	{
	writeln("Sorry, no Gopher access here.");
	log("!NO GUEST ACCOUNT CONFIGURED");
	exit();
}


// Get Request
request = client.socket.recvline(512 /*maxlen*/, 10 /*timeout*/);

if(request==null) {
	log("!TIMEOUT waiting for request");
	exit();
}

log(format("client request: '%s'",request));

var gopher_plus=false;
if((term=request.indexOf("\t+"))>=0) {
	gopher_plus=true;
	request=request.substr(0,term);
	writeln("+-2");
}

if(request=="" || request=='/') { /* "root" */
	for(g in msg_area.grp_list) 
		writeln(format("1%s\tgrp:%s\t%s\t%u"
			,msg_area.grp_list[g].description
			,msg_area.grp_list[g].name.toLowerCase()
			,system.inetaddr
			,GOPHER_PORT
			));
/** to-do
	for(l in file_area.lib_list) 
		writeln(format("1%s\tlib:%s\t%s\t%u"
			,file_area.lib_list[l].description
			,file_area.lib_list[l].name.toLowerCase()
			,system.inetaddr
			,GOPHER_PORT
			));
**/
	writeln(format("0%s\t%s\t%s\t%u"
		,"Node List"
		,"nodelist"
		,system.inetaddr
		,GOPHER_PORT
		));
	writeln(format("0%s\t%s\t%s\t%u"
		,"Logon List"
		,"logonlist"
		,system.inetaddr
		,GOPHER_PORT
		));
	writeln(format("0%s\t%s\t%s\t%u"
		,"Auto-Message"
		,"automsg"
		,system.inetaddr
		,GOPHER_PORT
		));

	exit();
}

switch(request) {
	case "nodelist":
		var user = new User(1);
		for(n=0;n<system.node_list.length;n++) {
			write(format("Node %2d ",n+1));
			if(system.node_list[n].status==NODE_INUSE) {
				user.number=system.node_list[n].useron;
				write(format("%s (%u %s) ", user.alias, user.age, user.gender));
				if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
					write(format("running %s",user.curxtrn));
				else
					write(format(NodeAction[system.node_list[n].action],system.node_list[n].aux));
			} else
				write(format(NodeStatus[system.node_list[n].status],system.node_list[n].aux));

			write("\r\n");
		}			
		break;
	case "logonlist":
		send_file(system.data_dir + "logon.lst");
		break;

	case "automsg":
		send_file(system.data_dir + "msgs/auto.msg");
		break;
}

field = request.split(':');
switch(field[0]) {
	case "grp":
		for(g in msg_area.grp_list) 
			if(msg_area.grp_list[g].name.toLowerCase()==field[1]) {
				for(s in msg_area.grp_list[g].sub_list)
					writeln(format("1[%s] %s\tsub:%s\t%s\t%u"
						,msg_area.grp_list[g].name
						,msg_area.grp_list[g].sub_list[s].description
						,msg_area.grp_list[g].sub_list[s].code.toLowerCase()
						,system.inetaddr
						,GOPHER_PORT
						));
				break;
			}
		break;
	case "lib":
		for(l in file_area.lib_list) 
			if(file_area.lib_list[l].name.toLowerCase()==field[1]) {
				for(d in file_area.lib_list[l].dir_list)
					writeln(format("1[%s] %s\tdir:%s\t%s\t%u"
						,file_area.lib_list[l].name
						,file_area.lib_list[l].dir_list[d].description
						,file_area.lib_list[l].dir_list[d].code.toLowerCase()
						,system.inetaddr
						,GOPHER_PORT
						));
				break;
			}
		break;
	case "sub":
		msgbase = new MsgBase(field[1]);
		if(Number(field[2])) {
			hdr=msgbase.get_msg_header(false,Number(field[2]));
			if(hdr.attr&MSG_DELETE)
				break;
			writeln(format("Subj : %s",hdr.subject));
			writeln(format("To   : %s",hdr.to));
			writeln(format("From : %s",hdr.from));
			writeln(format("Date : %s",system.timestr(hdr.when_written_time)));
			writeln("");
			body=msgbase.get_msg_body(false,Number(field[2]),true)
			writeln(body);
			break;
		}
/**
		msginfo=format("%-25.25s %-25.25s %-25.25s %s"
				,"Subject"
				,"From"
				,"To"
				,"Date"
				);
		writeln(format("0%s\tnull\tnull\tnull\r\n",msginfo));
**/
		first = msgbase.first_msg;
		for(i=msgbase.last_msg;i>=first;i--) {
			hdr=msgbase.get_msg_header(false,i);
			if(hdr==null)
				continue;
			if(hdr.attr&MSG_DELETE)
				continue;
			msginfo=format("%-25.25s %-25.25s %-25.25s %s"
				,hdr.subject
				,hdr.from
				,hdr.to
				,system.timestr(hdr.when_written_time)
				);
			writeln(format("0%s\tsub:%s:%u\t%s\t%u"
				,msginfo
				,field[1]
				,i
				,system.inetaddr
				,GOPHER_PORT
				));
		}
		msgbase.close();
		break;
	case "dir":
		/* to-do */
		break;
}

/* End of gopherservice.js */
