// fingerservice.js

// Synchronet Service for the Finger protocol (RFC 1288)

// $Id$

// Example configuration (in ctrl/services.cfg):

// Finger		79	0-unlimited	0		fingerservice.js

// Add -n to the configuration line to eliminate user age and gender
// information from the query results.

// !WARNING!
// Finger is an open protocol utilizing no forms of authorization
// or authentication. FINGER IS A KNOWN AND ACCEPTED SECURITY RISK. 
// Detailed information about your system and its users is made 
// available to ANYONE using this service. If there is anything in
// this script that you do not want to be made available to anyone
// and everyone, please comment-out (using /* and */) that portion
// of the script.

const REVISION = "$Revision$".split(' ')[1];

var include_age_gender=true;
var include_real_name=true;

load("nodedefs.js");
load("sockdefs.js");

for(i=0;i<argc;i++) {
	switch(argv[i].toLowerCase()) {
		case "-n":	// no age or gender
			include_age_gender = false;
			break;
		case "-a":	// aliases only
			include_real_name = false;
			break;
	}
}


var output_buf = "";

// Write a string to the client socket
function write(str)
{
	output_buf += str;
}

// Write all the output at once
function flush()
{
	client.socket.send(output_buf);
}

// Write a crlf terminated string to the client socket
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
		writeln(txt[l]);
}

// Returns true if a connection on the local 'port' was succesful
function test_port(port)
{
	sock = new Socket();
	success = sock.connect("localhost",port);
	sock.close();

	return(success);
}

// Get Finger Request (the main entry point) 
if(datagram == undefined)	// TCP
	request = client.socket.recvline(128 /*maxlen*/, 10 /*timeout*/);
else						// UDP
	request = datagram;

if(request==null) {
	log("!TIMEOUT waiting for request");
	exit();
}

request = truncsp(request);

log("client request: " + request);

if(request.substr(0,2).toUpperCase()=="/W")	// "higher level of verbosity"
	request=request.slice(2);				// ignored...

while(request.charAt(0)==' ')	// skip prepended spaces
	request=request.slice(1);

if(request=="") {	// no specific user requested, give list of active users
	log("client requested active user list");
	write(format("%-25.25s %-31.31s   Time   %7s Node\r\n"
		,"User","Action",include_age_gender ? "Age Sex":""));
	var dashes="----------------------------------------";
	write(format("%-25.25s %-31.31s %8.8s %3.3s %3.3s %4.4s\r\n"
		,dashes,dashes,dashes
		,include_age_gender ? dashes : ""
		,include_age_gender ? dashes : ""
		,dashes));
	var user = new User(1);
	for(n=0;n<system.node_list.length;n++) {
		if(system.node_list[n].status!=NODE_INUSE)
			continue;
		user.number=system.node_list[n].useron;
		if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
			action=format("running %s",user.curxtrn);
		else
			action=format(NodeAction[system.node_list[n].action]
							,system.node_list[n].aux);
		t=time()-user.logontime;
		if(t&0x80000000) t=0;
		write(format("%-25.25s %-31.31s%3u:%02u:%02u %3s %3s %4d\r\n"
			,user.alias
			,action
			,Math.floor(t/(60*60))
			,Math.floor(t/60)%60
			,t%60
			,include_age_gender ? user.age.toString() : ""
			,include_age_gender ? user.gender : ""
			,n+1
			));
	}
	flush();
	exit();
}

if(request.charAt(0)=='?') {	// Handle "special" requests
	request=request.slice(1);
	switch(request.toLowerCase()) {

		case "ver":
			writeln("Synchronet Finger Service " + REVISION);
			writeln(server.version);
			writeln(system.version_notice + system.revision);
			writeln("Compiled " + system.compiled_when + " with " + system.compiled_with);
			writeln(system.js_version);
			writeln(system.os_version);
			break;

		case "time":
			t=time();
			writeln(system.timestr(t) + " " + system.zonestr() + " 0x" + t.toString(16));
			break;

		case "logon.lst":
			send_file(system.data_dir + "logon.lst");
			break;

		case "auto.msg":
			send_file(system.data_dir + "msgs/auto.msg");
			break;

		case "sockopts":
			for(i in sockopts)
				writeln(format("%-10s = %d"
					,sockopts[i],client.socket.getoption(sockopts[i])));
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
					t=time()-user.logontime;
					if(t&0x80000000) t=0;
					write(format(" for %u minutes",Math.floor(t/60)));
				} else
					write(format(NodeStatus[system.node_list[n].status],system.node_list[n].aux));

				write("\r\n");
			}			
			break;

		case "services":	/* Services running on this host */
			if(test_port(23))
				writeln("Telnet");
			if(test_port(513))
				writeln("RLogin");
			if(test_port(21))
				writeln("FTP");
			if(test_port(25))
				writeln("SMTP");
			if(test_port(110))
				writeln("POP3");
			if(test_port(119))
				writeln("NNTP");
			if(test_port(70))
				writeln("Gopher");
			if(test_port(80))
				writeln("HTTP");
			if(test_port(113))
				writeln("IDENT");
			if(test_port(6667))
				writeln("IRC");
			break;

		default:
			writeln("Supported special requests (prepended with '?'):");
			writeln("\tver");
			writeln("\ttime");
			writeln("\tstats");
			writeln("\tservices");
			writeln("\tsockopts");
			writeln("\tnodelist");
			writeln("\tauto.msg");
			writeln("\tlogon.lst");
			log(format("!UNSUPPORTED SPECIAL REQUEST: '%s'",request));
			break;
	}
	flush();
	exit();
}

// User info is handled here

var usernum=Number(request);
if(!usernum) {
	var at = request.indexOf('@');
	if(at>0)
		request = request.substr(0,at-1);

	usernum = system.matchuser(request);
	if(!usernum) {
		log(format("!UNKNOWN USER: '%s'",request));
		exit();
	}
}
var user = new User(usernum);
if(user == null) {
	log(format("!INVALID USER NUMBER: %d",usernum));
	exit();
}

uname = format("%s #%d", user.alias, user.number);
write(format("User: %-30s", uname));
if(include_real_name)
	write(format(" In real life: %s", user.name));
write("\r\n");

write(format("From: %s\r\n",user.location));
if(include_age_gender) {
	birth=format("Birth: %s (Age: %u years)"
		  ,user.birthdate,user.age);
	write(format("%-42s Gender: %s\r\n"
		  ,birth,user.gender));
}
write(format("Shell: %-34s  Editor: %s\r\n"
	  ,user.command_shell,user.editor));
write(format("Last login %s %s\r\nvia %s from %s [%s]\r\n"
	  ,system.timestr(user.stats.laston_date)
	  ,system.zonestr()
	  ,user.connection
	  ,user.host_name
	  ,user.ip_address));

flush();

/* End of fingerservice.js */
