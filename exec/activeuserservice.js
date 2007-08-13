// activeuserservice.js

// Synchronet Service for the Active User protocol (RFC 866)

// $Id$

// Example configurations (in ctrl/services.ini):

// [ActiveUser]
// Port=11
// MaxClients=10
// Options=NO_HOST_LOOKUP
// Command=activeuserservice.js

// [ActiveUser-UDP]
// Port=11
// MaxClients=10
// Options=UDP | NO_HOST_LOOKUP
// Command=activeuserservice.js

// Command-line options:

// -n	to the configuration line to eliminate user age and gender
//		information from the query results.
// -a	report aliases only (no real names)

// !WARNING!
// Active User is an open protocol utilizing no forms of authorization
// or authentication. ACTIVE USER IS A KNOWN AND ACCEPTED SECURITY RISK. 
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
load("sbbsdefs.js");

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

function xtrn_name(code)
{
	if(this.xtrn_area==undefined)
		return(code);

	if(xtrn_area.prog!=undefined)
		if(xtrn_area.prog[code]!=undefined)
			return(xtrn_area.prog[code].name);
	else {	/* old way */
		for(s in xtrn_area.sec_list)
			for(p in xtrn_area.sec_list[s].prog_list)
				if(xtrn_area.sec_list[s].prog_list[p].code.toLowerCase()==code.toLowerCase())
					return(xtrn_area.sec_list[s].prog_list[p].name);
	}
	return(code);
}

function done()
{
	flush();
	exit();
}

log("client requested Active User list");
write(format("%-25.25s %-31.31s   Time   %7s Node\r\n"
	,"User","Action",include_age_gender ? "Age Sex":""));
var dashes="----------------------------------------";
write(format("%-25.25s %-31.31s %8.8s %3.3s %3.3s %4.4s\r\n"
	,dashes,dashes,dashes
	,include_age_gender ? dashes : ""
	,include_age_gender ? dashes : ""
	,dashes));
var u = new User(1);
for(n=0;n<system.node_list.length;n++) {
	if(system.node_list[n].status!=NODE_INUSE)
		continue;
	u.number=system.node_list[n].useron;
	if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
		action=format("running %s",xtrn_name(u.curxtrn));
	else
		action=format(NodeAction[system.node_list[n].action]
						,system.node_list[n].aux);
	t=time()-u.logontime;
	if(t&0x80000000) t=0;
	write(format("%-25.25s %-31.31s%3u:%02u:%02u %3s %3s %4d\r\n"
		,u.alias
		,action
		,Math.floor(t/(60*60))
		,Math.floor(t/60)%60
		,t%60
		,include_age_gender ? u.age.toString() : ""
		,include_age_gender ? u.gender : ""
		,n+1
		));
}
done();
