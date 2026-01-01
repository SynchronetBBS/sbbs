// gopherservice.js

// Synchronet Service for the Gopher Protocol (RFC 1436)

// Example configuration (in ctrl/services.ini):

// [Gopher]
// Port=70
// MaxClients=10
// Options=0
// Command=gopherservice.js

load("sbbsdefs.js");
load("nodedefs.js");

"use strict";

const REVISION = "1.18";
const GOPHER_PORT = client.socket.local_port;

var debug = false;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-d")
		debug = true;

// Write a string to the client socket
function write(str)
{
	if(debug)
		log(LOG_DEBUG,"rsp: " + str);
	client.socket.send(str);
}

function writeln(str)
{
	write(str + "\r\n");
}

// Send the contents of a text file to the client socket
function send_file(fname, prefix, suffix)
{
	if (!prefix)
		prefix = "";
	if (!suffix)
		suffix = "";
	f = new File(fname);
	if(!f.open("r")) 
		return;
	txt = f.readAll();
	f.close();
	for(l in txt)
		writeln(prefix + strip_ctrl(txt[l]) + suffix);
}

function getQwkBulletins() {
	var dir = system.text_dir + 'qwk';
	if (!file_isdir(dir)) return [];
	var bulletins = directory(backslash(dir) + '*');
	return bulletins;
}

var msgbase=null;

if(!login("guest"))
	log(LOG_WARNING,"!WARNING: NO GUEST ACCOUNT CONFIGURED");

// Get Request
request = client.socket.recvline(512 /*maxlen*/, 10 /*timeout*/);

if(request==null) {
	log(LOG_WARNING,"!TIMEOUT waiting for request");
	exit();
}

log(LOG_DEBUG,"client request: '" + request + "'");

var prefix="";
var gopher_plus=false;
if((term=request.indexOf("\t+"))>=0) {
	gopher_plus=true;
	request=request.substr(0,term);
} else if((term=request.indexOf("\t$"))>=0) {
	prefix="+INFO: ";
	gopher_plus=true;
	request=request.substr(0,term);
}

if(gopher_plus)
	writeln("+-1");	// indicates '.' terminated data

if(request[0] == '/')
	request = request.slice(1);

if(request=="") { /* "root" */
	var files = directory(system.text_dir + "gopher_welcome*.msg");
	for (var i in files) {
		send_file(files[i], prefix + 'i', '\t\tfake\t1');
	}
	if (msg_area.grp_list.length) {
		writeln(prefix + 'iMessage Groups\t\tfake\t1');
		for(g in msg_area.grp_list) {
			writeln(
				prefix
				+ "1" + msg_area.grp_list[g].description 
				+ "\tgrp:" + msg_area.grp_list[g].name.toLowerCase() 
				+ "\t" + system.host_name
				+ "\t" + GOPHER_PORT
			);
		}
		writeln(prefix + 'i \t\tfake\t1');
	}
	if (file_area.lib_list.length) {
		writeln(prefix + 'iFile Libraries\t\tfake\t1');
		for(l in file_area.lib_list) {
			writeln(format("1%s\tlib:%s\t%s\t%u"
				,file_area.lib_list[l].description
				,file_area.lib_list[l].name.toLowerCase()
				,system.host_name
				,GOPHER_PORT
			));
		}
		writeln(prefix + 'i \t\tfake\t1');
	}
	var qwkBulletins = getQwkBulletins();
	if (qwkBulletins.length) {
		writeln(prefix + 'iQWK Bulletins\t\tfake\t1');
		qwkBulletins.forEach(function (e) {
			var f = file_getname(e);
			writeln(prefix + '0' + f + '\tqwkbulletin:' + f + '\t' + system.host_name + '\t' + GOPHER_PORT);
		});
		writeln(prefix + 'i \t\tfake\t1');
	}
	writeln(prefix + 'iSystem Information\t\tfake\t1');
	writeln(prefix
		+ "0Node List\tnodelist"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);
	writeln(prefix
		+ "0Logon List\tlogonlist"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);
	writeln(prefix
		+ "0Auto-Message\tautomsg"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);
	writeln(prefix
		+ "0System Statistics\tstats"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);
	writeln(prefix
		+ "0System Time\ttime"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);
	writeln(prefix
		+ "0Version Information\tver"
		+"\t" + system.host_name
		+"\t" + GOPHER_PORT);

	writeln(".");
	exit();
}

switch(request) {
	case "nodelist":
		var u = new User(1);
		for(n=0;n<system.node_list.length;n++) {
			var node = system.node_list[n];
			write(format("Node %2d ",n+1));
			if(node.status==NODE_INUSE) {
				u.number=node.useron;
				//write(format("%s (%u %s) ", u.alias, u.age, u.gender));
				write(u.alias + " (" + u.age + " " + u.gender +") ");
				if(node.activity)
					write(strip_ctrl(node.activity));
				else if(node.action==NODE_XTRN && node.aux)
					write("running %s" + u.curxtrn);
				else
					write(format(NodeAction[node.action],node.aux));
			} else
				write(strip_ctrl(node.vstatus || format(NodeStatus[node.status],node.aux)));

			write("\r\n");
		}			
		break;
	case "logonlist":
		send_file(system.data_dir + "logon.lst");
		break;

	case "automsg":
		send_file(system.data_dir + "msgs/auto.msg");
		break;

	case "ver":
		writeln("Synchronet Gopher Service " + REVISION);
		writeln(server.version);
		writeln(system.version_notice + system.revision + system.beta_version);
		writeln("Compiled " + system.compiled_when + " with " + system.compiled_with);
		writeln(system.js_version);
		writeln(system.os_version);
		break;

	case "time":
		t=time();
		writeln(system.timestr(t) + " " + system.zonestr() + " 0x" + t.toString(16));
		break;

	case "stats":	/* Statistics */
		for(i in system.stats)
			writeln(format("%-25s = ", i) + system.stats[i]);

		total	= time()-system.uptime;
		days	= Math.floor(total/(24*60*60));
		if(days) 
			total%=(24*60*60);
		hours	= Math.floor(total/(60*60));
		min		= (Math.floor(total/60))%60;
		sec		= total%60;

		writeln(format("uptime = %u days, %u hours, %u minutes and %u seconds"
			,days,hours,min,sec));
		break;
}

field = request.split(':');
if(field.length > 1) {
	switch(field[0]) {
		case "grp":
			for(g in msg_area.grp_list) 
				if(msg_area.grp_list[g].name.toLowerCase()==field[1]) {
					for(s in msg_area.grp_list[g].sub_list)
						writeln(prefix
							+ "1[" + msg_area.grp_list[g].name + "] "
							+ msg_area.grp_list[g].sub_list[s].description 
							+ "\tsub:"
							+ msg_area.grp_list[g].sub_list[s].code.toLowerCase() 
							+ "\t"
							+ system.host_name 
							+ "\t"
							+ GOPHER_PORT
							);
					break;
				}
			break;
		case "lib":
			for(l in file_area.lib_list) 
				if(file_area.lib_list[l].name.toLowerCase()==field[1]) {
					for(d in file_area.lib_list[l].dir_list) {
						if(file_area.lib_list[l].dir_list[d].can_download
						&& file_area.lib_list[l].dir_list[d].settings&DIR_FREE)
							writeln(format(prefix
								+ "1[%s] %s\tdir:%s\t%s\t%u"
								,file_area.lib_list[l].name
								,file_area.lib_list[l].dir_list[d].description
								,file_area.lib_list[l].dir_list[d].code.toLowerCase()
								,system.host_name
								,GOPHER_PORT
								));
					}
					break;
				}
			break;
		case "sub":
			if(!msg_area.sub[field[1]]) {
				writeln(log(LOG_NOTICE, "Invalid message area: " + field[1]));
				break;
			}
			msgbase = new MsgBase(field[1]);
			if(msgbase.open!=undefined && msgbase.open()==false) {
				writeln("!ERROR " + msgbase.last_error);
				break;
			}

			if(Number(field[2])) {
				hdr=msgbase.get_msg_header(false,Number(field[2]));
				if(hdr==null || hdr.attr&MSG_DELETE)
					break;
				writeln("Subj : " + hdr.subject);
				writeln("To   : " + hdr.to);
				writeln("From : " + hdr.from);
				writeln("Date : " + system.timestr(hdr.when_written_time));
				writeln("");
				body=msgbase.get_msg_body(false,Number(field[2]),true,true)
				writeln(body);
				msgbase.close();
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
			var hdr_list = msgbase.get_all_msg_headers();
			first = msgbase.first_msg;
			for(i=msgbase.last_msg; hdr_list != null && i>=first; i--) {
				hdr = hdr_list[i];
				if(hdr==null)
					continue;
				if(hdr.attr&MSG_DELETE)
					continue;
				date = system.timestr(hdr.when_written_time);
				msginfo=format("%-25.25s %-25.25s %-25.25s %s"
					,hdr.subject
					,hdr.from
					,hdr.to
					,date
					);
				writeln("0" + msginfo + "\tsub:"
					+ field[1] + ":" + i + "\t"
					+ system.host_name + "\t"
					+ GOPHER_PORT
					);
			}
			msgbase.close();
			break;
		case "dir":
			var code=field[1];
			if((dir = file_area.dir[code]) != undefined) {
				var files = directory(dir.path + '*');
				for(i in files) {
					var fileinfo=format("%-25.25s %10u   %s"
						,file_getname(files[i])
						,file_size(files[i])
						,system.timestr(file_date(files[i]))
						);
					writeln("9" + fileinfo + "\tfile:"
						+ code + "/" + file_getname(files[i]) + "\t"
						+ system.host_name + "\t"
						+ GOPHER_PORT
						);
				}
			}
			break;
		case "file":
			var args=field[1].split('/');
			var code=args[0];
			if((dir = file_area.dir[code]) != undefined) {
				if(dir.can_download	&& dir.settings&DIR_FREE) {
					var fname = dir.path + file_getname(args[1]);
					if(file_exists(fname)) {
						log(LOG_INFO,format("Downloading file: %s (%u bytes)"
							,fname, file_size(fname)));
						if(client.socket.sendfile(fname))
							log(LOG_INFO,"Sent file: " + fname);
						else
							log(LOG_NOTICE,"!ERROR sending file: " + fname);
					}	
				}
			}
			break;
		case "qwkbulletin":
			var fname = backslash(system.text_dir + 'qwk') + field[1];
			if (file_exists(fname)) client.socket.sendfile(fname);
			break;
	}
}

writeln(".");

/* End of gopherservice.js */
