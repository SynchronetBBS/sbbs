// newslink.js

// Synchronet Newsgroup Link/Gateway Module

// Configuration file (in ctrl/newslink.cfg) format:

// ;this line is a comment
// server	servername
// port		TCP port number (defaults to 119)
// user		username (optional)
// pass		password (optional)
// area		subboard (internal code) newsgroup
// ...

const VERSION="1.00 Beta"

printf("Synchronet NewsLink session started (v%s)\r\n", VERSION);

var tearline = format("--- Synchronet NewsLink v%s\r\n",VERSION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);
var antispam = format("remove-%s-this."
					  ,random(50000).toString(36));

var cfg_fname = system.ctrl_dir + "newslink.cfg";

load("sbbsdefs.js");

var debug = false;
var slave = false;
var reset_import_ptrs = false;		// Reset import pointers, import all messages
var update_import_ptrs = false;		// Update import pointers, don't import anything
var reset_export_ptrs = false;		// Reset export pointers, export all messages
var update_export_ptrs = false;		// Update export pointers, don't export anything
var email_addresses = true;			// Include e-mail addresses in headers
var import_amount = 0;				// Import a fixed number of messages per group

// Parse arguments
for(i=0;i<argc;i++) {
	if(argv[i].toLowerCase()=="-d")			// debug
		debug = true;
	else if(argv[i].toLowerCase()=="-ri")	// reset import pointers (import all)
		reset_import_ptrs = true;
	else if(argv[i].toLowerCase()=="-ui")	// update import pointers (import none)
		update_import_ptrs = true;
	else if(argv[i].toLowerCase()=="-re")	// reset export pointers (export all)
		reset_export_ptrs = true;
	else if(argv[i].toLowerCase()=="-ue")	// update export pointers (export none)
		update_export_ptrs = true;
	else if(argv[i].toLowerCase()=="-ne")	// no e-mail addresses
		email_addresses = false;
	else if(argv[i].toLowerCase()=="-nm")	// no mangling of e-mail addresses
		antispam = "";
	else if(argv[i].toLowerCase()=="-ix") 	// import a fixed number of messages
	{
		import_amount = Number(argv[i+1]);
		if(import_amount)
			i++;
		else
			import_amount = 500;			// default 500
	}
	else
		cfg_fname = argv[i];
}

// Write a string to the server socket
function write(str)
{
	socket.send(str);
}

function writeln(str)
{
	if(debug)
		printf("cmd: %s\r\n",str);
	write(str + "\r\n");
}

function readln(str)
{
	rsp = socket.readln();
	if(debug)
		printf("rsp: %s\r\n",rsp);
	return(rsp);
}

var server;
var port=119;
var username;
var password;
area = new Array();

/******************************/
/* Read/Parse the Config File */
/******************************/

cfg_file = new File(cfg_fname);
if(!cfg_file.open("r")) {
	printf("!Error %d opening %s\r\n",errno,cfg_fname);
	delete cfg_file;
	exit();
}

while(!cfg_file.eof) {
	line = cfg_file.readln();
	if(line==null || line[0] == ';' || !line.length)
		continue;
	str=line.split(/\s+/);
	switch(str[0].toLowerCase()) {
		case "server":
			server=str[1];
			break;
		case "port":
			port=Number(str[1]);
			break;
		case "user":
			username=str[1];
			break;
		case "pass":
			password=str[1];
			break;
		case "area":
			area.push(str);
			break;
		case "debug":
			debug=true;
			break;
		case "slave":
			slave=true;
			break;
		default:
			printf("!UNRECOGNIZED configuration keyword: %s\r\n",str[0]);
			break;
	}
}
delete cfg_file;

printf("server: %s\r\n",server);
if(debug) {
	printf("username: %s\r\n",username);
	printf("password: %s\r\n",password);
}
printf("%ld areas\r\n",area.length);

if(server==undefined || !server.length) {
	print("!No news server specified");
	exit();
}

printf("Connecting to %s port %d ...\r\n",server,port);
socket = new Socket();
//socket.debug=true;
if(!socket.connect(server,port)) {
	printf("!Error %d connecting to %s port %d\r\n"
		,socket.last_error,server,port);
	delete socket;
	exit();
}
print("Connected");
readln();

if(username!=undefined && username.length) {
	print("Authenticating...");
	writeln(format("AUTHINFO USER %s",username));
	readln();
	if(password!=undefined && password.length) {
		writeln(format("AUTHINFO PASS %s",password));
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			printf("!Authentication FAILURE: %s\r\n", rsp);
			delete socket;
			exit();
		}
	}
	print("Authenticated");
}

if(slave) {
	writeln("slave");
	readln();
}

/******************************/
/* Export and Import Messages */
/******************************/
var exported=0;
var imported=0;

printf("Scanning %lu message bases...\r\n",area.length);
for(i in area) {
	
	if(!socket.is_connected) {
		print("Disconnected");
		break;
	}

//	printf("%s\r\n",area[i].toString());
	
	sub = area[i][1];
	newsgroup = area[i][2];
	flags = area[i][3];
	if(flags==undefined)
		flags="";
	flags = flags.toLowerCase();

	printf("sub: %s, newsgroup: %s\r\n",sub,newsgroup);
	msgbase = new MsgBase(sub);
	if(msgbase == null) {
		printf("!ERROR opening msgbase: %s\r\n",sub);
		continue;
	}
	/*********************/
	/* Read Pointer File */
	/*********************/
	export_ptr = 0;
	import_ptr = 0;
	ptr_fname = msgbase.file + ".snl";
	ptr_file = new File(ptr_fname);
	if(ptr_file.open("rb")) {
		export_ptr = ptr_file.readBin();
		printf("%s export ptr: %ld\r\n",sub,export_ptr);
		import_ptr = ptr_file.readBin();
		printf("%s import ptr: %ld\r\n",sub,import_ptr);
		ptr_file.close();
	}

	if(reset_export_ptrs)
		ptr = 0;
	else if(update_export_ptrs)
		ptr = msgbase.last_msg;
	else 
		ptr = export_ptr;

	if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;
	else
		ptr++;

	/*************************/
	/* EXPORT Local Messages */
	/*************************/
	if(debug)
		print("exporting local messages");
	for(;socket.is_connected && ptr<=msgbase.last_msg;ptr++) {
		console.line_counter = 0;
		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr,
			/* regenerate msg-id? */	true
			);
		if(hdr == null)
			continue;
		if(hdr.attr&MSG_DELETE)	/* marked for deletion */
			continue;
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED))
			continue;
		if(hdr.attr&MSG_PRIVATE)/* no private messages on NNTP */
			continue;
		if(hdr.from_net_type==NET_INTERNET)	/* no dupe loop */
			continue;

		body = msgbase.get_msg_body(false, ptr
				,true	/* remove ctrl-a codes */
				,true	/* rfc822 formatted text */
				,true	/* include tails */);
		if(body == null) {
			printf("!FAILED to read message number %ld\r\n",ptr);
			continue;
		}
		body = ascii_str(body);
		if(flags.indexOf('x')==-1) {
			body += tearline;
			body += tagline;
		}
		if(0) 
			writeln(format("IHAVE %s",hdr.id));
		else
			writeln("POST");

		rsp = readln();
		if(rsp==null || rsp[0]!='3') {
			printf("!POST FAILURE: %s\r\n",rsp);
			break;
		}

		writeln("Path: " + hdr.path);
		if(!email_addresses)
			writeln(format("From: %s@%s",hdr.from,newsgroup));
		else if(hdr.from.indexOf('@')!=-1)
			writeln(format("From: %s%s",antispam,hdr.from));
		else if(hdr.from_net_type && hdr.from_net_addr!=null) {
			if(hdr.from_net_addr.indexOf('@')!=-1)
				writeln(format("From: \"%s\" <%s%s>"
					,hdr.from
					,antispam,hdr.from_net_addr));
			else
				writeln(format("From: \"%s\" <%s@%s%s>"
					,hdr.from
					,hdr.from.replace(/ /g,".").toLowerCase()
					,antispam,hdr.from_net_addr));
		}
		else
			writeln(format("From: \"%s\" <%s@%s%s>"
				,hdr.from
				,hdr.from.replace(/ /g,".").toLowerCase()
				,antispam,system.inetaddr));
		writeln("To: " + hdr.to);
		writeln("X-Comment-To: " + hdr.to);
		writeln("Subject: " + hdr.subject);
		writeln("Message-ID: " + hdr.id);
		writeln("Date: " + hdr.date);
		writeln("Newsgroups: " + newsgroup);
		if(hdr.reply_id!=undefined)
			writeln("References: " + hdr.reply_id);

		/* FidoNet header */
		if(hdr.ftn_pid!=undefined)
			writeln("X-FTN-PID: " + hdr.ftn_pid);
		if(hdr.ftn_area!=undefined)
			writeln("X-FTN-AREA: " + hdr.ftn_area);
		if(hdr.ftn_flags!=undefined)
			writeln("X-FTN-FLAGS: " + hdr.ftn_flags);
		if(hdr.ftn_msgid!=undefined)
			writeln("X-FTN-MSGID: " + hdr.ftn_msgid);
		if(hdr.ftn_reply!=undefined)
			writeln("X-FTN-REPLY: " + hdr.ftn_reply);

		writeln("");
		if(hdr.to.toLowerCase()!="all") {
			writeln("  To: " + hdr.to);
		}
		write(body);
		writeln(".");
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			printf("!POST FAILURE: %s\r\n",rsp);
			break;
		}
		printf("Exported message %lu to newsgroup: %s\r\n",ptr,newsgroup);
		exported++;
	}
	if(ptr > msgbase.last_msg)
		ptr = msgbase.last_msg;
	export_ptr = ptr;

	/***************************/
	/* IMPORT Network Messages */
	/***************************/	

	writeln(format("GROUP %s",newsgroup));
	rsp = readln();
	if(rsp==null || rsp[0]!='2') {
		printf("!GROUP %s FAILURE: %s\r\n",newsgroup,rsp);
		delete ptr_file;
		delete msgbase;
		continue;
	}
	str = rsp.split(' ');

	first_msg = Number(str[2]);
	last_msg = Number(str[3]);

	if(reset_import_ptrs)
		ptr = 0;
	else if(update_import_ptrs)
		ptr = last_msg;
	else if(import_amount)
		ptr = last_msg - import_amount;
	else
		ptr = import_ptr;

	printf("%s import ptr: %ld, last_msg: %ld\r\n",newsgroup,ptr,last_msg);

	if(ptr < first_msg)
		ptr = first_msg;
	else {
		if(ptr > last_msg)
			ptr = last_msg;
		ptr++;
	}
	for(;socket.is_connected && ptr<=last_msg;ptr++) {
		console.line_counter = 0;
		writeln(format("ARTICLE %lu",ptr));
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			if(debug)
				printf("!ARTICLE %lu ERROR: %s\r\n",ptr,rsp);
			continue;
		}
		if(flags.indexOf('n')==-1)
			body=format("\1n\1b\1hFrom Newsgroup\1n\1b: \1h\1c%s\1n\r\n\r\n",newsgroup);
		else
			body="";
		header=true;
		var hdr={ from: "", to: newsgroup, subject: "", id: "" };
		while(socket.is_connected) {

			line = socket.recvline(512 /*maxlen*/, 300 /*timeout*/);

			if(line==null) {
				print("!TIMEOUT waiting for text line");
				break;
			}

			//printf("msgtxt: %s\r\n",line);

			if(line==".") {
//				print("End of message text");
				break;
			}
			if(line=="" && header) {
				header=false;
				continue;
			}

			if(!header) {	/* Body text, append to 'body' */
				if(line.charAt(0)=='.')
					line=line.slice(1);		// Skip prepended dots
				body += line;
				body += "\r\n";
				continue;
			}
			//print(line);

			/* Parse header lines */
			if((sp=line.indexOf(':'))==-1)
				continue;

			data=line.slice(sp+1);
			while(data.charAt(0)==' ')	// skip prepended spaces
				data=data.slice(1);

			line=line.substr(0,sp);
			while(line.charAt(0)==' ')	// skip prepended spaces
				line=line.slice(1);

			switch(line.toLowerCase()) {
				case "to":
				case "apparently-to":
				case "x-comment-to":
					hdr.to=data;
					break;
				case "newsgroups":
					if(hdr.to==newsgroup)
						hdr.to=data;
					hdr.newsgroups=data;
					break;
				case "path":
					hdr.path=data;
					break;
				case "from":
					hdr.from=data;
					break;
				case "date":
					hdr.date=data;
					break;
				case "subject":
					hdr.subject=data;
					break;
				case "message-id":
					hdr.id=data;
					break;
				case "references":
					hdr.reply_id=data;
					break;
				case "nntp-posting-host":
					hdr.nntp_posting_host=data;
					break;
				/* FidoNet headers */
				case "x-ftn-pid":
					hdr.ftn_pid=data;
					break;
				case "x-ftn-area":
					hdr.ftn_area=data;
					break;
				case "x-ftn-flags":
					hdr.ftn_flags=data;
					break;
				case "x-ftn-msgid":
					hdr.ftn_msgid=data;
					break;
				case "x-ftn-reply":
					hdr.ftn_reply=data;
					break;
			}
		}
		// Duplicate/looped message detection here
		if(hdr.id.indexOf('@' + system.inetaddr)!=-1)
			continue;
		if(hdr.path.indexOf(system.inetaddr)!=-1)
			continue;
		if(0 && hdr.nntp_posting_host!=undefined) {
			if(hdr.nntp_posting_host.indexOf(system.inetaddr)!=-1)
				continue;
			if(hdr.nntp_posting_host.indexOf(system.host_name)!=-1)
				continue;
			if(hdr.nntp_posting_host == socket.local_ip_address)
				continue;
		}

		if(system.trashcan("subject",hdr.subject)) {
			printf("!BLOCKED subject: %s\r\n",hdr.subject);
			var reason = format("Blocked subject (%s)",hdr.subject);
			system.spamlog("NNTP",reason,hdr.from,server,hdr.to);
			continue;
		}

		hdr.from_net_type=NET_INTERNET;
//		hdr.from_net_addr=hdr.from;
		if(flags.indexOf('t')==-1)
			body += tearline;
		if(msgbase.save_msg(hdr,body)) {
			imported++;
			printf("Message %lu imported into %s (%lu of %lu total)\r\n"
				,ptr,sub,imported,msgbase.total_msgs);
		} else
			printf("!IMPORT %lu ERROR: %s\r\n", ptr, msgbase.last_error);
	}
	if(ptr > last_msg)
		ptr = last_msg;
	import_ptr = ptr;

	/* Save Pointers */
	if(!ptr_file.open("wb"))
		printf("!ERROR %d creating/opening %s\r\n",errno,ptr_fname);
	else {
		ptr_file.writeBin(export_ptr);
		ptr_file.writeBin(import_ptr);
		ptr_file.close();
	}
	delete ptr_file;
	delete msgbase;
}

writeln("quit");
readln();

delete socket;

printf("Synchronet NewsLink session complete (%lu exported, %lu imported)\r\n"
	   ,exported, imported);

/* End of newslink.js */
