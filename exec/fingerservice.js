// fingerservice.js

// Synchronet Service for the Finger protocol (RFC 1288)

load("nodedefs.js");

// This is just an example of how you access command-line args
for(i=0;i<argc;i++)
	log(format("argv[%d]=%s",i,argv[i]));
// End of example :-)

// Write a string to the client socket
function write(str)
{
	client.socket.send(str);
}

// Get Finger Request
request = client.socket.recvline(128 /*maxlen*/, 3 /*timeout*/);

if(request==null) {
	log("!TIMEOUT waiting for request");
	exit();
}

log(format("client request: '%s'",request));

if(request.substr(0,2).toUpperCase()=="/W")	// "higher level of verbosity"
	request=request.slice(2);

while(request.charAt(0)==' ')	// skip prepended spaces
	request=request.slice(1);

if(request=="") {	/* no specific user requested, give list of active users */
	log("client requested active user list");
	write(format("%-25.25s %-40.40s Age Sex Node\r\n"
		,"User","Action"));
	var dashes="----------------------------------------";
	write(format("%-25.25s %-40.40s %.3s %.3s %.4s\r\n"
		,dashes,dashes,dashes,dashes,dashes));
	var user = new User(1);
	for(n=0;n<system.node_list.length;n++) {
		if(system.node_list[n].status!=NODE_INUSE)
			continue;
		user.number=system.node_list[n].useron;
		var action=format(NodeAction[system.node_list[n].action]
			,system.node_list[n].aux);
		write(format("%-25.25s %-40.40s %3d %3s %4d\r\n"
			,user.alias
			,action
			,user.age
			,user.gender
			,n+1
			));
	}
	exit();
}

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

write(format("Site: %s\r\n"
	  ,system.inetaddr));
write(format("Login name: %-30s In real life: %s\r\n"
	  ,user.alias,user.name));
write(format("Shell: %s\r\n",user.command_shell));
write(format("Last login %s via %s from %s [%s]\r\n"
	  ,system.timestr(user.stats.laston_date)
	  ,user.connection
	  ,user.host_name
	  ,user.ip_address));

/* End of fingerservice.js */
