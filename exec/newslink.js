// newslink.js

// Synchronet Newsgroup Link/Gateway Module

// $Id$

// Configuration file (in ctrl/newslink.cfg) format:

// ;this line is a comment
// server	servername
// port		TCP port number (defaults to 119)
// user		username (optional)
// pass		password (optional)
// area		subboard (internal code) newsgroup flags [attachment_dir]
// ...

// Defined area flags:
// x		do not add tearlines & taglines to exported messages
// n		do not add "From Newsgroup" text to imported messages
// t		do not add tearline to imported messages
// a		convert extended-ASCII chars to ASCII on imported messages
// r		remove "Newsgroups:" header field from imported messages
// u		uudecode attachments
// i		import all (not just new articles)

const REVISION = "$Revision$".split(' ')[1];

printf("Synchronet NewsLink %s session started\r\n", REVISION);

var tearline = format("--- Synchronet %s%s-%s NewsLink %s\r\n"
					  ,system.version,system.revision,system.platform,REVISION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);
var antispam = format(".remove-%s-this"
					  ,random(50000).toString(36));

var cfg_fname = system.ctrl_dir + "newslink.cfg";

var global_flags = "";	// global flags  - area flags applied to all areas

load("sbbsdefs.js");
load("newsutil.js");	// write_news_header() and parse_news_header()

var debug = false;
var slave = false;
var reset_import_ptrs = false;		// Reset import pointers, import all messages
var update_import_ptrs = false;		// Update import pointers, don't import anything
var reset_export_ptrs = false;		// Reset export pointers, export all messages
var update_export_ptrs = false;		// Update export pointers, don't export anything
var email_addresses = true;			// Include e-mail addresses in headers
var import_amount = 0;				// Import a fixed number of messages per group
var lines_per_yield = 5;			// Release time-slices ever x number of lines

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
		case "disabled":
			printf("!NewsLink DISABLED in %s\r\n",cfg_fname);
			delete cfg_file;
			exit();
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
		case "flags":
			global_flags=str[1];
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
		case "tagline":
			str.shift();				// Remove first element (keyword)
			tagline=str.join(' ');		// Combine remaining elements (tagline)
			tagline+="\r\n";
			break;
		case "lines_per_yield":
			lines_per_yield=Number(str[1]);
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
	flags += global_flags;
	flags = flags.toLowerCase();

	printf("sub: %s, newsgroup: %s\r\n",sub,newsgroup);
	msgbase = new MsgBase(sub);
	if(msgbase.open!=undefined && msgbase.open()==false) {
		printf("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub);
		delete msgbase;
		continue;
	}

	attachment_dir=area[i][4];
	if(attachment_dir==undefined)
		attachment_dir=msgbase.cfg.data_dir+"attach";
	mkdir(attachment_dir);
	if(attachment_dir.substr(-1)!='/')
		attachment_dir+="/";

	md5_fname=attachment_dir + "md5.lst";
	md5_list=new Array();
	md5_file=new File(md5_fname);
	if(md5_file.open("r")) {
		md5_list=md5_file.readAll();
		md5_file.close();
	}

	/*********************/
	/* Read Pointer File */
	/*********************/
	export_ptr = 0;
	import_ptr = NaN;			// Set to highest possible message number (by default)
	if(flags.indexOf('i')>0)	// import all
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
			/* regenerate msg-id? */	false
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
		if(hdr.from_net_type	/* don't gate messages between net types */
			&& msgbase.cfg!=undefined && !(msgbase.cfg.settings&SUB_GATE))
			continue;

		body = msgbase.get_msg_body(
				 false	/* retrieve by offset */
				,ptr	/* message number */
				,true	/* remove ctrl-a codes */
				,true	/* rfc822 formatted text */
				,true	/* include tails */
				);
		if(body == null) {
			printf("!FAILED to read message number %ld\r\n",ptr);
			continue;
		}
		if(msgbase.cfg!=undefined && msgbase.cfg.settings&SUB_ASCII) {
			/* Convert Ex-ASCII chars to approximate ASCII equivalents */
			body = ascii_str(body);
			hdr.subject = ascii_str(hdr.subject);
		}
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

		if(!email_addresses)
			writeln(format("From: %s@%s",hdr.from,newsgroup));
		else if(hdr.from.indexOf('@')!=-1)
			writeln(format("From: %s%s",hdr.from,antispam));
		else if(hdr.from_net_type && hdr.from_net_addr!=null) {
			if(hdr.from_net_addr.indexOf('@')!=-1)
				writeln(format("From: \"%s\" <%s%s>"
					,hdr.from
					,hdr.from_net_addr,antispam));
			else
				writeln(format("From: \"%s\" <%s@%s%s>"
					,hdr.from
					,hdr.from.replace(/ /g,".").toLowerCase()
					,hdr.from_net_addr,antispam));
		}
		else
			writeln(format("From: \"%s\" <%s@%s%s>"
				,hdr.from
				,hdr.from.replace(/ /g,".").toLowerCase()
				,system.inetaddr,antispam));

		if(hdr.newsgroups==undefined)
			hdr.newsgroups=newsgroup;

		if(hdr.from_org==undefined && !hdr.from_net_type)
			hdr.from_org=system.name;

		write_news_header(hdr,writeln); // from newsutil.js

		writeln("X-Gateway: "
			+ system.inetaddr
			+ " [Synchronet "
			+ system.version + system.revision 
			+ "-" + system.platform
			+ " NewsLink " + REVISION
			+ "]"
			);

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
		if(ptr > last_msg || isNaN(ptr))
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
		body="";
		header=true;
		var hdr={ from: "", to: newsgroup, subject: "", id: "" };
		var line_counter=0;
		var recv_lines=0;
		var file;
		var md5;
		while(socket.is_connected) {

			if(recv_lines && lines_per_yield && (recv_lines%lines_per_yield)==0)
				sleep(1);

			line = socket.recvline(512 /*maxlen*/, 300 /*timeout*/);

			if(line==null) {
				print("!TIMEOUT waiting for text line\r\n");
				break;
			}

			recv_lines++;

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

				if(flags.indexOf('u')>=0) {	// uudecode attachments
					if(line.substr(0,6)=="begin ") {
						// Parse uuencode header
						arg=line.split(' ');
						arg.splice(0,2);	// strip "begin 666 "
						fname=getfilename(arg.join(" "));
						if(file_exists(attachment_dir + fname)) // generate unique name, if necessary
							fname=ptr + "_" + fname;
						fname=attachment_dir + fname;

						file=new File(fname);
						file.uue=true;
						if(file.open("w+b"))
							printf("Receiving/decoding attachment: %s\r\n",file.name);
						else
							printf("!ERROR %s opening %s\r\n",errno_str,file.name);
						continue;
					} 
					if(file!=undefined && file.is_open==true) {
						if(line=="end") {
							md5=file.md5_hex;
							file.close();
						} else
							if(!file.write(line))
								printf("!ERROR decoding/writing: %s\r\n",line);
						continue;
					}
				}
				body += line;
				body += "\r\n";
				line_counter++;
				continue;
			}
			//print(line);

			parse_news_header(hdr,line);	// from newsutil.js
		}

		
		if(file!=undefined) {
			var partial=false;
			if(file.is_open==true) { /* Partial attachment? */
				md5=file.md5_hex;
				file.close();
				partial=true;
			}
			for(mi=0;mi<md5_list.length;mi++)
				if(md5_list[mi]==md5)
					break;
			if(mi<md5_list.length) {
				printf("Duplicate MD5 digest found: %s\r\n",md5);
				if(file_remove(file.name))
					printf("Duplicate file removed: %s\r\n",file.name);
				else
					printf("!ERROR removing duplicate file: %s\r\n",file.name);
				continue;
			}
			md5_list.push(md5);
			if(partial)
				file_rename(file.name,hdr.subject.replace(/\//g,'.'));
		}

		if(truncsp(body).length==0) {
			printf("Message %lu not imported (blank)",ptr);
			continue;
		}

		if(hdr.to==newsgroup && hdr.newsgroups!=undefined)
			hdr.to=hdr.newsgroups;

		// Duplicate/looped message detection here
		if(hdr.id.indexOf('@' + system.inetaddr)!=-1)
			continue;
		if(hdr.path
			&& hdr.path.indexOf(system.inetaddr)!=-1)
			continue;
		if(hdr.gateway
			&& hdr.gateway.indexOf(system.inetaddr)!=-1)
			continue;

		if(system.trashcan("subject",hdr.subject)) {
			printf("!BLOCKED subject: %s\r\n",hdr.subject);
			var reason = format("Blocked subject (%s)",hdr.subject);
			system.spamlog("NNTP","NOT IMPORTED",reason,hdr.from,server,hdr.to);
			continue;
		}

		if(flags.indexOf('n')==-1)
			body=format("\1n\1b\1hFrom Newsgroup\1n\1b: \1h\1c%s\1n\r\n\r\n",newsgroup) + body;

		if(flags.indexOf('a')>=0) {	// import ASCII only (convert ex-ASCII to ASCII)
			body = ascii_str(body);
			hdr.subject = ascii_str(hdr.subject);
		}
		if(flags.indexOf('r')>=0) 	// remove "Newsgroups:" header field
			delete hdr.newsgroups;

		hdr.from_net_type=NET_INTERNET;
//		hdr.from_net_addr=hdr.from;
		if(flags.indexOf('t')==-1)
			body += tearline;
		if(msgbase.save_msg(hdr,body)) {
			imported++;
			printf("Message %lu imported into %s (%lu of %lu total) %lu lines\r\n"
				,ptr, sub, imported, msgbase.total_msgs, line_counter);
		} else
			printf("!IMPORT %lu ERROR: %s\r\n", ptr, msgbase.last_error);
	}

	/* Save Attachment MD5 history */
	if(md5_list.length) {
		if(md5_file.open("w")) {
			md5_file.writeAll(md5_list);
			md5_file.close();
		}
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

printf("Synchronet NewsLink %s session complete (%lu exported, %lu imported)\r\n"
	   ,REVISION, exported, imported);

/* End of newslink.js */
