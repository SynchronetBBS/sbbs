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

// $Id: sbbsimsg.js,v 1.41 2019/07/24 22:17:27 rswindell Exp $

require("sbbsdefs.js", 'K_UPPER');
require("sockdefs.js", 'SOCK_DGRAM');
var options=load({}, "modopts.js", "sbbsimsg");
if(!options)
	options = {};
if(!options.from_user_prop)
	options.from_user_prop = "alias";
var userprops = load({}, "userprops.js");
var lib = load({}, "sbbsimsg_lib.js");

var last_user=0;

lib.read_sys_list();

// Parse arguments
if(this.argc) {
	for(i=0; i<argc; i++) {
		if(argv[i].toLowerCase()=="-l") {
			console.clear(LIGHTGRAY);
			writeln("\1hInter-BBS Active Users:");
			var timeout = 2500;
			var sent = lib.request_active_users();
			if(parseInt(argv[i+1]))
				timeout = parseInt(argv[i+1]);
			function poll_callback(loop)
			{
				printf("%c\1[", "/-\\|"[loop%4]);
				if(console.inkey(0))
					return true;
			}
			lib.poll_systems(sent, 0.25, timeout, poll_callback);
			list_users();
			exit();
		}
		if(argv[i].toLowerCase()=="-d") {
			print(lfexpand(JSON.stringify(lib.sys_list, null, 4)));
			exit();
		}
	}
}

function print_header(sys)
{
	printf("\1n\1c%-25.25s \1h%-34.34s\1h\1gTime   Age Sex\r\n", sys.name, sys.host);
}

function list_user(user, sys)
{
	if(sys)
		print_header(sys);
	var action = user.action;
	if(user.do_not_disturb)
		action += " (P)";
	else if(user.msg_waiting)
		action += " (M)";
	print(format("\1h\1y%-25.25s \1n\1g%-31.31s%9s %3s %3s"	
		,user.name
		,action
		,system.secondstr(user.timeon)
		,user.age ? user.age : ''
		,user.sex ? user.sex : ''
		));
}

function list_users()
{
	for(var i in lib.sys_list) {
		var sys = lib.sys_list[i];
		if(!sys.users.length)
			continue;
		print_header(sys);
		for(var u in sys.users) {
			list_user(sys.users[u]);
		}
	}
}

function send_msg(dest, msg)
{
	var result = lib.send_msg(dest, msg, user[options.from_user_prop]);
	if(result == true)
		print("\1nMessage sent to: \1h" + dest + "\1n successfully");
	else
		alert(result);
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

	if(!lines || !bbs.online || console.aborted) {
		console.aborted = false;
		return("");
	}

	return(msg);
}

function imsg_user_list()
{
	var imsg_user = [];
	for(var i in lib.sys_list) {
		var sys = lib.sys_list[i];
		for(var u in sys.users) {
			var user = sys.users[u];
			imsg_user.push( { name: user.name, host: sys.host, bbs: sys.name } );
		}
	}
	return imsg_user;
}

function get_default_dest(addr_list, last_send)
{
	var rx = userprops.get(lib.props_recv);
	
	if(rx && rx.localtime && (!last_send || new Date(rx.localtime) > new Date(last_send))) {
		var sys = lib.sys_list[rx.ip_address];
		if(sys)
			return rx.name + '@' + sys.host;
		return rx.name + '@' + rx.ip_address;
	}
	if(addr_list.length)
		return addr_list[0];
	var imsg_user = imsg_user_list();
	if(imsg_user.length)
		return format("%s@%s",imsg_user[last_user].name,imsg_user[last_user].host);
	return "";
}

function logon_callback(user, sys)
{
	console.clearline();
	list_user(user, sys);
	console.line_counter=0;	// defeat pause
}

function logoff_callback(user, sys)
{
	console.clearline();
	print(format("\1n\1h\1y%s \1n\1glogged-off \1h\1w%s\1n", user.name, sys.name));
	console.line_counter=0;	// defeat pause
}

prompt:
while(bbs.online) {
	console.line_counter=0;	// defeat pause
	console.clearline();
	console.print("\1n\1h\1bInterBBS: ");
	console.mnemonics("Anyone: ~Telegram, Active-Users: ~Message/~List, or ~Quit: ");
	console.aborted = false;
	var key;
	var last_request = 0;
	var request_interval = 60;	// seconds
	var valid_keys = "QLTM\rD";
	while(bbs.online && !console.aborted) {
		if(time() - last_request >= request_interval) {
			lib.request_active_users();
			last_request = time();
		}
		console.line_counter=0;	// defeat pause
		while(lib.sock.poll(0.1) && bbs.online) {
			var message = lib.receive_active_users();
			if(message) {
				var result = lib.parse_active_users(message, logon_callback, logoff_callback);
				if(result !== true)
					log(LOG_WARNING, format("%s: %s", result, JSON.stringify(message)));
			}
		}
		bbs.nodesync(true);
		if(console.current_column == 0 || console.line_counter != 0) {
			continue prompt;
		}
		key=console.inkey(K_UPPER, 500);
		if(key && valid_keys.indexOf(key) >= 0)
			break;
	}
	switch(key) {
		case 'D':
			for(var i in lib.sys_list)
				print(i + ' = ' + JSON.stringify(lib.sys_list[i]));
			break;
		case 'L':
			print("\1h\1cList\r\n");
			list_users();
			break;
		case 'T':
			printf("\1h\1cTelegram\r\n\r\n");
			var addr_list = userprops.get(lib.props_sent, "address", []);
			var last_send = userprops.get(lib.props_sent, "localtime");
			printf("\1n\1h\1yDestination (user@hostname): \1w");
			dest=console.getstr(get_default_dest(addr_list, last_send),64,K_EDIT|K_AUTODEL, addr_list);
			if(dest==null || dest=='' || console.aborted) {
				console.aborted = false;
				break;
			}
			if(!lib.dest_host(dest)) {
				alert("Invalid destination");
				break;
			}
			if((msg=getmsg())=='')
				break;
			send_msg(dest, msg);
			console.crlf();
			break;
		case 'M':
			print("\1h\1cMessage\r\n");
			var imsg_user = imsg_user_list();
			if(!imsg_user.length) {
				alert("No active users!\r\n");
				break;
			}
			done=false;
			while(bbs.online && !done && !console.aborted) {
				printf("\1[\1n\1h\x11\1n-[\1hQ\1nuit/\1hA\1nll]-\1h\x10 \1y%-25s \1c%s\1>"
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
						send_msg(dest, msg);
						console.crlf();
						break;
					case 'A':
						done=true;
						printf("\r\1n\1cSending message to \1h%s (%u users)\1>\r\n", "All", imsg_user.length);
						if((msg=getmsg())=='')
							break;
						for(var u in imsg_user) {
							dest=format("%s@%s"
								,imsg_user[u].name,imsg_user[u].host);
							printf("\r\1n\1cSending message: \1h%s\1>\r\n", dest);
							send_msg(dest, msg);
						}
						console.crlf();
						break;
				}
			}
			break;
		default:
			print("\1h\1cQuit");
			break prompt;
	}
}
