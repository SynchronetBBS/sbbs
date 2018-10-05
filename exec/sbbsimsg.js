// sbbsimsg.js

// Synchronet inter-bbs instant message module

/* History of this module/feature:
  
   Original 2001: Queried systems using Finger (TCP port 79)
                  Sent message using SMTP-SOML (TCP port 25)
   Rev 1.11 2002: Queried system using both TCP and UDP-Finger (port 79)
                  Still sent messages using SMTP-SOML
   Rev 1.22 2007: Queried using SYSTAT/ActiveUser protocol (TCP & UDP port 11)
                  in addition to Finger (TCP and UDP port 79)
                  If send via SMTP failed, used MSP protocol (TCP port 18)
   Rev 1.25 2009: Removed Finger (both TCP and UDP support), SYSTAT-TCP
                  and SMTP support, so it now *only*:
				  Queries using SYSTAT/ActiveUser protocol over UDP port 11
				  Sends messages using MSP (TCP port 18)

   So, while originally the requirements for systems to participate were:
   - Synchronet SMTP Server listening on TCP port 25
   - fingerservice.js listing on TCP port 79

   Now, the requirements are:
   - activeuserservice.js (or fingerservice.js) listening on UDP port 11
   - mspservice.js listening on TCP port 18
*/

// $Id$

const UDP_RESPONSE_TIMEOUT = 5000	// milliseconds

load("sbbsdefs.js");
load("nodedefs.js");
load("sockdefs.js");	// SOCK_DGRAM
var options=load({}, "modopts.js", "sbbsimsg");
if(!options)
	options = {};
if(!options.from_user_prop)
	options.from_user_prop = "alias";
var userprops = load({}, "userprops.js");
var ini_section = "imsg sent";
var addr_list = userprops.get(ini_section, "address", []);
var last_send = userprops.get(ini_section, "localtime");

const RcptAddressHistoryLength = 10;

// Global vars
var imsg_user;
var last_user=0;
var users=0;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-l") {
		list_users(true);
		exit();
	}

// Read the list of systems into list array
fname = system.ctrl_dir + "sbbsimsg.lst";

f = new File(fname);
if(!f.open("r")) {
	alert("Error opening " + fname);
	exit();
}

var sys = new Array();
list = f.readAll();
f.close();
for(i in list) {
	if(list[i]==null)
		break;
	var line = list[i].trimLeft();
	if(line.charAt(0)==';')		// comment? 
		continue;

	var word = line.split('\t');
	var host = word[0].trimRight();

	if(host == system.host_name
		|| host == system.inetaddr)		// local system?
		continue;						// ignore

	var ip_addr = word[1];
	var bbs_name = word[2];

	if(ip_addr == client.socket.local_ip_address || ip_addr == server.interface_ip_address)
		continue;

	sys.push( { addr: host, ip : ip_addr, name: bbs_name, failed: false, reply: 999999 } );
}

function save_sys_list()
{
//	sys.sort(sortarray);
	fname = system.ctrl_dir + "sbbsimsg.lst";
	f = new File(fname);
	if(!f.open("w"))
		return;
	for(i=0;sys[i]!=undefined;i++) {
		if(sys[i].ip == undefined)
			f.writeln(sys[i].addr);
		else
			f.writeln(format("%-63s\t%s\t%s", sys[i].addr, sys[i].ip, sys[i].name));
	}
	f.close();
}

function sortarray(a, b)
{
	return(a.reply-b.reply);
}

function parse_response(response, verbosity, sys)
{
	// Skip header
	while(response.length && response[0].charAt(0)!='-')
		response.shift();
	if(response.length && response[0].charAt(0)=='-')
		response.shift();	// Delete the separator line
	while(response.length && !response[0].length)
		response.shift();	// Delete any blank lines
	while(response.length && !response[response.length-1].length)
		response.pop();		// Delete trailing blank lines

	if(!response.length) {
		if(verbosity > 1)
			print();
		return;
	}

	if(verbosity < 1)
		printf("\1n\1h%-25.25s ", sys.name);
	printf("\1g\1h                                  Time   Age Sex\r\n");

	for(j in response) {
		if(response[j]=="")
			continue;

		console.line_counter=0;	// defeat pause
		print(format("\1h\1y%.25s\1n\1g %.48s"
			,response[j],response[j].slice(26)));
		var u = new Object;
		u.host = sys.addr;
		u.bbs  = sys.name;
		u.ip   = sys.ip;
		u.name = format("%.25s",response[j]);
		u.name = truncsp(u.name);
		imsg_user.push(u);
		users++;
	}
}

function list_users(verbosity)
{
	imsg_user = new Array();
	var udp_req=0;
	var replies=0;

	users = 0;
	start = new Date();
	print(format("\1m\1hListing %sUsers (Ctrl-C to Abort)...", verbosity ? "Systems and " : ""));

	sock = new Socket(SOCK_DGRAM);
	//sock.debug=true;
	for(var i=0; sys[i]!=undefined && !(bbs.sys_status&SS_ABORT);i++) {
		if(sys[i].ip==undefined)
			continue;
		if(!sock.sendto("\r\n",sys[i].ip,IPPORT_SYSTAT))	// Get list of active users
			continue;
		udp_req++;
	}

	begin = new Date();
	for(var loop = 0; replies<udp_req && new Date().valueOf()-begin.valueOf() < UDP_RESPONSE_TIMEOUT
		&& !(bbs.sys_status&SS_ABORT); loop++)
	{
		printf("%c\r", "/-\\|"[loop%4]);
		if(!sock.poll(0.25))
			continue;

		message=sock.recvfrom(20000);
		if(message==null)
			continue;
		var found = get_system_by_ip(message.ip_address);
		if(!found) {
			printf("Unexpected response from %s\r\n", message.ip_address);
			continue;
		}
		replies++;
		found.reply=new Date().valueOf()-start.valueOf();

		response=message.data.split("\r\n");

		if(verbosity > 1) {
			console.line_counter=0;	// defeat pause
			printf("\1n\1h%-25.25s\1n ",found.name);
		}

		parse_response(response, verbosity, found);
	}
	console.clearline();
	sock.close();

	if(verbosity > 1) {
		t = new Date().valueOf()-start.valueOf();
		printf("\1m\1h%lu systems and %lu users listed in %d seconds.\r\n"
			,replies, users, t/1000);
	}
	save_sys_list();
}

function get_system_by_ip(ip)
{
	for(var i in sys)
		if(sys[i].ip==ip)
			return(sys[i]);
	return null;
}

function send_msg(dest, msg)
{

	if((hp = dest.indexOf('@'))==-1) {
		alert("Invalid user");
		exit();
	}
	host = dest.slice(hp+1);
	destuser = dest.substr(0,hp);

	printf("\1h\1ySending...\r\1w");
	sock = new Socket();
	//sock.debug = true;
	do {
		if(!sock.connect(host,IPPORT_MSP)) {
			alert("MSP Connection to " + host + " failed with error " + sock.last_error);
		}
		else {
			sock.send("B"+destuser+"\0"+/* Dest node +*/"\0"+msg+"\0"+user[options.from_user_prop]+"\0"+"\0\0"+system.name+"\0");
		}
	} while(0);

	sock.close();

	var addr_idx = addr_list.indexOf(dest);
	if(addr_idx >= 0)
		addr_list.splice(addr_idx, 1);
	addr_list.unshift(dest);
	if(addr_list.length > RcptAddressHistoryLength)
		addr_list.length = RcptAddressHistoryLength;
	userprops.set(ini_section, "address", addr_list);
	last_send = new Date().toString();
	userprops.set(ini_section, "localtime", last_send);
}

function getmsg()
{
	var lines=0;
	var msg="";
	const max_lines = 5;

	printf("\1n\1g\1h%lu\1n\1g lines maximum (blank line sends, Ctrl-C to abort)\r\n",max_lines);
	while(bbs.online && lines<max_lines) {
		console.print("\1n: \1h");
		mode=0;
		if(lines+1<max_lines)
			mode|=K_WRAP;
		str=console.getstr(76, mode);
		if(str=="")
			break;
		msg+=str;
		msg+="\r\n";
		lines++;
	}

	if(!lines || !bbs.online || bbs.sys_status&SS_ABORT)
		return("");

	return(msg);
}

function get_default_dest()
{
	var rx = userprops.get("imsg received");
	
	if(rx && rx.localtime && (!last_send || new Date(rx.localtime) > new Date(last_send))) {
		var sys = get_system_by_ip(rx.ip_address);
		if(sys)
			return rx.name + '@' + sys.addr;
		return rx.name + '@' + rx.ip_address;
	}
	if(addr_list.length)
		return addr_list[0];
	if(imsg_user.length)
		return format("%s@%s",imsg_user[last_user].name,imsg_user[last_user].host);
	return "";
}

list_users(0);	// Needed to initialize imsg_user[]
console.crlf();

var key;
menu:
while(bbs.online) {
	console.line_counter=0;	// defeat pause
	console.print("\1n\1h\1bInter-BBS: ");
	console.mnemonics("~Telegram, ~Message, ~List, or ~Quit: ");
	bbs.sys_status&=~SS_ABORT;
	while(bbs.online && !(bbs.sys_status&SS_ABORT)) {
		key=console.inkey(K_UPPER, 500);
		if(key=='Q' || key=='L' || key=='T' || key=='M' || key=='\r')
			break;
		if(system.node_list[bbs.node_num-1].misc&(NODE_MSGW|NODE_NMSG)) {
			console.line_counter=0;	// defeat pause
			console.saveline();
			console.crlf();
			bbs.nodesync();
			console.crlf();
			console.restoreline();
		}
	}
//	printf("key=%s\r\n",key);
	switch(key) {
		case 'L':
			print("\1h\1cList\r\n");
			list_users(0);
			console.crlf();
			break;
		case 'T':
			printf("\1h\1cTelegram\r\n\r\n");
			printf("\1n\1h\1y(user@hostname): \1w");
			dest=console.getstr(get_default_dest(),64,K_EDIT|K_AUTODEL, addr_list);
			if(dest==null || dest=='' || bbs.sys_status&SS_ABORT)
				break;
			if((msg=getmsg())=='')
				break;
			send_msg(dest,msg);
			console.crlf();
			break;
		case 'M':
			print("\1h\1cMessage\r\n");
			if(!imsg_user.length) {
				alert("No active users!\r\n");
				break;
			}
			done=false;
			while(bbs.online && !done) {
				printf("\r\1n\1h\x11\1n-[\1hQ\1nuit]-\1h\x10 \1y%-25s \1c%s\1>"
					,imsg_user[last_user].name,imsg_user[last_user].bbs);
				switch(console.getkey(K_UPPER|K_NOECHO)) {
					case '+':
					case '>':
					case ']':
					case '\x06':	/* right arrow */
					case 'N':
					case '\n':		/* dn arrrow */
						last_user++;
						if(last_user>=imsg_user.length)
							last_user=0;
						break;

					case '-':
					case '<':
					case '[':
					case '\x1d':	/* left arrow */
					case 'P':
					case '\x1e':	/* up arrow */
						last_user--;
						if(last_user<0)
							last_user=imsg_user.length-1;
						break;
					case '\x1b':	/* ESC */
					case 'Q':
						printf("\r\1>");
						done=true;
						break;
					case '\r':
						done=true;
						dest=format("%s@%s"
							,imsg_user[last_user].name,imsg_user[last_user].host);
						printf("\r\1n\1cSending message to \1h%s\1>\r\n",dest);
						if((msg=getmsg())=='')
							break;
						send_msg(dest,msg);
						console.crlf();
						break;
				}
			}
			break;
		default:
			print("\1h\1cQuit");
			break menu;
	}
}
