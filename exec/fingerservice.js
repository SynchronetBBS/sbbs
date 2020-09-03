// $Id: fingerservice.js,v 1.48 2020/01/12 00:46:45 rswindell Exp $
// vi: tabstop=4

// Synchronet Service for the Finger protocol (RFC 1288)
// and/or the Active Users (SYSTAT) protocol (RFC 866)

// Example configurations (in ctrl/services.ini)
//
// [Finger]
// Port=79
// Command=fingerservice.js
//
// [ActiveUser-UDP]
// Port=11
// Options=UDP
// Command=fingerservice.js -u

// Example configuration (in ctrl/modopts.ini) with default values:
//
// [fingerservice]
// include_age = true
// include_gender = true
// include_location = true
// include_real_name = true
// findfile = false
// bbslist = false

// Command-line options:
//
// -n	add to the Command line to eliminate user age and gender
//		information from the query results.
// -a	report aliases only (no real names)
// -ff	enable the findfile feature (requires a "guest" account)
// -u   report users only (ignore any request), a.k.a. Active Users protocol

// !WARNING!
// Finger is an open protocol utilizing no forms of authorization
// or authentication. FINGER IS A KNOWN AND ACCEPTED SECURITY RISK. 
// Detailed information about your system and its users is made 
// available to ANYONE using this service. If there is anything in
// this script that you do not want to be made available to anyone
// and everyone, please comment-out (using /* and */) that portion
// of the script or use the command-line options or modopts.ini to
// disable those elements.

"use strict";
const REVISION = "$Revision: 1.48 $".split(' ')[1];

var active_users = false;	// Active-Users/SYSTAT protocol mode (Finger when false)
var options = load({}, 'modopts.js', 'fingerservice');
if(!options)
	options = {};
if(options.include_age === undefined)
	options.include_age = true;
if(options.include_gender === undefined)
	options.include_gender = true;
if(options.include_location === undefined)
	options.include_location = true;
if(options.include_real_name === undefined)
	options.include_real_name = true;
if(options.findfile === undefined)
	options.findfile = false;
if(options.bbslist === undefined)
	options.bbslist = false;

load("nodedefs.js");
load("sockdefs.js");
load("sbbsdefs.js");
load("portdefs.js");
var presence = load({}, "presence_lib.js");
if(options.bbslist)
	var sbbslist = load({}, "sbbslist_lib.js");

for(i=0;i<argc;i++) {
	switch(argv[i].toLowerCase()) {
		case "-n":	// no age or gender
			options.include_age = false;
			options.include_gender = false;
			break;
		case "-a":	// aliases only
			options.include_real_name = false;
			break;
		case "-ff":	// enable findfile (requires "guest" account)
			options.findfile=true;
			break;
        case "-u": // Active Users only
            active_users=true;
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
	var f = new File(fname);
	if(!f.open("r")) 
		return;
	var txt = f.readAll();
	f.close();
	for(var l in txt)
		writeln(txt[l]);
}

// Returns true if a connection on the local 'port' was succesful
function test_port(port)
{
	var sock = new Socket();
	var success = sock.connect(system.host_name,port);
	sock.close();

	return(success);
}

function done()
{
	flush();
	exit();
}

var request="";

if(datagram || !active_users) {

    // Get Finger Request (the main entry point) 
    if(datagram == undefined) 	// TCP
        request = client.socket.recvline(128 /*maxlen*/, 10 /*timeout*/);
    else						// UDP
	    request = datagram;

    if(request==null) {
	    log(LOG_WARNING,"!TIMEOUT waiting for request");
	    exit();
    }

    request = truncsp(request);

    log(LOG_DEBUG,"client request: " + request);

    if(request.substr(0,2).toUpperCase()=="/W")	// "higher level of verbosity"
	    request=request.slice(2);				// ignored...

    while(request.charAt(0)==' ')	// skip prepended spaces
	    request=request.slice(1);
}

if(request=="") {	// no specific user requested, give list of active users
	log("client requested active user list");
	write(format("%-25.25s %-31.31s  Time-on %3s %3s Node\r\n"
		,"User","Action"
		,options.include_age ? "Age":""
		,options.include_gender ? "Sex":""
		));
	var dashes="----------------------------------------";
	write(format("%-25.25s %-31.31s %8.8s %3.3s %3.3s %4.4s\r\n"
		,dashes,dashes,dashes
		,options.include_age ? dashes : ""
		,options.include_gender ? dashes : ""
		,dashes));
	var u = new User;
	for(n=0;n<system.node_list.length;n++) {
		var node = system.node_list[n];
		if(node.status!=NODE_INUSE)
			continue;
		if(node.misc&NODE_ANON)
			continue;
		u.number=node.useron;
		var action;
		if(node.action==NODE_XTRN && node.aux)
			action=format("running %s", presence.xtrn_name(u.curxtrn));
		else
			action=format(NodeAction[node.action],node.aux);
		action += presence.node_misc(node, /* is_sysop: */false);
		t=time()-u.logontime;
		if(t&0x80000000) t=0;
		write(format("%-25.25s %-31.31s%3u:%02u:%02u %3s %3s %4d\r\n"
			,u.alias
			,action
			,Math.floor(t/(60*60))
			,Math.floor(t/60)%60
			,t%60
			,options.include_age ? u.age.toString() : ""
			,options.include_gender ? u.gender : ""
			,n+1
			));
	}
	var web_user = presence.web_users(options.web_inactivity_timeout);
	for(var w in web_user) {
		var u = web_user[w];
		t=time()-u.logontime;
		if(t&0x80000000) t=0;
		var action = u.action ? u.action : '';
		action += presence.web_user_misc(u);
		write(format("%-25.25s %-31.31s%3u:%02u:%02u %3s %3s %4d\r\n"
			,u.name
			,action
			,Math.floor(t/(60*60))
			,Math.floor(t/60)%60
			,t%60
			,options.include_age ? u.age.toString() : ""
			,options.include_gender ? u.gender : ""
			,++n
			));
	}
	done();
}

// MODIFICATION BY MERLIN PART 1 STARTS HERE...

if(options.findfile && 0) {	// What is this supposed to do?

	if ((request.slice(0,9)) == "?findfile")  {
		request=request.slice(9);
		request="findfile?".concat(request);
	}

	if ((request.slice(0,9)) == "?filefind")  {
		request=request.slice(9);
		request="findfile?".concat(request);
	}
}

// MODIFICATION BY MERLIN PART 1 ENDS HERE


if(request.charAt(0)=='?' || request.charAt(0)=='.') {	// Handle "special" requests
	request=request.slice(1);
	switch(request.toLowerCase()) {

		case "ver":
			writeln("Synchronet Finger Service " + REVISION);
			writeln(server.version);
			writeln(system.version_notice + system.revision + system.beta_version);
			writeln("Compiled " + system.compiled_when + " with " + system.compiled_with);
			writeln(system.js_version);
			writeln(system.os_version);
			break;

		case "uptime":
			t=system.uptime;
			writeln("Duration: " + String(time()-t));
			writeln(t);
			writeln(system.timestr(t) + " 0x" + t.toString(16));
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
				writeln(format("%-12s = %d"
					,sockopts[i],client.socket.getoption(sockopts[i])));
			break;

		case "stats":	/* Statistics */
			for(i in system.stats)
				writeln(format("%-25s = ", i) + system.stats[i]);

			var total	= time()-system.uptime;
			var days	= Math.floor(total/(24*60*60));
		    if(days) 
				total%=(24*60*60);
			var hours	= Math.floor(total/(60*60));
			var min		= (Math.floor(total/60))%60;
			var sec		= total%60;

			writeln(format("uptime = %u days, %u hours, %u minutes and %u seconds"
				,days,hours,min,sec));
			break;

		case "stats.json":
			write(JSON.stringify(system.stats));
			break;

		case "nodelist":
			options.format = "Node %2d %s";
			var output = presence.nodelist(/* print: */false, /* active: */false, /* self: */true, /* is_sysop: */false, options);
			for(var i in output)
				writeln(output[i]);
			break;

		case "active-users.json":
			var u = new User;
			var list = [];
			for(var n=0;n<system.node_list.length;n++) {
				var node = system.node_list[n];
				if(node.status!=NODE_INUSE)
					continue;
				if(node.misc&NODE_ANON)
					continue;
				u.number=node.useron;
				var action;
				if(node.action==NODE_XTRN && node.aux)
					action=format("running %s", presence.xtrn_name(u.curxtrn));
				else
					action=format(NodeAction[node.action]
								,node.aux);
				var t = time()-u.logontime;
				if(t&0x80000000) t = 0;
				list.push({ 
					name: u.alias, 
					action: action, 
					naction: node.action, 
					aux: node.aux, 
					xtrn: presence.xtrn_name(u.curxtrn), 
					timeon: t, 
					node: n + 1, 
					prot: NodeConnection[node.connection],
					age: options.include_age ? u.age : undefined,
					sex: options.include_gender ? u.gender: undefined, 
					location: options.include_location ? u.location : undefined,
					do_not_disturb: u.chat_settings & CHAT_NOPAGE ? true : undefined,
					msg_waiting: node.misc&(NODE_NMSG|NODE_MSGW) ? true: undefined
				});
			}
			var web_user = presence.web_users(options.web_inactivity_timeout);
			for(var w in web_user) {
				var u = web_user[w];
				t=time()-u.logontime;
				if(t&0x80000000) t=0;
				list.push({ 
					name: u.name, 
					action: u.action,
					timeon: t, 
					node: ++n,
					prot: "web",
					age: options.include_age ? u.age : undefined,
					sex: options.include_gender ? u.gender: undefined, 
					location: options.include_location ? u.location : undefined,
					do_not_disturb: u.do_not_disturb,
					msg_waiting: u.msg_waiting
				});
			}
			write(JSON.stringify(list));
			break;

		case "services":	/* Services running on this host */
            var ports = [];
            for(i in standard_service_port) {
                if(i == "finger")
                    continue;
                if(ports.indexOf(standard_service_port[i]) >= 0) // Already tested this port
                    continue;
                ports.push(standard_service_port[i]);
			    if(test_port(standard_service_port[i]))
				    writeln(i);
            }
			break;

		case "bbslist":
			if(options.bbslist) {
				var list = sbbslist.read_list();
				for(var i in list)
					writeln(list[i].name);
			}
			break;

		default:
			if(options.bbslist && request.indexOf("bbs:") == 0) {
				var list = sbbslist.read_list();
				var index = sbbslist.system_index(list, request.slice(4));
				if(index < 0) {
					writeln("!BBS NOT FOUND: " + request.slice(4));
					break;
				}
				writeln(JSON.stringify(list[index]));
				break;
			}
			if(file_exists(system.data_dir + "finger/" + file_getname(request))) {
				send_file(system.data_dir + "finger/" + file_getname(request));
				break;
			}
			writeln("Supported special requests (prepended with '?' or '.'):");
			writeln("\tver");
			writeln("\ttime");
			writeln("\tstats");
			writeln("\tstats.json");
			writeln("\tservices");
			writeln("\tsockopts");
			writeln("\tnodelist");
			writeln("\tactive-users.json");
			if(options.findfile)
				writeln("\tfindfile");
			if(options.bbslist) {
				writeln("\tbbslist");
				writeln("\tbbs:<name>");
			}
            writeln("\tauto.msg");
			writeln("\tlogon.lst");
			var more = directory(system.data_dir + "finger/*");
			for(var m in more)
				if(!file_isdir(more[m]))
					writeln("\t" + file_getname(more[m]));
			log(format("!UNSUPPORTED SPECIAL REQUEST: '%s'",request));
			break;
	}
	done();
}

// MODIFICATION BY MERLIN PART 3 STARTS HERE...

if(options.findfile) {
	request=request.toLowerCase();

	if ((request.slice(0,9)) == "filefind?")  {
		request=request.slice(9);
		request="findfile?".concat(request);
	}

	if ((request == "filefind") || (request == "findfile") || (request == "")) {
		request="findfile?";
	}

	if ((request.slice(0,9)) == "findfile?")  {
		request=request.slice(9);
		write(format("\r\nFinger FindFile at %s",system.inetaddr));

		if (request.indexOf("?") != -1) {
			writeln("");
			writeln("");
			writeln("Invalid: You can not use wildcards");
			done();
		}

		if (request.indexOf("*") != -1) {
			writeln("");
			writeln("");
			writeln("Invalid: You can not use wildcards");
			done();
		}

		if (request == "") {
			writeln("");
			writeln("");
			writeln(format("Invalid: You must use findfile?filename.ext@%s",system.inetaddr));
			done();
		}
		if(!login("guest")) {
			writeln("\r\n\r\nFailed to login as 'guest'");
			done();
		}

		var notfound = true;
		writeln(format(" searching for '%s'...",request));
		log(format("FindFile searching for: '%s'",request));
		writeln("");
		for(l in file_area.lib_list) {
			for(d in file_area.lib_list[l].dir_list) {
				var dirpath=file_area.lib_list[l].dir_list[d].path;
				dirpath=dirpath.concat(request);
				if (file_exists(dirpath)) {
					var path="ftp://".concat(system.inetaddr,file_area.lib_list[l].dir_list[d].link,request);
					path=path.toLowerCase();
					writeln(format("Found at %s",path));
					notfound=false;
				}
			}
		}
		if (notfound) {
			writeln("Sorry, that file is not available here");
		}
	done();
	}
}

// MODIFICATION BY MERLIN PART 3 ENDS HERE...


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
var u = new User(usernum);
if(u == null) {
	log(format("!INVALID USER NUMBER: %d",usernum));
	exit();
}

var uname = format("%s #%d", u.alias, u.number);
write(format("User: %-30s", uname));
if(options.include_real_name)
	write(format(" In real life: %s", u.name));
write("\r\n");

write(format("From: %-36s Handle: %s\r\n", options.include_location ? u.location : "undisclosed" ,u.handle));
if(options.include_age)
	write(format("%-42s ", format("Birth: %s (Age: %u years)" , u.birthdate,u.age)));
if(options.include_gender)
	write(format("Gender: %s", u.gender));
if(options.include_age || options.include_gender)
	write("\r\n");

write(format("Shell: %-34s  Editor: %s\r\n"
	  ,u.command_shell,u.editor));
write(format("Last login %s %s\r\nvia %s from %s [%s]\r\n"
	  ,system.timestr(u.stats.laston_date)
	  ,system.zonestr()
	  ,u.connection
	  ,u.host_name
	  ,u.ip_address));
var plan;
plan=format("%suser/%04d.plan",system.data_dir,u.number);
if(file_exists(plan)) {
	write("Plan:\r\n");
	send_file(plan);
	}
else
	write("No plan.\r\n");
done();

/* End of fingerservice.js */
